/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/equityfuturesoption.hpp
    \brief Engine builder for equity futures options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/vanillaoption.hpp>

namespace ore {
namespace data {

/*! Engine builder for European commodity options
    \ingroup builders
 */
class EquityFutureEuropeanOptionEngineBuilder : public EuropeanOptionEngineBuilder {
public:
    EquityFutureEuropeanOptionEngineBuilder()
        : EuropeanOptionEngineBuilder("BlackScholes", {"EquityFutureOption"}, AssetClass::EQ) {}
};

/*! Engine builder for European cash-settled commodity options
    \ingroup builders
 */
class EquityFutureEuropeanCSOptionEngineBuilder : public EuropeanCSOptionEngineBuilder {
public:
    EquityFutureEuropeanCSOptionEngineBuilder()
        : EuropeanCSOptionEngineBuilder("BlackScholes", {"EquityFutureOptionEuropeanCS"}, AssetClass::EQ) {}
};

/*! Engine builder for American commodity options using finite difference.
    \ingroup builders
 */
class EquityFutureAmericanOptionFDEngineBuilder : public AmericanOptionFDEngineBuilder {
public:
    EquityFutureAmericanOptionFDEngineBuilder()
        : AmericanOptionFDEngineBuilder("BlackScholes", {"EquityFutureOptionAmerican"}, AssetClass::EQ, expiryDate_) {}
};

/*! Engine builder for American commodity options using Barone-Adesi and Whaley approximation.
    \ingroup builders
 */
class EquityFutureAmericanOptionBAWEngineBuilder : public AmericanOptionBAWEngineBuilder {
public:
    EquityFutureAmericanOptionBAWEngineBuilder()
        : AmericanOptionBAWEngineBuilder("BlackScholes", {"EquityFutureOptionAmerican"}, AssetClass::EQ) {}
};

} // namespace data
} // namespace ore
