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

Leg makeNonStandardIborLeg(const QuantLib::ext::shared_ptr<IborIndex>& index, const std::vector<Date>& calcDates,
                           const std::vector<Date>& payDatesInput, const std::vector<Date>& fixingDatesInput,
                           const std::vector<Date>& resetDatesInput, const Size fixingDays,
                           const std::vector<Real>& notionals, const std::vector<Date>& notionalDatesInput,
                           const std::vector<Real>& spreadsInput, const std::vector<Date>& spreadDatesInput,
                           const std::vector<Real>& gearingsInput, const std::vector<Date>& gearingDatesInput,
                           const bool strictNotionalDates, const DayCounter& dayCounter, const Calendar& payCalendar,
                           const BusinessDayConvention payConv, const Period& payLag, const bool isInArrears) {

    // add spread and gearing if none is given

    std::vector<Real> spreads = spreadsInput, gearings = gearingsInput;

    if (spreads.empty()) {
        spreads.push_back(0.0);
    }

    if (gearings.empty()) {
        gearings.push_back(1.0);
    }

    // checks

    QL_REQUIRE(calcDates.size() >= 2,
               "makeNonStandardIborLeg(): calc dates size (" << calcDates.size() << ") >= 2 required");
    QL_REQUIRE(!notionals.empty(), "makeNonStandardIborLeg(): empty notinoals");
    QL_REQUIRE(notionalDatesInput.empty() || notionalDatesInput.size() == notionals.size() - 1,
               "makeNonStandardIborLeg(): notional dates (" << notionalDatesInput.size() << ") must match notional ("
                                                            << notionals.size() << ") minus 1");
    QL_REQUIRE(spreadDatesInput.empty() || spreadDatesInput.size() == spreads.size() - 1,
               "makeNonStandardIborLeg(): spread dates (" << spreadDatesInput.size() << ") must match spread ("
                                                          << spreads.size() << ") minus 1");
    QL_REQUIRE(gearingDatesInput.empty() || gearingDatesInput.size() == gearings.size() - 1,
               "makeNonStandardIborLeg(): gearing dates (" << gearingDatesInput.size() << ") must match gearing ("
                                                           << gearings.size() << ") minus 1");

    // populate pay dates, reset dates, fixing dates, notional dates if not explicitly given

    std::vector<Date> payDates(payDatesInput);
    std::vector<Date> resetDates(resetDatesInput);
    std::vector<Date> fixingDates(fixingDatesInput);
    std::vector<Date> notionalDates(notionalDatesInput);
    std::vector<Date> spreadDates(spreadDatesInput);
    std::vector<Date> gearingDates(gearingDatesInput);

    if (payDates.empty()) {
        for (Size i = 1; i < calcDates.size(); ++i) {
            payDates.push_back(payCalendar.advance(calcDates[i], payLag, payConv));
        }
    }

    if (resetDates.empty() && fixingDates.empty()) {
        for (Size i = 0; i < calcDates.size() - 1; ++i) {
            resetDates.push_back(calcDates[i]);
            fixingDates.push_back(index->fixingCalendar().advance(isInArrears ? calcDates[i + 1] : calcDates[i],
                                                                  -static_cast<int>(fixingDays) * Days, Preceding));
        }
    } else if (resetDates.empty()) {
        for (Size i = 0; i < fixingDates.size(); ++i) {
            resetDates.push_back(index->fixingCalendar().advance(fixingDates[i], fixingDays * Days, Following));
        }
    } else if (fixingDates.empty()) {
        for (Size i = 0; i < resetDates.size(); ++i) {
            fixingDates.push_back(
                index->fixingCalendar().advance(resetDates[i], -static_cast<int>(fixingDays) * Days, Preceding));
        }
    }

    if (notionalDates.empty()) {
        for (Size i = 1; i < notionals.size(); ++i)
            notionalDates.push_back(calcDates[i]);
    }

    if (spreadDates.empty()) {
        for (Size i = 1; i < spreads.size(); ++i)
            spreadDates.push_back(calcDates[i]);
    }

    if (gearingDates.empty()) {
        for (Size i = 1; i < gearings.size(); ++i)
            gearingDates.push_back(calcDates[i]);
    }

    // more checks

    QL_REQUIRE(payDates.size() == calcDates.size() - 1, "makeNonStandardIborLeg(): pay dates size ("
                                                            << payDates.size() << ") = calc dates size ("
                                                            << calcDates.size() << ") minus 1 required");
    QL_REQUIRE(fixingDates.size() == resetDates.size(), "makeNonStandardIborLeg(): fixing dates ("
                                                            << fixingDates.size() << ") must match reset dates ("
                                                            << resetDates.size() << ")");

    for (Size i = 0; i < fixingDates.size(); ++i) {
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
                                                             << " (" << fixingDates[i + 1] << ")");
    }

    for (Size i = 0; i < resetDates.size() - 1; ++i) {
        QL_REQUIRE(resetDates[i] <= resetDates[i + 1], "makeNonStandardIborLeg(): reset date at "
                                                           << i << " (" << resetDates[i]
                                                           << ") must be less or equal reset date at " << (i + 1)
                                                           << " (" << resetDates[i + 1] << ")");
    }

    // build calculation periods including broken periods due to notional resets or fixing resets

    std::set<Date> effCalcDates;

    for (auto const& d : calcDates)
        effCalcDates.insert(d);

    for (auto const& d : resetDates) {
        if (d >= calcDates.front() && d < calcDates.back())
            effCalcDates.insert(d);
    }

    if (strictNotionalDates) {
        for (auto const& d : notionalDates) {
            if (d >= calcDates.front() && d < calcDates.back())
                effCalcDates.insert(d);
        }
    }

    // build coupons

    Leg leg;

    for (auto startDate = effCalcDates.begin(); startDate != std::next(effCalcDates.end(), -1); ++startDate) {
        // determine calc end date from start date

        Date endDate = *std::next(startDate, 1);

        if (endDate > calcDates.back())
            continue;

        // determine pay date

        auto nextCalcDate = std::lower_bound(calcDates.begin(), calcDates.end(), endDate);
        Date payDate = payDates[std::max<Size>(1, std::distance(calcDates.begin(), nextCalcDate)) - 1];

        // determine reset and thereby fixing date

        auto nextReset = std::upper_bound(resetDates.begin(), resetDates.end(), *startDate);
        QL_REQUIRE(nextReset != resetDates.begin(),
                   "makeNonStandardIborLeg(): calc start date "
                       << *startDate << " is before first reset date " << *resetDates.begin()
                       << ". Ensure that there is a reset date on or before the calc start date.");
        Date fixingDate = fixingDates[std::distance(resetDates.begin(), nextReset) - 1];

        // determine notional

        auto notionalDate = std::upper_bound(notionalDates.begin(), notionalDates.end(), *startDate);
        Real notional =
            notionals[std::min<Size>(notionals.size() - 1, std::distance(notionalDates.begin(), notionalDate))];

        // determine spread

        auto spreadDate = std::upper_bound(spreadDates.begin(), spreadDates.end(), *startDate);
        Real spread = spreads[std::min<Size>(spreads.size() - 1, std::distance(spreadDates.begin(), spreadDate))];

        // determine gearing

        auto gearingDate = std::upper_bound(gearingDates.begin(), gearingDates.end(), *startDate);
        Real gearing = gearings[std::min<Size>(gearings.size() - 1, std::distance(gearingDates.begin(), gearingDate))];

        // build coupon

        leg.push_back(QuantLib::ext::make_shared<IborCoupon>(payDate, notional, *startDate, endDate, fixingDate, index, gearing,
                                                     spread, Date(), Date(), dayCounter));
    }

    return leg;
}

