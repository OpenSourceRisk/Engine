/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file portfolio/makenonstandardlegs.hpp
    \brief make functions for non-standard ibor and fixed legs
    \ingroup tradedata
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/indexes/iborindex.hpp>

namespace ore::data {

QuantLib::Leg makeNonStandardIborLeg(
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index, const std::vector<QuantLib::Date>& calcDates,
    const std::vector<QuantLib::Date>& payDates, const std::vector<QuantLib::Date>& fixingDates,
    const std::vector<QuantLib::Date>& resetDates, const QuantLib::Size fixingDays,
    const std::vector<QuantLib::Real>& notionals, const std::vector<QuantLib::Date>& notionalDates,
    const std::vector<QuantLib::Real>& spreads, const std::vector<QuantLib::Date>& spreadDates,
    const std::vector<QuantLib::Real>& gearings, const std::vector<QuantLib::Date>& gearingDates,
    const bool strictNotionalDates, const QuantLib::DayCounter& dayCounter, const QuantLib::Calendar& payCalendar,
    const QuantLib::BusinessDayConvention payConv, const QuantLib::Period& payLag, const bool isInArrears);

QuantLib::Leg makeNonStandardFixedLeg(const std::vector<QuantLib::Date>& calcDates,
                                      const std::vector<QuantLib::Date>& payDates,
                                      const std::vector<QuantLib::Real>& notionals,
                                      const std::vector<QuantLib::Date>& notionalDates,
                                      const std::vector<QuantLib::Real>& rates,
                                      const std::vector<QuantLib::Date>& rateDates, const bool strictNotionalDates,
                                      const QuantLib::DayCounter& dayCounter, const QuantLib::Calendar& payCalendar,
                                      const QuantLib::BusinessDayConvention payConv, const QuantLib::Period& payLag);

} // namespace ore::data
