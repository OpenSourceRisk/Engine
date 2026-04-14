/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/utilities/irconventionbasedfutureexpiry.hpp>
#include <ored/utilities/marketdata.hpp>

namespace ore {
namespace data {

IrConventionBasedFutureExpiry::IrConventionBasedFutureExpiry(const std::string& irFutureConventionName) {
    auto [found, convention] =
        InstrumentConventions::instance().conventions()->get(irFutureConventionName, Convention::Type::Future);
    QL_REQUIRE(found, "IR convention based future expiry: no convention found with id " << irFutureConventionName
                                                                                        << " and type Future");
    convention_ = QuantLib::ext::dynamic_pointer_cast<ore::data::FutureConvention>(convention);
}

QuantLib::Date IrConventionBasedFutureExpiry::nextExpiry(bool includeExpiry, const QuantLib::Date& referenceDate,
                                                         QuantLib::Natural offset) const {
    // Set the date relative to which we are calculating the next expiry
    Date today = referenceDate == Date() ? Settings::instance().evaluationDate() : referenceDate;

    // Get the next expiry date relative to referenceDate
    Date expiryDate = nextExpiry(today);

    // If expiry date equals today and we have asked not to include expiry, return next contract's expiry
    if (expiryDate == today && !includeExpiry && offset == 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days);
    }

    // If offset is greater than 0, keep getting next expiry out
    while (offset > 0) {
        expiryDate = nextExpiry(expiryDate + 1 * Days);
        offset--;
    }

    return expiryDate;
}

QuantLib::Date IrConventionBasedFutureExpiry::nextExpiry(const QuantLib::Date& date) const {
    QL_REQUIRE(convention_, "IR convention based future expiry: convention not set");
    QL_REQUIRE(convention_->index(), "IR convention based future expiry: convention index not set");
    auto oisIndex = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndex>(convention_->index());
    bool isMMFuture = oisIndex == nullptr;

    Month m = date.month();
    Year y = date.year();
    auto nextFutureDate = [&isMMFuture, this](Month m, Year y) {
        if (isMMFuture) {
            return getMmFutureExpiryDate(m, y, convention_->dateGenerationRule());
        } else {
            return getOiFutureStartEndDate(m, y, convention_->tenor(), convention_->dateGenerationRule(),
                                           convention_->calendar())
                .second;
        };
    };
    auto nextDate = nextFutureDate(m, y);
    // the expiry date could be before the current date, in which case we need to move to the next month
    if (nextDate >= date)
        return nextDate;
    y = m == Dec ? y + 1 : y;
    m = m == Dec ? Jan : Month(m + 1);
    nextDate =  nextFutureDate(m, y);
    QL_REQUIRE(nextDate >= date, "IR convention based future expiry: next expiry date "
                                     << nextDate << " is before reference date " << date);
    return nextDate;
}


QuantLib::Date expiryToIrCurveDate(const QuantLib::ext::shared_ptr<Expiry>& expiry, const QuantLib::Date& refDate,
                                   const std::optional<IrConventionBasedFutureExpiry>& irFutureExpiry) {
    QL_REQUIRE(expiry, "expiryToIrCurveDate: expiry not provided");
    if (auto periodExpiry = QuantLib::ext::dynamic_pointer_cast<ExpiryPeriod>(expiry)) {
        Date asof = refDate == Date() ? Settings::instance().evaluationDate() : refDate;
        return asof + periodExpiry->expiryPeriod();
    }
    if (auto dateExpiry = QuantLib::ext::dynamic_pointer_cast<ExpiryDate>(expiry)) {
        return dateExpiry->expiryDate();
    }
    if (auto futureExpiry = QuantLib::ext::dynamic_pointer_cast<FutureContinuationExpiry>(expiry)) {
        QL_REQUIRE(irFutureExpiry.has_value(),
                   "expiryToIrCurveDate: irFutureExpiry is required to convert FutureContinuationExpiry");
        auto offset = futureExpiry->expiryIndex() > 1 ? futureExpiry->expiryIndex() - 1 : 0;
        return irFutureExpiry->nextExpiry(true, refDate, offset);
    }
    QL_FAIL("expiryToIrCurveDate: unsupported Expiry type");
}

} // namespace data
} // namespace ore
