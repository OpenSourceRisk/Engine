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
#ifndef ORE_MAURITIUS_HPP
#define ORE_MAURITIUS_HPP

#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;
//! Mauritius calendar
/*! Holidays for which rule is not given are from SEM
 * https://www.stockexchangeofmauritius.com/about-us/market-holidays
 * SEM only provides non-gregorian holidays for the years 2022-2023

\ingroup calendars
        */
class Mauritius : public Calendar{
private:
    class SemImpl : public Calendar::WesternImpl{
        std::string name() const override { return "Stock Exchange of Mauritius"; }
        bool isBusinessDay(const Date&) const override;
    };
public:
    enum Market { SEM // Stock exchange of Mauritius
    };
    Mauritius(Market m = SEM);
};

}
#endif // ORE_MAURITIUS_HPP
