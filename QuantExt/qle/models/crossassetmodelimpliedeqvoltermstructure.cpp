/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/models/crossassetmodelimpliedeqvoltermstructure.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>

#include <ql/payoff.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

CrossAssetModelImpliedEqVolTermStructure::CrossAssetModelImpliedEqVolTermStructure(
    const boost::shared_ptr<CrossAssetModel>& model, const Size equityIndex, BusinessDayConvention bdc,
    const DayCounter& dc, const bool purelyTimeBased)
    : BlackVolTermStructure(bdc, dc == DayCounter() ? model->irlgm1f(0)->termStructure()->dayCounter() : dc),
      model_(model), eqIndex_(equityIndex), purelyTimeBased_(purelyTimeBased),
      engine_(boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(model_, eqIndex_, eqCcyIndex())),
      referenceDate_(purelyTimeBased ? Null<Date>() : model_->irlgm1f(0)->termStructure()->referenceDate()) {

    registerWith(model_);
    // engine_->cache(false);
    Real eqSpot = model_->eqbs(eqIndex_)->eqSpotToday()->value();
    QL_REQUIRE(eqSpot > 0, "EQ Spot for index " << eqIndex_ << " must be positive");
    state(0.0, std::log(eqSpot));
    update();
}

Real CrossAssetModelImpliedEqVolTermStructure::blackVarianceImpl(Time t, Real strike) const {

    Real tmpStrike = strike;

    Real eqSpot = std::exp(eq_);
    Real rateDisc = model_->discountBond(eqCcyIndex(), relativeTime_, relativeTime_ + t, eqIr_);
    Real divDisc = model_->eqbs(eqIndex_)->equityDivYieldCurveToday()->discount(t);
    // Real forDisc = model_->discountBond(fxIndex_ + 1, relativeTime_, relativeTime_ + t, irFor_);
    Real atm = eqSpot * divDisc / rateDisc;

    if (tmpStrike == Null<Real>()) {
        tmpStrike = atm;
    }

    Option::Type type = tmpStrike >= atm ? Option::Call : Option::Put;

    boost::shared_ptr<StrikedTypePayoff> payoff = boost::make_shared<PlainVanillaPayoff>(type, tmpStrike);

    Real premium = engine_->value(relativeTime_, relativeTime_ + t, payoff, rateDisc, atm);

    Real impliedStdDev = 0.0;
    try {
        impliedStdDev = blackFormulaImpliedStdDev(type, tmpStrike, atm, premium, rateDisc);
    } catch (...) {
    }

    return impliedStdDev * impliedStdDev;
}

Volatility CrossAssetModelImpliedEqVolTermStructure::blackVolImpl(Time t, Real strike) const {
    Real tmp = std::max(1.0E-6, t);
    return std::sqrt(blackVarianceImpl(tmp, strike) / tmp);
}

const Date& CrossAssetModelImpliedEqVolTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

void CrossAssetModelImpliedEqVolTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

void CrossAssetModelImpliedEqVolTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
}

void CrossAssetModelImpliedEqVolTermStructure::state(const Real eqIr, const Real logEq) {
    eqIr_ = eqIr;
    eq_ = logEq;
}

void CrossAssetModelImpliedEqVolTermStructure::move(const Date& d, const Real eqIr, const Real logEq) {
    state(eqIr, logEq);
    referenceDate(d);
}

void CrossAssetModelImpliedEqVolTermStructure::move(const Time t, const Real eqIr, const Real logEq) {
    state(eqIr, logEq);
    referenceTime(t);
}

void CrossAssetModelImpliedEqVolTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(model_->irlgm1f(0)->termStructure()->referenceDate(), referenceDate_);
    }
    notifyObservers();
}

Date CrossAssetModelImpliedEqVolTermStructure::maxDate() const { return Date::maxDate(); }

Time CrossAssetModelImpliedEqVolTermStructure::maxTime() const { return QL_MAX_REAL; }

Real CrossAssetModelImpliedEqVolTermStructure::minStrike() const { return 0.0; }

Real CrossAssetModelImpliedEqVolTermStructure::maxStrike() const { return QL_MAX_REAL; }

} // namespace QuantExt
