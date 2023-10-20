/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/cashflows/commoditycashflow.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::Visitor;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Days;
using std::set;

namespace QuantExt {

void CommodityCashFlow::accept(QuantLib::AcyclicVisitor& v) {
    if (QuantLib::Visitor<CommodityCashFlow>* v1 = dynamic_cast<QuantLib::Visitor<CommodityCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

CommodityCashFlow::CommodityCashFlow(QuantLib::Real quantity, QuantLib::Real spread, QuantLib::Real gearing,
                                     bool useFuturePrice, const ext::shared_ptr<CommodityIndex>& index,
                                     const ext::shared_ptr<FxIndex>& fxIndex)
    : quantity_(quantity), spread_(spread), gearing_(gearing), useFuturePrice_(useFuturePrice), index_(index),
      fxIndex_(fxIndex) {
    registerWith(index_);
    if (fxIndex) {
        registerWith(fxIndex);
    }
}

set<Date> pricingDates(const Date& s, const Date& e, const Calendar& pricingCalendar,
    bool excludeStart, bool includeEnd, bool useBusinessDays) {

    // If start date is after end date, return no dates.
    if (s > e)
        return set<Date>();

    Date start = s;
    Date end = e;

    // Cover the possible exclusion of the start date
    if ((useBusinessDays && pricingCalendar.isBusinessDay(start)) && excludeStart) {
        start = pricingCalendar.advance(start, 1, Days);
    }

    if ((!useBusinessDays && pricingCalendar.isHoliday(start)) && excludeStart) {
        while (pricingCalendar.isHoliday(start) && start <= end)
            start++;
    }

    // Cover the possible exclusion of the end date
    if ((useBusinessDays && pricingCalendar.isBusinessDay(end)) && !includeEnd) {
        end = pricingCalendar.advance(end, -1, Days);
    }

    if ((!useBusinessDays && pricingCalendar.isHoliday(end)) && !includeEnd) {
        while (pricingCalendar.isHoliday(end) && start <= end)
            end--;
    }

    // Create the set of dates, which may be empty.
    set<Date> res;
    for (; start <= end; start++) {
        if (isPricingDate(start, pricingCalendar, useBusinessDays))
            res.insert(start);
    }

    return res;
}

bool isPricingDate(const Date& d, const Calendar& pricingCalendar, bool useBusinessDays) {
    return ((useBusinessDays && pricingCalendar.isBusinessDay(d)) ||
        (!useBusinessDays && pricingCalendar.isHoliday(d)));
}

}
