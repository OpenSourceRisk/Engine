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

/*! \file qle/utilities/time.hpp
    \brief time related utilities.
*/

#include <qle/utilities/time.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/period.hpp>
#include <ql/time/schedule.hpp>
#include <ql/types.hpp>

#include <vector>

namespace QuantExt {
using namespace QuantLib;

Real periodToTime(const Period& p) {
    switch (p.units()) {
    case Days:
        return static_cast<Real>(p.length()) / 365.25;
    case Weeks:
        return static_cast<Real>(p.length()) * 7.0 / 365.25;
    case Months:
        return static_cast<Real>(p.length()) / 12.0;
    case Years:
        return static_cast<Real>(p.length());
    default:
        QL_FAIL("periodToTime(): time unit (" << p.units() << ") not handled");
    }
}

QuantLib::Period implyIndexTerm(const Date& startDate, const Date& endDate) {
    static const std::vector<Period> eligibleTerms = {5 * Years, 7 * Years, 10 * Years, 3 * Years, 1 * Years,
                                                      2 * Years, 4 * Years, 6 * Years,  8 * Years, 9 * Years};
    static const int gracePeriod = 15;

    for (auto const& p : eligibleTerms) {
        if (std::abs(cdsMaturity(startDate, p, DateGeneration::CDS2015) - endDate) < gracePeriod) {
            return p;
        }
    }

    return 0 * Days;
}

QuantLib::Date lowerDate(const Real t, const QuantLib::Date& refDate, const QuantLib::DayCounter& dc) {
    if (close_enough(t, 0.0))
        return refDate;
    QL_REQUIRE(t > 0.0, "lowerDate(" << t << "," << refDate << "," << dc.name()
                                     << ") was called with negative time, this is not allowed.");
    bool done = false;
    Date d = refDate + static_cast<int>(t * 365.25);
    Real tmp = dc.yearFraction(refDate, d);
    Size attempts = 0;
    while ((tmp < t || close_enough(tmp, t)) && (++attempts < 10000)) {
        ++d;
        tmp = dc.yearFraction(refDate, d);
        done = true;
    }
    QL_REQUIRE(attempts < 10000, "lowerDate(" << t << "," << refDate << "," << dc.name() << ") could not be computed.");
    if (done)
        return --d;
    while ((tmp > t && !close_enough(tmp, t)) && (++attempts < 10000)) {
        --d;
        tmp = dc.yearFraction(refDate, d);
        done = true;
    }
    QL_REQUIRE(attempts < 10000, "lowerDate(" << t << "," << refDate << "," << dc.name() << ") could not be computed.");
    if (done)
        return d;
    QL_FAIL("lowerDate(" << t << "," << refDate << "," << dc.name() << ") could not be computed.");
}

} // namespace QuantExt
