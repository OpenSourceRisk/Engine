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

#include <ored/marketdata/wrappedmarket.hpp>

namespace ore {
namespace data {

WrappedMarket::WrappedMarket(const QuantLib::ext::shared_ptr<Market>& m, const bool handlePseudoCurrencies)
    : Market(handlePseudoCurrencies), market_(m) {}

QuantLib::ext::shared_ptr<Market> WrappedMarket::underlyingMarket() const { return market_; }

Date WrappedMarket::asofDate() const { return market_->asofDate(); }

Handle<YieldTermStructure> WrappedMarket::yieldCurve(const YieldCurveType& type, const string& name,
                                                     const string& configuration) const {
    return market_->yieldCurve(type, name, configuration);
}

Handle<YieldTermStructure> WrappedMarket::discountCurveImpl(const string& ccy, const string& configuration) const {
    return market_->discountCurve(ccy, configuration);
}

Handle<YieldTermStructure> WrappedMarket::yieldCurve(const string& name, const string& configuration) const {
    return market_->yieldCurve(name, configuration);
}

Handle<IborIndex> WrappedMarket::iborIndex(const string& indexName, const string& configuration) const {
    return market_->iborIndex(indexName, configuration);
}

Handle<SwapIndex> WrappedMarket::swapIndex(const string& indexName, const string& configuration) const {
    return market_->swapIndex(indexName, configuration);
}

Handle<SwaptionVolatilityStructure> WrappedMarket::swaptionVol(const string& key, const string& configuration) const {
    return market_->swaptionVol(key, configuration);
}

string WrappedMarket::shortSwapIndexBase(const string& ccy, const string& configuration) const {
    return market_->shortSwapIndexBase(ccy, configuration);
}

string WrappedMarket::swapIndexBase(const string& ccy, const string& configuration) const {
    return market_->swapIndexBase(ccy, configuration);
}

Handle<SwaptionVolatilityStructure> WrappedMarket::yieldVol(const string& securityID,
                                                            const string& configuration) const {
    return market_->yieldVol(securityID, configuration);
}

Handle<QuantExt::FxIndex> WrappedMarket::fxIndexImpl(const string& fxIndex, const string& configuration) const {
    return market_->fxIndex(fxIndex, configuration);
}

Handle<Quote> WrappedMarket::fxSpotImpl(const string& ccypair, const string& configuration) const {
    return market_->fxSpot(ccypair, configuration);
}

Handle<Quote> WrappedMarket::fxRateImpl(const string& ccypair, const string& configuration) const {
    return market_->fxRate(ccypair, configuration);
}

Handle<BlackVolTermStructure> WrappedMarket::fxVolImpl(const string& ccypair, const string& configuration) const {
    return market_->fxVol(ccypair, configuration);
}

Handle<QuantExt::CreditCurve> WrappedMarket::defaultCurve(const string& name, const string& configuration) const {
    return market_->defaultCurve(name, configuration);
}

Handle<Quote> WrappedMarket::recoveryRate(const string& name, const string& configuration) const {
    return market_->recoveryRate(name, configuration);
}

Handle<QuantExt::CreditVolCurve> WrappedMarket::cdsVol(const string& name, const string& configuration) const {
    return market_->cdsVol(name, configuration);
}

Handle<QuantExt::BaseCorrelationTermStructure> WrappedMarket::baseCorrelation(const string& name,
                                                                            const string& configuration) const {
    return market_->baseCorrelation(name, configuration);
}

Handle<OptionletVolatilityStructure> WrappedMarket::capFloorVol(const string& key, const string& configuration) const {
    return market_->capFloorVol(key, configuration);
}

std::pair<string, QuantLib::Period> WrappedMarket::capFloorVolIndexBase(const string& key,
                                                                        const string& configuration) const {
    return market_->capFloorVolIndexBase(key, configuration);
}

Handle<QuantExt::YoYOptionletVolatilitySurface> WrappedMarket::yoyCapFloorVol(const string& indexName,
                                                                              const string& configuration) const {
    return market_->yoyCapFloorVol(indexName, configuration);
}

Handle<ZeroInflationIndex> WrappedMarket::zeroInflationIndex(const string& indexName,
                                                             const string& configuration) const {
    return market_->zeroInflationIndex(indexName, configuration);
}

Handle<YoYInflationIndex> WrappedMarket::yoyInflationIndex(const string& indexName, const string& configuration) const {
    return market_->yoyInflationIndex(indexName, configuration);
}

Handle<CPIVolatilitySurface> WrappedMarket::cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                                                                  const string& configuration) const {
    return market_->cpiInflationCapFloorVolatilitySurface(indexName, configuration);
}

Handle<Quote> WrappedMarket::equitySpot(const string& eqName, const string& configuration) const {
    return market_->equitySpot(eqName, configuration);
}

Handle<YieldTermStructure> WrappedMarket::equityDividendCurve(const string& eqName, const string& configuration) const {
    return market_->equityDividendCurve(eqName, configuration);
}

Handle<YieldTermStructure> WrappedMarket::equityForecastCurve(const string& eqName, const string& configuration) const {
    return market_->equityForecastCurve(eqName, configuration);
}

Handle<QuantExt::EquityIndex2> WrappedMarket::equityCurve(const string& eqName, const string& configuration) const {
    return market_->equityCurve(eqName, configuration);
}

Handle<BlackVolTermStructure> WrappedMarket::equityVol(const string& eqName, const string& configuration) const {
    return market_->equityVol(eqName, configuration);
}

void WrappedMarket::refresh(const string& s) { market_->refresh(s); }

Handle<Quote> WrappedMarket::securitySpread(const string& securityID, const string& configuration) const {
    return market_->securitySpread(securityID, configuration);
}

QuantLib::Handle<QuantExt::PriceTermStructure>
WrappedMarket::commodityPriceCurve(const std::string& commodityName, const std::string& configuration) const {
    return market_->commodityPriceCurve(commodityName, configuration);
}

QuantLib::Handle<QuantExt::CommodityIndex> WrappedMarket::commodityIndex(const std::string& commodityName,
                                                                         const std::string& configuration) const {
    return market_->commodityIndex(commodityName, configuration);
}

QuantLib::Handle<QuantLib::BlackVolTermStructure>
WrappedMarket::commodityVolatility(const std::string& commodityName, const std::string& configuration) const {
    return market_->commodityVolatility(commodityName, configuration);
}

QuantLib::Handle<QuantExt::CorrelationTermStructure>
WrappedMarket::correlationCurve(const std::string& index1, const std::string& index2,
                                const std::string& configuration) const {
    return market_->correlationCurve(index1, index2, configuration);
}

Handle<Quote> WrappedMarket::cpr(const string& securityID, const string& configuration) const {
    return market_->cpr(securityID, configuration);
}

} // namespace data
} // namespace ore
