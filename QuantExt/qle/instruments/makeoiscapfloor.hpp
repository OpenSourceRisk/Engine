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

/*! \file makeoiscapfloor.hpp
    \brief helper class to instantiate standard market OIS cap / floors
*/

#pragma once

#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/instruments/capfloor.hpp>

namespace QuantExt {

class MakeOISCapFloor {
public:
    // optional discount curve is used to determine ATM level (and only that)
    MakeOISCapFloor(CapFloor::Type type, const Period& tenor, const ext::shared_ptr<OvernightIndex>& index,
                    const Period& rateComputationPeriod, Rate strike,
                    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());

    operator Leg() const;

    MakeOISCapFloor& withNominal(Real n);
    MakeOISCapFloor& withEffectiveDate(const Date& effectiveDate);
    MakeOISCapFloor& withSettlementDays(Natural settlementDays);
    MakeOISCapFloor& withCalendar(const Calendar& cal);
    MakeOISCapFloor& withConvention(BusinessDayConvention bdc);
    MakeOISCapFloor& withRule(DateGeneration::Rule r);
    MakeOISCapFloor& withDayCount(const DayCounter& dc);
    MakeOISCapFloor& withTelescopicValueDates(bool telescopicValueDates);

    MakeOISCapFloor& withCouponPricer(const ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer>& pricer);
    MakeOISCapFloor& withDiscountCurve();

private:
    CapFloor::Type type_;
    Period tenor_;
    QuantLib::ext::shared_ptr<OvernightIndex> index_;
    Period rateComputationPeriod_;
    Rate strike_;

    Real nominal_;
    Date effectiveDate_;
    Natural settlementDays_;
    Calendar calendar_;
    BusinessDayConvention convention_;
    DateGeneration::Rule rule_;
    DayCounter dayCounter_;
    bool telescopicValueDates_;

    ext::shared_ptr<CappedFlooredOvernightIndexedCouponPricer> pricer_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
};

//! get the underlying ON coupons from an OIS cf
Leg getOisCapFloorUnderlying(const Leg& oisCapFloor);

//! get the (cap, floor) - strikes from an OIS cf
std::vector<std::pair<Real, Real>> getOisCapFloorStrikes(const Leg& oisCapFloor);

} // namespace QuantExt
