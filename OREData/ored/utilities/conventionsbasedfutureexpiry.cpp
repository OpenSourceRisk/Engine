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

#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

ConventionsBasedFutureExpiry::ConventionsBasedFutureExpiry(const CommodityFutureConvention& convention,
                                                           Size maxIterations)
    : convention_(convention), maxIterations_(maxIterations) {}

Date ConventionsBasedFutureExpiry::nextExpiry(bool includeExpiry, const Date& referenceDate, Natural offset,
                                              bool forOption) {

    // Set the date relative to which we are calculating the next expiry
    Date today = referenceDate == Date() ? Settings::instance().evaluationDate() : referenceDate;

    // Get the next expiry date relative to referenceDate
    Date expiryDate = nextExpiry(today, forOption);

    // If expiry date equals today and we have asked not to include expiry, return next contract's expiry
    if (expiryDate == today && !includeExpiry && offset == 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days, forOption);
    }

    // If offset is greater than 0, keep getting next expiry out
    while (offset > 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days, forOption);
        offset--;
    }

    return expiryDate;
}

Date ConventionsBasedFutureExpiry::priorExpiry(bool includeExpiry, const Date& referenceDate, bool forOption) {

    // Set the date relative to which we are calculating the preceding expiry
    Date today = referenceDate == Date() ? Settings::instance().evaluationDate() : referenceDate;

    // Get the next expiry relative to the reference date (including the reference date)
    Date expiry = nextExpiry(true, today, 0, forOption);

    // If that expiry is equal to reference date and we have set includeExpiry to true, we are done.
    if (includeExpiry && expiry == today)
        return expiry;

    // Get the preceding expiry.
    Period p(convention_.contractFrequency());
    Date baseDate = convention_.expiryCalendar().advance(expiry, -p);
    expiry = nextExpiry(true, baseDate, 0, forOption);

    // May still not have the preceding expiry but must be close
    Size counter = maxIterations_;
    while (expiry >= today && counter > 0) {
        baseDate--;
        counter--;
        expiry = nextExpiry(true, baseDate, 0, forOption);
    }
    QL_REQUIRE(expiry < today, "Expected that expiry " << io::iso_date(expiry) << " would be less than reference date "
                                                       << io::iso_date(today) << ".");

    return expiry;
}

Date ConventionsBasedFutureExpiry::expiryDate(const Date& contractDate, Natural monthOffset, bool forOption) {
    if (convention_.contractFrequency() == Daily) {
        return nextExpiry(contractDate, forOption);
    } else {
        return expiry(contractDate.month(), contractDate.year(), monthOffset, forOption);
    }
}

