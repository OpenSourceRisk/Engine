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
    bool includeExpiry, const Date& referenceDate, Natural offset) {
    
    auto convention = getConvention(contractName);

    // Set the date relative to which we are calculating the next expiry
    Date today = referenceDate == Date() ? Settings::instance().evaluationDate() : referenceDate;

    // Get the next expiry date relative to referenceDate
    Date expiryDate = nextExpiry(referenceDate, *convention);

    // If expiry date equals today and we have asked not to include expiry, return next contract's expiry
    if (expiryDate == today && !includeExpiry && offset == 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days, *convention);
    }

    // If offset is greater than 0, keep getting next expiry out
    while (offset > 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days, *convention);
        offset--;
    }

    return expiryDate;
}

Date ConventionsBasedFutureExpiry::expiryDate(const string& contractName, Month contractMonth,
    Year contractYear, Natural monthOffset) {

    auto convention = getConvention(contractName);
    return expiry(contractMonth, contractYear, *convention, monthOffset);
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
    const CommodityFutureConvention& conventions, QuantLib::Natural monthOffset) const {

    Date expiry;

    // Apply month offset if non-zero
    if (monthOffset > 0) {
        Date newDate = Date(15, contractMonth, contractYear) + monthOffset * Months;
        contractMonth = newDate.month();
        contractYear = newDate.year();
    }

    // Move n months previously for the expiry if necessary
    if (conventions.expiryMonthLag() > 0) {
        Date newDate = Date(15, contractMonth, contractYear) - conventions.expiryMonthLag() * Months;
        contractMonth = newDate.month();
        contractYear = newDate.year();
    }

    // Calculate the relevant date in the expiry month and year
    if (conventions.dayOfMonthBased()) {
        Date last = Date::endOfMonth(Date(1, contractMonth, contractYear));
        if (conventions.dayOfMonth() > static_cast<Natural>(last.dayOfMonth())) {
            expiry = last;
        } else {
            expiry = Date(conventions.dayOfMonth(), contractMonth, contractYear);
        }
    } else {
        expiry = Date::nthWeekday(conventions.nth(), conventions.weekday(), contractMonth, contractYear);
    }

    // If expiry date is not a good business day, adjust it before applying the offset.
    if (conventions.adjustBeforeOffset()) {
        expiry = conventions.calendar().adjust(expiry, conventions.businessDayConvention());
    }

    // Apply offset adjustments if necessary
    expiry = conventions.calendar().advance(expiry, -static_cast<Integer>(conventions.offsetDays()), 
        Days, conventions.businessDayConvention());

    // If expiry date is one of the prohibited dates, move to preceding business day
    while (conventions.prohibitedExpiries().count(expiry) > 0) {
        expiry = conventions.calendar().advance(expiry, -1, Days, Preceding);
    }

    return expiry;
}

Date ConventionsBasedFutureExpiry::nextExpiry(const Date& referenceDate,
    const CommodityFutureConvention& convention) const {

    // Get a contract expiry before today and increment until expiryDate is >= today
    Date guideDate(15, convention.oneContractMonth(), referenceDate.year() - 1);
    Date expiryDate = expiry(convention.oneContractMonth(), referenceDate.year() - 1, convention);
    QL_REQUIRE(expiryDate < referenceDate, "Expected the expiry date in the previous year to be before reference");
    while (expiryDate < referenceDate) {
        guideDate += Period(convention.contractFrequency());
        expiryDate = expiry(guideDate.month(), guideDate.year(), convention);
    }

    return expiryDate;
}

}
}
