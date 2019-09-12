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
    bool includeExpiry, const Date& referenceDate) {
    
    // Get the future expiry conventions for the given contractName
    QL_REQUIRE(conventions_->has(contractName), "Need conventions for contract " << contractName << ".");
    auto convention = boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions_->get(contractName));
    QL_REQUIRE(convention, "Expected conventions for contract " << contractName << " to be of type CommodityFuture.");

    // Set the date relative to which we are calculating the next expiry
    Date today = referenceDate == Date() ? Settings::instance().evaluationDate() : referenceDate;

    // Get a contract expiry before today and increment until expiryDate is >= today
    Date guideDate(15, convention->oneContractMonth(), today.year() - 1);
    Date expiryDate = expiry(convention->oneContractMonth(), today.year() - 1, *convention);
    QL_REQUIRE(expiryDate < today, "Expected the expiry date in the previous year to be before today");
    while (expiryDate < today) {
        guideDate += Period(convention->contractFrequency());
        expiryDate = expiry(guideDate.month(), guideDate.year(), *convention);
    }

    // If expiry date equals today and we have asked not to include expiry, return next contracts expiry
    if (expiryDate == today && !includeExpiry) {
        guideDate += Period(convention->contractFrequency());
        expiryDate = expiry(guideDate.month(), guideDate.year(), *convention);
    }

    return expiryDate;
}

Date ConventionsBasedFutureExpiry::expiry(Month contractMonth, Year contractYear, 
    const CommodityFutureConvention& conventions) const {

    Date expiry;

    // Move to previous month for the expiry if necessary
    if (conventions.expiryInPreviousMonth()) {
        Date newDate = Date(15, contractMonth, contractYear) - 1 * Months;
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

    // If expiry date is not a good business day, adjust it before applying the offset. We might need another 
    // flag or element in the conventions to control this but do it now in all cases
    expiry = conventions.calendar().adjust(expiry, conventions.businessDayConvention());

    // Apply offset adjustments if necessary
    expiry = conventions.calendar().advance(expiry, -static_cast<Integer>(conventions.offsetDays()), 
        Days, conventions.businessDayConvention());

    return expiry;
}

}
}
