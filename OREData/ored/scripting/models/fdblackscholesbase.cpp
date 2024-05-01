/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/fdblackscholesbase.hpp>
#include <ored/scripting/utilities.hpp>

#include <qle/methods/fdmblackscholesmesher.hpp>
#include <qle/methods/fdmblackscholesop.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/model/utilities.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

FdBlackScholesBase::FdBlackScholesBase(const Size stateGridPoints, const std::string& currency,
                                       const Handle<YieldTermStructure>& curve, const std::string& index,
                                       const std::string& indexCurrency, const Handle<BlackScholesModelWrapper>& model,
                                       const std::set<Date>& simulationDates,
                                       const IborFallbackConfig& iborFallbackConfig, const std::string& calibration,
                                       const std::vector<Real>& calibrationStrikes, const Real mesherEpsilon,
                                       const Real mesherScaling, const Real mesherConcentration,
                                       const Size mesherMaxConcentratingPoints, const bool staticMesher)
    : FdBlackScholesBase(stateGridPoints, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, {currency}, model,
                         {}, simulationDates, iborFallbackConfig, calibration, {{index, calibrationStrikes}},
                         mesherEpsilon, mesherScaling, mesherConcentration, mesherMaxConcentratingPoints,
                         staticMesher) {}

FdBlackScholesBase::FdBlackScholesBase(
    const Size stateGridPoints, const std::vector<std::string>& currencies,
    const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const std::set<std::string>& payCcys, const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig, const std::string& calibration,
    const std::map<std::string, std::vector<Real>>& calibrationStrikes, const Real mesherEpsilon,
    const Real mesherScaling, const Real mesherConcentration, const Size mesherMaxConcentratingPoints,
    const bool staticMesher)
    : ModelImpl(curves.at(0)->dayCounter(), stateGridPoints, currencies, irIndices, infIndices, indices,
                indexCurrencies, simulationDates, iborFallbackConfig),
      curves_(curves), fxSpots_(fxSpots), payCcys_(payCcys), model_(model), correlations_(correlations),
      calibration_(calibration), calibrationStrikes_(calibrationStrikes), mesherEpsilon_(mesherEpsilon),
      mesherScaling_(mesherScaling), mesherConcentration_(mesherConcentration),
      mesherMaxConcentratingPoints_(mesherMaxConcentratingPoints), staticMesher_(staticMesher) {

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

    // register with observables

    for (auto const& o : fxSpots_)
        registerWith(o);
    for (auto const& o : correlations_)
        registerWith(o.second);

    registerWith(model_);

    // if we have one (or no) underlying, everything works as usual

    if (model_->processes().size() <= 1)
        return;

    // if we have one underlying + one FX index, we do a 1D PDE with a quanto adjustment under certain circumstances

    if (model_->processes().size() == 2) {
        // check whether we have exactly one pay ccy ...
        if (payCcys.size() == 1) {
            std::string payCcy = *payCcys.begin();
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
                DLOG("FdBlackScholesBase model will be run for index '"
                     << indices_[0].name() << "' with a quanto-adjustment " << currencies_[quantoSourceCcyIndex_]
                     << " => " << currencies_[quantoTargetCcyIndex_] << " derived from index '" << indices_[1].name()
                     << "'");
                return;
            }
        }
    }

    // otherwise we need more than one dimension, which we currently not support

    QL_FAIL("FdBlackScholesBase: model does not support multi-dim fd schemes currently.");

} // FdBlackScholesBase ctor

Matrix FdBlackScholesBase::getCorrelation() const {
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
    TLOG("FdBlackScholesBase correlation matrix:");
    TLOGGERSTREAM(correlation);
    return correlation;
}

const Date& FdBlackScholesBase::referenceDate() const {
    calculate();
    return referenceDate_;
}

