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

/*! \file cme.hpp
    \brief CME Group exchange calendars
*/

#ifndef quantext_cme_calendar_hpp
#define quantext_cme_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantExt {

/*! CME group calendar outlined on the website at:
    https://www.cmegroup.com/tools-information/holiday-calendar.html
*/
class CME : public QuantLib::Calendar {

private:
    class Impl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "CME Group"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

public:
    CME();
};

} // namespace QuantExt

#endif
