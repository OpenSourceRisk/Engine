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

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

ConventionsBasedFutureExpiry::ConventionsBasedFutureExpiry(const boost::shared_ptr<Conventions>& conventions)
    : conventions_(conventions) {}

Date ConventionsBasedFutureExpiry::nextExpiry(const string& contractName, 
    bool includeExpiry, const Date& referenceDate, Natural offset, bool forOption) {
    
    auto convention = getConvention(contractName);

    // Set the date relative to which we are calculating the next expiry
    Date today = referenceDate == Date() ? Settings::instance().evaluationDate() : referenceDate;

    // Get the next expiry date relative to referenceDate
    Date expiryDate = nextExpiry(referenceDate, *convention, forOption);

    // If expiry date equals today and we have asked not to include expiry, return next contract's expiry
    if (expiryDate == today && !includeExpiry && offset == 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days, *convention, forOption);
    }

    // If offset is greater than 0, keep getting next expiry out
    while (offset > 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days, *convention, forOption);
        offset--;
    }

    return expiryDate;
}

Date ConventionsBasedFutureExpiry::expiryDate(const string& contractName, Month contractMonth,
    Year contractYear, Natural monthOffset, bool forOption) {

    auto convention = getConvention(contractName);
    return expiry(contractMonth, contractYear, *convention, monthOffset, forOption);
}

boost::shared_ptr<CommodityFutureConvention> ConventionsBasedFutureExpiry::getConvention(
    const string& contractName) const {
    
    // Get the future expiry conventions for the given contractName
    QL_REQUIRE(conventions_->has(contractName), "Need conventions for contract " << contractName << ".");
    auto convention = boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions_->get(contractName));
    QL_REQUIRE(convention, "Expected conventions for contract " << contractName << " to be of type CommodityFuture.");
    return convention;
}

Date ConventionsBasedFutureExpiry::expiry(Month contractMonth, Year contractYear, 
    const CommodityFutureConvention& convention, QuantLib::Natural monthOffset, bool forOption) const {

    Date expiry;

    // Apply month offset if non-zero
    if (monthOffset > 0) {
        Date newDate = Date(15, contractMonth, contractYear) + monthOffset * Months;
        contractMonth = newDate.month();
        contractYear = newDate.year();
    }

    // Move n months previously for the expiry if necessary
    if (convention.expiryMonthLag() > 0) {
        Date newDate = Date(15, contractMonth, contractYear) - convention.expiryMonthLag() * Months;
        contractMonth = newDate.month();
        contractYear = newDate.year();
    }

    // Calculate the relevant date in the expiry month and year
    if (convention.anchorType() == CommodityFutureConvention::AnchorType::DayOfMonth) {
        Date last = Date::endOfMonth(Date(1, contractMonth, contractYear));
        if (convention.dayOfMonth() > static_cast<Natural>(last.dayOfMonth())) {
            expiry = last;
        } else {
            expiry = Date(convention.dayOfMonth(), contractMonth, contractYear);
        }
    } else if (convention.anchorType() == CommodityFutureConvention::AnchorType::NthWeekday) {
        expiry = Date::nthWeekday(convention.nth(), convention.weekday(), contractMonth, contractYear);
    } else if (convention.anchorType() == CommodityFutureConvention::AnchorType::CalendarDaysBefore) {
        expiry = Date(1, contractMonth, contractYear) - convention.calendarDaysBefore() * Days;
    } else {
        QL_FAIL("Did not recognise the commodity future convention's anchor type");
    }

    // If expiry date is not a good business day, adjust it before applying the offset.
    if (convention.adjustBeforeOffset()) {
        expiry = convention.expiryCalendar().adjust(expiry, convention.businessDayConvention());
    }

    // Apply offset adjustments if necessary
    expiry = convention.expiryCalendar().advance(expiry, -static_cast<Integer>(convention.offsetDays()), Days);

    // If expiry date is one of the prohibited dates, move to preceding business day
    while (convention.prohibitedExpiries().count(expiry) > 0) {
        expiry = convention.calendar().advance(expiry, -1, Days);
    }

    // If we want the option contract expiry, do the extra work here.
    if (forOption) {
        expiry = convention.expiryCalendar().advance(expiry, 
            -static_cast<Integer>(convention.optionExpiryOffset()), Days);

        // If expiry date is one of the prohibited dates, move to preceding business day
        while (convention.prohibitedExpiries().count(expiry) > 0) {
            expiry = convention.expiryCalendar().advance(expiry, -1, Days);
        }
    }

    return expiry;
}

Date ConventionsBasedFutureExpiry::nextExpiry(const Date& referenceDate,
    const CommodityFutureConvention& convention, bool forOption) const {

    // Get a contract expiry before today and increment until expiryDate is >= today
    Date guideDate(15, convention.oneContractMonth(), referenceDate.year() - 1);
    Date expiryDate = expiry(convention.oneContractMonth(), referenceDate.year() - 1, convention, 0, forOption);
    QL_REQUIRE(expiryDate < referenceDate, "Expected the expiry date in the previous year to be before reference");
    while (expiryDate < referenceDate) {
        guideDate += Period(convention.contractFrequency());
        expiryDate = expiry(guideDate.month(), guideDate.year(), convention, 0, forOption);
    }

    return expiryDate;
}

}
}
