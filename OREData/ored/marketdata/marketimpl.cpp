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
#include <ored/utilities/parsers.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>

using namespace std;
using std::make_pair;
using std::map;
using std::string;

using namespace QuantLib;

using QuantExt::PriceTermStructure;

namespace ore {
namespace data {

namespace {

template <class A, class B, class C>
A lookup(const B& map, const C& key, const string& configuration, const string& type) {
    auto it = map.find(make_pair(configuration, key));
    if (it == map.end()) {
        // fall back to default configuration
        it = map.find(make_pair(Market::defaultConfiguration, key));
        QL_REQUIRE(it != map.end(),
                   "did not find object " << key << " of type " << type << " under configuration " << configuration);
    }
    return it->second;
}

template <class A, class B, class C>
A lookup(const B& map, const C& key, const YieldCurveType y, const string& configuration, const string& type) {
    auto it = map.find(make_tuple(configuration, y, key));
    if (it == map.end()) {
        // fall back to default configuration
        it = map.find(make_tuple(Market::defaultConfiguration, y, key));
        QL_REQUIRE(it != map.end(), "did not find object " << key << " of type " << type << " under configuration "
                                                           << configuration << " in YieldCurves");
    }
    return it->second;
}

Handle<QuantExt::CorrelationTermStructure>
lookup(const map<tuple<string, string, string>, Handle<QuantExt::CorrelationTermStructure>>& map,
       const std::string& key1, const std::string& key2, const string& configuration) {
    // straight pair
    auto it = map.find(make_tuple(configuration, key1, key2));
    if (it != map.end())
        return it->second;
    // inverse pair
    it = map.find(make_tuple(configuration, key2, key1));
    if (it != map.end())
        return it->second;
    // inverse fx index1
    if (isFxIndex(key1)) {
        it = map.find(make_tuple(configuration, inverseFxIndex(key1), key2));
        if (it != map.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                boost::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
        it = map.find(make_tuple(configuration, key2, inverseFxIndex(key1)));
        if (it != map.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                boost::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
    }
    // inverse fx index2
    if (isFxIndex(key2)) {
        it = map.find(make_tuple(configuration, key1, inverseFxIndex(key2)));
        if (it != map.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                boost::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
        it = map.find(make_tuple(configuration, inverseFxIndex(key2), key1));
        if (it != map.end())
            return Handle<QuantExt::CorrelationTermStructure>(
                boost::make_shared<QuantExt::NegativeCorrelationTermStructure>(it->second));
    }
    // both fx indices inverted
    if (isFxIndex(key1) && isFxIndex(key2)) {
        it = map.find(make_tuple(configuration, inverseFxIndex(key1), inverseFxIndex(key2)));
        if (it != map.end())
            return it->second;
        it = map.find(make_tuple(configuration, inverseFxIndex(key2), inverseFxIndex(key1)));
        if (it != map.end())
            return it->second;
    }
    // if not found, fall back to default configuration
    if (configuration == Market::defaultConfiguration) {
        QL_FAIL("did not find object " << key1 << "/" << key2 << " in CorrelationCurves");
    } else {
        return lookup(map, key1, key2, Market::defaultConfiguration);
    }
}

} // anonymous namespace

Handle<YieldTermStructure> MarketImpl::yieldCurve(const YieldCurveType& type, const string& key,
                                                  const string& configuration) const {
    // we allow for standard (i.e. not convention based) ibor index names as keys and return the index forward curve in
    // case of a match
    boost::shared_ptr<IborIndex> notUsed;
    if (tryParseIborIndex(key, notUsed)) {
        return iborIndex(key, configuration)->forwardingTermStructure();
    }
    // no ibor index found under key => look for a genuine yield curve
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, type, configuration, "yield curve");
}

Handle<YieldTermStructure> MarketImpl::discountCurve(const string& key, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::Discount, configuration,
                                              "discount curve");
}

Handle<YieldTermStructure> MarketImpl::yieldCurve(const string& key, const string& configuration) const {
    return yieldCurve(YieldCurveType::Yield, key, configuration);
}

Handle<IborIndex> MarketImpl::iborIndex(const string& key, const string& configuration) const {
    return lookup<Handle<IborIndex>>(iborIndices_, key, configuration, "ibor index");
}

Handle<SwapIndex> MarketImpl::swapIndex(const string& key, const string& configuration) const {
    return lookup<Handle<SwapIndex>>(swapIndices_, key, configuration, "swap index");
}

Handle<QuantLib::SwaptionVolatilityStructure> MarketImpl::swaptionVol(const string& key,
                                                                      const string& configuration) const {
    return lookup<Handle<QuantLib::SwaptionVolatilityStructure>>(swaptionCurves_, key, configuration, "swaption curve");
}

const string MarketImpl::shortSwapIndexBase(const string& key, const string& configuration) const {
    return lookup<pair<string, string>>(swaptionIndexBases_, key, configuration, "short swap index base").first;
}

const string MarketImpl::swapIndexBase(const string& key, const string& configuration) const {
    return lookup<pair<string, string>>(swaptionIndexBases_, key, configuration, "swap index base").second;
}

Handle<QuantLib::SwaptionVolatilityStructure> MarketImpl::yieldVol(const string& key,
                                                                   const string& configuration) const {
    return lookup<Handle<QuantLib::SwaptionVolatilityStructure>>(yieldVolCurves_, key, configuration,
                                                                 "yield volatility curve");
}

Handle<Quote> MarketImpl::fxSpot(const string& ccypair, const string& configuration) const {
    auto it = fxSpots_.find(configuration);
    if (it == fxSpots_.end())
        it = fxSpots_.find(Market::defaultConfiguration);
    QL_REQUIRE(it != fxSpots_.end(),
               "did not find object " << ccypair << " of type fx spot under configuration " << configuration);
    return it->second.getQuote(ccypair); // will throw if not found
}

Handle<BlackVolTermStructure> MarketImpl::fxVol(const string& ccypair, const string& configuration) const {
    auto it = fxVols_.find(make_pair(configuration, ccypair));
    if (it != fxVols_.end())
        return it->second;
    else {
        // check for reverse EURUSD or USDEUR and add to the map
        QL_REQUIRE(ccypair.length() == 6, "invalid ccy pair length");
        std::string ccypairInverted = ccypair.substr(3, 3) + ccypair.substr(0, 3);
        it = fxVols_.find(make_pair(configuration, ccypairInverted));
        if (it != fxVols_.end()) {
            Handle<BlackVolTermStructure> h(boost::make_shared<QuantExt::BlackInvertedVolTermStructure>(it->second));
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

Handle<DefaultProbabilityTermStructure> MarketImpl::defaultCurve(const string& key, const string& configuration) const {
    return lookup<Handle<DefaultProbabilityTermStructure>>(defaultCurves_, key, configuration, "default curve");
}

Handle<Quote> MarketImpl::recoveryRate(const string& key, const string& configuration) const {
    return lookup<Handle<Quote>>(recoveryRates_, key, configuration, "recovery rate");
}

Handle<BlackVolTermStructure> MarketImpl::cdsVol(const string& key, const string& configuration) const {
    return lookup<Handle<BlackVolTermStructure>>(cdsVols_, key, configuration, "cds vol curve");
}

Handle<BaseCorrelationTermStructure<BilinearInterpolation>>
MarketImpl::baseCorrelation(const string& key, const string& configuration) const {
    return lookup<Handle<BaseCorrelationTermStructure<BilinearInterpolation>>>(baseCorrelations_, key, configuration,
                                                                               "base correlation curve");
}

Handle<OptionletVolatilityStructure> MarketImpl::capFloorVol(const string& key, const string& configuration) const {
    return lookup<Handle<OptionletVolatilityStructure>>(capFloorCurves_, key, configuration, "capfloor curve");
}

Handle<QuantExt::YoYOptionletVolatilitySurface> MarketImpl::yoyCapFloorVol(const string& key,
                                                                           const string& configuration) const {
    return lookup<Handle<QuantExt::YoYOptionletVolatilitySurface>>(yoyCapFloorVolSurfaces_, key, configuration,
                                                                   "yoy inflation capfloor curve");
}

Handle<ZeroInflationIndex> MarketImpl::zeroInflationIndex(const string& indexName, const string& configuration) const {
    return lookup<Handle<ZeroInflationIndex>>(zeroInflationIndices_, indexName, configuration, "zero inflation index");
}

Handle<YoYInflationIndex> MarketImpl::yoyInflationIndex(const string& indexName, const string& configuration) const {
    return lookup<Handle<YoYInflationIndex>>(yoyInflationIndices_, indexName, configuration, "yoy inflation index");
}

Handle<CPIVolatilitySurface> MarketImpl::cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                                                               const string& configuration) const {
    return lookup<Handle<CPIVolatilitySurface>>(cpiInflationCapFloorVolatilitySurfaces_, indexName, configuration,
                                                "cpi cap floor volatility surface");
}

Handle<Quote> MarketImpl::equitySpot(const string& key, const string& configuration) const {
    return lookup<Handle<Quote>>(equitySpots_, key, configuration, "equity spot");
}

Handle<QuantExt::EquityIndex> MarketImpl::equityCurve(const string& key, const string& configuration) const {
    return lookup<Handle<QuantExt::EquityIndex>>(equityCurves_, key, configuration, "equity curve");
};

Handle<YieldTermStructure> MarketImpl::equityDividendCurve(const string& key, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::EquityDividend, configuration,
                                              "dividend yield curve");
}

Handle<BlackVolTermStructure> MarketImpl::equityVol(const string& key, const string& configuration) const {
    return lookup<Handle<BlackVolTermStructure>>(equityVols_, key, configuration, "equity vol curve");
}

Handle<YieldTermStructure> MarketImpl::equityForecastCurve(const string& eqName, const string& configuration) const {
    return equityCurve(eqName, configuration)->equityForecastCurve();
}

Handle<Quote> MarketImpl::securitySpread(const string& key, const string& configuration) const {
    return lookup<Handle<Quote>>(securitySpreads_, key, configuration, "security spread");
}

Handle<QuantExt::InflationIndexObserver> MarketImpl::baseCpis(const string& key, const string& configuration) const {
    return lookup<Handle<QuantExt::InflationIndexObserver>>(baseCpis_, key, configuration, "base CPI");
}

Handle<PriceTermStructure> MarketImpl::commodityPriceCurve(const string& commodityName,
                                                           const string& configuration) const {
    return lookup<Handle<PriceTermStructure>>(commodityCurves_, commodityName, configuration, "commodity price curve");
}

Handle<BlackVolTermStructure> MarketImpl::commodityVolatility(const string& commodityName,
                                                              const string& configuration) const {
    return lookup<Handle<BlackVolTermStructure>>(commodityVols_, commodityName, configuration, "commodity volatility");
}

Handle<QuantExt::CorrelationTermStructure> MarketImpl::correlationCurve(const string& index1, const string& index2,
                                                                        const string& configuration) const {
    return lookup(correlationCurves_, index1, index2, configuration);
}

Handle<Quote> MarketImpl::cpr(const string& securityID, const string& configuration) const {
    return lookup<Handle<Quote>>(cprs_, securityID, configuration, "cpr");
}

void MarketImpl::addSwapIndex(const string& swapIndex, const string& discountIndex, const string& configuration) {
    try {
        std::vector<string> tokens;
        split(tokens, swapIndex, boost::is_any_of("-"));
        QL_REQUIRE(tokens.size() == 3 || tokens.size() == 4,
                   "three or four tokens required in " << swapIndex << ": CCY-CMS-TENOR or CCY-CMS-TAG-TENOR");
        QL_REQUIRE(tokens[0].size() == 3, "invalid currency code in " << swapIndex);
        QL_REQUIRE(tokens[1] == "CMS", "expected CMS as middle token in " << swapIndex);

        auto di = iborIndex(discountIndex, configuration)->forwardingTermStructure();

        boost::shared_ptr<data::Convention> tmp = conventions_.get(swapIndex);
        QL_REQUIRE(tmp, "Need conventions for swap index " << swapIndex);
        boost::shared_ptr<data::SwapIndexConvention> swapCon =
            boost::dynamic_pointer_cast<data::SwapIndexConvention>(tmp);
        QL_REQUIRE(swapCon, "Conventions are not Swap Conventions for " << swapIndex);
        tmp = conventions_.get(swapCon->conventions());
        boost::shared_ptr<data::IRSwapConvention> con = boost::dynamic_pointer_cast<data::IRSwapConvention>(tmp);
        QL_REQUIRE(con, "Cannot find IRSwapConventions");

        auto fi = iborIndex(con->indexName(), configuration)->forwardingTermStructure();

        boost::shared_ptr<SwapIndex> si = data::parseSwapIndex(swapIndex, fi, di, con);
        swapIndices_[make_pair(configuration, swapIndex)] = Handle<SwapIndex>(si);
    } catch (std::exception& e) {
        QL_FAIL("Failure in MarketImpl::addSwapIndex() with index " << swapIndex << " : " << e.what());
    }
}

void MarketImpl::refresh(const string& configuration) {

    auto it = refreshTs_.find(configuration);
    if (it == refreshTs_.end()) {
        it = refreshTs_.insert(make_pair(configuration, std::set<boost::shared_ptr<TermStructure>>())).first;
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
                it->second.insert(*x.second);
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
        for (auto& x : baseCpis_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : commodityCurves_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
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

    // update fx spot quotes
    auto fxSpots = fxSpots_.find(configuration);
    if (fxSpots != fxSpots_.end()) {
        for (auto& x : fxSpots->second.quotes()) {
            auto dq = boost::dynamic_pointer_cast<Observer>(*x.second);
            if (dq != nullptr)
                dq->update();
        }
    }

} // refresh

} // namespace data
} // namespace ore
