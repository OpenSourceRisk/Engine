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
#include <qle/time/dateutilities.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

ConventionsBasedFutureExpiry::ConventionsBasedFutureExpiry(const std::string& commName, Size maxIterations)
    : maxIterations_(maxIterations) {
    auto p = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(
        InstrumentConventions::instance().conventions()->get(commName));
    QL_REQUIRE(p, "ConventionsBasedFutureExpiry: could not cast to CommodityFutureConvention for '"
                      << commName << "', this is an internal error. Contact support.");
    convention_ = *p;
}

ConventionsBasedFutureExpiry::ConventionsBasedFutureExpiry(const CommodityFutureConvention& convention,
                                                           QuantLib::Size maxIterations)
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
        return expiry(contractDate.dayOfMonth(), contractDate.month(), contractDate.year(), monthOffset, forOption);
    }
}

QuantLib::Date ConventionsBasedFutureExpiry::contractDate(const QuantLib::Date& expiryDate) {

    if (convention_.contractFrequency() == Monthly) {

        // do not attempt to invert the logic in expiry(), instead just search for a valid contract month
        // in a reasonable range

        for (Size m = 0; m < 120; ++m) {
            try {
                Date tmp = Date(15, expiryDate.month(), expiryDate.year()) + m * Months;
                if (expiry(tmp.dayOfMonth(), tmp.month(), tmp.year(), 0, false) == expiryDate)
                    return tmp;
            } catch (...) {
            }
            try {
                Date tmp = Date(15, expiryDate.month(), expiryDate.year()) - m * Months;
                if (expiry(tmp.dayOfMonth(), tmp.month(), tmp.year(), 0, false) == expiryDate)
                    return tmp;
            } catch (...) {
            }
        }

    } else {

        // daily of weekly contract frequency => we can use contract date = expiry date

        return expiryDate;
    }

    QL_FAIL("ConventionsBasedFutureExpiry::contractDate(" << expiryDate << "): could not imply contract date. This is an internal error. Contact support.");
}

QuantLib::Date ConventionsBasedFutureExpiry::applyFutureMonthOffset(const QuantLib::Date& contractDate,
                                                                    Natural futureMonthOffset) {

    Date tmp = contractDate;

    if (convention_.contractFrequency() == Monthly) {
        tmp = Date(15, contractDate.month(), contractDate.year()) + futureMonthOffset * Months;
    }

    return tmp;
}