void FdBlackScholesBase::performCalculations() const {

    referenceDate_ = curves_.front()->referenceDate();

    // 0a set up time grid

    effectiveSimulationDates_ = model_->effectiveSimulationDates();

    std::vector<Real> times;
    for (auto const& d : effectiveSimulationDates_) {
        times.push_back(timeFromReference(d));
    }

    timeGrid_ = model_->discretisationTimeGrid();
    positionInTimeGrid_.resize(times.size());
    for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
        positionInTimeGrid_[i] = timeGrid_.index(times[i]);

    // 0b nothing to do if we do not have any indices

    if (indices_.empty())
        return;

    // 0c if we only have one effective sim date (today), we set the underlying values = spot

    if (effectiveSimulationDates_.size() == 1) {
        underlyingValues_ = RandomVariable(size(), model_->processes()[0]->x0());
        return;
    }

    // 1 set the calibration strikes

    std::vector<Real> calibrationStrikes;
    if (calibration_ == "ATM") {
        calibrationStrikes.resize(indices_.size(), Null<Real>());
    } else if (calibration_ == "Deal") {
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
    } else {
        QL_FAIL("FdBlackScholes: calibration '" << calibration_ << "' not supported, expected ATM, Deal");
    }

    // 1b set up the critical points for the mesher

    std::vector<std::vector<QuantLib::ext::tuple<Real, Real, bool>>> cPoints;
    for (Size i = 0; i < indices_.size(); ++i) {
        cPoints.push_back(std::vector<QuantLib::ext::tuple<Real, Real, bool>>());
        auto f = calibrationStrikes_.find(indices_[i].name());
        if (f != calibrationStrikes_.end()) {
            for (Size j = 0; j < std::min(f->second.size(), mesherMaxConcentratingPoints_); ++j) {
                cPoints.back().push_back(QuantLib::ext::make_tuple(std::log(f->second[j]), mesherConcentration_, false));
                TLOG("added critical point at strike " << f->second[j] << " with concentration "
                                                       << mesherConcentration_);
            }
        }
    }

    // 2 set up mesher if we do not have one already or if we want to rebuild it every time

    if (mesher_ == nullptr || !staticMesher_) {
        mesher_ = QuantLib::ext::make_shared<FdmMesherComposite>(QuantLib::ext::make_shared<QuantExt::FdmBlackScholesMesher>(
            size(), model_->processes()[0], timeGrid_.back(),
            calibrationStrikes[0] == Null<Real>()
                ? atmForward(model_->processes()[0]->x0(), model_->processes()[0]->riskFreeRate(),
                             model_->processes()[0]->dividendYield(), timeGrid_.back())
                : calibrationStrikes[0],
            Null<Real>(), Null<Real>(), mesherEpsilon_, mesherScaling_, cPoints[0]));
    }

    // 3 set up operator using atmf vol and without discounting, floor forward variances at zero

    QuantLib::ext::shared_ptr<QuantExt::FdmQuantoHelper> quantoHelper;

    if (applyQuantoAdjustment_) {
        Real quantoCorr = quantoCorrelationMultiplier_ * getCorrelation()[0][1];
        quantoHelper = QuantLib::ext::make_shared<QuantExt::FdmQuantoHelper>(
            *curves_[quantoTargetCcyIndex_], *curves_[quantoSourceCcyIndex_],
            *model_->processes()[1]->blackVolatility(), quantoCorr, Null<Real>(), model_->processes()[1]->x0(), false,
            true);
    }

    operator_ =
        QuantLib::ext::make_shared<QuantExt::FdmBlackScholesOp>(mesher_, model_->processes()[0], calibrationStrikes[0], false,
                                                        -static_cast<Real>(Null<Real>()), 0, quantoHelper, false, true);

    // 4 set up bwd solver, hardcoded Douglas scheme (= CrankNicholson)

    solver_ = QuantLib::ext::make_shared<FdmBackwardSolver>(
        operator_, std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>>(), nullptr, FdmSchemeDesc::Douglas());

    // 5 fill random variable with underlying values, these are valid for all times

    auto locations = mesher_->locations(0);
    underlyingValues_ = exp(RandomVariable(locations));

    // set additional results provided by this model

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_["FdBlackScholes.CalibrationStrike_" + indices_[i].name()] =
            (calibrationStrikes[i] == Null<Real>() ? "ATMF" : std::to_string(calibrationStrikes[i]));
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        Size timeStep = 0;
        for (auto const& d : effectiveSimulationDates_) {
            Real t = timeGrid_[positionInTimeGrid_[timeStep]];
            Real forward = atmForward(model_->processes()[i]->x0(), model_->processes()[i]->riskFreeRate(),
                                      model_->processes()[i]->dividendYield(), t);
            if (timeStep > 0) {
                Real volatility = model_->processes()[i]->blackVolatility()->blackVol(
                    t, calibrationStrikes[i] == Null<Real>() ? forward : calibrationStrikes[i]);
                additionalResults_["FdBlackScholes.Volatility_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                    volatility;
            }
            additionalResults_["FdBlackScholes.Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                forward;
            ++timeStep;
        }
    }
}

RandomVariable FdBlackScholesBase::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {

    QL_REQUIRE(indexNo == 0, "FdBlackScholesBase::getIndexValue(): indexNo (" << indexNo << ") must be 0");

    // determine the effective forward date (if applicable)

    Date effFwd = fwd;
    if (indices_[indexNo].isComm()) {
        const Date& expiry = indices_[indexNo].comm(d)->expiryDate();
        // if a future is referenced we set the forward date effectively used below to the future's expiry date
        if (expiry != Date())
            effFwd = expiry;
        // if the future expiry is past the obsdate, we return the spot as of the obsdate, i.e. we
        // freeze the future value after its expiry, but keep it available for observation
        // TOOD should we throw an exception instead?
        effFwd = std::max(effFwd, d);
    }

    // init the result with the underlying values themselves

    RandomVariable res(underlyingValues_);

    // compute forwarding factor and multiply the result by this factor

    if (effFwd != Null<Date>()) {
        auto p = model_->processes().at(indexNo);
        res *= RandomVariable(size(), p->dividendYield()->discount(effFwd) / p->dividendYield()->discount(d) /
                                          (p->riskFreeRate()->discount(effFwd) / p->riskFreeRate()->discount(d)));
    }

    // set the observation time in the result random variable
    res.setTime(timeFromReference(d));

    // return the result

    return res;
}

RandomVariable FdBlackScholesBase::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    // ensure a valid fixing date
    effFixingDate = irIndices_.at(indexNo).second->fixingCalendar().adjust(effFixingDate);
    return RandomVariable(size(), irIndices_.at(indexNo).second->fixing(effFixingDate));
}

