/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/dependencies.hpp
    \brief Utility functions for determining curve dependencies
    \ingroup utilities
*/

#pragma once

#include <ored/marketdata/market.hpp>

namespace ore {
namespace data {

//! Add additional curve dependencies to the given map by MarketObject
/*!
  \ingroup utilities
*/
void addMarketObjectDependencies(
    std::map<std::string, std::map<ore::data::MarketObject, std::set<std::string>>>* objects,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs, const std::string& baseCcy,
    const std::string& baseCcyDiscountCurve);

//! Generate the curve spec name for a market object
/*!
  \ingroup utilities
*/
std::string marketObjectToCurveSpec(const MarketObject& mo, const std::string& name, const string& baseCcy,
                                    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs);

/*! Get the discount curve for the currency \p ccy.
        - if the \p ccy is the base currency, determines the discount curve from the config
        - if the \p ccy is not the base currency, the discount curve is by convention `ccy-IN-base`
  \ingroup utilities
*/
std::string currencyToDiscountCurve(const std::string& ccy, const std::string& baseCcy,
                                    const std::string& baseCcyDiscountCurve,
                                    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs);

//! Find an appropriate discount curve for a SwapIndex if non provided
/*!
  \ingroup utilities
*/
std::string swapIndexDiscountCurve(const std::string& ccy, const std::string& baseCcy = std::string(), 
    const std::string& swapIndexConvId = std::string());

/*! Auto build a curve config for a \p curveId of form CCY1-IN-CCY2, which represents a curve of CCY1
    collateralised in CCY 2

    \ingroup utilities
*/
void buildCollateralCurveConfig(const string& curveId, const std::string& baseCcy,
                               const std::string& baseCcyDiscountCurve,
                                const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs);

/*! Get a list of ccy's for which we have a discount curve that the given ccy is collateralised in
    e.g. given ccy is CHF, we have CHF-IN-EUR, CHF-IN-USD, method returns EUR, USD

    \ingroup utilities
*/
std::set<std::string> getCollateralisedDiscountCcy(const std::string& ccy,
    const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs);

/*! Check if curve of form CCY1-IN-CCY2

    \ingroup utilities
*/
const bool isCollateralCurve(const std::string& id, std::vector<std::string>& tokens);

} // namespace data
} // namespace ore