Leg makeNonStandardFixedLeg(const std::vector<Date>& calcDates, const std::vector<Date>& payDatesInput,
                            const std::vector<Real>& notionals, const std::vector<Date>& notionalDatesInput,
                            const std::vector<Real>& rates, const std::vector<Date>& rateDatesInput,
                            const bool strictNotionalDates, const DayCounter& dayCounter, const Calendar& payCalendar,
                            const BusinessDayConvention payConv, const Period& payLag) {

    // checks

    QL_REQUIRE(calcDates.size() >= 2,
               "makeNonStandardFixedLeg(): calc dates size (" << calcDates.size() << ") >= 2 required");
    QL_REQUIRE(!notionals.empty(), "makeNonStandardFixedLeg(): empty notinoals");
    QL_REQUIRE(!rates.empty(), "makeNonStandardFixedLeg(): empty rates");
    QL_REQUIRE(notionalDatesInput.empty() || notionalDatesInput.size() == notionals.size() - 1,
               "makeNonStandardFixedLeg(): notional dates (" << notionalDatesInput.size() << ") must match notional ("
                                                             << notionals.size() << ") minus 1");
    QL_REQUIRE(rateDatesInput.empty() || rateDatesInput.size() == rates.size() - 1,
               "makeNonStandardIborLeg(): rate dates (" << rateDatesInput.size() << ") must match rate ("
                                                        << rates.size() << ") minus 1");

    for (Size i = 0; i < calcDates.size() - 1; ++i) {
        QL_REQUIRE(calcDates[i] <= calcDates[i + 1], "makeNonStandardFixedLeg(): calc date at "
                                                         << i << " (" << calcDates[i]
                                                         << ") must be less or equal calc date at " << (i + 1) << " ("
                                                         << calcDates[i + 1] << ")");
    }

    // populate pay dates, notional dates if not given

    std::vector<Date> payDates(payDatesInput);
    std::vector<Date> notionalDates(notionalDatesInput);
    std::vector<Date> rateDates(rateDatesInput);

    if (payDates.empty()) {
        for (Size i = 1; i < calcDates.size(); ++i) {
            payDates.push_back(payCalendar.advance(calcDates[i], payLag, payConv));
        }
    }

    if (notionalDates.empty()) {
        for (Size i = 1; i < notionals.size(); ++i)
            notionalDates.push_back(calcDates[i]);
    }

    if (rateDates.empty()) {
        for (Size i = 1; i < rates.size(); ++i)
            rateDates.push_back(calcDates[i]);
    }

    // more checks

    QL_REQUIRE(payDates.size() == calcDates.size() - 1, "makeNonStandardFixedLeg(): pay dates size ("
                                                            << payDates.size() << ") = calc dates size ("
                                                            << calcDates.size() << ") minus 1 required");

    // build calculation periods including broken periods due to notional resets or fixing resets

    std::set<Date> effCalcDates;

    for (auto const& d : calcDates)
        effCalcDates.insert(d);

    if (strictNotionalDates)
        for (auto const& d : notionalDates) {
            if (d >= calcDates.front() && d < calcDates.back())
                effCalcDates.insert(d);
        }

    // build coupons

    Leg leg;

    for (auto startDate = effCalcDates.begin(); startDate != std::next(effCalcDates.end(), -1); ++startDate) {

        // determine calc end date from start date

        Date endDate = *std::next(startDate, 1);

        if (endDate >= calcDates.back())
            continue;

        // determine pay date

        auto nextCalcDate = std::lower_bound(calcDates.begin(), calcDates.end(), endDate);
        Date payDate = payDates[std::max<Size>(1, std::distance(calcDates.begin(), nextCalcDate)) - 1];

        // determine notional

        auto notionalDate = std::upper_bound(notionalDates.begin(), notionalDates.end(), *startDate);
        Real notional =
            notionals[std::min<Size>(notionals.size() - 1, std::distance(notionalDates.begin(), notionalDate))];

        // determine rate

        auto rateDate = std::upper_bound(rateDates.begin(), rateDates.end(), *startDate);
        Real rate = rates[std::min<Size>(rates.size() - 1, std::distance(rateDates.begin(), rateDate))];

        // build coupon

        leg.push_back(QuantLib::ext::make_shared<FixedRateCoupon>(payDate, notional, rate, dayCounter, *startDate, endDate,
                                                          Date(), Date()));
    }

    return leg;
}

} // namespace ore::data
