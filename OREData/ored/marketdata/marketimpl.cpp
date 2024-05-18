/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>

using namespace std;
using std::make_pair;
using std::map;
using std::string;

using namespace QuantLib;

using QuantExt::PriceTermStructure;
using QuantExt::CommodityIndex;
using QuantExt::FxIndex;

namespace ore {
namespace data {

namespace {

template <class A, class B, class C>
A lookup(const B& map, const C& key, const string& configuration, const string& type, bool continueOnError = false) {
    auto it = map.find(make_pair(configuration, key));
    if (it == map.end()) {
        // fall back to default configuration
        it = map.find(make_pair(Market::defaultConfiguration, key));
        if (it == map.end()) {
            if (!continueOnError)
                QL_FAIL("did not find object '" << key << "' of type " << type
                    << " under configuration '" << configuration << "' or 'default'");
            else
                return A();
        }
    }
    return it->second;
}

template <class A, class B, class C>
A lookup(const B& map, const C& key, const YieldCurveType y, const string& configuration, const string& type) {
    auto it = map.find(make_tuple(configuration, y, key));
    if (it == map.end()) {
        // fall back to default configuration
        it = map.find(make_tuple(Market::defaultConfiguration, y, key));
        QL_REQUIRE(it != map.end(), "did not find object " << key << " of type " << type << " under configuration '"
                                                           << configuration << "' or 'default' in YieldCurves");
    }
    return it->second;
}
} // anonymous namespace

Handle<YieldTermStructure> MarketImpl::yieldCurve(const YieldCurveType& type, const string& key,
                                                  const string& configuration) const {
    // we allow for standard (i.e. not convention based) ibor index names as keys and return the index forward curve in
    // case of a match
    QuantLib::ext::shared_ptr<IborIndex> notUsed;
    if (tryParseIborIndex(key, notUsed)) {
        return iborIndex(key, configuration)->forwardingTermStructure();
    }
    // no ibor index found under key => look for a genuine yield curve
    DLOG("no ibor index found under '" << key << "' - look for a genuine yield curve");
    if (type == YieldCurveType::Discount)
        require(MarketObject::DiscountCurve, key, configuration);
    else if (type == YieldCurveType::Yield)
        require(MarketObject::YieldCurve, key, configuration);
    else if (type == YieldCurveType::EquityDividend)
        require(MarketObject::EquityCurve, key, configuration);
    else {
        QL_FAIL("yield curve type not handled");
    }
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, type, configuration, "yield curve / ibor index");
}

Handle<YieldTermStructure> MarketImpl::discountCurveImpl(const string& key, const string& configuration) const {
    require(MarketObject::DiscountCurve, key, configuration);
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::Discount, configuration,
                                              "discount curve");
}

Handle<YieldTermStructure> MarketImpl::yieldCurve(const string& key, const string& configuration) const {
    require(MarketObject::YieldCurve, key, configuration);
    return yieldCurve(YieldCurveType::Yield, key, configuration);
}

Handle<IborIndex> MarketImpl::iborIndex(const string& key, const string& configuration) const {
    require(MarketObject::IndexCurve, key, configuration);
    return lookup<Handle<IborIndex>>(iborIndices_, key, configuration, "ibor index");
}

Handle<SwapIndex> MarketImpl::swapIndex(const string& key, const string& configuration) const {
    require(MarketObject::SwapIndexCurve, key, configuration);
    return lookup<Handle<SwapIndex>>(swapIndices_, key, configuration, "swap index");
}

Handle<QuantLib::SwaptionVolatilityStructure> MarketImpl::swaptionVol(const string& key,
                                                                      const string& configuration) const {
    require(MarketObject::SwaptionVol, key, configuration);
    auto it = swaptionCurves_.find(make_pair(configuration, key));
    if (it != swaptionCurves_.end())
        return it->second;
    // try the default config with the same key
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::SwaptionVol, key, Market::defaultConfiguration);
        auto it2 = swaptionCurves_.find(make_pair(Market::defaultConfiguration, key));
        if (it2 != swaptionCurves_.end())
            return it2->second;
    }
    // if key is an index name and we have a swaption curve for its ccy, we return that
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (!tryParseIborIndex(key, index)) {
        QL_FAIL("did not find swaption curve for key '" << key << "'");
    }
    auto ccy = index->currency().code();
    require(MarketObject::SwaptionVol, ccy, configuration);
    auto it3 = swaptionCurves_.find(make_pair(configuration, ccy));
    if (it3 != swaptionCurves_.end()) {
        return it3->second;
    }
    // check if we have a curve for the ccy in the default config
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::SwaptionVol, ccy, configuration);
        auto it4 = swaptionCurves_.find(make_pair(Market::defaultConfiguration, ccy));
        if (it4 != swaptionCurves_.end())
            return it4->second;
    }
    QL_FAIL("did not find swaption curve for key '" << key << "'");
}