RandomVariable FdBlackScholesBase::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    return RandomVariable(size(), infIndices_.at(indexNo).second->fixing(effFixingDate));
}

RandomVariable FdBlackScholesBase::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                              const Date& start, const Date& end, const Real spread, const Real gearing,
                                              const Integer lookback, const Natural rateCutoff,
                                              const Natural fixingDays, const bool includeSpread, const Real cap,
                                              const Real floor, const bool nakedOption,
                                              const bool localCapFloor) const {
    calculate();
    IndexInfo indexInfo(indexInput);
    auto index = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(indexInfo.irIbor());
    QL_REQUIRE(index, "BlackScholesBase::fwdCompAvg(): expected on index for " << indexInput);
    // if we want to support cap / floors we need the OIS CF surface
    QL_REQUIRE(cap > 999998.0 && floor < -999998.0,
               "BlackScholesCGBase:fwdCompAvg(): cap (" << cap << ") / floor (" << floor << ") not supported");
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> coupon;
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> pricer;
    if (isAvg) {
        coupon = QuantLib::ext::make_shared<QuantExt::AverageONIndexedCoupon>(
            end, 1.0, start, end, index, gearing, spread, rateCutoff, DayCounter(), lookback * Days, fixingDays);
        pricer = QuantLib::ext::make_shared<AverageONIndexedCouponPricer>();
    } else {
        coupon = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(
            end, 1.0, start, end, index, gearing, spread, Date(), Date(), DayCounter(), false, includeSpread,
            lookback * Days, rateCutoff, fixingDays);
        pricer = QuantLib::ext::make_shared<OvernightIndexedCouponPricer>();
    }
    coupon->setPricer(pricer);
    return RandomVariable(size(), coupon->rate());
}

