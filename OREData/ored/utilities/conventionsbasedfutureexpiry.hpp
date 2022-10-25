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

/*! \file ored/utilities/conventionsbasedfutureexpiry.hpp
    \brief Base class for classes that perform date calculations for future contracts
    \ingroup utilities
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace ore {
namespace data {

//! Perform date calculations for future contracts based on conventions
class ConventionsBasedFutureExpiry : public QuantExt::FutureExpiryCalculator {
public:
    ConventionsBasedFutureExpiry(const std::string& commName, QuantLib::Size maxIterations = 10);
    ConventionsBasedFutureExpiry(const CommodityFutureConvention& convention, QuantLib::Size maxIterations = 10);

    QuantLib::Date nextExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                              QuantLib::Natural offset = 0, bool forOption = false) override;

    QuantLib::Date priorExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                               bool forOption = false) override;

    QuantLib::Date expiryDate(const QuantLib::Date& contractDate, QuantLib::Natural monthOffset = 0,
                              bool forOption = false) override;

    QuantLib::Date contractDate(const QuantLib::Date& expiryDate) override;

    QuantLib::Date applyFutureMonthOffset(const QuantLib::Date& contractDate, Natural futureMonthOffset) override;

    //! \name Inspectors
    //@{
    //! Return the commodity future convention.
    const CommodityFutureConvention& commodityFutureConvention() const;

    //! Return the maximum iterations parameter.
    QuantLib::Size maxIterations() const;
    //@}

private:
    CommodityFutureConvention convention_;
    QuantLib::Size maxIterations_;

    //! Given a \p contractMonth, a \p contractYear and \p conventions, calculate the contract expiry date
    QuantLib::Date expiry(QuantLib::Day dayOfMonth, QuantLib::Month contractMonth, QuantLib::Year contractYear,
                          QuantLib::Natural monthOffset, bool forOption) const;

    //! Do the next expiry work
    QuantLib::Date nextExpiry(const QuantLib::Date& referenceDate, bool forOption) const;

    //! Account for prohibited expiries
    QuantLib::Date avoidProhibited(const QuantLib::Date& expiry, bool forOption) const;
};

} // namespace data
} // namespace ore
