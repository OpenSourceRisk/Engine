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

/*! \file qle/calendars/israel.hpp
    \brief Israel calendar extension to cover TELBOR publication days
*/

#ifndef quantext_israel_calendar_hpp
#define quantext_israel_calendar_hpp

#include <ql/time/calendar.hpp>
#include <ql/time/calendars/israel.hpp>

namespace QuantExt {

//! Israel calendar
/*! Extend Israel calendar to cover TELBOR publication dates as described at:
    https://www.boi.org.il/en/Markets/TelborMarket/Documents/telbordef_eng.pdf
    Telbor holidays 2019, 2020:
    https://www.boi.org.il/en/Markets/TelborMarket/Documents/NoTelborRates2019.pdf
    https://www.boi.org.il/en/Markets/TelborMarket/Documents/NoTelborRates2020.pdf

    \ingroup calendars
*/
class Israel : public QuantLib::Israel {

private:
    class TelborImpl : public Calendar::Impl {
    public:
        std::string name() const { return "Israel Telbor Implementation"; }
        bool isWeekend(QuantLib::Weekday w) const;
        bool isBusinessDay(const QuantLib::Date& date) const;
    };

public:
    enum MarketExt { Settlement, TASE, Telbor };
    Israel(MarketExt market = Telbor);
};

} // namespace QuantExt

#endif
