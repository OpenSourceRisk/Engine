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

%include <std_pair.i>
%include ored_conventions.i

%shared_ptr(ore::data::MarketImpl)
%shared_ptr(ore::data::TodaysMarket)
%template(CPICapFloorTermPriceSurfaceHandle) Handle<QuantLib::CPICapFloorTermPriceSurface>;
%template(YoYCapFloorTermPriceSurfaceHandle) Handle<QuantLib::YoYCapFloorTermPriceSurface>;
%template(StringPeriodPair) std::pair<std::string, Period>;

namespace ore {
namespace data {

enum class YieldCurveType {
  Discount = 0,
  Yield = 1,
  EquityDividend = 2
};

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
class MarketImpl {
  public:
    MarketImpl(const bool handlePseudoCurrencies);
    QuantLib::Date asofDate() const;

    // Yield curves
    QuantLib::Handle<QuantLib::YieldTermStructure> yieldCurve(const ore::data::YieldCurveType& type, const std::string& name,
                                          const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> yieldCurve(const std::string& name,
                                          const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve(const std::string& ccy,
                              const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    %extend {
      ext::shared_ptr<IborIndex> iborIndex(const std::string& indexName,
                                           const std::string& configuration = ore::data::Market::defaultConfiguration) const {
          return self->iborIndex(indexName, configuration).currentLink();
      }
      ext::shared_ptr<SwapIndex> swapIndex(const std::string& indexName,
                                           const std::string& configuration = ore::data::Market::defaultConfiguration) const {
          return self->swapIndex(indexName, configuration).currentLink();
      }
    }

    // Swaptions
    QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>
      swaptionVol(const std::string& ccy,
                  const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    const std::string shortSwapIndexBase(const std::string& ccy,
                                         const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    const std::string swapIndexBase(const std::string& ccy,
                                    const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    // Yield volatilities
    QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>
      yieldVol(const std::string& securityID, const std::string& configuration = Market::defaultConfiguration) const;

    // FX
    QuantLib::Handle<QuantLib::Quote> fxRate(const std::string& ccypair,
           const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::Quote> fxSpot(const std::string& ccypair,
               const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> fxVol(const std::string& ccypair,
                      const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    // Default Curves and Recovery Rates
    QuantLib::Handle<QuantExt::CreditCurve>
      defaultCurve(const std::string& name,
                   const std::string& configuration = Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::Quote> recoveryRate(const std::string& name,
                               const std::string& configuration = Market::defaultConfiguration) const;

    // CDS Option volatilities
    QuantLib::Handle<QuantExt::CreditVolCurve> cdsVol(const std::string& name,
                                         const std::string& configuration = Market::defaultConfiguration) const;

    // Base correlation structures
    QuantLib::Handle<QuantExt::BaseCorrelationTermStructure>
      baseCorrelation(const std::string& name,
                      const std::string& configuration = Market::defaultConfiguration) const;

    // CapFloor volatilities
    QuantLib::Handle<QuantLib::OptionletVolatilityStructure>
      capFloorVol(const std::string& ccy,
                  const std::string& configuration = Market::defaultConfiguration) const;
    std::pair<std::string, Period>
      capFloorVolIndexBase(const std::string& ccy,
                           const std::string& configuration = Market::defaultConfiguration) const;

    // YoY Inflation CapFloor volatilities
    QuantLib::Handle<QuantExt::YoYOptionletVolatilitySurface>
      yoyCapFloorVol(const std::string& ccy,
                     const std::string& configuration = Market::defaultConfiguration) const;

    // Inflation Indexes (Pointer rather than Handle)
    %extend {
      ext::shared_ptr<ZeroInflationIndex> zeroInflationIndex(const std::string& indexName,
                                                             const std::string& configuration =
                                                               ore::data::Market::defaultConfiguration) const {
          return self->zeroInflationIndex(indexName, configuration).currentLink();
      }
      ext::shared_ptr<YoYInflationIndex> yoyInflationIndex(const std::string& indexName,
                                                           const std::string& configuration =
                                                             ore::data::Market::defaultConfiguration) const {
          return self->yoyInflationIndex(indexName, configuration).currentLink();
      }
    }

    // CPI Inflation Cap Floor Volatility Surfaces
    QuantLib::Handle<QuantLib::CPIVolatilitySurface>
      cpiInflationCapFloorVolatilitySurface(const std::string& indexName,
					    const std::string& configuration = Market::defaultConfiguration) const;

    // Equity curves
    QuantLib::Handle<QuantLib::Quote> equitySpot(const std::string& eqName,
                             const std::string& configuration = Market::defaultConfiguration) const;
    %extend {
      QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve(const std::string& indexName,
                                                           const std::string& configuration =
							   ore::data::Market::defaultConfiguration) const {
          return self->equityCurve(indexName, configuration).currentLink();
      }
    }

    // Equity dividend curves
    QuantLib::Handle<QuantLib::YieldTermStructure>
      equityDividendCurve(const std::string& eqName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Equity forecasting curves
    QuantLib::Handle<QuantLib::YieldTermStructure>
      equityForecastCurve(const std::string& eqName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Equity volatilities
    QuantLib::Handle<QuantLib::BlackVolTermStructure>
      equityVol(const std::string& eqName,
                const std::string& configuration = Market::defaultConfiguration) const;

    // Bond Spreads
    QuantLib::Handle<QuantLib::Quote>
      securitySpread(const std::string& securityID,
                     const std::string& configuration = Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::Quote>
      conversionFactor(const std::string& securityID,
                       const std::string& configuration = Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::Quote>
      securityPrice(const std::string& securityID,
                    const std::string& configuration = Market::defaultConfiguration) const;

    // Commodity price curve
    QuantLib::Handle<QuantExt::PriceTermStructure>
      commodityPriceCurve(const std::string& commodityName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    %extend {
      QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> commodityIndex(
          const std::string& commodityName,
          const std::string& configuration = ore::data::Market::defaultConfiguration) const {
          return self->commodityIndex(commodityName, configuration).currentLink();
      }
    }

    // Commodity volatility
    QuantLib::Handle<QuantLib::BlackVolTermStructure>
      commodityVolatility(const std::string& commodityName,
                          const std::string& configuration = Market::defaultConfiguration) const;

    // Index-Index Correlations
    QuantLib::Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const std::string& index1, const std::string& index2,
                     const std::string& configuration = Market::defaultConfiguration) const;

    // Conditional Prepayment Rates
    QuantLib::Handle<QuantLib::Quote> cpr(const std::string& securityID,
              const std::string& configuration = ore::data::Market::defaultConfiguration) const;
};

class TodaysMarket : public MarketImpl {
 public:
  TodaysMarket(const QuantLib::Date& asof,
         const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& params,
         const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
         const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                 const bool continueOnError = false,
                 bool loadFixings = true,
                 const bool lazyBuild = false,
                 const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                 const bool preserveQuoteLinkage = false,
         const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
           QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(ore::data::IborFallbackConfig::defaultConfig()));
};

} // namespace data
} // namespace ore

#endif
