/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2021 StatPro Italia srl

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

#include <qle/calendars/islamicweekendsonly.hpp>

namespace QuantExt {

    IslamicWeekendsOnly::IslamicWeekendsOnly() {
        // all calendar instances share the same implementation instance
        static ext::shared_ptr<Calendar::Impl> impl(new IslamicWeekendsOnly::Impl);
        impl_ = impl;
    }

    bool IslamicWeekendsOnly::Impl::isWeekend(Weekday w) const {
        return w == Friday || w == Saturday;
    }

    bool IslamicWeekendsOnly::Impl::isBusinessDay(const Date& date) const {        
        return !isWeekend(date.weekday());
    }

}

