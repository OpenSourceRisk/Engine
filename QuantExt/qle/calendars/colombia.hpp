/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file colombia.hpp
    \brief Colombian calendar
*/

#ifndef quantext_colombian_calendar_hpp
#define quantext_colombian_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantLib {

class Colombia : public Calendar {
private:
    class CseImpl : public Calendar::WesternImpl {
    public:
        std::string name() const override { return "Colombia Stock Exchange"; }
        bool isBusinessDay(const Date&) const override;
    };

public:
    enum Market {
        CSE // Colombia Stock Exchange
    };
    Colombia(Market m = CSE);
};

} // namespace QuantLib

#endif
