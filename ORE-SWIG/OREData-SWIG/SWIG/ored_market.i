/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_market_i
#define ored_market_i

%include ored_conventions.i

%{
using ore::data::Market;
using ore::data::MarketImpl;
using ore::data::TodaysMarket;
using ore::data::MarketObject;

using ore::data::YieldCurveType;
using ore::data::TodaysMarketParameters;
using ore::data::Loader;
using ore::data::CurveConfigurations;
using ore::IborFallbackConfig;
%}

enum class MarketObject {
    DiscountCurve = 0,
    YieldCurve = 1,
    IndexCurve = 2,
    SwapIndexCurve = 3,
    FXSpot = 4,
    FXVol = 5,
    SwaptionVol = 6,
    DefaultCurve = 7,
    CDSVol = 8,
    BaseCorrelation = 9,
    CapFloorVol = 10,
    ZeroInflationCurve = 11,
    YoYInflationCurve = 12,
    ZeroInflationCapFloorVol = 13,
    YoYInflationCapFloorVol = 14,
    EquityCurve = 15,
    EquityVol = 16,
    Security = 17,
    CommodityCurve = 18,
    CommodityVolatility = 19,
    Correlation = 20,
    YieldVol = 21
};

// Market class passed around as pointer, no construction
%shared_ptr(MarketImpl)
class MarketImpl {
  public:
    MarketImpl(const bool handlePseudoCurrencies);
    Date asofDate() const;

    // Yield curves
    Handle<YieldTermStructure> yieldCurve(const YieldCurveType& type, const std::string& name,
                                          const std::string& configuration = Market::defaultConfiguration) const;
    Handle<YieldTermStructure> yieldCurve(const std::string& name,
                                          const std::string& configuration = Market::defaultConfiguration) const;
    Handle<YieldTermStructure> discountCurve(const std::string& ccy,
                              const std::string& configuration = Market::defaultConfiguration) const;
    
    %extend {
      ext::shared_ptr<IborIndex> iborIndex(const std::string& indexName,
                                             const std::string& configuration = Market::defaultConfiguration) const {
          return self->iborIndex(indexName, configuration).currentLink();
      }
      ext::shared_ptr<SwapIndex> swapIndex(const std::string& indexName,
                                             const std::string& configuration = Market::defaultConfiguration) const {
          return self->swapIndex(indexName, configuration).currentLink();
      }
    }

    // Swaptions
    Handle<QuantLib::SwaptionVolatilityStructure>
      swaptionVol(const std::string& ccy,
                  const std::string& configuration = Market::defaultConfiguration) const;
    const std::string shortSwapIndexBase(const std::string& ccy,
                                         const std::string& configuration = Market::defaultConfiguration) const;
    const std::string swapIndexBase(const std::string& ccy,
                                    const std::string& configuration = Market::defaultConfiguration) const;

    // Yield volatilities
    Handle<SwaptionVolatilityStructure>
      yieldVol(const std::string& securityID, const std::string& configuration = Market::defaultConfiguration) const;

    // FX
    Handle<Quote> fxSpot(const std::string& ccypair,
                         const std::string& configuration = Market::defaultConfiguration) const;
    Handle<BlackVolTermStructure> fxVol(const std::string& ccypair,
                                        const std::string& configuration = Market::defaultConfiguration) const;

    // Default Curves and Recovery Rates
    Handle<QuantExt::CreditCurve>
      defaultCurve(const std::string& name,
                   const std::string& configuration = Market::defaultConfiguration) const;
    Handle<Quote> recoveryRate(const std::string& name,
                               const std::string& configuration = Market::defaultConfiguration) const;

    // CDS Option volatilities
    Handle<QuantExt::CreditVolCurve> cdsVol(const std::string& name,
                                         const std::string& configuration = Market::defaultConfiguration) const;

    // Base correlation structures
    Handle<QuantExt::BaseCorrelationTermStructure>
      baseCorrelation(const std::string& name,
                      const std::string& configuration = Market::defaultConfiguration) const;

    // CapFloor volatilities
    Handle<OptionletVolatilityStructure>
      capFloorVol(const std::string& ccy,
                  const std::string& configuration = Market::defaultConfiguration) const;

    // YoY Inflation CapFloor volatilities
    Handle<QuantExt::YoYOptionletVolatilitySurface>
      yoyCapFloorVol(const std::string& ccy,
                     const std::string& configuration = Market::defaultConfiguration) const;

    // Inflation Indexes (Pointer rather than Handle)
    %extend {
      ext::shared_ptr<ZeroInflationIndex> zeroInflationIndex(const std::string& indexName,
                                                               const std::string& configuration =
                                                                 Market::defaultConfiguration) const {
          return self->zeroInflationIndex(indexName, configuration).currentLink();
      }
      ext::shared_ptr<YoYInflationIndex> yoyInflationIndex(const std::string& indexName,
                                                             const std::string& configuration =
                                                               Market::defaultConfiguration) const {
          return self->yoyInflationIndex(indexName, configuration).currentLink();
      }
    }

    // CPI Inflation Cap Floor Volatility Surfaces
    Handle<QuantLib::CPIVolatilitySurface>
      cpiInflationCapFloorVolatilitySurface(const std::string& indexName,
					    const std::string& configuration = Market::defaultConfiguration) const;

    // Equity curves
    Handle<Quote> equitySpot(const std::string& eqName,
                             const std::string& configuration = Market::defaultConfiguration) const;
    %extend {
      ext::shared_ptr<QuantExt::EquityIndex2> equityCurve(const std::string& indexName,
                                                           const std::string& configuration =
							   Market::defaultConfiguration) const {
          return self->equityCurve(indexName, configuration).currentLink();
      }
    }

    // Equity dividend curves
    Handle<YieldTermStructure>
      equityDividendCurve(const std::string& eqName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Equity forecasting curves
    Handle<YieldTermStructure>
      equityForecastCurve(const std::string& eqName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Equity volatilities
    Handle<BlackVolTermStructure>
      equityVol(const std::string& eqName,
                const std::string& configuration = Market::defaultConfiguration) const;

    // Bond Spreads
    Handle<Quote>
      securitySpread(const std::string& securityID,
                     const std::string& configuration = Market::defaultConfiguration) const;

    // Commodity price curve
    Handle<PriceTermStructure>
      commodityPriceCurve(const std::string& commodityName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Commodity volatility
    Handle<BlackVolTermStructure>
      commodityVolatility(const std::string& commodityName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Index-Index Correlations
    Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const std::string& index1, const std::string& index2,
                     const std::string& configuration = Market::defaultConfiguration) const;

    // Conditional Prepayment Rates
    Handle<Quote> cpr(const std::string& securityID,
		      const std::string& configuration = Market::defaultConfiguration) const;
};


%template(CPICapFloorTermPriceSurfaceHandle) Handle<QuantLib::CPICapFloorTermPriceSurface>;
%template(YoYCapFloorTermPriceSurfaceHandle) Handle<QuantLib::YoYCapFloorTermPriceSurface>;

%shared_ptr(TodaysMarket)
class TodaysMarket : public MarketImpl {
 public:
    TodaysMarket(const Date& asof,
                 const ext::shared_ptr<TodaysMarketParameters>& params,
                 const ext::shared_ptr<Loader>& loader,
                 const ext::shared_ptr<CurveConfigurations>& curveConfigs,
                 const bool continueOnError = false,
                 bool loadFixings = true,
                 const bool lazyBuild = false,
                 const ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                 const bool preserveQuoteLinkage = false,
                 const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig());
};

#endif
