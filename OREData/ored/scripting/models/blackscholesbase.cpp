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

#include <ored/scripting/models/blackscholesbase.hpp>

#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

BlackScholesBase::BlackScholesBase(const Size paths, const std::string& currency,
                                   const Handle<YieldTermStructure>& curve, const std::string& index,
                                   const std::string& indexCurrency, const Handle<BlackScholesModelWrapper>& model,
                                   const Model::McParams& mcParams, const std::set<Date>& simulationDates,
                                   const IborFallbackConfig& iborFallbackConfig)
    : BlackScholesBase(paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, model, {}, mcParams,
                       simulationDates, iborFallbackConfig) {}

BlackScholesBase::BlackScholesBase(
    const Size paths, const std::vector<std::string>& currencies, const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const Model::McParams& mcParams, const std::set<Date>& simulationDates,
    const IborFallbackConfig& iborFallbackConfig)
    : ModelImpl(curves.at(0)->dayCounter(), paths, currencies, irIndices, infIndices, indices, indexCurrencies,
                simulationDates, iborFallbackConfig),
      curves_(curves), fxSpots_(fxSpots), model_(model), correlations_(correlations), mcParams_(mcParams) {

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

    // register with observables

    for (auto const& o : fxSpots_)
        registerWith(o);
    for (auto const& o : correlations_)
        registerWith(o.second);

    registerWith(model_);

} // BlackScholesBase ctor

Matrix BlackScholesBase::getCorrelation() const {
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
    DLOG("BlackScholesBase correlation matrix:");
    DLOGGERSTREAM(correlation);
    return correlation;
}

const Date& BlackScholesBase::referenceDate() const {
    calculate();
    return referenceDate_;
}

void BlackScholesBase::performCalculations() const {

    QL_REQUIRE(!inTrainingPhase_,
               "BlackScholesBase::performCalculations(): state inTrainingPhase should be false, this was "
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
}

RandomVariable BlackScholesBase::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
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
    QL_REQUIRE(underlyingPaths_.find(d) != underlyingPaths_.end(), "did not find path for " << d);
    auto res = underlyingPaths_.at(d).at(indexNo);
    // compute forwarding factor
    if (effFwd != Null<Date>()) {
        auto p = model_->processes().at(indexNo);
        res *= RandomVariable(size(), p->dividendYield()->discount(effFwd) / p->dividendYield()->discount(d) /
                                          (p->riskFreeRate()->discount(effFwd) / p->riskFreeRate()->discount(d)));
    }
    return res;
}

RandomVariable BlackScholesBase::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    // ensure a valid fixing date
    effFixingDate = irIndices_.at(indexNo).second->fixingCalendar().adjust(effFixingDate);
    return RandomVariable(size(), irIndices_.at(indexNo).second->fixing(effFixingDate));
}

RandomVariable BlackScholesBase::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    return RandomVariable(size(), infIndices_.at(indexNo).second->fixing(effFixingDate));
}

namespace {
struct comp {
    comp(const std::string& indexInput) : indexInput_(indexInput) {}
    template <typename T> bool operator()(const std::pair<IndexInfo, QuantLib::ext::shared_ptr<T>>& p) const {
        return p.first.name() == indexInput_;
    }
    const std::string indexInput_;
};
} // namespace

RandomVariable BlackScholesBase::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                            const Date& start, const Date& end, const Real spread, const Real gearing,
                                            const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                            const bool includeSpread, const Real cap, const Real floor,
                                            const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto index = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
    QL_REQUIRE(index != irIndices_.end(),
               "BlackScholesBase::fwdCompAvg(): did not find ir index " << indexInput << " - this is unexpected.");
    auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index->second);
    QL_REQUIRE(on, "BlackScholesBase::fwdCompAvg(): expected on index for " << indexInput);
    // if we want to support cap / floors we need the OIS CF surface
    QL_REQUIRE(cap > 999998.0 && floor < -999998.0,
               "BlackScholesBase:fwdCompAvg(): cap (" << cap << ") / floor (" << floor << ") not supported");
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> coupon;
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> pricer;
    if (isAvg) {
        coupon = QuantLib::ext::make_shared<QuantExt::AverageONIndexedCoupon>(
            end, 1.0, start, end, on, gearing, spread, rateCutoff, on->dayCounter(), lookback * Days, fixingDays);
        pricer = QuantLib::ext::make_shared<AverageONIndexedCouponPricer>();
    } else {
        coupon = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(end, 1.0, start, end, on, gearing, spread, Date(),
                                                                      Date(), on->dayCounter(), false, includeSpread,
                                                                      lookback * Days, rateCutoff, fixingDays);
        pricer = QuantLib::ext::make_shared<OvernightIndexedCouponPricer>();
    }
    coupon->setPricer(pricer);
    return RandomVariable(size(), coupon->rate());
}

