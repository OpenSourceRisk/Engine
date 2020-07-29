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

/*! \file actual364.hpp
     \brief day counter that with basis 364
*/

#ifndef quantext_actual364_counter_hpp
#define quantext_actual364_counter_hpp

#include <ql/time/daycounter.hpp>

namespace QuantExt {

//! Actual364 day counter
/*! This day counter computes a day count fraction using
    364 base days.

    \ingroup daycounters
*/
class Actual364 : public QuantLib::DayCounter {
public:
    Actual364() : QuantLib::DayCounter(boost::shared_ptr<DayCounter::Impl>(new Actual364::Impl())) {}

private:
    class Impl : public DayCounter::Impl {
    public:
        std::string name() const { return "Actual/364"; }
        QuantLib::Date::serial_type dayCount(const QuantLib::Date& d1, const QuantLib::Date& d2) const;
        QuantLib::Time yearFraction(const QuantLib::Date& d1, const QuantLib::Date& d2, const QuantLib::Date&,
                                    const QuantLib::Date&) const;
    };
};

} // namespace QuantExt

#endif
