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

#include <ql/cashflows/fixedratecoupon.hpp>

#include <qle/cashflows/couponpricer.hpp>
#include <qle/instruments/averageois.hpp>

namespace QuantExt {

AverageOIS::AverageOIS(Type type, Real nominal, const Schedule& fixedLegSchedule, Rate fixedRate,
                       const DayCounter& fixedDCB, BusinessDayConvention fixedLegPaymentAdjustment,
                       const Calendar& fixedLegPaymentCalendar, const Schedule& onLegSchedule,
                       const boost::shared_ptr<OvernightIndex>& overnightIndex,
                       BusinessDayConvention onLegPaymentAdjustment, const Calendar& onLegPaymentCalendar,
                       Natural rateCutoff, Spread onLegSpread, Real onLegGearing, const DayCounter& onLegDCB,
                       const boost::shared_ptr<AverageONIndexedCouponPricer>& onLegCouponPricer)
    : Swap(2), type_(type), nominals_(std::vector<Real>(1, nominal)), fixedRates_(std::vector<Rate>(1, fixedRate)),
      fixedDayCounter_(fixedDCB), fixedPaymentAdjustment_(fixedLegPaymentAdjustment),
      fixedPaymentCalendar_(fixedLegPaymentCalendar), overnightIndex_(overnightIndex),
      onPaymentAdjustment_(onLegPaymentAdjustment), onPaymentCalendar_(onLegPaymentCalendar), rateCutoff_(rateCutoff),
      onSpreads_(std::vector<Spread>(1, onLegSpread)), onGearings_(std::vector<Real>(1, onLegGearing)),
      onDayCounter_(onLegDCB), onCouponPricer_(onLegCouponPricer) {
    initialize(fixedLegSchedule, onLegSchedule);
}

AverageOIS::AverageOIS(Type type, std::vector<Real> nominals, const Schedule& fixedLegSchedule,
                       std::vector<Rate> fixedRates, const DayCounter& fixedDCB,
                       BusinessDayConvention fixedLegPaymentAdjustment, const Calendar& fixedLegPaymentCalendar,
                       const Schedule& onLegSchedule, const boost::shared_ptr<OvernightIndex>& overnightIndex,
                       BusinessDayConvention onLegPaymentAdjustment, const Calendar& onLegPaymentCalendar,
                       Natural rateCutoff, std::vector<Spread> onLegSpreads, std::vector<Real> onLegGearings,
                       const DayCounter& onLegDCB,
                       const boost::shared_ptr<AverageONIndexedCouponPricer>& onLegCouponPricer)
    : Swap(2), type_(type), nominals_(nominals), fixedRates_(fixedRates), fixedDayCounter_(fixedDCB),
      fixedPaymentAdjustment_(fixedLegPaymentAdjustment), fixedPaymentCalendar_(fixedLegPaymentCalendar),
      overnightIndex_(overnightIndex), onPaymentAdjustment_(onLegPaymentAdjustment),
      onPaymentCalendar_(onLegPaymentCalendar), rateCutoff_(rateCutoff), onSpreads_(onLegSpreads),
      onGearings_(onLegGearings), onDayCounter_(onLegDCB), onCouponPricer_(onLegCouponPricer) {
    initialize(fixedLegSchedule, onLegSchedule);
}

void AverageOIS::initialize(const Schedule& fixedLegSchedule, const Schedule& onLegSchedule) {
    // Fixed leg.
    legs_[0] = FixedRateLeg(fixedLegSchedule)
                   .withNotionals(nominals_)
                   .withCouponRates(fixedRates_, fixedDayCounter_)
                   .withPaymentAdjustment(fixedPaymentAdjustment_)
                   .withPaymentCalendar(fixedPaymentCalendar_);

    // Average ON leg.
    AverageONLeg tempAverageONLeg = AverageONLeg(onLegSchedule, overnightIndex_)
                                        .withNotionals(nominals_)
                                        .withPaymentAdjustment(onPaymentAdjustment_)
                                        .withPaymentCalendar(onPaymentCalendar_)
                                        .withRateCutoff(rateCutoff_)
                                        .withSpreads(onSpreads_)
                                        .withGearings(onGearings_)
                                        .withPaymentDayCounter(onDayCounter_);

    if (onCouponPricer_) {
        tempAverageONLeg = tempAverageONLeg.withAverageONIndexedCouponPricer(onCouponPricer_);
    }

    legs_[1] = tempAverageONLeg;

    // Set the fixed and ON leg to pay (receive) and receive (pay) resp.
    switch (type_) {
    case Payer:
        payer_[0] = -1.0;
        payer_[1] = +1.0;
        break;
    case Receiver:
        payer_[0] = +1.0;
        payer_[1] = -1.0;
        break;
    default:
        QL_FAIL("Unknown average ON index swap type");
    }
}

Real AverageOIS::nominal() const {
    QL_REQUIRE(nominals_.size() == 1, "Swap has varying nominals");
    return nominals_[0];
}

Rate AverageOIS::fixedRate() const {
    QL_REQUIRE(fixedRates_.size() == 1, "Swap has varying fixed rates");
    return fixedRates_[0];
}

Spread AverageOIS::onSpread() const {
    QL_REQUIRE(onSpreads_.size() == 1, "Swap has varying ON spreads");
    return onSpreads_[0];
}

Real AverageOIS::onGearing() const {
    QL_REQUIRE(onGearings_.size() == 1, "Swap has varying ON gearings");
    return onGearings_[0];
}

Real AverageOIS::fixedLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "fixedLegBPS not available");
    return legBPS_[0];
}

Real AverageOIS::fixedLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[0] != Null<Real>(), "fixedLegNPV not available");
    return legNPV_[0];
}

Real AverageOIS::fairRate() const {
    static const Spread basisPoint = 1.0e-4;
    calculate();
    return -overnightLegNPV() / (fixedLegBPS() / basisPoint);
}

Real AverageOIS::overnightLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[1] != Null<Real>(), "overnightLegBPS not available");
    return legBPS_[1];
}

Real AverageOIS::overnightLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[1] != Null<Real>(), "overnightLegNPV not available");
    return legNPV_[1];
}

Spread AverageOIS::fairSpread() const {
    QL_REQUIRE(onSpreads_.size() == 1, "fairSpread not implemented for varying spreads.");
    static const Spread basisPoint = 1.0e-4;
    calculate();
    return onSpreads_[0] - NPV_ / (overnightLegBPS() / basisPoint);
}

void AverageOIS::setONIndexedCouponPricer(const boost::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer) {
    QuantExt::setCouponPricer(legs_[1], onCouponPricer);
    update();
}
} // namespace QuantExt
