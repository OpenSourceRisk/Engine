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

/*! \file qle/calendars/ireland.hpp
    \brief Ireland Calendar
*/

#pragma once

#include <ql/time/calendar.hpp>

namespace QuantExt {

//! Ireland Calendars
/*! Public holidays:
    <ul>
    <li>Saturdays</li>
    <li>Sundays</li>
    <li>New Year's Day, January 1st (possibly moved to Monday)</li>
    <li>Good Friday</li>
    <li>Easter Monday</li>
    <li>St. Patricks Day,March 17th</li>
    <li>May Bank Holiday, first Monday of May</li>
    <li>June Bank Holiday, first Monday of June</li>
    <li>August Bank Holiday, first Monday of August</li>
    <li>October Bank Holiday, last Monday of August</li>
    <li>Christmas Day, December 25th (possibly moved to Monday or
        Tuesday)</li>
    <li>Boxing Day, December 26th (possibly moved to Monday or
        Tuesday)</li>
    </ul>


    \ingroup calendars

    \test the correctness of the returned results is tested
          against a list of known holidays.
*/
class Ireland : public QuantLib::Calendar {
private:
    class IrishStockExchangeImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "IrishStockExchange"; }
        bool isBusinessDay(const QuantLib::Date&) const override;
    };
    class BankHolidaysImpl : public IrishStockExchangeImpl {
    public:
        std::string name() const override { return "Ireland"; }
        bool isBusinessDay(const QuantLib::Date&) const override;
    };

public:
    enum Market {IrishStockExchange, BankHolidays};

    Ireland(const Market market=IrishStockExchange);
};
} // namespace QuantExt