RandomVariable BlackScholesBase::getDiscount(const Size idx, const Date& s, const Date& t) const {
    return RandomVariable(size(), curves_.at(idx)->discount(t) / curves_.at(idx)->discount(s));
}

RandomVariable BlackScholesBase::getNumeraire(const Date& s) const {
    return RandomVariable(size(), 1.0 / curves_.at(0)->discount(s));
}

Real BlackScholesBase::getFxSpot(const Size idx) const { return fxSpots_.at(idx)->value(); }

RandomVariable BlackScholesBase::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                                     const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                                     const RandomVariable& addRegressor2) const {

    calculate();

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
                       "BlackScholesBase::npv(): stored regression coefficients at mem slot "
                           << *memSlot << " are for state size " << std::get<1>(it->second) << ", actual state size is "
                           << state.size() << " (before possible coordinate transform).");
            haveStoredModel = true;
        }
    }

    // if we do not have retrieved a model in the previous step, we create it now

    std::vector<RandomVariable> transformedState;

    if(!haveStoredModel) {

        // factor reduction to reduce dimensionalitty and handle collinearity

        if (mcParams_.regressionVarianceCutoff != Null<Real>()) {
            coordinateTransform = pcaCoordinateTransform(state, mcParams_.regressionVarianceCutoff);
            transformedState = applyCoordinateTransform(state, coordinateTransform);
            state = vec2vecptr(transformedState);
        }

        // train coefficients

        coeff = regressionCoefficients(amount, state,
                                       multiPathBasisSystem(state.size(), mcParams_.regressionOrder,
                                                            mcParams_.polynomType, std::min(size(), trainingSamples())),
                                       filter, RandomVariableRegressionMethod::QR);
        DLOG("BlackScholesBase::npv(" << ore::data::to_string(obsdate) << "): regression coefficients are " << coeff
                                      << " (got model state size " << nModelStates << " and " << nAddReg
                                      << " additional regressors, coordinate transform "
                                      << coordinateTransform.columns() << " -> " << coordinateTransform.rows() << ")");

        // store model if requried

        if (memSlot) {
            storedRegressionModel_[*memSlot] = std::make_tuple(coeff, nModelStates, coordinateTransform);
        }

    } else {

        // apply the stored coordinate transform to the state

        if(!coordinateTransform.empty()) {
            transformedState = applyCoordinateTransform(state, coordinateTransform);
            state = vec2vecptr(transformedState);
        }
    }

    // compute conditional expectation and return the result

    return conditionalExpectation(state,
                                  multiPathBasisSystem(state.size(), mcParams_.regressionOrder, mcParams_.polynomType,
                                                       std::min(size(), trainingSamples())),
                                  coeff);
}

void BlackScholesBase::releaseMemory() {
    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();
}

void BlackScholesBase::resetNPVMem() { storedRegressionModel_.clear(); }

void BlackScholesBase::toggleTrainingPaths() const {
    std::swap(underlyingPaths_, underlyingPathsTraining_);
    inTrainingPhase_ = !inTrainingPhase_;
}

Size BlackScholesBase::trainingSamples() const { return mcParams_.trainingSamples; }

Size BlackScholesBase::size() const {
    if (inTrainingPhase_)
        return mcParams_.trainingSamples;
    else
        return Model::size();
}

} // namespace data
} // namespace ore
