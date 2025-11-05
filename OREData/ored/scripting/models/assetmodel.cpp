/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/scripting/models/blackscholes.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

AssetModel::AssetModel(const Model::Type type, const Size paths, const std::string& currency,
                       const Handle<YieldTermStructure>& curve, const std::string& index,
                       const std::string& indexCurrency, const Handle<AssetModelWrapper>& model,
                       const std::set<Date>& simulationDates,
                       const ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig, const std::string& calibration,
                       const std::vector<Real>& calibrationStrikes, const Params& params)
    : AssetModel(type, paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, {currency}, model, {},
                 simulationDates, iborFallbackConfig, calibration, {{index, calibrationStrikes}}, params) {}

AssetModel::AssetModel(
    const Model::Type type, const Size paths, const std::vector<std::string>& currencies,
    const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const std::set<std::string>& payCcys, const Handle<AssetModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const std::set<Date>& simulationDates, const ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
    const std::string& calibration, const std::map<std::string, std::vector<Real>>& calibrationStrikes,
    const Params& params)
    : ModelImpl(type, params, curves.at(0)->dayCounter(), paths, currencies, irIndices, infIndices, indices,
                indexCurrencies, simulationDates, iborFallbackConfig),
      curves_(curves), fxSpots_(fxSpots), payCcys_(payCcys), model_(model), correlations_(correlations),
      calibration_(calibration), calibrationStrikes_(calibrationStrikes) {

    // check inputs

    QL_REQUIRE(!model_.empty(), "model is empty");
    QL_REQUIRE(!curves_.empty(), "no curves given");
    QL_REQUIRE(currencies_.size() == curves_.size(), "number of currencies (" << currencies_.size()
                                                                              << ") does not match number of curves ("
                                                                              << curves_.size() << ")");
    QL_REQUIRE(currencies_.size() == fxSpots_.size() + 1,
               "number of currencies (" << currencies_.size() << ") does not match number of fx spots ("
                                        << fxSpots_.size() << ") + 1");

    QL_REQUIRE(indices_.size() == model_->processes().size(),
               "mismatch of processes size (" << model_->processes().size() << ") and number of indices ("
                                              << indices_.size() << ")");

    for (auto const& c : payCcys) {
        QL_REQUIRE(std::find(currencies_.begin(), currencies_.end(), c) != currencies_.end(),
                   "pay ccy '" << c << "' not found in currencies list.");
    }

    QL_REQUIRE(calibration_ == "ATM" || calibration_ == "Deal" || calibration == "Smile",
               "calibration '" << calibration_ << "' invalid, expected one of ATM, Deal, Smile");

    // register with observables

    for (auto const& o : fxSpots_)
        registerWith(o);
    for (auto const& o : correlations_)
        registerWith(o.second);

    registerWith(model_);

    // FD only: for one (or no) underlying, everything works as usual

    if (type_ == Type::MC || model_->processes().size() <= 1)
        return;

    // if we have one underlying + one FX index, we do a 1D PDE with a quanto adjustment under certain circumstances

    if (model_->processes().size() == 2) {
        // check whether we have exactly one pay ccy ...
        if (payCcys_.size() == 1) {
            std::string payCcy = *payCcys_.begin();
            // ... and the second index is an FX index suitable to do a quanto adjustment
            // from the first index's currency to the pay ccy ...
            std::string mainIndexCcy = indexCurrencies_[0];
            if (indices_[0].isFx()) {
                mainIndexCcy = indices_[0].fx()->targetCurrency().code();
            }
            if (indices_[1].isFx()) {
                std::string ccy1 = indices_[1].fx()->sourceCurrency().code();
                std::string ccy2 = indices_[1].fx()->targetCurrency().code();
                if ((ccy1 == mainIndexCcy && ccy2 == payCcy) || (ccy1 == payCcy && ccy2 == mainIndexCcy)) {
                    applyQuantoAdjustment_ = true;
                    quantoSourceCcyIndex_ = std::distance(
                        currencies.begin(), std::find(currencies.begin(), currencies.end(), mainIndexCcy));
                    quantoTargetCcyIndex_ =
                        std::distance(currencies.begin(), std::find(currencies.begin(), currencies.end(), payCcy));
                    quantoCorrelationMultiplier_ = ccy2 == payCcy ? 1.0 : -1.0;
                }
                DLOG("AssetModel model will be run for index '"
                     << indices_[0].name() << "' with a quanto-adjustment " << currencies_[quantoSourceCcyIndex_]
                     << " => " << currencies_[quantoTargetCcyIndex_] << " derived from index '" << indices_[1].name()
                     << "'");
                return;
            }
        }
    }

    // otherwise we need more than one dimension, which we currently not support

    QL_FAIL("AssetModel: model does not support multi-dim fd schemes currently, use mc instead.");

} // AssetModel ctor

