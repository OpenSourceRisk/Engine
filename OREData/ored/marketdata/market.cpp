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

#include <ored/marketdata/market.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/indexes/fxindex.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>
#include <qle/termstructures//blacktriangulationatmvol.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

#include <ql/quotes/compositequote.hpp>
#include <ql/time/daycounters/actualactual.hpp>

namespace ore {
namespace data {

const string Market::defaultConfiguration = "default";
const string Market::inCcyConfiguration = "inccy";

namespace {
bool hasPseudoCurrencyConfig(const string& code) {
    QL_REQUIRE(code.size() == 3, "Invalid currency code \"" << code << "\" for hasPseudoCurrencyConfig()");
    const PseudoCurrencyMarketParameters& params = GlobalPseudoCurrencyMarketParameters::instance().get();
    return params.curves.find(code) != params.curves.end();
}

bool hasPseudoCurrencyConfigPair(const string& pair) {
    QL_REQUIRE(pair.size() == 6, "Invalid currency pair \"" << pair << "\" for isPseudoCurrencyConfigPair()");
    return hasPseudoCurrencyConfig(pair.substr(0, 3)) || hasPseudoCurrencyConfig(pair.substr(3));
}
} // namespace

PseudoCurrencyMarketParameters buildPseudoCurrencyMarketParameters(const std::map<string, string>& pegp) {
    PseudoCurrencyMarketParameters params;

    // default
    params.treatAsFX = true;

    auto it = pegp.find("PseudoCurrency.TreatAsFX");
    if (it != pegp.end()) {
        DLOG("Building PseudoCurrencyMarketParameters from PricingEngine GlobalParameters");
        try {
            // Build the params
            params.treatAsFX = parseBool(it->second);
            it = pegp.find("PseudoCurrency.BaseCurrency");
            QL_REQUIRE(it != pegp.end(), "No BaseCurrency field");
            params.baseCurrency = it->second;
            parseCurrency(params.baseCurrency); // to check it is valid

            // Now just search for the 4 precious metals and 7 crypto currencies
            for (const string& pm : CurrencyParser::instance().pseudoCurrencyCodes()) {
                it = pegp.find("PseudoCurrency.Curve." + pm);
                if (it != pegp.end())
                    params.curves[pm] = it->second;
            }
            QL_REQUIRE(!params.curves.empty(), "At least one PM Curve required");

            // Look for fxIndexTag
            it = pegp.find("PseudoCurrency.FXIndexTag");
            QL_REQUIRE(it != pegp.end(), "No FXIndexTag field");
            params.fxIndexTag = it->second;

            // Look for Optional default correlation
            it = pegp.find("PseudoCurrency.DefaultCorrelation");
            if (it == pegp.end()) {
                LOG("No Default Correlation present");
                params.defaultCorrelation = Null<Real>();
            } else {
                LOG("Default Correlation is \"" << it->second << "\"");
                params.defaultCorrelation = parseReal(it->second);
                QL_REQUIRE(-1.0 <= params.defaultCorrelation && params.defaultCorrelation <= 1.0,
                           "Invalid DefaultCorrelation value " << it->second);
            }
        } catch (std::exception& e) {
            QL_FAIL("Failed to build PseudoCurrencyMarketParameters : " << e.what());
        }
    } else {
        DLOG("Building default PseudoCurrencyMarketParameters");
    }
    DLOG(params);

    return params;
}

std::ostream& operator<<(std::ostream& os, const struct PseudoCurrencyMarketParameters& p) {
    // we don't need to write everything out, this is enough for debugging most things
    return os << "PseudoCurrencyMarketParameters { "
              << "TreatAsFX:" << (p.treatAsFX ? "True" : "False") << ", BaseCurrency:" << p.baseCurrency << "}";
}

string Market::commodityCurveLookup(const string& pm) const {
    QL_REQUIRE(handlePseudoCurrencies_, "Market::commodityCurveLookup() disabled - this is an internal error.");
    auto it = GlobalPseudoCurrencyMarketParameters::instance().get().curves.find(pm);
    QL_REQUIRE(it != GlobalPseudoCurrencyMarketParameters::instance().get().curves.end(),
               "Unable to find a commodity curve for pseudo currency " << pm << " in Market");
    return it->second;
}

Handle<Quote> Market::getFxBaseQuote(const string& ccy, const string& config) const {
    QL_REQUIRE(handlePseudoCurrencies_, "Market::commodityCurveLookup() disabled - this is an internal error.");
    if (hasPseudoCurrencyConfig(ccy)) {
        auto priceCurve = commodityPriceCurve(commodityCurveLookup(ccy), config);
        QL_REQUIRE(!priceCurve.empty(),
                   "Failed to get Commodity Price curve for " << ccy << " using " << commodityCurveLookup(ccy));
        TLOG("PseudoCurrencyMarket building DerivedPriceQuote for "
             << ccy << "/" << GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency
             << " with curve that has minTime of " << priceCurve->minTime());
        return Handle<Quote>(QuantLib::ext::make_shared<QuantExt::DerivedPriceQuote>(priceCurve));
    } else {
        return fxRateImpl(ccy + GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency, config);
    }
}

Handle<Quote> Market::getFxSpotBaseQuote(const string& ccy, const string& config) const {
    QL_REQUIRE(handlePseudoCurrencies_, "Market::commodityCurveLookup() disabled - this is an internal error.");
    if (hasPseudoCurrencyConfig(ccy)) {
        // TODO: this gives back the commodity rate at t=0, should be at spot
        auto priceCurve = commodityPriceCurve(commodityCurveLookup(ccy), config);
        QL_REQUIRE(!priceCurve.empty(),
                   "Failed to get Commodity Price curve for " << ccy << " using " << commodityCurveLookup(ccy));
        TLOG("PseudoCurrencyMarket building DerivedPriceQuote for "
             << ccy << "/" << GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency
             << " with curve that has minTime of " << priceCurve->minTime());
        return Handle<Quote>(QuantLib::ext::make_shared<QuantExt::DerivedPriceQuote>(priceCurve));
    } else {
        return fxSpotImpl(ccy + GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency, config);
    }
}

QuantLib::Handle<QuantExt::FxIndex> Market::fxIndex(const string& fxIndex, const string& configuration) const {
    if (!handlePseudoCurrencies_ || GlobalPseudoCurrencyMarketParameters::instance().get().treatAsFX)
        return fxIndexImpl(fxIndex, configuration);

    std::string familyName;
    std::string forCcy;
    std::string domCcy;

    if (isFxIndex(fxIndex)) {
        auto ind = parseFxIndex(fxIndex);
        familyName = ind->familyName();
        forCcy = ind->sourceCurrency().code();
        domCcy = ind->targetCurrency().code();
    } else {
        familyName = "GENERIC";
        forCcy = fxIndex.substr(0, 3);
        domCcy = fxIndex.substr(3);
    }

    if (hasPseudoCurrencyConfigPair(forCcy + domCcy)) {
        DLOG("Market::fxIndex() requested for PM pair " << forCcy << domCcy);
        string index = "FX-" + familyName + "-" + forCcy + "-" + domCcy;
        Handle<QuantExt::FxIndex> fxInd;
        auto it = fxIndicesCache_.find(make_pair(configuration, index));
        // if no index found we build it
        if (it == fxIndicesCache_.end()) {
            // Parse the index we have with no term structures
            QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndexBase = parseFxIndex(index);

            string source = fxIndexBase->sourceCurrency().code();
            string target = fxIndexBase->targetCurrency().code();

            // use todays rate here
            Handle<Quote> spot = fxRate(source + target, configuration);

            Handle<YieldTermStructure> sorTS = discountCurve(source, configuration);
            Handle<YieldTermStructure> tarTS = discountCurve(target, configuration);

            // spot is always zero here as we use the fxRates, which give rate today
            Natural spotDays = 0;
            Calendar calendar = NullCalendar();

            if (source != target) {
                try {
                    auto [sDays, cal, _] = getFxIndexConventions(fxIndex);
                    calendar = cal;
                } catch (...) {
                    WLOG("Market::fxIndex Cannot find commodity conventions for " << fxIndex);   
                }
            }
            fxInd = Handle<QuantExt::FxIndex>(QuantLib::ext::make_shared<QuantExt::FxIndex>(
                fxIndexBase->familyName(), spotDays, fxIndexBase->sourceCurrency(), fxIndexBase->targetCurrency(),
                calendar, spot, sorTS, tarTS));
            // add it to the cache
            fxIndicesCache_[make_pair(configuration, index)] = fxInd;
        } else {
            fxInd = it->second;
        }
        return fxInd;
    }
    return fxIndexImpl(fxIndex, configuration);
}

// These calls are intercepted

Handle<Quote> Market::fxRate(const string& pair, const string& config) const {
    if (!handlePseudoCurrencies_ || GlobalPseudoCurrencyMarketParameters::instance().get().treatAsFX)
        return fxRateImpl(pair, config);
    if (hasPseudoCurrencyConfigPair(pair)) {
        DLOG("Market::fxSpot() requested for PM pair " << pair);
        if (fxRateCache_.find(pair) == fxRateCache_.end()) {
            // Get the FX Spot rate. Rather than deal with all the combinations we just get the FX rate for each vs
            // the baseCcy and create a ratio quote. This might mean we have combined a USD/USD spot quote below,
            // but it all works fine and this is cleaner code.
            //
            // Note that we could just call market->fxSpot(forCode, domCode) and this would give us the correct
            // quote from the markets FXTriangulation, however this would create a depedendacy on the pair
            // and cause the configuration builder to go off building XAU-IN-USD and the like.
            auto forBaseSpot = getFxBaseQuote(pair.substr(0, 3), config);
            auto domBaseSpot = getFxBaseQuote(pair.substr(3), config);
            auto fx = Handle<Quote>(QuantLib::ext::make_shared<CompositeQuote<std::function<Real(Real, Real)>>>(
                forBaseSpot, domBaseSpot, [](Real a, Real b) { return b > 0.0 ? a / b : 0.0; }));
            DLOG("Market returning " << fx->value() << " for " << pair << ".");
            fxRateCache_[pair] = fx;
        }
        return fxRateCache_[pair];
    }
    return fxRateImpl(pair, config);
}

Handle<Quote> Market::fxSpot(const string& pair, const string& config) const {
    if (!handlePseudoCurrencies_ || GlobalPseudoCurrencyMarketParameters::instance().get().treatAsFX)
        return fxSpotImpl(pair, config);
    if (hasPseudoCurrencyConfigPair(pair)) {
        DLOG("Market::fxSpot() requested for PM pair " << pair);
        if (spotCache_.find(pair) == spotCache_.end()) {
            // Get the FX Spot rate. Rather than deal with all the combinations we just get the FX rate for each vs
            // the baseCcy and create a ratio quote. This might mean we have combined a USD/USD spot quote below,
            // but it all works fine and this is cleaner code.
            //
            // Note that we could just call market->fxSpot(forCode, domCode) and this would give us the correct
            // quote from the markets FXTriangulation, however this would create a depedendacy on the pair
            // and cause the configuration builder to go off building XAU-IN-USD and the like.
            auto forBaseSpot = getFxSpotBaseQuote(pair.substr(0, 3), config);
            auto domBaseSpot = getFxSpotBaseQuote(pair.substr(3), config);
            auto fx = Handle<Quote>(QuantLib::ext::make_shared<CompositeQuote<std::function<Real(Real, Real)>>>(
                forBaseSpot, domBaseSpot, [](Real a, Real b) { return b > 0.0 ? a / b : 0.0; }));
            DLOG("Market returning " << fx->value() << " for " << pair << ".");
            spotCache_[pair] = fx;
        }
        return spotCache_[pair];
    }
    return fxSpotImpl(pair, config);
}

Handle<BlackVolTermStructure> Market::getVolatility(const string& ccy, const string& config) const {
    QL_REQUIRE(handlePseudoCurrencies_, "Market::getVolatility() disabled - this is an internal error.");
    if (hasPseudoCurrencyConfig(ccy)) {
        return commodityVolatility(commodityCurveLookup(ccy), config);
    } else {
        return fxVolImpl(ccy + GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency, config);
    }
}

string Market::getCorrelationIndexName(const string& ccy) const {
    QL_REQUIRE(handlePseudoCurrencies_, "Market::getCorrelationIndexName() disabled - this is an internal error.");
    if (hasPseudoCurrencyConfig(ccy)) {
        // e.g. COMM-PM:XAUUSD
        return "COMM-" + commodityCurveLookup(ccy);
    } else {
        // e.g. FX-GENERIC-XAU-USD
        return "FX-" + GlobalPseudoCurrencyMarketParameters::instance().get().fxIndexTag + "-" + ccy + "-" +
               GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency;
    }
}

Handle<BlackVolTermStructure> Market::fxVol(const string& pair, const string& config) const {
    if (!handlePseudoCurrencies_ || GlobalPseudoCurrencyMarketParameters::instance().get().treatAsFX)
        return fxVolImpl(pair, config);

    if (hasPseudoCurrencyConfigPair(pair)) {
        DLOG("Market::fxVol() requested for PM pair " << pair);
        if (volCache_.find(pair) == volCache_.end()) {

            Handle<BlackVolTermStructure> vol;

            auto forCode = pair.substr(0, 3);
            auto domCode = pair.substr(3);

            // we handle the easy and common case first.
            if (forCode == GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency ||
                domCode == GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency) {
                // this is a straight mapping
                string pm =
                    forCode == GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency ? domCode : forCode;
                auto comVol = commodityVolatility(commodityCurveLookup(pm), config);
                if (domCode == GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency)
                    vol = comVol;
                else
                    vol = Handle<BlackVolTermStructure>(
                        QuantLib::ext::make_shared<QuantExt::BlackInvertedVolTermStructure>(comVol));
            } else {

                // otherwise we must triangulate - we get both surfaces vs baseCcy
                auto forBaseVol = getVolatility(forCode, config);
                auto domBaseVol = getVolatility(domCode, config);

                // get the correlation
                string forIndex = getCorrelationIndexName(forCode);
                string domIndex = getCorrelationIndexName(domCode);
                Handle<QuantExt::CorrelationTermStructure> rho;
                try {
                    rho = correlationCurve(forIndex, domIndex, config);
                } catch (std::exception& e) {
                    // No correlation, if we have a default we use it
                    WLOG("No correlation found for " << forIndex << "/" << domIndex);
                    if (GlobalPseudoCurrencyMarketParameters::instance().get().defaultCorrelation != Null<Real>()) {
                        WLOG("Using default correlation value "
                             << GlobalPseudoCurrencyMarketParameters::instance().get().defaultCorrelation);
                        rho = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(
                            asofDate(), GlobalPseudoCurrencyMarketParameters::instance().get().defaultCorrelation,
                            ActualActual(ActualActual::ISDA)));
                    } else {
                        QL_FAIL("No Correlation which is needed for PseudoCurrency Volatility :" << e.what());
                    }
                }

                // build and return triangulation
                vol = Handle<BlackVolTermStructure>(
                    QuantLib::ext::make_shared<QuantExt::BlackTriangulationATMVolTermStructure>(forBaseVol, domBaseVol, rho));
            }

            DLOG("Market returning vol surface for " << pair << ".");
            volCache_[pair] = vol;
        }
        return volCache_[pair];
    }
    return fxVolImpl(pair, config);
}

Handle<YieldTermStructure> Market::discountCurve(const string& ccy, const string& config) const {
    if (!handlePseudoCurrencies_ || GlobalPseudoCurrencyMarketParameters::instance().get().treatAsFX)
        return discountCurveImpl(ccy, config);

    string baseCcy = GlobalPseudoCurrencyMarketParameters::instance().get().baseCurrency;

    if (hasPseudoCurrencyConfig(ccy)) {
        DLOG("Market::discount() requested for PM " << ccy);
        if (discountCurveCache_.find(ccy) == discountCurveCache_.end()) {
            auto baseDiscount = discountCurveImpl(baseCcy, config);
            auto priceCurve = commodityPriceCurve(commodityCurveLookup(ccy), config);
            QL_REQUIRE(!priceCurve.empty(),
                       "Failed to get Commodity Price curve for " << ccy << " using " << commodityCurveLookup(ccy));
            discountCurveCache_[ccy] =
                Handle<YieldTermStructure>(QuantLib::ext::make_shared<QuantExt::PriceTermStructureAdapter>(
                    priceCurve.currentLink(), baseDiscount.currentLink(), fxRate(ccy + baseCcy, config)));
            discountCurveCache_[ccy]->enableExtrapolation();
        }
        return discountCurveCache_[ccy];
    }

    return discountCurveImpl(ccy, config);
}

} // namespace data
} // namespace ore
