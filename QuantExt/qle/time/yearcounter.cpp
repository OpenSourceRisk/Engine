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

#include <qle/time/yearcounter.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using QuantLib::Date;
using QuantLib::Time;

namespace QuantExt {
    namespace { QuantLib::DayCounter underlyingDCF = QuantLib::ActualActual(); }
    
    Date::serial_type YearCounter::Impl::dayCount(const Date& d1,
                                                       const Date& d2) const {
        return underlyingDCF.dayCount(d1,d2);
    }

    Time YearCounter::Impl::yearFraction(const Date& d1,
                                              const Date& d2,
                                              const Date&,
                                              const Date&) const {
        Time t = underlyingDCF.yearFraction(d1,d2);

        return std::round(t);
    }

}

