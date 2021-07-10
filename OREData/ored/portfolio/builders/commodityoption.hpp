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

/*! \file portfolio/builders/commodityoption.hpp
    \brief Engine builder for commodity options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/vanillaoption.hpp>

namespace ore {
namespace data {

/*! Engine builder for European commodity options
    \ingroup builders
 */
class CommodityEuropeanOptionEngineBuilder : public EuropeanOptionEngineBuilder {
public:
    CommodityEuropeanOptionEngineBuilder()
        : EuropeanOptionEngineBuilder("BlackScholes", {"CommodityOption"}, AssetClass::COM) {}
};

/*! Engine builder for European commodity options
  \ingroup builders
*/
class CommodityEuropeanForwardOptionEngineBuilder : public EuropeanForwardOptionEngineBuilder {
public:
    CommodityEuropeanForwardOptionEngineBuilder()
        : EuropeanForwardOptionEngineBuilder("BlackScholes", {"CommodityOptionForward"}, AssetClass::COM) {}
};

/*! Engine builder for European cash-settled commodity options
    \ingroup builders
 */
class CommodityEuropeanCSOptionEngineBuilder : public EuropeanCSOptionEngineBuilder {
public:
    CommodityEuropeanCSOptionEngineBuilder()
        : EuropeanCSOptionEngineBuilder("BlackScholes", {"CommodityOptionEuropeanCS"}, AssetClass::COM) {}
};

/*! Engine builder for American commodity options using finite difference.
    \ingroup builders
 */
class CommodityAmericanOptionFDEngineBuilder : public AmericanOptionFDEngineBuilder {
public:
    CommodityAmericanOptionFDEngineBuilder()
        : AmericanOptionFDEngineBuilder("BlackScholes", {"CommodityOptionAmerican"}, AssetClass::COM, expiryDate_) {}
};

/*! Engine builder for American commodity options using Barone-Adesi and Whaley approximation.
    \ingroup builders
 */
class CommodityAmericanOptionBAWEngineBuilder : public AmericanOptionBAWEngineBuilder {
public:
    CommodityAmericanOptionBAWEngineBuilder()
        : AmericanOptionBAWEngineBuilder("BlackScholes", {"CommodityOptionAmerican"}, AssetClass::COM) {}
};

} // namespace data
} // namespace ore
