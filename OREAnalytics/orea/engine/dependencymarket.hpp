/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file orea/engine/dependencymarket.hpp
    \brief Class for recording information about requested curves
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <qle/indexes/fxindex.hpp>

namespace ore {
namespace analytics {

/*! DependencyMarket acts as a dummy Market and always returns a handle to a requested curve.
    It stores the name of every curve it is asked for and can return a list of them when inspected.
    This way it can be used to analyse any module that requires a Market.
*/
class DependencyMarket : public ore::data::Market {
public:
    DependencyMarket(
        const std::string& baseCcy, bool useFxDominance = true,
        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
        const ore::data::IborFallbackConfig& iborFallbackConfig = ore::data::IborFallbackConfig::defaultConfig(),
        const bool recordSecuritySpecificCreditCurves = false)
        : ore::data::Market(true), baseCcy_(baseCcy), useFxDominance_(useFxDominance), curveConfigs_(curveConfigs),
          iborFallbackConfig_(iborFallbackConfig),
          recordSecuritySpecificCreditCurves_(recordSecuritySpecificCreditCurves) {}

    //! Get the asof Date
    virtual QuantLib::Date asofDate() const override { return QuantLib::Settings::instance().evaluationDate(); }

    //! \name Market Interface
    //@{
    virtual QuantLib::Handle<QuantLib::YieldTermStructure>
    yieldCurve(const ore::data::YieldCurveType&, const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::YieldTermStructure> discountCurveImpl(const std::string&,
                                                                             const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::YieldTermStructure> yieldCurve(const std::string&,
                                                                      const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::IborIndex> iborIndex(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::SwapIndex> swapIndex(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::SwaptionVolatilityStructure> swaptionVol(const std::string&,
                                                                                const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::SwaptionVolatilityStructure> yieldVol(const std::string&,
                                                                             const std::string&) const override;
    virtual std::string shortSwapIndexBase(const std::string& ccy, const std::string&) const override;
    virtual std::string swapIndexBase(const std::string& ccy, const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::FxIndex> fxIndexImpl(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> fxSpotImpl(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> fxRateImpl(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::BlackVolTermStructure> fxVolImpl(const std::string&,
                                                                        const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::CreditCurve> defaultCurve(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> recoveryRate(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> conversionFactor(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::CreditVolCurve> cdsVol(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::BaseCorrelationTermStructure> baseCorrelation(const std::string&,
                                                                                     const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::OptionletVolatilityStructure> capFloorVol(const std::string&,
                                                                                 const std::string&) const override;
    virtual std::pair<std::string, QuantLib::Period> capFloorVolIndexBase(const std::string&,
                                                                          const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::ZeroInflationIndex> zeroInflationIndex(const std::string&,
                                                                              const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::YoYInflationIndex> yoyInflationIndex(const std::string&,
                                                                            const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::CPIVolatilitySurface>
    cpiInflationCapFloorVolatilitySurface(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::YoYOptionletVolatilitySurface> yoyCapFloorVol(const std::string&,
                                                                                     const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> equitySpot(const std::string& eqName, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::YieldTermStructure> equityDividendCurve(const std::string&,
                                                                               const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::BlackVolTermStructure> equityVol(const std::string&,
                                                                        const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::YieldTermStructure> equityForecastCurve(const std::string&,
                                                                               const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::EquityIndex2> equityCurve(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> securitySpread(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::PriceTermStructure> commodityPriceCurve(const std::string&,
                                                                               const std::string&) const override;
    QuantLib::Handle<QuantExt::CommodityIndex> commodityIndex(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::BlackVolTermStructure> commodityVolatility(const std::string&,
                                                                                  const std::string&) const override;
    virtual QuantLib::Handle<QuantLib::Quote> cpr(const std::string&, const std::string&) const override;
    virtual QuantLib::Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const std::string&, const std::string&, const std::string&) const override;
    virtual void refresh(const std::string&) override {}
    //@}

    //! \name Inspectors
    //@{
    bool hasRiskFactorType(const ore::analytics::RiskFactorKey::KeyType& riskFactorType) const;
    std::set<std::string> riskFactorNames(const ore::analytics::RiskFactorKey::KeyType& riskFactorType) const;
    std::set<ore::analytics::RiskFactorKey::KeyType> riskFactorTypes() const;
    std::set<std::string> swapindices() const { return swapindices_; }
    std::map<ore::analytics::RiskFactorKey::KeyType, std::set<std::string>> riskFactors() const {
        return riskFactors_;
    }

    bool hasMarketObjectType(const ore::data::MarketObject& marketObjectType) const;
    std::set<std::string> marketObjectNames(const ore::data::MarketObject& marketObjectType) const;
    std::set<ore::data::MarketObject> marketObjectTypes() const;
    std::map<std::string, std::map<ore::data::MarketObject, std::set<std::string>>> marketObjects() const {
        return marketObjects_;
    }

    //@}

private:
    // base currency so that we can store fxSpots
    std::string baseCcy_;
    // bool to determine if ccy pairs are converted to market standard i.e USDEUR -> EURUSD, JPYUSD -> USDJPY etc
    bool useFxDominance_;
    // Configs to be used in constructin market curves - only used for equity at the moment as correct calender is
    // needed in Index to determine fixings
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    // The ibor fallback config
    ore::data::IborFallbackConfig iborFallbackConfig_;
    // Whether to record security specific credit curve names or normalize them to the original credit curve id
    bool recordSecuritySpecificCreditCurves_;
    //! \name helper functions
    //@{
    QuantLib::Handle<QuantLib::YieldTermStructure> flatRateYts() const;
    QuantLib::Handle<QuantExt::PriceTermStructure> flatRatePts(const QuantLib::Currency& currency) const;
    QuantLib::Handle<QuantLib::SwaptionVolatilityStructure> flatRateSvs() const;
    QuantLib::Handle<QuantExt::OptionletVolatilityStructure> flatRateCvs() const;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> flatRateFxv() const;
    QuantLib::Handle<QuantExt::CreditCurve> flatRateDcs(QuantLib::Volatility forward) const;
    QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface>
    flatRateCps(QuantLib::Handle<QuantLib::ZeroInflationIndex> infIndex) const;
    QuantLib::Handle<QuantLib::CPIVolatilitySurface>
    flatRateCPIvs(QuantLib::Handle<QuantLib::ZeroInflationIndex> infIndex) const;
    QuantLib::Handle<QuantLib::YoYOptionletVolatilitySurface>
    flatRateYoYvs(QuantLib::Handle<QuantLib::YoYInflationIndex> infIndex) const;
    //@}

    // Get the correct pair
    std::string ccyPair(const std::string& pair) const;

    void addRiskFactor(ore::analytics::RiskFactorKey::KeyType type, const std::string& name) const;
    void addMarketObject(ore::data::MarketObject type, const std::string& name, const std::string& config) const;

    // Mutable member as we add to it from const methods
    mutable std::map<ore::analytics::RiskFactorKey::KeyType, std::set<std::string>> riskFactors_;
    mutable std::map<std::string, std::map<ore::data::MarketObject, std::set<std::string>>> marketObjects_;

    // Swap indices recorded separately as they do not have a risk factor key type
    // Again mutable as we add to it from const methods
    mutable std::set<std::string> swapindices_;
};

} // namespace analytics
} // namespace ore
