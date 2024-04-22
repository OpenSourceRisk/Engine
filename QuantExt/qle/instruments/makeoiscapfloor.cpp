/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/instruments/makeoiscapfloor.hpp>

#include <ql/cashflows/cashflows.hpp>

namespace QuantExt {

MakeOISCapFloor::MakeOISCapFloor(CapFloor::Type type, const Period& tenor, const ext::shared_ptr<OvernightIndex>& index,
                                 const Period& rateComputationPeriod, Rate strike,
                                 const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve)
    : type_(type), tenor_(tenor), index_(index), rateComputationPeriod_(rateComputationPeriod), strike_(strike),
      nominal_(1.0), settlementDays_(2), calendar_(index->fixingCalendar()), convention_(ModifiedFollowing),
      rule_(DateGeneration::Backward), dayCounter_(index->dayCounter()), telescopicValueDates_(false),
      discountCurve_(discountCurve) {}

MakeOISCapFloor::operator Leg() const {
    Calendar calendar = index_->fixingCalendar();

    Date startDate;
    if (effectiveDate_ != Date()) {
        startDate = effectiveDate_;
    } else {
        Date refDate = Settings::instance().evaluationDate();
        startDate = calendar.advance(calendar.adjust(refDate), settlementDays_ * Days);
    }

    Date endDate = calendar.adjust(startDate + tenor_, ModifiedFollowing);

    Schedule schedule(startDate, endDate, rateComputationPeriod_, calendar, ModifiedFollowing, ModifiedFollowing,
                      DateGeneration::Backward, false);

    // determine atm strike if required
    Real effectiveStrike = strike_;
    if (effectiveStrike == Null<Real>()) {
        Leg leg = OvernightLeg(schedule, index_)
                      .withNotionals(nominal_)
                      .withPaymentDayCounter(dayCounter_)
                      .withPaymentAdjustment(convention_)
                      .withTelescopicValueDates(telescopicValueDates_);
        effectiveStrike =
            CashFlows::atmRate(leg, discountCurve_.empty() ? **index_->forwardingTermStructure() : **discountCurve_,
                               false, index_->forwardingTermStructure()->referenceDate());
    }

    Real cap = Null<Real>(), floor = Null<Real>();
    if (type_ == CapFloor::Cap)
        cap = effectiveStrike;
    else if (type_ == CapFloor::Floor)
        floor = effectiveStrike;
    else {
        QL_FAIL("MakeOISCapFloor: expected type Cap or Floor");
    }

    Leg leg = OvernightLeg(schedule, index_)
                  .withNotionals(nominal_)
                  .withPaymentDayCounter(dayCounter_)
                  .withPaymentAdjustment(convention_)
                  .withCaps(cap)
                  .withFloors(floor)
                  .withNakedOption(true)
                  .withTelescopicValueDates(telescopicValueDates_);

    if (pricer_) {
        for (auto &c : leg) {
            auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c);
            if (f) {
                f->setPricer(pricer_);
            }
        }
    }

    return leg;
}

MakeOISCapFloor& MakeOISCapFloor::withNominal(Real n) {
    nominal_ = n;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withEffectiveDate(const Date& effectiveDate) {
    effectiveDate_ = effectiveDate;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withSettlementDays(Natural settlementDays) {
    settlementDays_ = settlementDays;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withCalendar(const Calendar& cal) {
    calendar_ = cal;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withConvention(BusinessDayConvention bdc) {
    convention_ = bdc;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withRule(DateGeneration::Rule r) {
    rule_ = r;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withDayCount(const DayCounter& dc) {
    dayCounter_ = dc;
    return *this;
}

MakeOISCapFloor& MakeOISCapFloor::withTelescopicValueDates(bool t) {
    telescopicValueDates_ = t;
    return *this;
}

MakeOISCapFloor&
MakeOISCapFloor::withCouponPricer(const ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& pricer) {
    pricer_ = pricer;
    return *this;
}

Leg getOisCapFloorUnderlying(const Leg& oisCapFloor) {
    Leg underlying;
    for (auto const& c : oisCapFloor) {
        auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c);
        QL_REQUIRE(cfon, "getOisCapFloorUnderlying(): expected CappedFlooredOvernightIndexedCoupon");
        underlying.push_back(cfon->underlying());
    }
    return underlying;
}

std::vector<std::pair<Real, Real>> getOisCapFloorStrikes(const Leg& oisCapFloor) {
    std::vector<std::pair<Real, Real>> result;
    for (auto const& c : oisCapFloor) {
        auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c);
        QL_REQUIRE(cfon, "getOisCapFloorUnderlying(): expected CappedFlooredOvernightIndexedCoupon");
        result.push_back(std::make_pair(cfon->cap(), cfon->floor()));
    }
    return result;
}

} // namespace QuantExt
