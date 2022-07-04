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
    \brief Engine builder for equity options
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
class EquityEuropeanOptionEngineBuilder : public EuropeanOptionEngineBuilder {
public:
    EquityEuropeanOptionEngineBuilder()
        : EuropeanOptionEngineBuilder("BlackScholesMerton", {"EquityOption"}, AssetClass::EQ) {}
};

/*! Engine builder for European cash-settled equity options.
    \ingroup builders
 */
class EquityEuropeanCSOptionEngineBuilder : public EuropeanCSOptionEngineBuilder {
public:
    EquityEuropeanCSOptionEngineBuilder()
        : EuropeanCSOptionEngineBuilder("BlackScholesMerton", {"EquityOptionEuropeanCS"}, AssetClass::EQ) {}
};

//! Engine Builder for American Equity Options using Finite Difference Method
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EquityAmericanOptionFDEngineBuilder : public AmericanOptionFDEngineBuilder {
public:
    EquityAmericanOptionFDEngineBuilder()
        : AmericanOptionFDEngineBuilder("BlackScholesMerton", {"EquityOptionAmerican"}, AssetClass::EQ, expiryDate_) {}
};

//! Engine Builder for American Equity Options using Barone Adesi Whaley Approximation
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EquityAmericanOptionBAWEngineBuilder : public AmericanOptionBAWEngineBuilder {
public:
    EquityAmericanOptionBAWEngineBuilder()
        : AmericanOptionBAWEngineBuilder("BlackScholesMerton", {"EquityOptionAmerican"}, AssetClass::EQ) {}
};

} // namespace data
} // namespace ore
