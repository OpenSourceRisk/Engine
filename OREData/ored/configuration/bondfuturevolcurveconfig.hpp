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

/*! \file ored/configuration/bondfuturevolcurveconfig.hpp
    \brief Bond future volatility curve configuration
    \ingroup configuration
*/

#pragma once
#include <ql/shared_ptr.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ored/configuration/onedimsolverconfig.hpp>
#include <ored/configuration/volatilityconfig.hpp>

namespace ore {
namespace data {

/**
 * Bond future volatility configuration
 * \ingroup configuration
 */
class BondFutureVolatilityConfig : public CurveConfig {
public:
    //! Default constructor
    BondFutureVolatilityConfig();

    //! Detailed constructor
    BondFutureVolatilityConfig(
        const std::string& curveId,
        const std::string& curveDescription,
        std::string contractName,
        std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>> volatilityConfig,
        std::string dayCounter = "A365",
        std::string calendar = "NullCalendar",
        std::string yieldCurveId = "",
        QuantLib::Real strikeFactor = 1.0,
        std::string useOnlyPutCall = "",
        QuantLib::ext::optional<OneDimSolverConfig> solverConfig = QuantLib::ext::nullopt,
        QuantLib::ext::optional<bool> preferOutOfTheMoney = QuantLib::ext::nullopt,
        std::string engineOverride = "");

    //! \name Inspectors
    //@{
    const std::string& contractName() const;
    const std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& volatilityConfig() const;
    const std::string& dayCounter() const;
    const std::string& calendar() const;
    const std::string& yieldCurveId() const;
    QuantLib::Real strikeFactor() const;
    const std::string& useOnlyPutCall() const;
    const QuantLib::ext::optional<OneDimSolverConfig>& solverConfig() const;
    const QuantLib::ext::optional<bool>& preferOutOfTheMoney() const;
    const std::string& engineOverride() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string contractName_;
    std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>> volatilityConfig_;
    std::string dayCounter_;
    std::string calendar_;
    std::string yieldCurveId_;
    QuantLib::Real strikeFactor_;
    std::string useOnlyPutCall_;
    QuantLib::ext::optional<OneDimSolverConfig> solverConfig_;
    QuantLib::ext::optional<bool> preferOutOfTheMoney_;
    std::string engineOverride_;

    //! Perform basic checks.
    void validate() const;

    //! Populate required curve IDs
    void populateRequiredIds() const override;

    //! Populate CurveConfig::quotes_ with the required quotes.
    void populateQuotes();
};

} // namespace data
} // namespace ore