Date ConventionsBasedFutureExpiry::expiry(Month contractMonth, Year contractYear, QuantLib::Natural monthOffset,
                                          bool forOption) const {

    Date expiry;

    // Apply month offset if non-zero
    if (monthOffset > 0) {
        Date newDate = Date(15, contractMonth, contractYear) + monthOffset * Months;
        contractMonth = newDate.month();
        contractYear = newDate.year();
    }

    // Move n months before (+ve) or after (-ve) for the expiry if necessary
    if (convention_.expiryMonthLag() != 0) {
        Date newDate = Date(15, contractMonth, contractYear) - convention_.expiryMonthLag() * Months;
        contractMonth = newDate.month();
        contractYear = newDate.year();
    }

    // Calculate the relevant date in the expiry month and year
    if (convention_.anchorType() == CommodityFutureConvention::AnchorType::DayOfMonth) {
        Date last = Date::endOfMonth(Date(1, contractMonth, contractYear));
        if (convention_.dayOfMonth() > static_cast<Natural>(last.dayOfMonth())) {
            expiry = last;
        } else {
            expiry = Date(convention_.dayOfMonth(), contractMonth, contractYear);
        }
    } else if (convention_.anchorType() == CommodityFutureConvention::AnchorType::NthWeekday) {
        expiry = Date::nthWeekday(convention_.nth(), convention_.weekday(), contractMonth, contractYear);
    } else if (convention_.anchorType() == CommodityFutureConvention::AnchorType::CalendarDaysBefore) {
        expiry = Date(1, contractMonth, contractYear) - convention_.calendarDaysBefore() * Days;
    } else {
        QL_FAIL("Did not recognise the commodity future convention's anchor type");
    }

    // If expiry date is not a good business day, adjust it before applying the offset.
    if (convention_.adjustBeforeOffset()) {
        expiry = convention_.expiryCalendar().adjust(expiry, convention_.businessDayConvention());
    }

    // Apply offset adjustments if necessary. A negative integer indicates that we move forward that number of days.
    expiry = convention_.expiryCalendar().advance(expiry, -convention_.offsetDays(), Days);

    // If we want the option contract expiry, do the extra work here.
    if (forOption) {
        if (convention_.optionExpiryDay() != Null<Natural>()) {

            // Option expiry may be specified as n months before future expiry. Adjust here if necessary.
            auto optionMonth = expiry.month();
            auto optionYear = expiry.year();

            if (convention_.optionExpiryMonthLag() != 0) {
                Date newDate = Date(15, optionMonth, optionYear) - convention_.optionExpiryMonthLag() * Months;
                optionMonth = newDate.month();
                optionYear = newDate.year();
            }

            auto d = convention_.optionExpiryDay();
            expiry = Date(d, optionMonth, optionYear);
            expiry = convention_.expiryCalendar().adjust(expiry, convention_.optionBusinessDayConvention());

        } else {

            QL_REQUIRE(convention_.optionExpiryMonthLag() == 0 ||
                convention_.expiryMonthLag() == convention_.optionExpiryMonthLag(), "The expiry month lag " <<
                "and the option expiry month lag should be the same if using option expiry offset days.");

            expiry = convention_.expiryCalendar().advance(expiry,
                -static_cast<Integer>(convention_.optionExpiryOffset()), Days);
        }

        expiry = avoidProhibited(expiry, true);

    } else {
        // If expiry date is one of the prohibited dates, move to preceding or following business day
        expiry = avoidProhibited(expiry, false);
    }

    return expiry;
}

Date ConventionsBasedFutureExpiry::nextExpiry(const Date& referenceDate, bool forOption) const {

    // If contract frequency is daily, next expiry is simply the next valid date on expiry calendar.
    if (convention_.contractFrequency() == Daily) {
        Date expiry = convention_.expiryCalendar().adjust(referenceDate, Following);
        return avoidProhibited(expiry, false);
    }

    // Get a contract expiry before today and increment until expiryDate is >= today
    Date guideDate(15, convention_.oneContractMonth(), referenceDate.year() - 1);
    Date expiryDate = expiry(convention_.oneContractMonth(), referenceDate.year() - 1, 0, forOption);
    QL_REQUIRE(expiryDate < referenceDate, "Expected the expiry date in the previous year to be before reference");
    while (expiryDate < referenceDate) {
        guideDate += Period(convention_.contractFrequency());
        expiryDate = expiry(guideDate.month(), guideDate.year(), 0, forOption);
    }

    return expiryDate;
}

const CommodityFutureConvention& ConventionsBasedFutureExpiry::commodityFutureConvention() const {
    return convention_;
}

Size ConventionsBasedFutureExpiry::maxIterations() const {
    return maxIterations_;
}

Date ConventionsBasedFutureExpiry::avoidProhibited(const Date& expiry, bool forOption) const {

    // If expiry date is one of the prohibited dates, move to preceding or following business day.
    using PE = CommodityFutureConvention::ProhibitedExpiry;
    Date result = expiry;
    const auto& pes = convention_.prohibitedExpiries();
    auto it = pes.find(PE(result));

    while (it != pes.end()) {

        // If not relevant, exit.
        if ((!forOption && !it->forFuture()) || (forOption && !it->forOption()))
            break;

        auto bdc = !forOption ? it->futureBdc() : it->optionBdc();

        if (bdc == Preceding || bdc == ModifiedPreceding) {
            result = convention_.calendar().advance(result, -1, Days, bdc);
        } else if (bdc == Following || bdc == ModifiedFollowing) {
            result = convention_.calendar().advance(result, 1, Days, bdc);
        } else {
            QL_FAIL("Convention " << bdc << " associated with prohibited expiry " <<
                io::iso_date(result) << " is not supported.");
        }

        it = pes.find(PE(result));
    }

    return result;
}

} // namespace data
} // namespace ore