pair<string, string> MarketImpl::swapIndexBases(const string& key, const string& configuration) const {
    require(MarketObject::SwaptionVol, key, configuration);
    auto it = swaptionIndexBases_.find(make_pair(configuration, key));
    if (it != swaptionIndexBases_.end())
        return it->second;
    // try the default config with the same key
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::SwaptionVol, key, Market::defaultConfiguration);
        auto it2 = swaptionIndexBases_.find(make_pair(Market::defaultConfiguration, key));
        if (it2 != swaptionIndexBases_.end())
            return it2->second;
    }
    // if key is an index name and we have a swaption curve for its ccy, we return that
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (!tryParseIborIndex(key, index)) {
        QL_FAIL("did not find swaption index bases for key '" << key << "'");
    }
    auto ccy = index->currency().code();
    require(MarketObject::SwaptionVol, ccy, configuration);
    auto it3 = swaptionIndexBases_.find(make_pair(configuration, ccy));
    if (it3 != swaptionIndexBases_.end()) {
        return it3->second;
    }
    // check if we have a curve for the ccy in the default config
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::SwaptionVol, ccy, configuration);
        auto it4 = swaptionIndexBases_.find(make_pair(Market::defaultConfiguration, ccy));
        if (it4 != swaptionIndexBases_.end())
            return it4->second;
    }
    QL_FAIL("did not find swaption index bases for key '" << key << "'");
}

string MarketImpl::shortSwapIndexBase(const string& key, const string& configuration) const {
    return swapIndexBases(key, configuration).first;
}

string MarketImpl::swapIndexBase(const string& key, const string& configuration) const {
    return swapIndexBases(key, configuration).second;
}

Handle<QuantLib::SwaptionVolatilityStructure> MarketImpl::yieldVol(const string& key,
                                                                   const string& configuration) const {
    require(MarketObject::YieldVol, key, configuration);
    return lookup<Handle<QuantLib::SwaptionVolatilityStructure>>(yieldVolCurves_, key, configuration,
                                                                 "yield volatility curve");
}

Handle<QuantExt::FxIndex> MarketImpl::fxIndexImpl(const string& fxIndex, const string& configuration) const {
    QL_REQUIRE(fx_ != nullptr,
               "MarketImpl::fxIndex(" << fxIndex << "): fx_ is null. This is an internal error. Contact dev.");
    return fx_->getIndex(fxIndex, this, configuration);
}

Handle<Quote> MarketImpl::fxRateImpl(const string& ccypair, const string& configuration) const {
    // if rate requested for a currency against itself, return 1.0
    if (ccypair.substr(0,3) == ccypair.substr(3))
        return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    return fxIndex(ccypair, configuration)->fxQuote();
}

Handle<Quote> MarketImpl::fxSpotImpl(const string& ccypair, const string& configuration) const {
    if (ccypair.substr(0, 3) == ccypair.substr(3))
        return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    return fxIndex(ccypair, configuration)->fxQuote(true);
}

Handle<BlackVolTermStructure> MarketImpl::fxVolImpl(const string& ccypair, const string& configuration) const {
    require(MarketObject::FXVol, ccypair, configuration);
    auto it = fxVols_.find(make_pair(configuration, ccypair));
    if (it != fxVols_.end())
        return it->second;
    else {
        // check for reverse EURUSD or USDEUR and add to the map
        QL_REQUIRE(ccypair.length() == 6, "invalid ccy pair length");
        std::string ccypairInverted = ccypair.substr(3, 3) + ccypair.substr(0, 3);
        require(MarketObject::FXVol, ccypairInverted, configuration);
        it = fxVols_.find(make_pair(configuration, ccypairInverted));
        if (it != fxVols_.end()) {
            Handle<BlackVolTermStructure> h(QuantLib::ext::make_shared<QuantExt::BlackInvertedVolTermStructure>(it->second));
            h->enableExtrapolation();
            // we have found a surface for the inverted pair.
            // so we can invert the surface and store that under the original pair.
            fxVols_[make_pair(configuration, ccypair)] = h;
            return h;
        } else {
            if (configuration == Market::defaultConfiguration)
                QL_FAIL("did not find fx vol object " << ccypair);
            else
                return fxVol(ccypair, Market::defaultConfiguration);
        }
    }
}

