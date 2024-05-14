/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/fdgaussiancam.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgmfdsolver.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

FdGaussianCam::FdGaussianCam(const Handle<CrossAssetModel>& cam, const std::string& currency,
                             const Handle<YieldTermStructure>& curve,
                             const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
                             const std::set<Date>& simulationDates, const Size stateGridPoints,
                             const Size timeStepsPerYear, const Real mesherEpsilon,
                             const IborFallbackConfig& iborFallbackConfig)
    : ModelImpl(curve->dayCounter(), stateGridPoints, {currency}, irIndices, {}, {}, {}, simulationDates,
                iborFallbackConfig),
      cam_(cam), currency_(currency), curve_(curve), simulationDates_(simulationDates),
      stateGridPoints_(stateGridPoints), timeStepsPerYear_(timeStepsPerYear), mesherEpsilon_(mesherEpsilon),
      iborFallbackConfig_(iborFallbackConfig) {

    // check inputs

    QL_REQUIRE(!cam_.empty(), "model is empty");

    // register with observables

    registerWith(curve_);
    registerWith(cam_);
} // FdGaussianCam ctor

const Date& FdGaussianCam::referenceDate() const {
    calculate();
    return referenceDate_;
}

void FdGaussianCam::releaseMemory() { irIndexValueCache_.clear(); }

void FdGaussianCam::performCalculations() const {

    if (simulationDates_.empty())
        return;

    // set ref date

    referenceDate_ = curve_->referenceDate();

    // build solver

    solver_ = std::unique_ptr<LgmBackwardSolver>(
        new LgmFdSolver(cam_->lgm(0), timeFromReference(*simulationDates_.rbegin()), QuantLib::FdmSchemeDesc::Douglas(),
                        stateGridPoints_, timeStepsPerYear_, mesherEpsilon_));

    // set up eff sim dates

    effectiveSimulationDates_.clear();
    effectiveSimulationDates_.insert(referenceDate());
    for (auto const& d : simulationDates_) {
        if (d >= referenceDate())
            effectiveSimulationDates_.insert(d);
    }

    // set additional results provided by this model

    auto ar = getAdditionalResultsMap(cam_->lgm(0)->getCalibrationInfo());
    additionalResults_.insert(ar.begin(), ar.end());
}

RandomVariable FdGaussianCam::getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                                   const RandomVariable& barrier, const bool above) const {
    QL_FAIL("getFutureBarrierProb not implemented by FdGaussianCam");
}

RandomVariable FdGaussianCam::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    QL_FAIL("FdGaussianGam::getIndexValue(): non-ir indices are not allowed, got fx/eq/com index #" << indexNo);
}

RandomVariable FdGaussianCam::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date fixingDate = d;
    if (fwd != Null<Date>())
        fixingDate = fwd;
    // ensure a valid fixing date
    fixingDate = irIndices_[indexNo].second->fixingCalendar().adjust(fixingDate);
    // look up required fixing in cache and return it if found
    if (auto cacheValue = irIndexValueCache_.find(std::make_tuple(indexNo, d, fixingDate));
        cacheValue != irIndexValueCache_.end()) {
        return cacheValue->second;
    }
    // compute value, add to cache and return it
    LgmVectorised lgmv(cam_->irlgm1f(0));
    auto result = lgmv.fixing(irIndices_[indexNo].second, fixingDate, timeFromReference(d),
                              solver_->stateGrid(timeFromReference(d)));
    result.setTime(timeFromReference(d));
    irIndexValueCache_[std::make_tuple(indexNo, d, fixingDate)] = result;
    return result;
}

RandomVariable FdGaussianCam::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    QL_FAIL("FdGaussianGam::getInfIndexValue(): non-ir indices are not allowed, got inf index #" << indexNo);
}

