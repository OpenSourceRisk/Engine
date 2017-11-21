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

using namespace QuantLib;
using namespace std;
using std::string;
using std::map;
using std::make_pair;

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
A lookup(const B& map, const C& key, const YieldCurveType y ,const string& configuration, const string& type) {
    auto it = map.find(make_tuple(configuration, y, key));
    if (it == map.end()) {
        // fall back to default configuration
        it = map.find(make_tuple(Market::defaultConfiguration, y, key));
        QL_REQUIRE(it != map.end(),
                   "did not find object " << key << " of type " << type << " under configuration " << configuration << " in YieldCurves");
    }
    return it->second;
}

} // anonymous namespace
Handle<YieldTermStructure> MarketImpl::yieldCurve(const YieldCurveType& type, const string& key, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, type, configuration, "yield curve");
}

Handle<YieldTermStructure> MarketImpl::discountCurve(const string& key, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::Discount, configuration, "discount curve");
}

Handle<YieldTermStructure> MarketImpl::yieldCurve(const string& key, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::Yield, configuration, "yield curve");
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
            fxVols_[make_pair(configuration, ccypairInverted)] = h;
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

Handle<ZeroInflationIndex> MarketImpl::zeroInflationIndex(const string& indexName, const string& configuration) const {
    return lookup<Handle<ZeroInflationIndex>>(zeroInflationIndices_, indexName, configuration, "zero inflation index");
}

Handle<YoYInflationIndex> MarketImpl::yoyInflationIndex(const string& indexName, const string& configuration) const {
    return lookup<Handle<YoYInflationIndex>>(yoyInflationIndices_, indexName, configuration, "yoy inflation index");
}

Handle<CPICapFloorTermPriceSurface> MarketImpl::inflationCapFloorPriceSurface(const string& indexName,
                                                                              const string& configuration) const {
    return lookup<Handle<CPICapFloorTermPriceSurface>>(inflationCapFloorPriceSurfaces_, indexName, configuration,
                                                       "inflation cap floor price surface");
}

Handle<Quote> MarketImpl::equitySpot(const string& key, const string& configuration) const {
    return lookup<Handle<Quote>>(equitySpots_, key, configuration, "equity spot");
}

Handle<YieldTermStructure> MarketImpl::equityDividendCurve(const string& key, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, key, YieldCurveType::EquityDividend, configuration, "dividend yield curve");
}

Handle<BlackVolTermStructure> MarketImpl::equityVol(const string& key, const string& configuration) const {
    return lookup<Handle<BlackVolTermStructure>>(equityVols_, key, configuration, "equity vol curve");
}

Handle<YieldTermStructure> MarketImpl::equityForecastCurve(const string& eqName, const string& configuration) const {
    return lookup<Handle<YieldTermStructure>>(yieldCurves_, eqName, YieldCurveType::EquityForecast, configuration, "equity forecast yield curve");
}

Handle<Quote> MarketImpl::securitySpread(const string& key, const string& configuration) const {
    return lookup<Handle<Quote>>(securitySpreads_, key, configuration, "security spread");
}

Handle<InflationIndexObserver> MarketImpl::baseCpis(const string& key, const string& configuration) const {
    return lookup<Handle<InflationIndexObserver>>(baseCpis_, key, configuration, "base CPI");
}

void MarketImpl::addSwapIndex(const string& swapIndex, const string& discountIndex, const string& configuration) {
    try {
        std::vector<string> tokens;
        split(tokens, swapIndex, boost::is_any_of("-"));
        QL_REQUIRE(tokens.size() == 3, "three tokens required in " << swapIndex << ": CCY-CMS-TENOR");
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
        for (auto& x : inflationCapFloorPriceSurfaces_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }
        for (auto& x : equityVols_) {
            if (x.first.first == configuration || x.first.first == Market::defaultConfiguration)
                it->second.insert(*x.second);
        }

    }

    for (auto& x : it->second)
        x->update();

} // refresh

} // namespace data
} // namespace ore