Handle<QuantExt::CreditCurve> MarketImpl::defaultCurve(const string& key, const string& configuration) const {
    require(MarketObject::DefaultCurve, key, configuration);
    return lookup<Handle<QuantExt::CreditCurve>>(defaultCurves_, key, configuration, "default curve");
}

Handle<Quote> MarketImpl::recoveryRate(const string& key, const string& configuration) const {
    // recovery rates can be built together with default curve or securities
    require(MarketObject::DefaultCurve, key, configuration);
    require(MarketObject::Security, key, configuration);
    return lookup<Handle<Quote>>(recoveryRates_, key, configuration, "recovery rate");
}

Handle<QuantExt::CreditVolCurve> MarketImpl::cdsVol(const string& key, const string& configuration) const {
    require(MarketObject::CDSVol, key, configuration);
    return lookup<Handle<QuantExt::CreditVolCurve>>(cdsVols_, key, configuration, "cds vol curve");
}

Handle<QuantExt::BaseCorrelationTermStructure>
MarketImpl::baseCorrelation(const string& key, const string& configuration) const {
    require(MarketObject::BaseCorrelation, key, configuration);
    return lookup<Handle<QuantExt::BaseCorrelationTermStructure>>(baseCorrelations_, key, configuration,
                                                                               "base correlation curve");
}

Handle<OptionletVolatilityStructure> MarketImpl::capFloorVol(const string& key, const string& configuration) const {
    require(MarketObject::CapFloorVol, key, configuration);
    auto it = capFloorCurves_.find(make_pair(configuration, key));
    if (it != capFloorCurves_.end())
        return it->second;
    // first try the default config with the same key
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::CapFloorVol, key, Market::defaultConfiguration);
        auto it2 = capFloorCurves_.find(make_pair(Market::defaultConfiguration, key));
        if (it2 != capFloorCurves_.end())
            return it2->second;
    }
    // if key is an index name and we have a cap floor surface for its ccy, we return that
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (!tryParseIborIndex(key, index)) {
        QL_FAIL("did not find capfloor curve for key '" << key << "'");
    }
    auto ccy = index->currency().code();
    require(MarketObject::CapFloorVol, ccy, configuration);
    auto it3 = capFloorCurves_.find(make_pair(configuration, ccy));
    if (it3 != capFloorCurves_.end()) {
        return it3->second;
    }
    // check if we have a curve for the ccy in the default config
    if (configuration != Market::defaultConfiguration) {
	require(MarketObject::CapFloorVol, ccy, configuration);
        auto it4 = capFloorCurves_.find(make_pair(Market::defaultConfiguration, ccy));
        if (it4 != capFloorCurves_.end())
            return it4->second;
    }
    QL_FAIL("did not find capfloor curve for key '" << key << "'");
}

std::pair<string, QuantLib::Period> MarketImpl::capFloorVolIndexBase(const string& key,
                                                                     const string& configuration) const {
    require(MarketObject::CapFloorVol, key, configuration);
    auto it = capFloorIndexBase_.find(make_pair(configuration, key));
    if (it != capFloorIndexBase_.end())
        return it->second;
    // first try the default config with the same key
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::CapFloorVol, key, Market::defaultConfiguration);
        auto it2 = capFloorIndexBase_.find(make_pair(Market::defaultConfiguration, key));
        if (it2 != capFloorIndexBase_.end())
            return it2->second;
    }
    // if key is an index name and we have a cap floor surface for its ccy, we return that
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (!tryParseIborIndex(key, index)) {
        return std::make_pair(string(),0*Days);
    }
    auto ccy = index->currency().code();
    require(MarketObject::CapFloorVol, ccy, configuration);
    auto it3 = capFloorIndexBase_.find(make_pair(configuration, ccy));
    if (it3 != capFloorIndexBase_.end()) {
        return it3->second;
    }
    // check if we have a curve for the ccy in the default config
    if (configuration != Market::defaultConfiguration) {
        require(MarketObject::CapFloorVol, ccy, configuration);
        auto it4 = capFloorIndexBase_.find(make_pair(Market::defaultConfiguration, ccy));
        if (it4 != capFloorIndexBase_.end())
            return it4->second;
    }
    return std::make_pair(string(), 0 * Days);
}

