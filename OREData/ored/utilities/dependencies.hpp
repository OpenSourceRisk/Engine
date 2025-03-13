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
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs, const std::string& baseCcy);

//! Generate the curve spec name for a market object
/*!
  \ingroup utilities
*/
std::string marketObjectToCurveSpec(const MarketObject& mo, const std::string& name, const string& baseCcy,
                                    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs);

    //! Find an appropriate discount curve for a SwapIndex if non provided
/*!
  \ingroup utilities
*/
std::string swapIndexDiscountCurve(const std::string& ccy, const std::string& baseCcy = std::string(), 
    const std::string& swapIndexConvId = std::string());

} // namespace data
} // namespace ore