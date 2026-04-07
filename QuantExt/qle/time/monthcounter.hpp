/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file monthcounter.hpp
     \brief day counter that returns the nearest integer month fraction
*/

#ifndef quantext_month_counter_hpp
#define quantext_month_counter_hpp
#include <ql/time/daycounter.hpp>

namespace QuantExt {

//! Month counter for when we want a whole number month fraction.
/*! This day counter computers the number of months between two dates as an integer,
    and divides by 12 to get the year fraction.

    \ingroup daycounters
*/
class MonthCounter : public QuantLib::DayCounter {
public:
    MonthCounter() : QuantLib::DayCounter(QuantLib::ext::shared_ptr<DayCounter::Impl>(new MonthCounter::Impl())) {}
private:
    class Impl : public DayCounter::Impl {
    public:
        std::string name() const override { return "Month"; }
        //! number of months between d1 and d2 divided by 12
        QuantLib::Time yearFraction(const QuantLib::Date& d1, const QuantLib::Date& d2, const QuantLib::Date&,
                                    const QuantLib::Date&) const override;
    };
};

} // namespace QuantExt

#endif
