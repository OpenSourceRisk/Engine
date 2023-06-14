/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/portfolio/makenonstandardlegs.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>

using namespace QuantLib;

namespace ore::data {

Leg makeNonStandardIborLeg(const boost::shared_ptr<IborIndex>& index, const std::vector<Date>& calcDates,
                           const std::vector<Date>& payDates, const std::vector<Date>& fixingDatesInput,
                           const std::vector<Date>& resetDatesInput, const Size fixingDays,
                           const std::vector<Real>& notionals, const std::vector<Date>& notionalDates,
                           const std::vector<Real>& spreads, const std::vector<Date>& spreadDates,
                           const std::vector<Real>& gearings, const std::vector<Date>& gearingDates,
                           const bool strictResetDates, const bool strictNotionalDates, const DayCounter& dayCounter) {

    // checks

    QL_REQUIRE(calcDates.size() >= 2,
               "makeNonStandardIborLeg(): calc dates size (" << calcDates.size() << ") >= 2 required");
    QL_REQUIRE(payDates.size() == calcDates.size() - 1, "makeNonStandardIborLeg(): pay dates size ("
                                                            << payDates.size() << ") = calc dates size ("
                                                            << calcDates.size() << " required");
    QL_REQUIRE(!notionals.empty(), "makeNonStandardIborLeg(): empty notinoals");
    QL_REQUIRE(notionalDates.empty() || notionalDates.size() == notionals.size() - 1,
               "makeNonStandardIborLeg(): notional dates (" << notionalDates.size() << ") must match notional ("
                                                            << notionals.size() << ") minus 1");
    QL_REQUIRE(spreadDates.empty() || spreadDates.size() == spreads.size() - 1,
               "makeNonStandardIborLeg(): spread dates (" << spreadDates.size() << ") must match spread ("
                                                          << spreads.size() << ") minus 1");
    QL_REQUIRE(gearingDates.empty() || gearingDates.size() == gearings.size() - 1,
               "makeNonStandardIborLeg(): gearing dates (" << gearingDates.size() << ") must match gearing ("
                                                           << gearings.size() << ") minus 1");

    // populate reset dates, fixing dates if not explicitly given

    std::vector<Date> resetDates(resetDatesInput);
    std::vector<Date> fixingDates(fixingDatesInput);

    if (resetDates.empty()) {
        for (Size i = 0; i < calcDates.size() - 1; ++i) {
            resetDates.push_back(calcDates[i]);
        }
    }

    if (fixingDates.empty()) {
        for (Size i = 0; i < resetDates.size(); ++i) {
            fixingDates.push_back(
                index->fixingCalendar().advance(resetDates[i], -static_cast<int>(fixingDays) * Days, Preceding));
        }
    }

    // more checks

    QL_REQUIRE(fixingDates.size() == resetDates.size(), "makeNonStandardIborLeg(): fixing dates ("
                                                            << fixingDates.size() << ") must match reset dates ("
                                                            << resetDates.size() << ")");
    QL_REQUIRE(resetDates.size() == calcDates.size() - 1, "makeNonStandardIborLeg(): reset dates ("
                                                              << resetDates.size() << ") must match calc dates size ("
                                                              << calcDates.size() << ") minus 1");

    for (Size i = 0; i < fixingDates.size(); ++i) {
        QL_REQUIRE(fixingDates[i] <= resetDates[i], "makeNonStandardIborLeg(): fixing date at "
                                                        << i << " (" << fixingDates[i]
                                                        << ") must be less or equal to reset date at " << i << " ("
                                                        << resetDates[i] << ")");
        QL_REQUIRE(resetDates[i] <= calcDates.back(), "makeNonStandardIborLeg(): reset date at "
                                                          << i << " (" << resetDates[i]
                                                          << ") must be less or equal last calculation date ("
                                                          << calcDates.back());
    }

    for (Size i = 0; i < calcDates.size() - 1; ++i) {
        QL_REQUIRE(calcDates[i] <= calcDates[i + 1], "makeNonStandardIborLeg(): calc date at "
                                                         << i << " (" << calcDates[i]
                                                         << ") must be less or equal calc date at " << (i + 1) << " ("
                                                         << calcDates[i + 1]);
    }

    for (Size i = 0; i < fixingDates.size() - 1; ++i) {
        QL_REQUIRE(fixingDates[i] <= fixingDates[i + 1], "makeNonStandardIborLeg(): fixing date at "
                                                             << i << " (" << fixingDates[i]
                                                             << ") must be less or equal fixing date at " << (i + 1)
                                                             << " (" << fixingDates[i + 1]);
    }

    for (Size i = 0; i < resetDates.size() - 1; ++i) {
        QL_REQUIRE(resetDates[i] <= resetDates[i + 1], "makeNonStandardIborLeg(): reset date at "
                                                           << i << " (" << resetDates[i]
                                                           << ") must be less or equal reset date at " << (i + 1)
                                                           << " (" << resetDates[i + 1]);
    }

    // build calculation periods including broken periods due to notional resets or fixing resets

    std::set<Date> effCalcDates;

    for (auto const& d : calcDates)
        effCalcDates.insert(d);

    if (strictResetDates)
        for (auto const& d : resetDates)
            effCalcDates.insert(d);

    if (strictNotionalDates)
        for (auto const& d : notionalDates) {
            effCalcDates.insert(d);
        }

    // build coupons

    Leg leg;

    for (auto startDate = effCalcDates.begin(); startDate != std::next(effCalcDates.end(), -1); ++startDate) {

        // determine calc end date from start date

        Date endDate = *std::next(startDate, 1);

        // determine pay date

        auto nextCalcDate = std::lower_bound(calcDates.begin(), calcDates.end(), endDate);
        QL_REQUIRE(nextCalcDate != calcDates.begin() && nextCalcDate != calcDates.end(),
                   "makeNonStandardIborLeg(): internal error, nextCalcDate == calcDates.begin() or nextCalcDate == "
                   "calcDates.end(), contact dev.");
        Date payDate = payDates[std::distance(calcDates.begin(), nextCalcDate) - 1];

        // determine reset and thereby fixing date

        auto nextCalcDate2 = std::upper_bound(effCalcDates.begin(), effCalcDates.end(), *startDate);
        QL_REQUIRE(nextCalcDate2 != effCalcDates.begin(),
                   "makeNonStandardIborLeg(): internal error, nextCalcDate2 == effCalcDates.begin(), contact dev.");
        Date fixingDate = fixingDates[std::distance(effCalcDates.begin(), nextCalcDate2) - 1];

        // determine notional

        auto notionalDate = std::upper_bound(notionalDates.begin(), notionalDates.end(), *startDate);
        Real notional = notionals[std::max<std::size_t>(1, std::distance(notionalDates.begin(), notionalDate)) - 1];

        // determine spread

        auto spreadDate = std::upper_bound(spreadDates.begin(), spreadDates.end(), *startDate);
        Real spread = spreads[std::max<std::size_t>(1, std::distance(spreadDates.begin(), spreadDate)) - 1];

        // determine gearing

        auto gearingDate = std::upper_bound(gearingDates.begin(), gearingDates.end(), *startDate);
        Real gearing = gearings[std::max<std::size_t>(1, std::distance(gearingDates.begin(), gearingDate)) - 1];

        // build coupon

        leg.push_back(boost::make_shared<IborCoupon>(payDate, notional, *startDate, endDate, fixingDate, index, gearing,
                                                     spread, Date(), Date(), dayCounter));
    }

    return leg;
}

Leg makeNonStandardFixedLeg(const std::vector<Date>& calcDates, const std::vector<Date>& payDates,
                            const std::vector<Real>& notionals, const std::vector<Date>& notionalDates,
                            const std::vector<Real>& rates, const std::vector<Date>& rateDates,
                            const bool strictNotionalDates, const DayCounter& dayCounter) {

    // checks

    QL_REQUIRE(calcDates.size() >= 2,
               "makeNonStandardFixedLeg(): calc dates size (" << calcDates.size() << ") >= 2 required");
    QL_REQUIRE(payDates.size() == calcDates.size() - 1, "makeNonStandardFixedLeg(): pay dates size ("
                                                            << payDates.size() << ") = calc dates size ("
                                                            << calcDates.size() << " required");
    QL_REQUIRE(!notionals.empty(), "makeNonStandardFixedLeg(): empty notinoals");
    QL_REQUIRE(notionalDates.empty() || notionalDates.size() == notionals.size() - 1,
               "makeNonStandardFixedLeg(): notional dates (" << notionalDates.size() << ") must match notional ("
                                                             << notionals.size() << ") minus 1");
    QL_REQUIRE(rateDates.empty() || rateDates.size() == rates.size() - 1,
               "makeNonStandardIborLeg(): rate dates (" << rateDates.size() << ") must match rate (" << rates.size()
                                                        << ") minus 1");

    for (Size i = 0; i < calcDates.size() - 1; ++i) {
        QL_REQUIRE(calcDates[i] <= calcDates[i + 1], "makeNonStandardFixedLeg(): calc date at "
                                                         << i << " (" << calcDates[i]
                                                         << ") must be less or equal calc date at " << (i + 1) << " ("
                                                         << calcDates[i + 1]);
    }

    // build calculation periods including broken periods due to notional resets or fixing resets

    std::set<Date> effCalcDates;

    for (auto const& d : calcDates)
        effCalcDates.insert(d);

    if (strictNotionalDates)
        for (auto const& d : notionalDates) {
            effCalcDates.insert(d);
        }

    // build coupons

    Leg leg;

    for (auto startDate = effCalcDates.begin(); startDate != std::next(effCalcDates.end(), -1); ++startDate) {

        // determine calc end date from start date

        Date endDate = *std::next(startDate, 1);

        // determine pay date

        auto nextCalcDate = std::lower_bound(calcDates.begin(), calcDates.end(), endDate);
        QL_REQUIRE(nextCalcDate != calcDates.begin() && nextCalcDate != calcDates.end(),
                   "makeNonStandardFixedLeg(): internal error, nextCalcDate == calcDates.begin() or nextCalcDate == "
                   "calcDates.end(), contact dev.");
        Date payDate = payDates[std::distance(calcDates.begin(), nextCalcDate) - 1];

        // determine notional

        auto notionalDate = std::upper_bound(notionalDates.begin(), notionalDates.end(), *startDate);
        Real notional = notionals[std::max<std::size_t>(1, std::distance(notionalDates.begin(), notionalDate)) - 1];

        // determine rate

        auto rateDate = std::upper_bound(rateDates.begin(), rateDates.end(), *startDate);
        Real rate = rates[std::max<std::size_t>(1, std::distance(rateDates.begin(), rateDate)) - 1];

        // build coupon

        leg.push_back(boost::make_shared<FixedRateCoupon>(payDate, notional, rate, dayCounter, *startDate, endDate,
                                                          Date(), Date()));
    }

    return leg;
}

} // namespace ore::data
