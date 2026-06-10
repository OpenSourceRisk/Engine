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

/*! \file portfolio/utilities.hpp
    \brief utility functions used by multiple trade types
    \ingroup tradedata
*/
#pragma once
#include <ored/portfolio/schedule.hpp>
#include <qle/time/dateutilities.hpp>
#include <ql/time/date.hpp>
#include <vector>

namespace ore {
namespace data {

std::vector<QuantLib::Date> createPaymentDates(const ScheduleData& scheduleData, const QuantLib::Schedule& schedule,
    const QuantLib::Calendar& paymentCalendar, QuantLib::BusinessDayConvention paymentConvention,
    const Period& paymentLag, QuantLib::ext::optional<QuantExt::DateDeltaUnit> ddUnit,
    QuantLib::ext::optional<QuantExt::DateDeltaAnchor> ddAnchor,
    const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>(), bool endOfMonth = false,
    const QuantLib::ext::optional<QuantLib::BusinessDayConvention>& eomConvention = QuantLib::ext::nullopt);

} // namespace data
} // namespace ore
