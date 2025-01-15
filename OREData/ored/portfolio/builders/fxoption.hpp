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
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override;
};

class CamAmcFxEuropeanForwardOptionEngineBuilder : public CamAmcFxOptionEngineBuilderBase {
public:
    CamAmcFxEuropeanForwardOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                               const std::vector<Date>& simulationDates,
                                               const std::vector<Date>& stickyCloseOutDates)
        : CamAmcFxOptionEngineBuilderBase({"FxOptionForward"}, cam, simulationDates, stickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override;
};

class CamAmcFxEuropeanCSOptionEngineBuilder : public CamAmcFxOptionEngineBuilderBase {
public:
    CamAmcFxEuropeanCSOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                          const std::vector<Date>& simulationDates,
                                          const std::vector<Date>& stickyCloseOutDates)
        : CamAmcFxOptionEngineBuilderBase({"FxOptionEuropeanCS"}, cam, simulationDates, stickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override;
};

//! FX option engine builder for external cam, with additional simulation dates (AMC-CG)
class AmcCgFxOptionEngineBuilderBase : public VanillaOptionEngineBuilder {
public:
    AmcCgFxOptionEngineBuilderBase(const std::set<std::string>& tradeTypes,
                                   const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                   const std::set<Date>& valuationDates, const std::vector<Date>& closeOutDates,
                                   const bool useStickyCloseOutDates)
        : VanillaOptionEngineBuilder("CrossAssetModel", "AMCCG", tradeTypes, AssetClass::FX, Date()), modelCg_(modelCg),
          valuationDates_(valuationDates), closeOutDates_(closeOutDates),
          useStickyCloseOutDates_(useStickyCloseOutDates) {}

protected:
    template <typename E>
    QuantLib::ext::shared_ptr<PricingEngine> engineImplBase(const string& assetName, const Currency& domCcy,
                                                            const AssetClass& assetClassUnderlying,
                                                            const Date& expiryDate, const bool useFxSpot);

    QuantLib::ext::shared_ptr<ore::data::ModelCG> modelCg_;
    std::set<Date> valuationDates_;
    std::vector<Date> closeOutDates_;
    bool useStickyCloseOutDates_;
};

class AmcCgFxEuropeanOptionEngineBuilder : public AmcCgFxOptionEngineBuilderBase {
public:
    AmcCgFxEuropeanOptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                       const std::set<Date>& valuationDates, const std::vector<Date>& closeOutDates,
                                       const bool useStickyCloseOutDates)
        : AmcCgFxOptionEngineBuilderBase({"FxOption"}, modelCg, valuationDates, closeOutDates, useStickyCloseOutDates) {
    }

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override;
};

class AmcCgFxEuropeanForwardOptionEngineBuilder : public AmcCgFxOptionEngineBuilderBase {
public:
    AmcCgFxEuropeanForwardOptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                              const std::set<Date>& valuationDates,
                                              const std::vector<Date>& closeOutDates, const bool useStickyCloseOutDates)
        : AmcCgFxOptionEngineBuilderBase({"FxOptionForward"}, modelCg, valuationDates, closeOutDates,
                                         useStickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override;
};

class AmcCgFxEuropeanCSOptionEngineBuilder : public AmcCgFxOptionEngineBuilderBase {
public:
    AmcCgFxEuropeanCSOptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                         const std::set<Date>& valuationDates, const std::vector<Date>& closeOutDates,
                                         const bool useStickyCloseOutDates)
        : AmcCgFxOptionEngineBuilderBase({"FxOptionEuropeanCS"}, modelCg, valuationDates, closeOutDates,
                                         useStickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override;
};

} // namespace data
} // namespace ore
