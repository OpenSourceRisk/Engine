/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file switzerland.hpp
    \brief Swiss calendar
*/

#ifndef quantext_swiss_calendar_hpp
#define quantext_swiss_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Swiss calendar
/*! Holidays:
    <ul>
    <li>Saturdays</li>
    <li>Sundays</li>
    <li>New Year's Day, January 1st</li>
    <li>Berchtoldstag, January 2nd</li>
    <li>Good Friday</li>
    <li>Easter Monday</li>
    <li>Ascension Day</li>
    <li>Whit Monday</li>
    <li>Labour Day, May 1st</li>
    <li>National Day, August 1st</li>
    <li>Christmas, December 25th</li>
    <li>St. Stephen's Day, December 26th</li>
    </ul>

    \ingroup calendars
*/
class Switzerland : public Calendar {
private:
    class SettlementImpl : public Calendar::WesternImpl {
    public:
        std::string name() const override { return "Switzerland"; }
        bool isBusinessDay(const Date&) const override;
    };
    class SixImpl : public Calendar::WesternImpl {
    public:
        std::string name() const override { return "SIX Swiss Exchange"; }
        bool isBusinessDay(const Date&) const override;
    };

public:
    enum Market {
        Settlement, //!< generic settlement calendar
        SIX         //!< SIX Swiss Exchange calendar
    };
    Switzerland(Market market = Settlement);
};

} // namespace QuantExt

#endif
