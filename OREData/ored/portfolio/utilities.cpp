/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/portfolio/utilities.hpp>
#include <ranges>

namespace ore {
namespace data {

using namespace QuantLib;
using QuantExt::DateDeltaUnit;
using QuantExt::DateDeltaAnchor;
using std::vector;

vector<Date> createPaymentDates(const ScheduleData& scheduleData, const Schedule& schedule,
    const Calendar& paymentCalendar, BusinessDayConvention paymentConvention, const Period& paymentLag,
    ext::optional<DateDeltaUnit> ddUnit, ext::optional<DateDeltaAnchor> ddAnchor, bool alwaysCalc,
    const Date& openEndDateReplacement, bool endOfMonth, const ext::optional<BusinessDayConvention>& eomConvention) {

    vector<Date> res;

    // If the payment lag is 0, and alwaysCalc is false, return empty dates.
    if (paymentLag.length() == 0 && !alwaysCalc)
        return res;

    // If the payment lag anchor is set and unadjusted, we need to create an unadjusted version of the main schedule to 
    // get the anchor dates from which to calculate the payment dates.
    ext::optional<Schedule> unadjustedSchedule;
    if (ddAnchor && *ddAnchor == DateDeltaAnchor::Unadjusted) {
        unadjustedSchedule = Schedule();
        ScheduleBuilder scheduleBuilder;
        scheduleBuilder.add(*unadjustedSchedule, scheduleData);
        scheduleBuilder.makeSchedules(openEndDateReplacement, true);
    }

    // Set the anchor schedule to be the unadjusted schedule if we have one, otherwise the main schedule.
    const Schedule& anchorSchedule = unadjustedSchedule ? *unadjustedSchedule : schedule;
    QL_ENSURE(anchorSchedule.size() > 1, "createPaymentDates: anchor schedule must have at least two dates "
        "but only has " << anchorSchedule.size() << ".");
    QL_ENSURE(anchorSchedule.size() > schedule.size(), "createPaymentDates: anchor schedule size (" <<
        anchorSchedule.size() << ") must be the same as the main schedule size (" << schedule.size() << ").");

    // If the lag unit is calendar days, the payment lag should be given in days.
    if (ddUnit && *ddUnit == DateDeltaUnit::CalendarDays) {
        QL_REQUIRE(paymentLag.units() == Days, "createPaymentDates: when payment lag unit is calendar days, "
            "payment lag period must be in days but is " << paymentLag.units() << ".");
    }

    // Populate the payment dates by applying the payment lag to the anchor schedule, accounting for the lag unit.
    res.reserve(anchorSchedule.size() - 1);
    for (Size i = 1; i < anchorSchedule.size(); ++i) {
        if (ddUnit && *ddUnit == DateDeltaUnit::CalendarDays) {
            res.push_back(paymentCalendar.adjust(anchorSchedule[i] + paymentLag, paymentConvention));
        } else {
            res.push_back(paymentCalendar.advance(anchorSchedule[i], paymentLag, paymentConvention,
                endOfMonth, eomConvention));
        }
    }

    return res;

}


} // namespace data
} // namespace ore
