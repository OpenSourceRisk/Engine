/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
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

#include <qle/models/crossassetmodel.hpp>
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

//! FX option engine builder for external cam, with additional simulation dates (AMC)
class CamAmcFxOptionEngineBuilder : public VanillaOptionEngineBuilder {
public:
    // for external cam, with additional simulation dates (AMC)
    CamAmcFxOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                const std::vector<Date>& simulationDates)
        : VanillaOptionEngineBuilder("CrossAssetModel", "AMC", {"FxOption"}, AssetClass::FX, Date()), cam_(cam),
          simulationDates_(simulationDates) {}

protected:
    // the pricing engine depends on the ccys only, so the base class key implementation will just do fine
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                const AssetClass& assetClassUnderlying, const Date& expiryDate, const bool useFxSpot) override;

private:
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};
    
} // namespace data
} // namespace ore
