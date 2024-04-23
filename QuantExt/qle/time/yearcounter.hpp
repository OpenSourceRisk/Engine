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

/*! \file yearcounter.hpp
     \brief day counter that returns the nearest integer yearfraction
*/

#ifndef quantext_year_counter_hpp
#define quantext_year_counter_hpp

#include <ql/time/daycounter.hpp>

namespace QuantExt {

//! Year counter for when we want a whole number year fraction.
/*! This day counter computes a day count fraction using an underlying
    day counter (ACTACT) before rounding the result to
    the nearest integer.

    \ingroup daycounters
*/
class YearCounter : public QuantLib::DayCounter {
public:
    YearCounter() : QuantLib::DayCounter(QuantLib::ext::shared_ptr<DayCounter::Impl>(new YearCounter::Impl())) {}

private:
    class Impl : public DayCounter::Impl {
    public:
        std::string name() const override { return "Year"; }
        QuantLib::Date::serial_type dayCount(const QuantLib::Date& d1, const QuantLib::Date& d2) const override;
        QuantLib::Time yearFraction(const QuantLib::Date& d1, const QuantLib::Date& d2, const QuantLib::Date&,
                                    const QuantLib::Date&) const override;
    };
};

} // namespace QuantExt

#endif
