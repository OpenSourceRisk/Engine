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

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/utilities/curvepillar.hpp>
#include <optional>
#include <variant>

namespace ore {
namespace data {
class IrConventionBasedFutureExpiry {
public:
    IrConventionBasedFutureExpiry(const std::string& irFutureConventionName);

    QuantLib::Date nextExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                              QuantLib::Natural offset = 0) const;

    const QuantLib::ext::shared_ptr<ore::data::FutureConvention>& convention() const { return convention_; }

private:
    QuantLib::ext::shared_ptr<ore::data::FutureConvention> convention_;

    QuantLib::Date nextExpiry(const QuantLib::Date& date) const;
};

// Helper function to convert an expiry to a date
QuantLib::Date expiryToIrCurveDate(const CurvePillar& expiry,
                                   const QuantLib::Date& refDate = QuantLib::Date(),
                                   const std::optional<IrConventionBasedFutureExpiry>& irFutureExpiry = std::nullopt);

} // namespace data
} // namespace ore