void AssetModel::performCalculations() const {

    QL_REQUIRE(!inTrainingPhase_, "AssetModel::performCalculations(): state inTrainingPhase should be false, this was "
                                  "not resetted appropriately.");

    referenceDate_ = curves_.front()->referenceDate();

    // set up time grid

    effectiveSimulationDates_ = model_->effectiveSimulationDates();

    std::vector<Real> times;
    for (auto const& d : effectiveSimulationDates_) {
        times.push_back(timeFromReference(d));
    }

    timeGrid_ = model_->discretisationTimeGrid();
    positionInTimeGrid_.resize(times.size());
    for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
        positionInTimeGrid_[i] = timeGrid_.index(times[i]);

    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();

    // nothing to do if we do not have any indices

    if (indices_.empty())
        return;

    // do the model specific calculations

    performModelCalculations();
}

void AssetModel::initUnderlyingPathsMc() const {
    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<RandomVariable>(model_->processes().size(), RandomVariable(size(), 0.0));
        if (trainingSamples() != Null<Size>())
            underlyingPathsTraining_[d] =
                std::vector<RandomVariable>(model_->processes().size(), RandomVariable(trainingSamples(), 0.0));
    }
}

void AssetModel::setReferenceDateValuesMc() const {
    for (Size l = 0; l < indices_.size(); ++l) {
        underlyingPaths_[*effectiveSimulationDates_.begin()][l].setAll(initialValue(l));
        if (trainingSamples() != Null<Size>()) {
            underlyingPathsTraining_[*effectiveSimulationDates_.begin()][l].setAll(initialValue(l));
        }
    }
}

Matrix AssetModel::getCorrelation() const {
    Matrix correlation(indices_.size(), indices_.size(), 0.0);
    for (Size i = 0; i < indices_.size(); ++i)
        correlation[i][i] = 1.0;
    for (auto const& c : correlations_) {
        IndexInfo inf1(c.first.first), inf2(c.first.second);
        auto ind1 = std::find(indices_.begin(), indices_.end(), inf1);
        auto ind2 = std::find(indices_.begin(), indices_.end(), inf2);
        if (ind1 != indices_.end() && ind2 != indices_.end()) {
            // EQ, FX, COMM index
            Size i1 = std::distance(indices_.begin(), ind1);
            Size i2 = std::distance(indices_.begin(), ind2);
            correlation[i1][i2] = correlation[i2][i1] = c.second->correlation(0.0); // we assume a constant correlation!
        }
    }
    DLOG("AssetModel correlation matrix:");
    DLOGGERSTREAM(correlation);
    return correlation;
}

std::vector<Real> AssetModel::getCalibrationStrikes() const {
    std::vector<Real> calibrationStrikes;
    if (calibration_ == "ATM") {
        calibrationStrikes.resize(indices_.size(), Null<Real>());
    } else if (calibration_ == "Deal" || calibration_ == "Smile") {
        for (Size i = 0; i < indices_.size(); ++i) {
            auto f = calibrationStrikes_.find(indices_[i].name());
            if (f != calibrationStrikes_.end() && !f->second.empty()) {
                calibrationStrikes.push_back(f->second[0]);
                TLOG("calibration strike for index '" << indices_[i] << "' is " << f->second[0]);
            } else {
                calibrationStrikes.push_back(Null<Real>());
                TLOG("calibration strike for index '" << indices_[i] << "' is ATMF");
            }
        }
    }
    return calibrationStrikes;
}

