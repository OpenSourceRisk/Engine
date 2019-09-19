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

/*! \file ored/utilities/futureexpirycalculator.hpp
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
    //! Constructor that takes a set of \p conventions
    ConventionsBasedFutureExpiry(const boost::shared_ptr<ore::data::Conventions>& conventions);

    //! Provide implementation for the base class method
    QuantLib::Date nextExpiry(const std::string& contractName, bool includeExpiry = true,
        const QuantLib::Date& referenceDate = QuantLib::Date(), QuantLib::Natural offset = 0) override;

    //! Provide implementation for the base class method
    QuantLib::Date expiryDate(const std::string& contractName, QuantLib::Month contractMonth,
        QuantLib::Year contractYear, QuantLib::Natural monthOffset) override;

private:
    boost::shared_ptr<ore::data::Conventions> conventions_;

    //! Get the conventions for the future contract \p contractName
    boost::shared_ptr<CommodityFutureConvention> getConvention(const std::string& contractName) const;

    //! Given a \p contractMonth, a \p contractYear and \p conventions, calculate the contract expiry date
    QuantLib::Date expiry(QuantLib::Month contractMonth, QuantLib::Year contractYear,
        const CommodityFutureConvention& conventions, QuantLib::Natural monthOffset = 0) const;

    //! Do the next expiry work
    QuantLib::Date nextExpiry(const QuantLib::Date& referenceDate,
        const CommodityFutureConvention& convention) const;
};

}
}
