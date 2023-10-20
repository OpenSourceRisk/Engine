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

/*! \file qle/utilities/time.hpp
    \brief time related utilities.
*/

#pragma once

#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>

namespace QuantExt {

/* convert period to time using 1Y = 1, 1M = 1/12, 1W = 7/365, 1D = 1/365 */
QuantLib::Real periodToTime(const QuantLib::Period& p);

/*! Imply cds index term from start and end date. If no reasonable term can be implied, 0 * Days is returned */
QuantLib::Period implyIndexTerm(const QuantLib::Date& startDate, const QuantLib::Date& endDate);

/* For t >= 0 get the largest date d such that dc.yearFraction(refDate, d) <= t. If the condition
   dc.yearFraction(refDate, d+1) > t is not met, an exception is thrown. */
QuantLib::Date lowerDate(const QuantLib::Real t, const QuantLib::Date& refDate, const QuantLib::DayCounter& dc);

/* Find period such that the difference to length is < 1.0 / 365.25 and prefer unit years > months > days */
QuantLib::Period tenorFromLength(const QuantLib::Real length);

/* Get the accumulated daylight savings correction between two dates, both treated as "included" for different
   locations (see http://www.webexhibits.org/daylightsaving)
   Null          no daylight saving, returns 0 always
   US            Start: Second Sunday in March
                 End  : First Sunday in November
*/
QuantLib::Integer daylightSavingCorrection(const std::string& location, const QuantLib::Date& start,
                                           const QuantLib::Date& end);

} // namespace QuantExt