Date ConventionsBasedFutureExpiry::expiry(Day dayOfMonth, Month contractMonth, Year contractYear, QuantLib::Natural monthOffset,
                                          bool forOption) const {

    Date expiry;
    if (convention_.contractFrequency() == Weekly) {
        QL_REQUIRE(convention_.anchorType() == CommodityFutureConvention::AnchorType::WeeklyDayOfTheWeek,
                   "Please change anchorType to WeeklyDayOfTheWeek for weekly contract expiries");
        Date d(dayOfMonth, contractMonth, contractYear);
        expiry = convention_.expiryCalendar().adjust(d - d.weekday() + convention_.weekday(),
                                                     convention_.businessDayConvention());
    } else {
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

        if (convention_.contractFrequency() == Monthly && !convention_.validContractMonths().empty() &&
            convention_.validContractMonths().size() < 12 &&
            convention_.validContractMonths().count(contractMonth) == 0) {
            // contractMonth is in the not in the list of valid contract months
            return Date();
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
        } else if (convention_.anchorType() == CommodityFutureConvention::AnchorType::BusinessDaysAfter) {
            if (convention_.businessDaysAfter() > 0) {
                expiry = convention_.expiryCalendar().advance(Date(1, contractMonth, contractYear) - 1 * Days,
                                                              convention_.businessDaysAfter(), Days);
            } else {
                expiry = convention_.expiryCalendar().advance(Date(1, contractMonth, contractYear),
                                                              convention_.businessDaysAfter(), Days);
            }
        } else if (convention_.anchorType() == CommodityFutureConvention::AnchorType::LastWeekday) {
            expiry = QuantExt::DateUtilities::lastWeekday(convention_.weekday(), contractMonth, contractYear);
        } else {
            QL_FAIL("Did not recognise the commodity future convention's anchor type");
        }
        // If expiry date is not a good business day, adjust it before applying the offset.
        if (convention_.adjustBeforeOffset()) {
            expiry = convention_.expiryCalendar().adjust(expiry, convention_.businessDayConvention());
        }

        // Apply offset adjustments if necessary. A negative integer indicates that we move forward that number of days.
        expiry = convention_.expiryCalendar().advance(expiry, -convention_.offsetDays(), Days);
    }

    // If we want the option contract expiry, do the extra work here.
    if (forOption && convention_.optionContractFrequency() == Weekly) {
        QL_REQUIRE(convention_.optionAnchorType() == CommodityFutureConvention::OptionAnchorType::WeeklyDayOfTheWeek,
                   "Please change anchorType to WeeklyDayOfTheWeek for weekly contract expiries");
        Date d(expiry.dayOfMonth(), expiry.month(), expiry.year());
        expiry = convention_.expiryCalendar().adjust(d - d.weekday() + convention_.optionWeekday(),
                                                     convention_.businessDayConvention());
        expiry = avoidProhibited(expiry, true);
    } else if (forOption) {
        if (convention_.optionAnchorType() == CommodityFutureConvention::OptionAnchorType::DayOfMonth) {
            auto optionMonth = expiry.month();
            auto optionYear = expiry.year();

            if (convention_.optionExpiryMonthLag() != 0) {
                Date newDate = Date(15, optionMonth, optionYear) - convention_.optionExpiryMonthLag() * Months;
                optionMonth = newDate.month();
                optionYear = newDate.year();
            }

            auto d = convention_.optionExpiryDay();
            Date last = Date::endOfMonth(Date(1, optionMonth, optionYear));
            if (d > static_cast<Natural>(last.dayOfMonth())) {
                expiry = last;
            } else {
                expiry = Date(d, optionMonth, optionYear);
            }
            expiry = convention_.expiryCalendar().adjust(expiry, convention_.optionBusinessDayConvention());
        } else if (convention_.optionAnchorType() == CommodityFutureConvention::OptionAnchorType::BusinessDaysBefore) {
            QL_REQUIRE(convention_.optionExpiryMonthLag() == 0 ||
                           convention_.expiryMonthLag() == convention_.optionExpiryMonthLag(),
                       "The expiry month lag "
                           << "and the option expiry month lag should be the same if using option expiry offset days.");

            expiry = convention_.expiryCalendar().advance(
                expiry, -static_cast<Integer>(convention_.optionExpiryOffset()), Days);
        } else if (convention_.optionAnchorType() == CommodityFutureConvention::OptionAnchorType::NthWeekday) {
            auto optionMonth = expiry.month();
            auto optionYear = expiry.year();

            if (convention_.optionExpiryMonthLag() != 0) {
                Date newDate = Date(15, optionMonth, optionYear) - convention_.optionExpiryMonthLag() * Months;
                optionMonth = newDate.month();
                optionYear = newDate.year();
            }
            expiry = Date::nthWeekday(convention_.optionNth(), convention_.optionWeekday(), optionMonth, optionYear);
            expiry = convention_.expiryCalendar().adjust(expiry, convention_.optionBusinessDayConvention());
        } else if (convention_.optionAnchorType() == CommodityFutureConvention::OptionAnchorType::LastWeekday) {
            auto optionMonth = expiry.month();
            auto optionYear = expiry.year();

            if (convention_.optionExpiryMonthLag() != 0) {
                Date newDate = Date(15, optionMonth, optionYear) - convention_.optionExpiryMonthLag() * Months;
                optionMonth = newDate.month();
                optionYear = newDate.year();
            }
            expiry = QuantExt::DateUtilities::lastWeekday(convention_.optionWeekday(), optionMonth, optionYear);
            expiry = convention_.expiryCalendar().adjust(expiry, convention_.optionBusinessDayConvention());
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

    if ((convention_.contractFrequency() == Daily) && (!forOption || convention_.optionContractFrequency() == Daily)) {
        Date expiry = convention_.expiryCalendar().adjust(referenceDate, Following);
        return avoidProhibited(expiry, false);
    } 

    // Get a contract expiry before today and increment until expiryDate is >= today
    
    Date guideDate(15, convention_.oneContractMonth(), referenceDate.year() - 1);
    Date expiryDate =
        expiry(guideDate.dayOfMonth(), convention_.oneContractMonth(), referenceDate.year() - 1, 0, forOption);
    QL_REQUIRE(expiryDate < referenceDate, "Expected the expiry date in the previous year to be before reference");
    while (expiryDate < referenceDate) {
        if (forOption && (convention_.optionContractFrequency() != convention_.contractFrequency())) {
            guideDate += Period(convention_.optionContractFrequency());
        } else {
            guideDate += Period(convention_.contractFrequency());
        }
        expiryDate = expiry(guideDate.dayOfMonth(), guideDate.month(), guideDate.year(), 0, forOption);
    }

    return expiryDate;
}

const CommodityFutureConvention& ConventionsBasedFutureExpiry::commodityFutureConvention() const { return convention_; }

Size ConventionsBasedFutureExpiry::maxIterations() const { return maxIterations_; }

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
            QL_FAIL("Convention " << bdc << " associated with prohibited expiry " << io::iso_date(result)
                                  << " is not supported.");
        }

        it = pes.find(PE(result));
    }

    return result;
}

} // namespace data
} // namespace ore
