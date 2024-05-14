/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/wrappedmarket.hpp
    \brief wrapped market
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/market.hpp>

namespace ore {
namespace data {

//! Wrapped Market
/*!
  All incoming requests are passed through to an underlying market. This class can be used to override single
  methods for special markets. For example a derived class can override the securitySpread() method and
  return a dedicated simple quote handle that can be used to imply a bond spread. Another example is a market
  returning commodity term structures as fx term structures for precious metals.

  \ingroup marketdata
*/
class WrappedMarket : public Market {
public:
    WrappedMarket(const QuantLib::ext::shared_ptr<Market>& market, const bool handlePseudoCurrencies);
    QuantLib::ext::shared_ptr<Market> underlyingMarket() const;

    // market interface
    Date asofDate() const override;
    Handle<YieldTermStructure> yieldCurve(const YieldCurveType& type, const string& name,
                                          const string& configuration = Market::defaultConfiguration) const override;
    Handle<YieldTermStructure> discountCurveImpl(const string& ccy,
                                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<YieldTermStructure> yieldCurve(const string& name,
                                          const string& configuration = Market::defaultConfiguration) const override;
    Handle<IborIndex> iborIndex(const string& indexName,
                                const string& configuration = Market::defaultConfiguration) const override;
    Handle<SwapIndex> swapIndex(const string& indexName,
                                const string& configuration = Market::defaultConfiguration) const override;
    Handle<SwaptionVolatilityStructure>
    swaptionVol(const string& key, const string& configuration = Market::defaultConfiguration) const override;
    string shortSwapIndexBase(const string& ccy,
                                    const string& configuration = Market::defaultConfiguration) const override;
    string swapIndexBase(const string& ccy,
                               const string& configuration = Market::defaultConfiguration) const override;
    Handle<SwaptionVolatilityStructure>
    yieldVol(const string& securityID, const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::FxIndex> fxIndexImpl(const string& fxIndex,
                                          const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> fxSpotImpl(const string& ccypair,
                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> fxRateImpl(const string& ccypair,
                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<BlackVolTermStructure> fxVolImpl(const string& ccypair,
                                            const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::CreditCurve>
    defaultCurve(const string& name, const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> recoveryRate(const string& name,
                               const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::CreditVolCurve> cdsVol(const string& name,
                                            const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::BaseCorrelationTermStructure>
    baseCorrelation(const string& name, const string& configuration = Market::defaultConfiguration) const override;
    Handle<OptionletVolatilityStructure>
    capFloorVol(const string& key, const string& configuration = Market::defaultConfiguration) const override;
    std::pair<string, QuantLib::Period>
    capFloorVolIndexBase(const string& key, const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::YoYOptionletVolatilitySurface>
    yoyCapFloorVol(const string& indexName, const string& configuration = Market::defaultConfiguration) const override;
    Handle<ZeroInflationIndex>
    zeroInflationIndex(const string& indexName,
                       const string& configuration = Market::defaultConfiguration) const override;
    Handle<YoYInflationIndex>
    yoyInflationIndex(const string& indexName,
                      const string& configuration = Market::defaultConfiguration) const override;
    Handle<CPIVolatilitySurface>
    cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                          const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> equitySpot(const string& eqName,
                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<YieldTermStructure>
    equityDividendCurve(const string& eqName,
                        const string& configuration = Market::defaultConfiguration) const override;
    Handle<YieldTermStructure>
    equityForecastCurve(const string& eqName,
                        const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::EquityIndex2>
    equityCurve(const string& eqName, const string& configuration = Market::defaultConfiguration) const override;
    Handle<BlackVolTermStructure> equityVol(const string& eqName,
                                            const string& configuration = Market::defaultConfiguration) const override;
    void refresh(const string& s) override;

    Handle<Quote> securitySpread(const string& securityID,
                                 const string& configuration = Market::defaultConfiguration) const override;
    QuantLib::Handle<QuantExt::PriceTermStructure>
    commodityPriceCurve(const std::string& commodityName,
                        const std::string& configuration = Market::defaultConfiguration) const override;
    QuantLib::Handle<QuantExt::CommodityIndex> commodityIndex(const std::string& commodityName,
            const std::string& configuration = Market::defaultConfiguration) const override;
    QuantLib::Handle<QuantLib::BlackVolTermStructure>
    commodityVolatility(const std::string& commodityName,
                        const std::string& configuration = Market::defaultConfiguration) const override;
    QuantLib::Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const std::string& index1, const std::string& index2,
                     const std::string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> cpr(const string& securityID,
                      const string& configuration = Market::defaultConfiguration) const override;

protected:
    QuantLib::ext::shared_ptr<Market> market_;
};

} // namespace data
} // namespace ore
