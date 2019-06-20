/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/equityoption.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/vanillaoption.hpp>

namespace ore {
namespace data {

//! Engine Builder for European Equity Option Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EquityEuropeanVanillaOptionAnalyticEngineBuilder : public EuropeanVanillaOptionAnalyticEngineBuilder {
public:
    EquityEuropeanVanillaOptionAnalyticEngineBuilder()
        : EuropeanVanillaOptionAnalyticEngineBuilder("BlackScholesMerton", {"EquityOption"}, AssetClass::EQ) {}
};

//! Engine Builder for American Equity Options using Finite Difference Method
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EquityAmericanVanillaOptionFDEngineBuilder : public AmericanVanillaOptionFDEngineBuilder {
public:
    EquityAmericanVanillaOptionFDEngineBuilder()
        : AmericanVanillaOptionFDEngineBuilder("BlackScholesMerton", {"EquityOptionAmerican"}, AssetClass::EQ) {}
};

//! Engine Builder for American Equity Options using Barone Adesi Whaley Approximation
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EquityAmericanVanillaOptionBAWEngineBuilder : public AmericanVanillaOptionBAWEngineBuilder {
public:
    EquityAmericanVanillaOptionBAWEngineBuilder()
        : AmericanVanillaOptionBAWEngineBuilder("BlackScholesMerton", {"EquityOptionAmerican"}, AssetClass::EQ) {}
};

} // namespace data
} // namespace ore
