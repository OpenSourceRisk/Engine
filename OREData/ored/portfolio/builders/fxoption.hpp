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

/*! \file portfolio/builders/fxoption.hpp
    \brief Engine builder for FX Options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/vanillaoption.hpp>

namespace ore {
namespace data {

//! Engine Builder for European Fx Option Options
/*! Pricing engines are cached by currency pair/currency

    \ingroup builders
 */
class FxEuropeanOptionEngineBuilder : public EuropeanOptionEngineBuilder {
public:
    FxEuropeanOptionEngineBuilder() : EuropeanOptionEngineBuilder("GarmanKohlhagen", {"FxOption"}, AssetClass::FX) {}
};

/*! Engine builder for European cash-settled FX options.
    \ingroup builders
 */
class FxEuropeanCSOptionEngineBuilder : public EuropeanCSOptionEngineBuilder {
public:
    FxEuropeanCSOptionEngineBuilder()
        : EuropeanCSOptionEngineBuilder("GarmanKohlhagen", {"FxOptionEuropeanCS"}, AssetClass::FX) {}
};

//! Engine Builder for American Fx Options using Finite Difference Method
/*! Pricing engines are cached by currency pair

    \ingroup builders
 */
class FxAmericanOptionFDEngineBuilder : public AmericanOptionFDEngineBuilder {
public:
    FxAmericanOptionFDEngineBuilder()
        : AmericanOptionFDEngineBuilder("GarmanKohlhagen", {"FxOptionAmerican"}, AssetClass::FX, expiryDate_) {}
};

//! Engine Builder for American Fx Options using Barone Adesi Whaley Approximation
/*! Pricing engines are cached by currency pair

    \ingroup builders
 */
class FxAmericanOptionBAWEngineBuilder : public AmericanOptionBAWEngineBuilder {
public:
    FxAmericanOptionBAWEngineBuilder()
        : AmericanOptionBAWEngineBuilder("GarmanKohlhagen", {"FxOptionAmerican"}, AssetClass::FX) {}
};

} // namespace data
} // namespace ore
