/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qle/cashflows/commoditycashflow.hpp>

using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Days;
using std::set;

namespace QuantExt {

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