const Date& AssetModel::referenceDate() const {
    calculate();
    return referenceDate_;
}

RandomVariable AssetModel::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {

    Date effFwd = fwd;
    if (indices_[indexNo].isComm()) {
        Date expiry = indices_[indexNo].comm(d)->expiryDate();
        // if a future is referenced we set the forward date effectively used below to the future's expiry date
        if (expiry != Date())
            effFwd = expiry;
        // if the future expiry is past the obsdate, we return the spot as of the obsdate, i.e. we
        // freeze the future value after its expiry, but keep it available for observation
        // TOOD should we throw an exception instead?
        effFwd = std::max(effFwd, d);
    }

    RandomVariable res;

    if (type_ == Type::FD) {

        res = RandomVariable(underlyingValues_);

        // set the observation time in the result random variable
        res.setTime(timeFromReference(d));

    } else {

        QL_REQUIRE(underlyingPaths_.find(d) != underlyingPaths_.end(), "did not find path for " << d);
        res = underlyingPaths_.at(d).at(indexNo);
    }

    // compute forwarding factor and multiply the result by this factor

    if (effFwd != Null<Date>()) {
        res *= RandomVariable(size(), compoundingFactor(indexNo, effFwd, d));
    }

    return res;
}

RandomVariable AssetModel::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    // ensure a valid fixing date
    effFixingDate = irIndices_.at(indexNo).second->fixingCalendar().adjust(effFixingDate);
    return RandomVariable(size(), irIndices_.at(indexNo).second->fixing(effFixingDate));
}

RandomVariable AssetModel::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    return RandomVariable(size(), infIndices_.at(indexNo).second->fixing(effFixingDate));
}

RandomVariable AssetModel::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                      const Date& start, const Date& end, const Real spread, const Real gearing,
                                      const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                      const bool includeSpread, const Real cap, const Real floor,
                                      const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto index = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
    QL_REQUIRE(index != irIndices_.end(),
               "AssetModel::fwdCompAvg(): did not find ir index " << indexInput << " - this is unexpected.");
    auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index->second);
    QL_REQUIRE(on, "AssetModel::fwdCompAvg(): expected on index for " << indexInput);
    // if we want to support cap / floors we need the OIS CF surface
    QL_REQUIRE(cap > 999998.0 && floor < -999998.0,
               "AssetModel:fwdCompAvg(): cap (" << cap << ") / floor (" << floor << ") not supported");
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> coupon;
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> pricer;
    if (isAvg) {
        coupon = QuantLib::ext::make_shared<QuantExt::AverageONIndexedCoupon>(
            end, 1.0, start, end, on, gearing, spread, rateCutoff, on->dayCounter(), lookback * Days, fixingDays);
        pricer = QuantLib::ext::make_shared<AverageONIndexedCouponPricer>();
    } else {
        coupon = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(
            end, 1.0, start, end, on, gearing, spread, Date(), Date(), on->dayCounter(), false, includeSpread,
            lookback * Days, rateCutoff, fixingDays);
        pricer = QuantLib::ext::make_shared<OvernightIndexedCouponPricer>();
    }
    coupon->setPricer(pricer);
    return RandomVariable(size(), coupon->rate());
}

RandomVariable AssetModel::getDiscount(const Size idx, const Date& s, const Date& t) const {
    return RandomVariable(size(), curves_.at(idx)->discount(t) / curves_.at(idx)->discount(s));
}

RandomVariable AssetModel::getNumeraire(const Date& s) const {
    if (!applyQuantoAdjustment_)
        return RandomVariable(size(), 1.0 / curves_.at(0)->discount(s));
    else
        return RandomVariable(size(), 1.0 / curves_.at(quantoTargetCcyIndex_)->discount(s));
}

Real AssetModel::getFxSpot(const Size idx) const { return fxSpots_.at(idx)->value(); }

