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

#include <ored/portfolio/builders/vanillaoption.hpp>
#include <qle/models/crossassetmodel.hpp>

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

/*! Engine builder for European cash-settled FX options.
    \ingroup builders
 */
class FxEuropeanForwardOptionEngineBuilder : public EuropeanForwardOptionEngineBuilder {
public:
    FxEuropeanForwardOptionEngineBuilder()
        : EuropeanForwardOptionEngineBuilder("GarmanKohlhagen", {"FxOptionForward"}, AssetClass::FX) {}
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
class CamAmcFxOptionEngineBuilderBase : public VanillaOptionEngineBuilder {
public:
    // for external cam, with additional simulation dates (AMC)
    CamAmcFxOptionEngineBuilderBase(const std::set<std::string>& tradeTypes,
                                    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                    const std::vector<Date>& simulationDates,
                                    const std::vector<Date>& stickyCloseOutDates)
        : VanillaOptionEngineBuilder("CrossAssetModel", "AMC", tradeTypes, AssetClass::FX, Date()), cam_(cam),
          simulationDates_(simulationDates), stickyCloseOutDates_(stickyCloseOutDates) {}

protected:
    template <typename E>
    QuantLib::ext::shared_ptr<PricingEngine> engineImplBase(const string& assetName, const Currency& domCcy,
                                                            const AssetClass& assetClassUnderlying,
                                                            const Date& expiryDate, const bool useFxSpot);

    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    std::vector<Date> simulationDates_;
    std::vector<Date> stickyCloseOutDates_;
};

class CamAmcFxEuropeanOptionEngineBuilder : public CamAmcFxOptionEngineBuilderBase {
public:
    CamAmcFxEuropeanOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                        const std::vector<Date>& simulationDates,
                                        const std::vector<Date>& stickyCloseOutDates)
        : CamAmcFxOptionEngineBuilderBase({"FxOption"}, cam, simulationDates, stickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const string& discountCurveName,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot, const std::optional<Currency>&) override;
};

class CamAmcFxEuropeanForwardOptionEngineBuilder : public CamAmcFxOptionEngineBuilderBase {
public:
    CamAmcFxEuropeanForwardOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                               const std::vector<Date>& simulationDates,
                                               const std::vector<Date>& stickyCloseOutDates)
        : CamAmcFxOptionEngineBuilderBase({"FxOptionForward"}, cam, simulationDates, stickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const string& discountCurveName,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot, const std::optional<Currency>&) override;
};

class CamAmcFxEuropeanCSOptionEngineBuilder : public CamAmcFxOptionEngineBuilderBase {
public:
    CamAmcFxEuropeanCSOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                          const std::vector<Date>& simulationDates,
                                          const std::vector<Date>& stickyCloseOutDates)
        : CamAmcFxOptionEngineBuilderBase({"FxOptionEuropeanCS"}, cam, simulationDates, stickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const string& discountCurveName,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot, const std::optional<Currency>&) override;
};

//! FX option engine builder for external cam, with additional simulation dates (AMC-CG)
class AmcCgFxOptionEngineBuilderBase : public VanillaOptionEngineBuilder {
public:
    AmcCgFxOptionEngineBuilderBase(const std::set<std::string>& tradeTypes,
                                   const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                   const std::vector<Date>& simulationDates)
        : VanillaOptionEngineBuilder("CrossAssetModel", "AMCCG", tradeTypes, AssetClass::FX, Date()), modelCg_(modelCg),
          simulationDates_(simulationDates) {}

protected:
    template <typename E>
    QuantLib::ext::shared_ptr<PricingEngine> engineImplBase(const string& assetName, const Currency& domCcy,
                                                            const string& discountCurveName,
                                                            const AssetClass& assetClassUnderlying,
                                                            const Date& expiryDate, const bool useFxSpot, const std::optional<Currency>&);

    const QuantLib::ext::shared_ptr<ore::data::ModelCG> modelCg_;
    const std::vector<Date> simulationDates_;
};

class AmcCgFxEuropeanOptionEngineBuilder : public AmcCgFxOptionEngineBuilderBase {
public:
    AmcCgFxEuropeanOptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                       const std::vector<Date>& simulationDates)
        : AmcCgFxOptionEngineBuilderBase({"FxOption"}, modelCg, simulationDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const string& discountCurveName,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot, const std::optional<Currency>&) override;
};

class AmcCgFxEuropeanForwardOptionEngineBuilder : public AmcCgFxOptionEngineBuilderBase {
public:
    AmcCgFxEuropeanForwardOptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                              const std::vector<Date>& simulationDates)
        : AmcCgFxOptionEngineBuilderBase({"FxOptionForward"}, modelCg, simulationDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const string& discountCurveName,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot, const std::optional<Currency>&) override;
};

class AmcCgFxEuropeanCSOptionEngineBuilder : public AmcCgFxOptionEngineBuilderBase {
public:
    AmcCgFxEuropeanCSOptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                         const std::vector<Date>& simulationDates)
        : AmcCgFxOptionEngineBuilderBase({"FxOptionEuropeanCS"}, modelCg, simulationDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const string& discountCurveName,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot, const std::optional<Currency>&) override;
};

} // namespace data
} // namespace ore
