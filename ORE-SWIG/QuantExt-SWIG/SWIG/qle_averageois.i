/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_averageois_i
#define qle_averageois_i

%include instruments.i
%include termstructures.i
%include cashflows.i
%include timebasket.i
%include indexes.i
%include swap.i

%{
using QuantExt::AverageOIS;
using QuantExt::AverageONIndexedCouponPricer;
%}

%shared_ptr(AverageOIS)
class AverageOIS : public Swap {
  public:
    enum Type { Receiver = -1, Payer = 1 };
    AverageOIS(AverageOIS::Type type,
               QuantLib::Real nominal,
               const QuantLib::Schedule& fixedSchedule,
               QuantLib::Rate fixedRate,
               const QuantLib::DayCounter& fixedDayCounter,
               QuantLib::BusinessDayConvention fixedPaymentAdjustment,
               const QuantLib::Calendar& fixedPaymentCalendar,
               const QuantLib::Schedule& onSchedule,
               const ext::shared_ptr<OvernightIndex>& overnightIndex,
               QuantLib::BusinessDayConvention onPaymentAdjustment,
               const QuantLib::Calendar& onPaymentCalendar,
               QuantLib::Natural rateCutoff = 0,
               QuantLib::Spread onSpread = 0.0,
               QuantLib::Real onGearing = 1.0,
               const QuantLib::DayCounter& onDayCounter = QuantLib::DayCounter(),
               const ext::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer
               = ext::shared_ptr<AverageONIndexedCouponPricer>());
    AverageOIS::Type type();
    QuantLib::Real nominal();
    const std::vector<QuantLib::Real>& nominals();
    QuantLib::Rate fixedRate();
    const std::vector<QuantLib::Rate>& fixedRates();
    const QuantLib::DayCounter& fixedDayCounter();
    const ext::shared_ptr<OvernightIndex> overnightIndex();
    QuantLib::Natural rateCutoff();
    QuantLib::Spread onSpread();
    const std::vector<QuantLib::Spread>& onSpreads();
    QuantLib::Real onGearing();
    const std::vector<QuantLib::Real>& onGearings();
    const QuantLib::DayCounter& onDayCounter();
    const QuantLib::Leg& fixedLeg();
    const QuantLib::Leg& overnightLeg();
    QuantLib::Real fixedLegBPS();
    QuantLib::Real fixedLegNPV();
    QuantLib::Real fairRate();
    QuantLib::Real overnightLegBPS();
    QuantLib::Real overnightLegNPV();
    QuantLib::Spread fairSpread();
};

%shared_ptr(AverageONIndexedCouponPricer)
class AverageONIndexedCouponPricer : public FloatingRateCouponPricer {
  public:
    enum Approximation { Takada, None };
    AverageONIndexedCouponPricer(AverageONIndexedCouponPricer::Approximation approxType
        = AverageONIndexedCouponPricer::Takada);
    void initialize(const QuantLib::FloatingRateCoupon& coupon);
    QuantLib::Rate swapletRate() const;
};

#endif