RandomVariable AssetModel::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                               const QuantLib::ext::optional<long>& memSlot, const RandomVariable& addRegressor1,
                               const RandomVariable& addRegressor2) const {

    calculate();

    if (type_ == Type::FD) {

        // handle FD calculation

        QL_REQUIRE(!memSlot, "AssetModel::npv(): mem slot not allowed");
        QL_REQUIRE(!filter.initialised(), "AssetModel::npv(). filter not allowed");
        QL_REQUIRE(!addRegressor1.initialised(), "AssetModel::npv(). addRegressor1 not allowed");
        QL_REQUIRE(!addRegressor2.initialised(), "AssetModel::npv(). addRegressor2 not allowed");

        Real t1 = amount.time();
        Real t0 = timeFromReference(obsdate);

        // handle case when amount is deterministic

        if (amount.deterministic()) {
            RandomVariable result(amount);
            result.setTime(t0);
            return result;
        }

        // handle stochastic amount

        QL_REQUIRE(t1 != Null<Real>(),
                   "AssetModel::npv(): can not roll back amount wiithout time attached (to t0=" << t0 << ")");

        // might throw if t0, t1 are not found in timeGrid_

        Size ind1 = timeGrid_.index(t1);
        Size ind0 = timeGrid_.index(t0);

        // check t0 <= t1, i.e. ind0 <= ind1

        QL_REQUIRE(ind0 <= ind1, "AssetModel::npv(): can not roll back from t1= "
                                     << t1 << " (index " << ind1 << ") to t0= " << t0 << " (" << ind0 << ")");

        // if t0 = t1, no rollback is necessary and we can return the input random variable

        if (ind0 == ind1)
            return amount;

        // if t0 < t1, we roll back on the time grid

        Array workingArray(amount.size());
        amount.copyToArray(workingArray);

        for (int j = static_cast<int>(ind1) - 1; j >= static_cast<int>(ind0); --j) {
            solver_->rollback(workingArray, timeGrid_[j + 1], timeGrid_[j], 1, 0);
        }

        // return the rolled back value

        return RandomVariable(workingArray, t0);

    } else if (type_ == Type::MC) {

        // handle MC calculation

        // short cut, if amount is deterministic and no memslot is given

        if (amount.deterministic() && !memSlot)
            return amount;

        // if obsdate is today, take a plain expectation

        if (obsdate == referenceDate())
            return expectation(amount);

        // build the state

        std::vector<const RandomVariable*> state;

        if (!underlyingPaths_.empty()) {
            for (auto const& r : underlyingPaths_.at(obsdate))
                state.push_back(&r);
        }

        Size nModelStates = state.size();

        if (addRegressor1.initialised() && (memSlot || !addRegressor1.deterministic()))
            state.push_back(&addRegressor1);
        if (addRegressor2.initialised() && (memSlot || !addRegressor2.deterministic()))
            state.push_back(&addRegressor2);

        Size nAddReg = state.size() - nModelStates;

        // if the state is empty, return the plain expectation (no conditioning)

        if (state.empty()) {
            return expectation(amount);
        }

        // the regression model is given by coefficients and an optional coordinate transform

        Array coeff;
        Matrix coordinateTransform;

        // if a memSlot is given and coefficients / coordinate transform are stored, we use them

        bool haveStoredModel = false;

        if (memSlot) {
            if (auto it = storedRegressionModel_.find(*memSlot); it != storedRegressionModel_.end()) {
                coeff = std::get<0>(it->second);
                coordinateTransform = std::get<2>(it->second);
                QL_REQUIRE(std::get<1>(it->second) == state.size(),
                           "AssetModel::npv(): stored regression coefficients at mem slot "
                               << *memSlot << " are for state size " << std::get<1>(it->second)
                               << ", actual state size is " << state.size()
                               << " (before possible coordinate transform).");
                haveStoredModel = true;
            }
        }

        // if we do not have retrieved a model in the previous step, we create it now

        std::vector<RandomVariable> transformedState;

        if (!haveStoredModel) {

            // factor reduction to reduce dimensionalitty and handle collinearity

            if (params_.regressionVarianceCutoff != Null<Real>()) {
                coordinateTransform = pcaCoordinateTransform(state, params_.regressionVarianceCutoff);
                transformedState = applyCoordinateTransform(state, coordinateTransform);
                state = vec2vecptr(transformedState);
            }

            // train coefficients

            coeff =
                regressionCoefficients(amount, state,
                                       multiPathBasisSystem(state.size(), params_.regressionOrder, params_.polynomType,
                                                            {}, std::min(size(), trainingSamples())),
                                       filter, RandomVariableRegressionMethod::QR);
            DLOG("AssetModel::npv(" << ore::data::to_string(obsdate) << "): regression coefficients are " << coeff
                                    << " (got model state size " << nModelStates << " and " << nAddReg
                                    << " additional regressors, coordinate transform " << coordinateTransform.columns()
                                    << " -> " << coordinateTransform.rows() << ")");

            // store model if requried

            if (memSlot) {
                storedRegressionModel_[*memSlot] = std::make_tuple(coeff, nModelStates, coordinateTransform);
            }

        } else {

            // apply the stored coordinate transform to the state

            if (!coordinateTransform.empty()) {
                transformedState = applyCoordinateTransform(state, coordinateTransform);
                state = vec2vecptr(transformedState);
            }
        }

        // compute conditional expectation and return the result

        return conditionalExpectation(state,
                                      multiPathBasisSystem(state.size(), params_.regressionOrder, params_.polynomType,
                                                           {}, std::min(size(), trainingSamples())),
                                      coeff);
    } else {
        QL_FAIL("AssetModel::npv(): unhandled type, internal error.");
    }
}

