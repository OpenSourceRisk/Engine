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

    // Start from the given date's month/year and find the next expiry >= date
    Date candidate;
    Month m = date.month();
    Year y = date.year();
    // Try current month first, then advance month-by-month
    for (Size i = 0; i < 13; ++i) {
        if (isMMFuture) {
            candidate = getMmFutureExpiryDate(m, y, convention_->dateGenerationRule());
        } else {
            candidate = getOiFutureStartEndDate(m, y, convention_->tenor(),
                                                convention_->dateGenerationRule(), convention_->calendar())
                            .second;
        }
        if (candidate >= date)
            return candidate;
        // Advance to next month
        if (m == Dec) {
            m = Jan;
            y++;
        } else {
            m = static_cast<Month>(static_cast<int>(m) + 1);
        }
    }
    QL_FAIL("IrConventionBasedFutureExpiry::nextExpiry: could not find next expiry after " << date);
}

QuantLib::Date expiryToIrCurveDate(const QuantLib::ext::shared_ptr<Expiry>& expiry,
                                   const QuantLib::Date& refDate,
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
    QL_FAIL("expiryToIrCurveDate: unsupported Expiry type " << *expiry);
}

} // namespace data
} // namespace ore
