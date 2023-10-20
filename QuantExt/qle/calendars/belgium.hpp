/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file belgium.hpp
    \brief Belgian calendar
*/

#ifndef quantext_belgian_calendar_hpp
#define quantext_belgian_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;

class Belgium : public Calendar {
private:
    class SettlementImpl : public Calendar::WesternImpl {
    public:
        std::string name() const override { return "Belgian settlement"; }
        bool isBusinessDay(const Date&) const override;
    };

public:
    enum Market {
        Settlement // generic settlement calendar
    };
    Belgium(Market m = Settlement);
};

} // namespace QuantExt

#endif