void AssetModel::releaseMemory() {
    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();
}

void AssetModel::resetNPVMem() { storedRegressionModel_.clear(); }

void AssetModel::toggleTrainingPaths() const {
    std::swap(underlyingPaths_, underlyingPathsTraining_);
    inTrainingPhase_ = !inTrainingPhase_;
}

Size AssetModel::trainingSamples() const { return params_.trainingSamples; }

Size AssetModel::size() const {
    if (inTrainingPhase_)
        return params_.trainingSamples;
    else
        return Model::size();
}

const std::string& AssetModel::baseCcy() const {
    if (!applyQuantoAdjustment_)
        return ModelImpl::baseCcy();
    return currencies_[quantoTargetCcyIndex_];
}

Real AssetModel::extractT0Result(const RandomVariable& value) const {

    if (type_ == Type::MC)
        return ModelImpl::extractT0Result(value);

    // specific code for FD:

    calculate();

    // roll back to today (if necessary)

    RandomVariable r =
        npv(value, referenceDate(), Filter(), QuantLib::ext::nullopt, RandomVariable(), RandomVariable());

    // if result is deterministic, return the value

    if (r.deterministic())
        return r.at(0);

    // otherwise interpolate the result at the spot of the underlying process

    Array x(underlyingValues_.size());
    Array y(underlyingValues_.size());
    underlyingValues_.copyToArray(x);
    r.copyToArray(y);
    MonotonicCubicNaturalSpline interpolation(x.begin(), x.end(), y.begin());
    interpolation.enableExtrapolation();
    return interpolation(initialValue(0));
}

RandomVariable AssetModel::pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                               const std::string& currency) const {

    if (type_ == Type::MC)
        return ModelImpl::pay(amount, obsdate, paydate, currency);

    // specific code for FD:

    calculate();

    if (!applyQuantoAdjustment_) {
        auto res = ModelImpl::pay(amount, obsdate, paydate, currency);
        res.setTime(timeFromReference(obsdate));
        return res;
    }

    QL_REQUIRE(currency == currencies_[quantoTargetCcyIndex_],
               "pay ccy is '" << currency << "', expected '" << currencies_[quantoTargetCcyIndex_]
                              << "' in quanto-adjusted AssetModelBase model");

    Date effectiveDate = std::max(obsdate, referenceDate());

    auto res = amount * getDiscount(quantoTargetCcyIndex_, effectiveDate, paydate) / getNumeraire(effectiveDate);
    res.setTime(timeFromReference(obsdate));
    return res;
}

RandomVariable AssetModel::getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                                const RandomVariable& barrier, const bool above) const {
    QL_FAIL("AssetModel::getFutureBarrierProb(): not implemented for AssetModelWrapper process type ("
            << static_cast<int>(model_->processType()) << ").");
}

} // namespace data
} // namespace ore