Handle<YoYOptionletVolatilitySurface> MarketImpl::yoyCapFloorVol(const string& key, const string& configuration) const {
    require(MarketObject::YoYInflationCapFloorVol, key, configuration);
    return lookup<Handle<YoYOptionletVolatilitySurface>>(yoyCapFloorVolSurfaces_, key, configuration,
                                                         "yoy inflation capfloor curve");
}

Handle<ZeroInflationIndex> MarketImpl::zeroInflationIndex(const string& indexName, const string& configuration) const {
    require(MarketObject::ZeroInflationCurve, indexName, configuration);
    return lookup<Handle<ZeroInflationIndex>>(zeroInflationIndices_, indexName, configuration, "zero inflation index");
}

Handle<YoYInflationIndex> MarketImpl::yoyInflationIndex(const string& indexName, const string& configuration) const {
    require(MarketObject::YoYInflationCurve, indexName, configuration);
    return lookup<Handle<YoYInflationIndex>>(yoyInflationIndices_, indexName, configuration, "yoy inflation index");
}

Handle<CPIVolatilitySurface> MarketImpl::cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                                                               const string& configuration) const {
    require(MarketObject::ZeroInflationCapFloorVol, indexName, configuration);
    return lookup<Handle<CPIVolatilitySurface>>(cpiInflationCapFloorVolatilitySurfaces_, indexName, configuration,
                                                "cpi cap floor volatility surface");
}

Handle<Quote> MarketImpl::equitySpot(const string& key, const string& configuration) const {
    require(MarketObject::EquityCurve, key, configuration);
    return lookup<Handle<Quote>>(equitySpots_, key, configuration, "equity spot");
}

Handle<QuantExt::EquityIndex2> MarketImpl::equityCurve(const string& key, const string& configuration) const {
    require(MarketObject::EquityCurve, key, configuration);
    return lookup<Handle<QuantExt::EquityIndex2>>(equityCurves_, key, configuration, "equity curve");
};

Handle<YieldTermStructure> MarketImpl::equityDividendCurve(const string& key, const string& configuration) const {
    require(MarketObject::EquityCurve, key, configuration);
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::EquityDividend, configuration,
                                              "dividend yield curve");
}

Handle<BlackVolTermStructure> MarketImpl::equityVol(const string& key, const string& configuration) const {
    require(MarketObject::EquityVol, key, configuration);
    return lookup<Handle<BlackVolTermStructure>>(equityVols_, key, configuration, "equity vol curve");
}

Handle<YieldTermStructure> MarketImpl::equityForecastCurve(const string& eqName, const string& configuration) const {
    require(MarketObject::EquityCurve, eqName, configuration);
    return equityCurve(eqName, configuration)->equityForecastCurve();
}

Handle<Quote> MarketImpl::securitySpread(const string& key, const string& configuration) const {
    require(MarketObject::Security, key, configuration);
    return lookup<Handle<Quote>>(securitySpreads_, key, configuration, "security spread");
}

Handle<QuantExt::InflationIndexObserver> MarketImpl::baseCpis(const string& key, const string& configuration) const {
    require(MarketObject::ZeroInflationCurve, key, configuration);
    return lookup<Handle<QuantExt::InflationIndexObserver>>(baseCpis_, key, configuration, "base CPI");
}

Handle<PriceTermStructure> MarketImpl::commodityPriceCurve(const string& commodityName,
                                                           const string& configuration) const {
    return commodityIndex(commodityName, configuration)->priceCurve();
}

Handle<CommodityIndex> MarketImpl::commodityIndex(const string& commodityName, const string& configuration) const {
    require(MarketObject::CommodityCurve, commodityName, configuration);
    return lookup<Handle<CommodityIndex>>(commodityIndices_, commodityName, configuration, "commodity indices");
}

