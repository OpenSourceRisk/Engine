/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 RiskMap srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
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
        YearCounter()
        : QuantLib::DayCounter(boost::shared_ptr<DayCounter::Impl>(
                                             new YearCounter::Impl())){}
      private:
        class Impl : public DayCounter::Impl {
          public:
            std::string name() const { return "Year"; }
            QuantLib::Date::serial_type dayCount(const QuantLib::Date& d1,
                                       const QuantLib::Date& d2) const;
            QuantLib::Time yearFraction(const QuantLib::Date& d1,
                              const QuantLib::Date& d2,
                              const QuantLib::Date&,
                              const QuantLib::Date&) const;
        };
    };

}

#endif
