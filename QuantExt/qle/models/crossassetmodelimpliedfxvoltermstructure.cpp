/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/models/crossassetmodelimpliedfxvoltermstructure.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>

#include <ql/payoff.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

CrossAssetModelImpliedFxVolTermStructure::
    CrossAssetModelImpliedFxVolTermStructure(
        const boost::shared_ptr<CrossAssetModel> &model,
        const Size foreignCurrencyIndex, BusinessDayConvention bdc,
        const DayCounter &dc, const bool purelyTimeBased)
    : BlackVolTermStructure(
          bdc, dc == DayCounter()
                   ? model->irlgm1f(0)->termStructure()->dayCounter()
                   : dc),
      model_(model), fxIndex_(foreignCurrencyIndex),
      purelyTimeBased_(purelyTimeBased),
      engine_(boost::make_shared<AnalyticCcLgmFxOptionEngine>(
          model_, foreignCurrencyIndex)),
      referenceDate_(
          purelyTimeBased
              ? Null<Date>()
              : model_->irlgm1f(0)->termStructure()->referenceDate()) {

    registerWith(model_);
    engine_->cache(false);
    state(0.0, 0.0, std::log(model_->fxbs(fxIndex_)->fxSpotToday()->value()));
    update();
}

Real CrossAssetModelImpliedFxVolTermStructure::blackVarianceImpl(
    Time t, Real strike) const {

    Real tmpStrike = strike;

    Real fxSpot = std::exp(fx_);
    Real domDisc =
        model_->discountBond(0, relativeTime_, relativeTime_ + t, irDom_);
    Real forDisc = model_->discountBond(fxIndex_ + 1, relativeTime_,
                                        relativeTime_ + t, irFor_);
    Real atm = fxSpot * forDisc / domDisc;

    if (tmpStrike == Null<Real>()) {
        tmpStrike = atm;
    }

    Option::Type type = tmpStrike >= atm ? Option::Call : Option::Put;

    boost::shared_ptr<StrikedTypePayoff> payoff =
        boost::make_shared<PlainVanillaPayoff>(type, tmpStrike);

    Real premium =
        engine_->value(relativeTime_, relativeTime_ + t, payoff, domDisc, atm);

    Real impliedStdDev = 0.0;
    try {
        impliedStdDev =
            blackFormulaImpliedStdDev(type, tmpStrike, atm, premium, domDisc);
    } catch (...) {
    }

    return impliedStdDev * impliedStdDev;
}

Volatility
CrossAssetModelImpliedFxVolTermStructure::blackVolImpl(Time t,
                                                       Real strike) const {
    Real tmp = std::max(1.0E-6, t);
    return std::sqrt(blackVarianceImpl(tmp, strike) / tmp);
}

const Date &CrossAssetModelImpliedFxVolTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

void CrossAssetModelImpliedFxVolTermStructure::referenceDate(const Date &d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

void CrossAssetModelImpliedFxVolTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
}

void CrossAssetModelImpliedFxVolTermStructure::state(const Real domesticIr,
                                                     const Real foreignIr,
                                                     const Real logFx) {
    irDom_ = domesticIr;
    irFor_ = foreignIr;
    fx_ = logFx;
}

void CrossAssetModelImpliedFxVolTermStructure::move(const Date &d,
                                                    const Real domesticIr,
                                                    const Real foreignIr,
                                                    const Real logFx) {
    state(domesticIr, foreignIr, logFx);
    referenceDate(d);
}

void CrossAssetModelImpliedFxVolTermStructure::move(const Time t,
                                                    const Real domesticIr,
                                                    const Real foreignIr,
                                                    const Real logFx) {
    state(domesticIr, foreignIr, logFx);
    referenceTime(t);
}

void CrossAssetModelImpliedFxVolTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(
            model_->irlgm1f(0)->termStructure()->referenceDate(),
            referenceDate_);
    }
    notifyObservers();
}

Date CrossAssetModelImpliedFxVolTermStructure::maxDate() const {
    return Date::maxDate();
}

Time CrossAssetModelImpliedFxVolTermStructure::maxTime() const {
    return QL_MAX_REAL;
}

Real CrossAssetModelImpliedFxVolTermStructure::minStrike() const { return 0.0; }

Real CrossAssetModelImpliedFxVolTermStructure::maxStrike() const {
    return QL_MAX_REAL;
}

} // namespace QuantExt
