/*
 Copyright (C) 2021 Quaternion Risk Management Ltd

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