RandomVariable FdGaussianCam::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                         const Date& start, const Date& end, const Real spread, const Real gearing,
                                         const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                         const bool includeSpread, const Real cap, const Real floor,
                                         const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto ir = std::find_if(irIndices_.begin(), irIndices_.end(),
                           [&indexInput](const std::pair<IndexInfo, QuantLib::ext::shared_ptr<InterestRateIndex>>& p) {
                               return p.first.name() == indexInput;
                           });
    QL_REQUIRE(ir != irIndices_.end(),
               "FdGaussianCam::fwdComp() ir index " << indexInput << " not found, this is unexpected");
    LgmVectorised lgmv(cam_->lgm(0)->parametrization());
    auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(ir->second);
    QL_REQUIRE(on, "FdGaussianCam::fwdCompAvg(): expected on index for " << indexInput);
    // only used to extract fixing and value dates
    auto coupon = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(
        end, 1.0, start, end, on, gearing, spread, Date(), Date(), DayCounter(), false, includeSpread, lookback * Days,
        rateCutoff, fixingDays);
    // get model time and state
    Date effobsdate = std::max(referenceDate(), obsdate);
    if (isAvg) {
        return lgmv.averagedOnRate(on, coupon->fixingDates(), coupon->valueDates(), coupon->dt(), rateCutoff,
                                   includeSpread, spread, gearing, lookback * Days, cap, floor, localCapFloor,
                                   nakedOption, timeFromReference(effobsdate),
                                   solver_->stateGrid(timeFromReference(effobsdate)));
    } else {
        return lgmv.compoundedOnRate(on, coupon->fixingDates(), coupon->valueDates(), coupon->dt(), rateCutoff,
                                     includeSpread, spread, gearing, lookback * Days, cap, floor, localCapFloor,
                                     nakedOption, timeFromReference(effobsdate),
                                     solver_->stateGrid(timeFromReference(effobsdate)));
    }
}

RandomVariable FdGaussianCam::getDiscount(const Size idx, const Date& s, const Date& t) const {
    LgmVectorised lgmv(cam_->lgm(0)->parametrization());
    return lgmv.discountBond(timeFromReference(s), timeFromReference(t), solver_->stateGrid(timeFromReference(s)));
}

RandomVariable FdGaussianCam::getNumeraire(const Date& s) const {
    LgmVectorised lgmv(cam_->lgm(0)->parametrization());
    return lgmv.numeraire(timeFromReference(s), solver_->stateGrid(timeFromReference(s)));
}

Real FdGaussianCam::getFxSpot(const Size idx) const {
    QL_FAIL("FdGaussianCam::getFxSpot(): this is a single ccy model, there is no fx spot for idx " << idx
                                                                                                   << " available.");
}

RandomVariable FdGaussianCam::pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                                  const std::string& currency) const {
    auto result = ModelImpl::pay(amount, obsdate, paydate, currency);
    result.setTime(timeFromReference(obsdate));
    return result;
}

RandomVariable FdGaussianCam::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                                  const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                                  const RandomVariable& addRegressor2) const {

    QL_REQUIRE(!memSlot, "FdGaussianCam::npv(): mem slot not allowed.");
    QL_REQUIRE(!filter.initialised(), "FdGaussianCam::npv(): filter not allowed");
    QL_REQUIRE(!addRegressor1.initialised(), "FdGaussianCam::npv(). addRegressor1 not allowed");
    QL_REQUIRE(!addRegressor2.initialised(), "FdGaussianCam::npv(). addRegressor2 not allowed");

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
               "FdGaussianCam::npv(): can not roll back amount wiithout time attached (to t0=" << t0 << ")");
    RandomVariable result = solver_->rollback(amount, t1, t0);
    result.setTime(t0);
    return result;
}

Real FdGaussianCam::extractT0Result(const RandomVariable& result) const {

    calculate();

    // roll back to today (if necessary)

    RandomVariable r = npv(result, referenceDate(), Filter(), boost::none, RandomVariable(), RandomVariable());

    // we expect the results to be determinstic as per LgmBackwardSolver interface

    QL_REQUIRE(r.deterministic(), "FdGaussianCam::extractT0Result(): internal error, expected result to be "
                                  "deterministic after rollback to time t = 0");

    return r.at(0);
}

} // namespace data
} // namespace ore