RandomVariable FdBlackScholesBase::getDiscount(const Size idx, const Date& s, const Date& t) const {
    return RandomVariable(size(), curves_.at(idx)->discount(t) / curves_.at(idx)->discount(s));
}

RandomVariable FdBlackScholesBase::getNumeraire(const Date& s) const {
    if (!applyQuantoAdjustment_)
        return RandomVariable(size(), 1.0 / curves_.at(0)->discount(s));
    else
        return RandomVariable(size(), 1.0 / curves_.at(quantoTargetCcyIndex_)->discount(s));
}

Real FdBlackScholesBase::getFxSpot(const Size idx) const { return fxSpots_.at(idx)->value(); }

RandomVariable FdBlackScholesBase::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                                       const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                                       const RandomVariable& addRegressor2) const {

    QL_REQUIRE(!memSlot, "FdBlackScholesBase::npv(): mem slot not allowed");
    QL_REQUIRE(!filter.initialised(), "FdBlackScholesBase::npv(). filter not allowed");
    QL_REQUIRE(!addRegressor1.initialised(), "FdBlackScholesBase::npv(). addRegressor1 not allowed");
    QL_REQUIRE(!addRegressor2.initialised(), "FdBlackScholesBase::npv(). addRegressor2 not allowed");

    calculate();

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
               "FdBlackScholesBase::npv(): can not roll back amount wiithout time attached (to t0=" << t0 << ")");

    // might throw if t0, t1 are not found in timeGrid_

    Size ind1 = timeGrid_.index(t1);
    Size ind0 = timeGrid_.index(t0);

    // check t0 <= t1, i.e. ind0 <= ind1

    QL_REQUIRE(ind0 <= ind1, "FdBlackScholesBase::npv(): can not roll back from t1= "
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
}

void FdBlackScholesBase::releaseMemory() {}

RandomVariable FdBlackScholesBase::getFutureBarrierProb(const std::string& index, const Date& obsdate1,
                                                        const Date& obsdate2, const RandomVariable& barrier,
                                                        const bool above) const {
    QL_FAIL("FdBlackScholesBase::getFutureBarrierProb(): not supported");
}

Real FdBlackScholesBase::extractT0Result(const RandomVariable& value) const {

    calculate();

    // roll back to today (if necessary)

    RandomVariable r = npv(value, referenceDate(), Filter(), boost::none, RandomVariable(), RandomVariable());

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
    return interpolation(model_->processes()[0]->x0());
}

RandomVariable FdBlackScholesBase::pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                                       const std::string& currency) const {

    calculate();

    if (!applyQuantoAdjustment_) {
        auto res = ModelImpl::pay(amount, obsdate, paydate, currency);
        res.setTime(timeFromReference(obsdate));
        return res;
    }

    QL_REQUIRE(currency == currencies_[quantoTargetCcyIndex_],
               "pay ccy is '" << currency << "', expected '" << currencies_[quantoTargetCcyIndex_]
                              << "' in quanto-adjusted FDBlackScholesBase model");

    Date effectiveDate = std::max(obsdate, referenceDate());

    auto res = amount * getDiscount(quantoTargetCcyIndex_, effectiveDate, paydate) / getNumeraire(effectiveDate);
    res.setTime(timeFromReference(obsdate));
    return res;
}

const std::string& FdBlackScholesBase::baseCcy() const {
    if (!applyQuantoAdjustment_)
        return ModelImpl::baseCcy();

    return currencies_[quantoTargetCcyIndex_];
}

} // namespace data
} // namespace ore