Handle<BlackVolTermStructure> MarketImpl::commodityVolatility(const string& commodityName,
                                                              const string& configuration) const {
    require(MarketObject::CommodityVolatility, commodityName, configuration);
    return lookup<Handle<BlackVolTermStructure>>(commodityVols_, commodityName, configuration, "commodity volatility");
}

Handle<QuantExt::CorrelationTermStructure> MarketImpl::correlationCurve(const string& index1, const string& index2,
                                                                        const string& configuration) const {
    // straight pair
    require(MarketObject::Correlation, index1 + "&" + index2, configuration);
    auto it = correlationCurves_.find(make_tuple(configuration, index1, index2));
    if (it != correlationCurves_.end())
        return it->second;
    // inverse pair
    require(MarketObject::Correlation, index2 + "&" + index1, configuration);
    it = correlationCurves_.find(make_tuple(configuration, index2, index1));
    if (it != correlationCurves_.end())
        return it->second;
    // inverse fx index1
    if (isFxIndex(index1)) {
        require(MarketObject::Correlation, inverseFxIndex(index1) + "&" + index2, configuration);
        it = correlationCurves_.find(make_tuple(configuration, inverseFxIndex(index1), index2));
        if (it != correlationCurves_.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
        require(MarketObject::Correlation, index2 + "&" + inverseFxIndex(index1), configuration);
        it = correlationCurves_.find(make_tuple(configuration, index2, inverseFxIndex(index1)));
        if (it != correlationCurves_.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
    }
    // inverse fx index2
    if (isFxIndex(index2)) {
        require(MarketObject::Correlation, index1 + "&" + inverseFxIndex(index2), configuration);
        it = correlationCurves_.find(make_tuple(configuration, index1, inverseFxIndex(index2)));
        if (it != correlationCurves_.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
        require(MarketObject::Correlation, inverseFxIndex(index2) + "&" + index1, configuration);
        it = correlationCurves_.find(make_tuple(configuration, inverseFxIndex(index2), index1));
        if (it != correlationCurves_.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
    }
    // both fx indices inverted
    if (isFxIndex(index1) && isFxIndex(index2)) {
        require(MarketObject::Correlation, inverseFxIndex(index1) + "&" + inverseFxIndex(index2), configuration);
        it = correlationCurves_.find(make_tuple(configuration, inverseFxIndex(index1), inverseFxIndex(index2)));
        if (it != correlationCurves_.end())
            return it->second;
        require(MarketObject::Correlation, inverseFxIndex(index2) + "&" + inverseFxIndex(index1), configuration);
        it = correlationCurves_.find(make_tuple(configuration, inverseFxIndex(index2), inverseFxIndex(index1)));
        if (it != correlationCurves_.end())
            return it->second;
    }
    // if not found, fall back to default configuration
    if (configuration == Market::defaultConfiguration) {
        QL_FAIL("did not find object " << index1 << "/" << index2 << " in CorrelationCurves");
    } else {
        return correlationCurve(index1, index2, Market::defaultConfiguration);
    }
}

Handle<Quote> MarketImpl::cpr(const string& securityID, const string& configuration) const {
    require(MarketObject::Security, securityID, configuration);
    return lookup<Handle<Quote>>(cprs_, securityID, configuration, "cpr");
}

void MarketImpl::addSwapIndex(const string& swapIndex, const string& discountIndex, const string& configuration) const {
    if (swapIndices_.find(make_pair(configuration, swapIndex)) != swapIndices_.end())
        return;
    try {
        std::vector<string> tokens;
        split(tokens, swapIndex, boost::is_any_of("-"));
        QL_REQUIRE(tokens.size() == 3 || tokens.size() == 4,
                   "three or four tokens required in " << swapIndex << ": CCY-CMS-TENOR or CCY-CMS-TAG-TENOR");
        QL_REQUIRE(tokens[0].size() == 3, "invalid currency code in " << swapIndex);
        QL_REQUIRE(tokens[1] == "CMS", "expected CMS as second token in " << swapIndex);

        Handle<YieldTermStructure> discounting, forwarding;
        QuantLib::ext::shared_ptr<IborIndex> dummeyIndex;
        if (tryParseIborIndex(discountIndex, dummeyIndex))
            discounting = iborIndex(discountIndex, configuration)->forwardingTermStructure();
        else
            discounting = yieldCurve(discountIndex, configuration);

        const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
        auto swapCon = QuantLib::ext::dynamic_pointer_cast<data::SwapIndexConvention>(conventions->get(swapIndex));
        QL_REQUIRE(swapCon, "expected SwapIndexConvention for " << swapIndex);
        auto conIbor = QuantLib::ext::dynamic_pointer_cast<data::IRSwapConvention>(conventions->get(swapCon->conventions()));
        auto conOisComp = QuantLib::ext::dynamic_pointer_cast<data::OisConvention>(conventions->get(swapCon->conventions()));
        auto conOisAvg = QuantLib::ext::dynamic_pointer_cast<data::AverageOisConvention>(conventions->get(swapCon->conventions()));
        QL_REQUIRE(conIbor || conOisComp || conOisAvg,
                   "expected IRSwapConvention, OisConvention, AverageOisConvention for " << swapCon->conventions());

        string fi = conIbor ? conIbor->indexName() : (conOisComp ? conOisComp->indexName() : conOisAvg->indexName());
        if (isGenericIborIndex(fi))
            forwarding = discounting;
        else
            forwarding = iborIndex(fi, configuration)->forwardingTermStructure();

        QuantLib::ext::shared_ptr<SwapIndex> si = data::parseSwapIndex(swapIndex, forwarding, discounting);
        swapIndices_[make_pair(configuration, swapIndex)] = Handle<SwapIndex>(si);
    } catch (std::exception& e) {
        QL_FAIL("Failure in MarketImpl::addSwapIndex() with index " << swapIndex << " : " << e.what());
    }
}

void MarketImpl::refresh(const string& configuration) {

    auto it = refreshTs_.find(configuration);
    if (it == refreshTs_.end()) {
        it = refreshTs_.insert(make_pair(configuration, std::set<QuantLib::ext::shared_ptr<TermStructure>>())).first;
    }

    if (it->second.empty()) {
        for (auto& x : yieldCurves_) {
            if (get<0>(x.first) == configuration || get<0>(x.first) == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : iborIndices_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration) {
                Handle<YieldTermStructure> y = x.second->forwardingTermStructure();
                if (!y.empty())
                    it->second.insert(*y);
            }
        }
        for (auto& x : swapIndices_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration) {
                Handle<YieldTermStructure> y = x.second->forwardingTermStructure();
                if (!y.empty())
                    it->second.insert(*y);
                y = x.second->discountingTermStructure();
                if (!y.empty())
                    it->second.insert(*y);
            }
        }
        for (auto& x : swaptionCurves_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : capFloorCurves_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : yoyCapFloorVolSurfaces_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : fxVols_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : defaultCurves_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second->curve());
        }
        for (auto& x : cdsVols_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : baseCorrelations_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : zeroInflationIndices_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration) {
                it->second.insert(*x.second->zeroInflationTermStructure());
            }
        }
        for (auto& x : yoyInflationIndices_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration) {
                it->second.insert(*x.second->yoyInflationTermStructure());
            }
        }
        for (auto& x : cpiInflationCapFloorVolatilitySurfaces_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : yoyCapFloorVolSurfaces_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : equityVols_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : equityCurves_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration) {
                Handle<YieldTermStructure> y = x.second->equityForecastCurve();
                if (!y.empty())
                    it->second.insert(*y);
                y = x.second->equityDividendCurve();
                if (!y.empty())
                    it->second.insert(*y);
            }
        }
        for (auto& x : commodityIndices_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration) {
                const auto& pts = x.second->priceCurve();
                if (!pts.empty())
                    it->second.insert(*pts);
            }
        }
        for (auto& x : commodityVols_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }

        for (auto& x : correlationCurves_) {
            if (get<0>(x.first) == configuration || get<0>(x.first) == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
    }

    // term structures might be wrappers around nested termstructures that need to be updated as well,
    // therefore we need to call deepUpdate() (=update() if no such nesting is present)
    for (auto& x : it->second)
        x->deepUpdate();

} // refresh

} // namespace data
} // namespace ore
