/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/quantoequityoption.hpp
    \brief Engine builder for quanto equity options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/quantovanillaoption.hpp>

namespace ore {
namespace data {

//! Engine Builder for Quanto European Equity Option Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class QuantoEquityEuropeanOptionEngineBuilder : public QuantoEuropeanOptionEngineBuilder {
public:
    QuantoEquityEuropeanOptionEngineBuilder()
        : QuantoEuropeanOptionEngineBuilder("BlackScholes", {"QuantoEquityOption"}, AssetClass::EQ) {}
};

//! Engine Builder for Quanto American Equity Option Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class QuantoEquityAmericanOptionEngineBuilder : public QuantoAmericanOptionEngineBuilder {
public:
    QuantoEquityAmericanOptionEngineBuilder()
        : QuantoAmericanOptionEngineBuilder("BlackScholesMerton", {"QuantoEquityOptionAmerican"}, AssetClass::EQ) {}
};

} // namespace data
} // namespace ore
