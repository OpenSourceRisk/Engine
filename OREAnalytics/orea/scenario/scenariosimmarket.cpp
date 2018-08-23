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

/*! \file scenario/scenariosimmarket.cpp
    \brief A Market class that can be updated by Scenarios
    \ingroup
*/

#include <orea/engine/observationmode.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <ql/experimental/credit/basecorrelationstructure.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/credit/interpolatedsurvivalprobabilitycurve.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolatilitystructure.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionlet.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolcube.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/indexes/inflationindexobserver.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/strippedoptionletadapter2.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/yoyinflationcurveobservermoving.hpp>
#include <qle/termstructures/zeroinflationcurveobservermoving.hpp>

#include <boost/timer.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace std;

typedef QuantLib::BaseCorrelationTermStructure<QuantLib::BilinearInterpolation> BilinearBaseCorrelationTermStructure;

namespace ore {
namespace analytics {

RiskFactorKey::KeyType yieldCurveRiskFactor(const ore::data::YieldCurveType y) {

    if (y == ore::data::YieldCurveType::Discount) {
        return RiskFactorKey::KeyType::DiscountCurve;
    } else if (y == ore::data::YieldCurveType::Yield) {
        return RiskFactorKey::KeyType::YieldCurve;
    } else if (y == ore::data::YieldCurveType::EquityDividend) {
        return RiskFactorKey::KeyType::DividendYield;
    } else if (y == ore::data::YieldCurveType::EquityForecast) {
        return RiskFactorKey::KeyType::EquityForecastCurve;
    } else {
        QL_FAIL("yieldCurveType not supported");
    }
}

namespace {
ReactionToTimeDecay parseDecayMode(const string& s) {
    static map<string, ReactionToTimeDecay> m = {{"ForwardVariance", ForwardForwardVariance},
                                                 {"ConstantVariance", ConstantVariance}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Decay mode \"" << s << "\" not recognized");
    }
}

} // namespace

void ScenarioSimMarket::addYieldCurve(const boost::shared_ptr<Market>& initMarket, const std::string& configuration,
                                      const ore::data::YieldCurveType y, const string& key,
                                      const vector<Period>& tenors, const string& dayCounter) {
    Handle<YieldTermStructure> wrapper = initMarket->yieldCurve(y, key, configuration);
    QL_REQUIRE(!wrapper.empty(), "yield curve not provided for " << key);
    QL_REQUIRE(tenors.front() > 0 * Days, "yield curve tenors must not include t=0");
    // include today

    // constructing yield curves
    DayCounter dc = ore::data::parseDayCounter(dayCounter); // used to convert YieldCurve Periods to Times
    vector<Time> yieldCurveTimes(1, 0.0);                   // include today
    vector<Date> yieldCurveDates(1, asof_);
    for (auto& tenor : tenors) {
        yieldCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
        yieldCurveDates.push_back(asof_ + tenor);
    }

    vector<Handle<Quote>> quotes;
    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
    quotes.push_back(Handle<Quote>(q));
    vector<Real> discounts(yieldCurveTimes.size());
    RiskFactorKey::KeyType rf = yieldCurveRiskFactor(y);
    for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(wrapper->discount(yieldCurveDates[i + 1])));
        Handle<Quote> qh(q);
        quotes.push_back(qh);

        // Check if the risk factor is simulated before adding it
        if (nonSimulatedFactors_.find(rf) == nonSimulatedFactors_.end()) {
            simData_.emplace(std::piecewise_construct, std::forward_as_tuple(rf, key, i), std::forward_as_tuple(q));
            LOG("ScenarioSimMarket yield curve " << key << " discount[" << i << "]=" << q->value());
        }
    }

    boost::shared_ptr<YieldTermStructure> yieldCurve;

    if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
        yieldCurve = boost::shared_ptr<YieldTermStructure>(
            new QuantExt::InterpolatedDiscountCurve(yieldCurveTimes, quotes, 0, TARGET(), dc));
    } else {
        yieldCurve = boost::shared_ptr<YieldTermStructure>(
            new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, dc));
    }

    Handle<YieldTermStructure> ych(yieldCurve);
    if (wrapper->allowsExtrapolation())
        ych->enableExtrapolation();
    yieldCurves_.insert(pair<tuple<string, ore::data::YieldCurveType, string>, Handle<YieldTermStructure>>(
        make_tuple(Market::defaultConfiguration, y, key), ych));
}

ScenarioSimMarket::ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                                     const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
                                     const Conventions& conventions, const std::string& configuration)
    : SimMarket(conventions), parameters_(parameters), filter_(boost::make_shared<ScenarioFilter>()) {

    LOG("building ScenarioSimMarket...");
    asof_ = initMarket->asofDate();
    LOG("AsOf " << QuantLib::io::iso_date(asof_));

    // Set non simulated risk factor key types
    if (!parameters->simulateSwapVols())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::SwaptionVolatility);
    if (!parameters->simulateCapFloorVols())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::OptionletVolatility);
    if (!parameters->simulateSurvivalProbabilities())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::SurvivalProbability);
    if (!parameters->simulateCdsVols())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::CDSVolatility);
    if (!parameters->simulateFXVols())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::FXVolatility);
    if (!parameters->simulateEquityVols())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::EquityVolatility);
    if (!parameters->simulateBaseCorrelations())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::BaseCorrelation);
    if (!parameters->simulateEquityForecastCurve())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::EquityForecastCurve);
    if (!parameters->simulateDividendYield())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::DividendYield);
    if (!parameters->commodityCurveSimulate())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::CommodityCurve);
    if (!parameters->commodityVolSimulate())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::CommodityVolatility);
    if (!parameters->securitySpreadsSimulate())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::SecuritySpread);
    if (!parameters->simulateFxSpots())
        nonSimulatedFactors_.insert(RiskFactorKey::KeyType::FXSpot);

    // Build fixing manager
    fixingManager_ = boost::make_shared<FixingManager>(asof_);

    // constructing fxSpots_
    LOG("building FX triangulation..");
    for (const auto& ccyPair : parameters->fxCcyPairs()) {
        LOG("adding " << ccyPair << " FX rates");
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(initMarket->fxSpot(ccyPair, configuration)->value()));
        Handle<Quote> qh(q);
        fxSpots_[Market::defaultConfiguration].addQuote(ccyPair, qh);
        // Check if the risk factor is simulated before adding it
        RiskFactorKey::KeyType rf = RiskFactorKey::KeyType::FXSpot;
        if (nonSimulatedFactors_.find(rf) == nonSimulatedFactors_.end()) {
            simData_.emplace(std::piecewise_construct, std::forward_as_tuple(rf, ccyPair),
                std::forward_as_tuple(q));
        }
    }
    LOG("FX triangulation done");

    // constructing discount yield curves
    LOG("building discount yield curves...");
    for (const auto& ccy : parameters->ccys()) {
        LOG("building " << ccy << " discount yield curve..");
        vector<Period> tenors = parameters->yieldCurveTenors(ccy);
        addYieldCurve(initMarket, configuration, ore::data::YieldCurveType::Discount, ccy, tenors,
                      parameters->yieldCurveDayCounter(ccy));
        LOG("building " << ccy << " discount yield curve done");
    }
    LOG("discount yield curves done");

    LOG("building benchmark yield curves...");
    for (const auto& name : parameters->yieldCurveNames()) {
        LOG("building benchmark yield curve name " << name);
        vector<Period> tenors = parameters->yieldCurveTenors(name);
        addYieldCurve(initMarket, configuration, ore::data::YieldCurveType::Yield, name, tenors,
                      parameters->yieldCurveDayCounter(name));
        LOG("building benchmark yield curve " << name << " done");
    }
    LOG("benchmark yield curves done");

    // building equity yield curves
    LOG("building equity curves...");
    for (const auto& eqName : parameters->equityNames()) {

        // building equity spots
        LOG("adding " << eqName << " equity spot...");
        Real spotVal = initMarket->equitySpot(eqName, configuration)->value();
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(spotVal));
        Handle<Quote> qh(q);
        equitySpots_.insert(
            pair<pair<string, string>, Handle<Quote>>(make_pair(Market::defaultConfiguration, eqName), qh));
        simData_.emplace(std::piecewise_construct, std::forward_as_tuple(RiskFactorKey::KeyType::EquitySpot, eqName),
            std::forward_as_tuple(q));
        LOG("adding " << eqName << " equity spot done");
        
        LOG("building " << eqName << " equity dividend yield curve..");
        vector<Period> divTenors = parameters->equityDividendTenors(eqName);
        addYieldCurve(initMarket, configuration, ore::data::YieldCurveType::EquityDividend, eqName, divTenors,
                      parameters->yieldCurveDayCounter(eqName));
        LOG("building " << eqName << " equity dividend yield curve done");
        LOG("building " << eqName << " equity forecast curve..");
        vector<Period> foreTenors = parameters->equityForecastTenors(eqName);
        addYieldCurve(initMarket, configuration, ore::data::YieldCurveType::EquityForecast, eqName, foreTenors,
                      parameters->yieldCurveDayCounter(eqName));
        LOG("building " << eqName << " forecast curve done");

        Handle<EquityIndex> curve = initMarket->equityCurve(eqName, configuration);

        boost::shared_ptr<EquityIndex> ei(curve->clone(equitySpot(eqName, configuration),
                                                       yieldCurve(YieldCurveType::EquityForecast, eqName, configuration),
                                                       yieldCurve(YieldCurveType::EquityDividend, eqName, configuration)));
        Handle<EquityIndex> eh(ei);
        equityCurves_.insert(
            pair<pair<string, string>, Handle<EquityIndex>>(make_pair(Market::defaultConfiguration, eqName), eh));

    }
    LOG("equity yield curves done");

    // building security spreads
    LOG("building security spreads...");
    for (const auto& name : parameters->securities()) {
        DLOG("Adding security spread " << name <<" from configuration " << configuration);
        boost::shared_ptr<SimpleQuote> spreadQuote(new SimpleQuote(initMarket->securitySpread(name, configuration)->value()));
        if (parameters->securitySpreadsSimulate()) {
            simData_.emplace(std::piecewise_construct,
                std::forward_as_tuple(RiskFactorKey::KeyType::SecuritySpread, name),
                std::forward_as_tuple(spreadQuote));
        }
        securitySpreads_.insert(pair<pair<string, string>, Handle<Quote>>(make_pair(Market::defaultConfiguration, name),
                                                                          Handle<Quote>(spreadQuote)));
    }
    LOG("security spreads done...");

    // constructing index curves
    LOG("building index curves...");
    for (const auto& ind : parameters->indices()) {
        LOG("building " << ind << " index curve");
        std::vector<string> indexTokens;
        split(indexTokens, ind, boost::is_any_of("-"));
        Handle<IborIndex> index;
        if (indexTokens[1] == "GENERIC") {
            // If we have a generic curve build the index using the index currency's discount curve
            index = Handle<IborIndex>(parseIborIndex(ind, initMarket->discountCurve(indexTokens[0], configuration)));
        } else {
            index = initMarket->iborIndex(ind, configuration);
        }  
        QL_REQUIRE(!index.empty(), "index object for " << ind << " not provided");
        Handle<YieldTermStructure> wrapperIndex = index->forwardingTermStructure();
        QL_REQUIRE(!wrapperIndex.empty(), "no termstructure for index " << ind);
        vector<string> keys(parameters->yieldCurveTenors(ind).size());

        DayCounter dc = ore::data::parseDayCounter(
            parameters->yieldCurveDayCounter(ind)); // used to convert YieldCurve Periods to Times
        vector<Time> yieldCurveTimes(1, 0.0);       // include today
        vector<Date> yieldCurveDates(1, asof_);
        QL_REQUIRE(parameters->yieldCurveTenors(ind).front() > 0 * Days, "yield curve tenors must not include t=0");
        for (auto& tenor : parameters->yieldCurveTenors(ind)) {
            yieldCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
            yieldCurveDates.push_back(asof_ + tenor);
        }

        // include today
        vector<Handle<Quote>> quotes;
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
        quotes.push_back(Handle<Quote>(q));

        for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(wrapperIndex->discount(yieldCurveDates[i + 1])));
            Handle<Quote> qh(q);
            quotes.push_back(qh);

            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::IndexCurve, ind, i),
                             std::forward_as_tuple(q));

            LOG("ScenarioSimMarket index curve " << ind << " discount[" << i << "]=" << q->value());
        }
        // FIXME interpolation fixed to linear, added to xml??
        boost::shared_ptr<YieldTermStructure> indexCurve;
        if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
            indexCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve(yieldCurveTimes, quotes, 0, index->fixingCalendar(), dc));
        } else {
            indexCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, dc));
        }

        // wrapped curve, is slower than a native curve
        // boost::shared_ptr<YieldTermStructure> correctedIndexCurve(
        //     new StaticallyCorrectedYieldTermStructure(
        //         discountCurves_[ccy], initMarket->discountCurve(ccy, configuration),
        //         wrapperIndex));

        Handle<YieldTermStructure> ich(indexCurve);
        // Handle<YieldTermStructure> ich(correctedIndexCurve);
        if (wrapperIndex->allowsExtrapolation())
            ich->enableExtrapolation();

        boost::shared_ptr<IborIndex> i(index->clone(ich));
        Handle<IborIndex> ih(i);
        iborIndices_.insert(
            pair<pair<string, string>, Handle<IborIndex>>(make_pair(Market::defaultConfiguration, ind), ih));
        LOG("building " << ind << " index curve done");
    }
    LOG("index curves done");

    // swap indices
    LOG("building swap indices...");
    for (const auto& it : parameters->swapIndices()) {
        const string& indexName = it.first;
        const string& discounting = it.second;
        LOG("Adding swap index " << indexName << " with discounting index " << discounting);

        addSwapIndex(indexName, discounting, Market::defaultConfiguration);
        LOG("Adding swap index " << indexName << " done.");
    }

    // constructing swaption volatility curves
    LOG("building swaption volatility curves...");
    for (const auto& ccy : parameters->swapVolCcys()) {
        LOG("building " << ccy << " swaption volatility curve...");
        RelinkableHandle<SwaptionVolatilityStructure> wrapper(*initMarket->swaptionVol(ccy, configuration));

        LOG("Initial market " << ccy << " swaption volatility type = " << wrapper->volatilityType());

        string shortSwapIndexBase = initMarket->shortSwapIndexBase(ccy, configuration);
        string swapIndexBase = initMarket->swapIndexBase(ccy, configuration);

        bool isCube = parameters->swapVolIsCube();

        // If swaption volatility type is not Normal, convert to Normal for the simulation
        if (wrapper->volatilityType() != Normal) {
            // FIXME we can not convert constant swaption vol structures yet
            if (boost::dynamic_pointer_cast<ConstantSwaptionVolatility>(*wrapper) != nullptr) {
                ALOG("Constant swaption volatility found in configuration " << configuration << " for currency " << ccy
                                                                            << " will not be converted to normal");
            } else {
                // Get swap index associated with this volatility structure
                string swapIndexName = initMarket->swapIndexBase(ccy, configuration);
                string shortSwapIndexName = initMarket->shortSwapIndexBase(ccy, configuration);
                Handle<SwapIndex> swapIndex = initMarket->swapIndex(swapIndexName, configuration);
                Handle<SwapIndex> shortSwapIndex = initMarket->swapIndex(shortSwapIndexName, configuration);

                // Set up swaption volatility converter
                SwaptionVolatilityConverter converter(asof_, *wrapper, *swapIndex, *shortSwapIndex, Normal);
                wrapper.linkTo(converter.convert());

                LOG("Converting swaption volatilities in configuration " << configuration << " with currency " << ccy
                                                                         << " to normal swaption volatilities");
            }
        }
        Handle<SwaptionVolatilityStructure> svp;
        if (parameters->simulateSwapVols()) {
            LOG("Simulating (" << wrapper->volatilityType() << ") Swaption vols for ccy " << ccy);
            vector<Period> optionTenors = parameters->swapVolExpiries();
            vector<Period> swapTenors = parameters->swapVolTerms();
            vector<Real> strikeSpreads = parameters->swapVolStrikeSpreads();
            bool atmOnly = parameters->simulateSwapVolATMOnly();
            if (atmOnly) {
                QL_REQUIRE(strikeSpreads.size() == 1 && close_enough(strikeSpreads[0], 0),
                           "for atmOnly strikeSpreads must be {0.0}");
            }
            boost::shared_ptr<QuantLib::SwaptionVolatilityCube> cube;
            if (isCube) {
                boost::shared_ptr<SwaptionVolCubeWithATM> tmp =
                    boost::dynamic_pointer_cast<SwaptionVolCubeWithATM>(*wrapper);
                QL_REQUIRE(tmp, "swaption cube missing")
                cube = tmp->cube();
            }
            vector<vector<Handle<Quote>>> quotes, atmQuotes;
            quotes.resize(optionTenors.size() * swapTenors.size(),
                          vector<Handle<Quote>>(strikeSpreads.size(), Handle<Quote>()));
            atmQuotes.resize(optionTenors.size(), std::vector<Handle<Quote>>(swapTenors.size(), Handle<Quote>()));
            vector<vector<Real>> shift(optionTenors.size(), vector<Real>(swapTenors.size(), 0.0));
            Size atmSlice = std::find_if(strikeSpreads.begin(), strikeSpreads.end(),
                                         [](const Real s) { return close_enough(s, 0.0); }) -
                            strikeSpreads.begin();
            QL_REQUIRE(atmSlice < strikeSpreads.size(), "could not find atm slice (strikeSpreads do not contain 0.0)");
            for (Size k = 0; k < strikeSpreads.size(); ++k) {
                for (Size i = 0; i < optionTenors.size(); ++i) {
                    for (Size j = 0; j < swapTenors.size(); ++j) {
                        Real strike =
                            atmOnly ? Null<Real>() : cube->atmStrike(optionTenors[i], swapTenors[j]) + strikeSpreads[k];
                        Real vol = wrapper->volatility(optionTenors[i], swapTenors[j], strike);
                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));

                        Size index = i * swapTenors.size() * strikeSpreads.size() + j * strikeSpreads.size() + k;

                        simData_.emplace(std::piecewise_construct,
                                         std::forward_as_tuple(RiskFactorKey::KeyType::SwaptionVolatility, ccy, index),
                                         std::forward_as_tuple(q));
                        auto tmp = Handle<Quote>(q);
                        quotes[i * swapTenors.size() + j][k] = tmp;
                        if (k == atmSlice) {
                            atmQuotes[i][j] = tmp;
                            shift[i][j] = wrapper->volatilityType() == ShiftedLognormal
                                              ? wrapper->shift(optionTenors[i], swapTenors[j])
                                              : 0.0;
                        }
                    }
                }
            }
            bool flatExtrapolation = true; // FIXME: get this from curve configuration
            VolatilityType volType = wrapper->volatilityType();
            DayCounter dc = ore::data::parseDayCounter(parameters->swapVolDayCounter(ccy));
            Handle<SwaptionVolatilityStructure> atm(boost::make_shared<SwaptionVolatilityMatrix>(
                wrapper->calendar(), wrapper->businessDayConvention(), optionTenors, swapTenors, atmQuotes, dc,
                flatExtrapolation, volType, shift));
            if (atmOnly) {
                // floating reference date matrix in sim market
                // if we have a cube, we keep the vol spreads constant under scenarios
                // notice that cube is from todaysmarket, so it has a fixed reference date, which means that
                // we keep the smiles constant in terms of vol spreads when moving forward in time;
                // notice also that the volatility will be "sticky strike", i.e. it will not react to
                // changes in the ATM level
                if (isCube) {
                    svp = Handle<SwaptionVolatilityStructure>(
                        boost::make_shared<SwaptionVolatilityConstantSpread>(atm, wrapper));
                } else {
                    svp = atm;
                }
            } else {
                QL_REQUIRE(isCube, "Only atmOnly simulation supported for swaption vol surfaces");
                boost::shared_ptr<SwaptionVolatilityCube> tmp(new QuantExt::SwaptionVolCube2(
                    atm, optionTenors, swapTenors, strikeSpreads, quotes,
                    *initMarket->swapIndex(swapIndexBase, configuration),
                    *initMarket->swapIndex(shortSwapIndexBase, configuration), false, flatExtrapolation, false));
                svp = Handle<SwaptionVolatilityStructure>(boost::make_shared<SwaptionVolCubeWithATM>(tmp));
            }

        } else {
            string decayModeString = parameters->swapVolDecayMode();
            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
            LOG("Dynamic (" << wrapper->volatilityType() << ") Swaption vols (" << decayModeString << ") for ccy "
                            << ccy);
            if (isCube)
                WLOG("Only ATM slice is considered from init market's cube");
            boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svolp =
                boost::make_shared<QuantExt::DynamicSwaptionVolatilityMatrix>(*wrapper, 0, NullCalendar(), decayMode);
            svp = Handle<SwaptionVolatilityStructure>(svolp);
        }

        svp->enableExtrapolation(); // FIXME
        swaptionCurves_.insert(pair<pair<string, string>, Handle<SwaptionVolatilityStructure>>(
            make_pair(Market::defaultConfiguration, ccy), svp));

        LOG("Simulaton market " << ccy << " swaption volatility type = " << svp->volatilityType());

        swaptionIndexBases_.insert(pair<pair<string, string>, pair<string, string>>(
            make_pair(Market::defaultConfiguration, ccy), make_pair(shortSwapIndexBase, swapIndexBase)));
        swaptionIndexBases_.insert(pair<pair<string, string>, pair<string, string>>(
            make_pair(Market::defaultConfiguration, ccy), make_pair(swapIndexBase, swapIndexBase)));
    }

    LOG("swaption volatility curves done");

    // Constructing caplet/floorlet volatility surfaces
    LOG("building cap/floor volatility curves...");
    for (const auto& ccy : parameters->capFloorVolCcys()) {
        LOG("building " << ccy << " cap/floor volatility curve...");
        Handle<OptionletVolatilityStructure> wrapper = initMarket->capFloorVol(ccy, configuration);

        LOG("Initial market cap/floor volatility type = " << wrapper->volatilityType());

        Handle<OptionletVolatilityStructure> hCapletVol;

        if (parameters->simulateCapFloorVols()) {
            LOG("Simulating Cap/Floor Optionlet vols for ccy " << ccy);
            vector<Period> optionTenors = parameters->capFloorVolExpiries(ccy);
            vector<Date> optionDates(optionTenors.size());
            vector<Real> strikes = parameters->capFloorVolStrikes();
            vector<vector<Handle<Quote>>> quotes(optionTenors.size(),
                                                 vector<Handle<Quote>>(strikes.size(), Handle<Quote>()));
            for (Size i = 0; i < optionTenors.size(); ++i) {
                optionDates[i] = wrapper->optionDateFromTenor(optionTenors[i]);
                for (Size j = 0; j < strikes.size(); ++j) {
                    Real vol = wrapper->volatility(optionTenors[i], strikes[j], wrapper->allowsExtrapolation());
                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                    Size index = i * strikes.size() + j;
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::OptionletVolatility, ccy, index),
                                     std::forward_as_tuple(q));
                    quotes[i][j] = Handle<Quote>(q);
                }
            }
            DayCounter dc = ore::data::parseDayCounter(parameters->capFloorVolDayCounter(ccy));
            // FIXME: Works as of today only, i.e. for sensitivity/scenario analysis.
            // TODO: Build floating reference date StrippedOptionlet class for MC path generators
            boost::shared_ptr<StrippedOptionlet> optionlet = boost::make_shared<StrippedOptionlet>(
                0, // FIXME: settlement days
                wrapper->calendar(), wrapper->businessDayConvention(),
                boost::shared_ptr<IborIndex>(), // FIXME: required for ATM vol calculation
                optionDates, strikes, quotes, dc, wrapper->volatilityType(), wrapper->displacement());
            boost::shared_ptr<StrippedOptionletAdapter2> adapter =
                boost::make_shared<StrippedOptionletAdapter2>(optionlet);
            hCapletVol = Handle<OptionletVolatilityStructure>(adapter);
        } else {
            string decayModeString = parameters->capFloorVolDecayMode();
            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
            boost::shared_ptr<OptionletVolatilityStructure> capletVol =
                boost::make_shared<DynamicOptionletVolatilityStructure>(*wrapper, 0, NullCalendar(), decayMode);
            hCapletVol = Handle<OptionletVolatilityStructure>(capletVol);
        }

        hCapletVol->enableExtrapolation(); // FIXME
        capFloorCurves_.emplace(std::piecewise_construct, std::forward_as_tuple(Market::defaultConfiguration, ccy),
                                std::forward_as_tuple(hCapletVol));

        LOG("Simulaton market cap/floor volatility type = " << hCapletVol->volatilityType());
    }

    LOG("cap/floor volatility curves done");

    // building default curves
    LOG("building default curves...");
    for (const auto& name : parameters->defaultNames()) {
        LOG("building " << name << " default curve..");
        Handle<DefaultProbabilityTermStructure> wrapper = initMarket->defaultCurve(name, configuration);
        vector<Handle<Quote>> quotes;

        QL_REQUIRE(parameters->defaultTenors(name).front() > 0 * Days, "default curve tenors must not include t=0");

        vector<Date> dates(1, asof_);

        for (Size i = 0; i < parameters->defaultTenors(name).size(); i++) {
            dates.push_back(asof_ + parameters->defaultTenors(name)[i]);
        }

        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
        quotes.push_back(Handle<Quote>(q));
        for (Size i = 0; i < dates.size() - 1; i++) {
            Probability prob = wrapper->survivalProbability(dates[i + 1], true);
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(prob));
            if (parameters->simulateSurvivalProbabilities()) {
                simData_.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(RiskFactorKey::KeyType::SurvivalProbability, name, i),
                                 std::forward_as_tuple(q));
            }
            Handle<Quote> qh(q);
            quotes.push_back(qh);
        }
        DayCounter dc = ore::data::parseDayCounter(parameters->defaultCurveDayCounter(name));
        Calendar cal = ore::data::parseCalendar(parameters->defaultCurveCalendar(name));
        // FIXME riskmarket uses SurvivalProbabilityCurve but this isn't added to ore
        boost::shared_ptr<DefaultProbabilityTermStructure> defaultCurve(
            new QuantExt::SurvivalProbabilityCurve<Linear>(dates, quotes, dc, cal));
        Handle<DefaultProbabilityTermStructure> dch(defaultCurve);

        dch->enableExtrapolation();

        defaultCurves_.insert(pair<pair<string, string>, Handle<DefaultProbabilityTermStructure>>(
            make_pair(Market::defaultConfiguration, name), dch));

        // add recovery rate
        boost::shared_ptr<SimpleQuote> rrQuote(new SimpleQuote(initMarket->recoveryRate(name, configuration)->value()));
        if (parameters->simulateRecoveryRates()) {
            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                             std::forward_as_tuple(rrQuote));
        }
        recoveryRates_.insert(pair<pair<string, string>, Handle<Quote>>(make_pair(Market::defaultConfiguration, name),
                                                                        Handle<Quote>(rrQuote)));
    }
    LOG("default curves done");

    // building cds volatilities
    LOG("building cds volatilities...");
    for (const auto& name : parameters->cdsVolNames()) {
        LOG("building " << name << "  cds vols..");
        Handle<BlackVolTermStructure> wrapper = initMarket->cdsVol(name, configuration);
        Handle<BlackVolTermStructure> cvh;
        if (parameters->simulateCdsVols()) {
            LOG("Simulating CDS Vols for " << name);
            vector<Handle<Quote>> quotes;
            vector<Time> times;
            for (Size i = 0; i < parameters->cdsVolExpiries().size(); i++) {
                Date date = asof_ + parameters->cdsVolExpiries()[i];
                Volatility vol = wrapper->blackVol(date, Null<Real>(), true);
                times.push_back(wrapper->timeFromReference(date));
                boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                if (parameters->simulateCdsVols()) {
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::CDSVolatility, name, i),
                                     std::forward_as_tuple(q));
                }
                quotes.emplace_back(q);
            }

            DayCounter dc = ore::data::parseDayCounter(parameters->cdsVolDayCounter(name));
            boost::shared_ptr<BlackVolTermStructure> cdsVolCurve(
                new BlackVarianceCurve3(0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes));

            cvh = Handle<BlackVolTermStructure>(cdsVolCurve);
        } else {
            string decayModeString = parameters->cdsVolDecayMode();
            LOG("Deterministic CDS Vols with decay mode " << decayModeString << " for " << name);
            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

            // currently only curves (i.e. strike indepdendent) CDS volatility structures are
            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
            // strike stickyness, since then no yield term structures and no fx spot are required
            // that define the ATM level
            cvh = Handle<BlackVolTermStructure>(boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                wrapper, 0, NullCalendar(), decayMode, StickyStrike));
        }

        if (wrapper->allowsExtrapolation())
            cvh->enableExtrapolation();
        cdsVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
            make_pair(Market::defaultConfiguration, name), cvh));
    }
    LOG("cds volatilities done");
    // building fx volatilities
    LOG("building fx volatilities...");
    for (const auto& ccyPair : parameters->fxVolCcyPairs()) {
        Handle<BlackVolTermStructure> wrapper = initMarket->fxVol(ccyPair, configuration);
        Handle<Quote> spot = fxSpot(ccyPair);
        QL_REQUIRE(ccyPair.length() == 6, "invalid ccy pair length");
        string forCcy = ccyPair.substr(0, 3);
        string domCcy = ccyPair.substr(3, 3);
        Handle<YieldTermStructure> forTS = discountCurve(forCcy);
        Handle<YieldTermStructure> domTS = discountCurve(domCcy);
        Handle<BlackVolTermStructure> fvh;

        if (parameters->simulateFXVols()) {
            LOG("Simulating FX Vols (BlackVarianceCurve3) for " << ccyPair);
            Size n = parameters->fxVolExpiries().size();
            Size m = parameters->fxVolMoneyness().size();
            vector<vector<Handle<Quote>>> quotes(m, vector<Handle<Quote>>(n, Handle<Quote>()));
            Calendar cal = wrapper->calendar();
            // FIXME hardcoded in todaysmarket
            DayCounter dc = ore::data::parseDayCounter(parameters->fxVolDayCounter(ccyPair));
            vector<Time> times;

            for (Size i = 0; i < n; i++) {
                Date date = asof_ + parameters->fxVolExpiries()[i];

                times.push_back(wrapper->timeFromReference(date));

                for (Size j = 0; j < m; j++) {
                    Size idx = j * n + i;
                    Real mon = parameters->fxVolMoneyness()[j]; // 0 if ATM

                    // strike (assuming forward prices)
                    Real k = spot->value() * mon * forTS->discount(date) / domTS->discount(date);
                    Volatility vol = wrapper->blackVol(date, k, true);
                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::FXVolatility, ccyPair, idx),
                                     std::forward_as_tuple(q));
                    quotes[j][i] = Handle<Quote>(q);
                }
            }

            boost::shared_ptr<BlackVolTermStructure> fxVolCurve;
            if (parameters->fxVolIsSurface()) {
                bool stickyStrike = true;
                fxVolCurve = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceSurfaceMoneynessForward(
                    cal, spot, times, parameters->fxVolMoneyness(), quotes, dc, forTS, domTS, stickyStrike));
            } else {
                fxVolCurve = boost::shared_ptr<BlackVolTermStructure>(
                    new BlackVarianceCurve3(0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes[0]));
            }
            fvh = Handle<BlackVolTermStructure>(fxVolCurve);

        } else {
            string decayModeString = parameters->fxVolDecayMode();
            LOG("Deterministic FX Vols with decay mode " << decayModeString << " for " << ccyPair);
            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

            // currently only curves (i.e. strike indepdendent) FX volatility structures are
            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
            // strike stickyness, since then no yield term structures and no fx spot are required
            // that define the ATM level - to be revisited when FX surfaces are supported
            fvh = Handle<BlackVolTermStructure>(boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                wrapper, 0, NullCalendar(), decayMode, StickyStrike));
        }

        fvh->enableExtrapolation();
        fxVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
            make_pair(Market::defaultConfiguration, ccyPair), fvh));

        // build inverted surface
        QL_REQUIRE(ccyPair.size() == 6, "Invalid Ccy pair " << ccyPair);
        string reverse = ccyPair.substr(3) + ccyPair.substr(0, 3);
        Handle<QuantLib::BlackVolTermStructure> ifvh(boost::make_shared<BlackInvertedVolTermStructure>(fvh));
        ifvh->enableExtrapolation();
        fxVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
            make_pair(Market::defaultConfiguration, reverse), ifvh));
    }
    LOG("fx volatilities done");

    // building eq volatilities
    LOG("building eq volatilities...");
    for (const auto& equityName : parameters->equityVolNames()) {
        Handle<BlackVolTermStructure> wrapper = initMarket->equityVol(equityName, configuration);

        Handle<BlackVolTermStructure> evh;

        if (parameters->simulateEquityVols()) {
            Handle<Quote> spot = equitySpots_[make_pair(Market::defaultConfiguration, equityName)];
            Size n = parameters->equityVolMoneyness().size();
            Size m = parameters->equityVolExpiries().size();
            vector<vector<Handle<Quote>>> quotes(n, vector<Handle<Quote>>(m, Handle<Quote>()));
            vector<Time> times(m);
            Calendar cal = wrapper->calendar();
            DayCounter dc = ore::data::parseDayCounter(parameters->equityVolDayCounter(equityName));
            bool atmOnly = parameters->simulateEquityVolATMOnly();

            for (Size i = 0; i < n; i++) {
                Real mon = parameters->equityVolMoneyness()[i];
                // strike
                Real k = atmOnly ? Null<Real>() : spot->value() * mon;

                for (Size j = 0; j < m; j++) {
                    // Index is expires then moneyness. TODO: is this the best?
                    Size idx = i * m + j;
                    times[j] = dc.yearFraction(asof_, asof_ + parameters->equityVolExpiries()[j]);
                    Volatility vol = wrapper->blackVol(asof_ + parameters->equityVolExpiries()[j], k);
                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::EquityVolatility, equityName, idx),
                                     std::forward_as_tuple(q));
                    quotes[i][j] = Handle<Quote>(q);
                }
            }
            boost::shared_ptr<BlackVolTermStructure> eqVolCurve;
            if (!parameters->simulateEquityVolATMOnly()) {
                LOG("Simulating EQ Vols (BlackVarianceSurfaceMoneyness) for " << equityName);
                // If true, the strikes are fixed, if false they move with the spot handle
                // Should probably be false, but some people like true for sensi runs.
                bool stickyStrike = true;

                eqVolCurve = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceSurfaceMoneynessSpot(
                    cal, spot, times, parameters->equityVolMoneyness(), quotes, dc, stickyStrike));
                eqVolCurve->enableExtrapolation();
            } else {
                LOG("Simulating EQ Vols (BlackVarianceCurve3) for " << equityName);
                eqVolCurve = boost::shared_ptr<BlackVolTermStructure>(
                    new BlackVarianceCurve3(0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes[0]));
            }

            // if we have a surface but are only simulating atm vols we wrap the atm curve and the full t0 surface
            if (parameters->equityVolIsSurface() && parameters->simulateEquityVolATMOnly()) {
                LOG("Simulating EQ Vols (EquityVolatilityConstantSpread) for " << equityName);
                evh = Handle<BlackVolTermStructure>(boost::make_shared<EquityVolatilityConstantSpread>(
                    Handle<BlackVolTermStructure>(eqVolCurve), wrapper));
            } else {
                evh = Handle<BlackVolTermStructure>(eqVolCurve);
            }
        } else {
            string decayModeString = parameters->equityVolDecayMode();
            DLOG("Deterministic EQ Vols with decay mode " << decayModeString << " for " << equityName);
            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

            // currently only curves (i.e. strike indepdendent) EQ volatility structures are
            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
            // strike stickyness, since then no yield term structures and no EQ spot are required
            // that define the ATM level - to be revisited when EQ surfaces are supported
            evh = Handle<BlackVolTermStructure>(boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                wrapper, 0, NullCalendar(), decayMode, StickyStrike));
        }
        if (wrapper->allowsExtrapolation())
            evh->enableExtrapolation();
        equityVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
            make_pair(Market::defaultConfiguration, equityName), evh));
        DLOG("EQ volatility curve built for " << equityName);
    }
    LOG("equity volatilities done");

    // building base correlation structures
    LOG("building base correlations...");
    for (const auto& bcName : parameters->baseCorrelationNames()) {
        Handle<BaseCorrelationTermStructure<BilinearInterpolation>> wrapper =
            initMarket->baseCorrelation(bcName, configuration);
        if (!parameters->simulateBaseCorrelations())
            baseCorrelations_.insert(
                pair<pair<string, string>, Handle<BaseCorrelationTermStructure<BilinearInterpolation>>>(
                    make_pair(Market::defaultConfiguration, bcName), wrapper));
        else {
            Size nd = parameters->baseCorrelationDetachmentPoints().size();
            Size nt = parameters->baseCorrelationTerms().size();
            vector<vector<Handle<Quote>>> quotes(nd, vector<Handle<Quote>>(nt));
            vector<Period> terms(nt);
            for (Size i = 0; i < nd; ++i) {
                Real lossLevel = parameters->baseCorrelationDetachmentPoints()[i];
                for (Size j = 0; j < nt; ++j) {
                    Period term = parameters->baseCorrelationTerms()[j];
                    if (i == 0)
                        terms[j] = term;
                    Real bc = wrapper->correlation(asof_ + term, lossLevel, true); // extrapolate
                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(bc));
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::BaseCorrelation, bcName, i * nt + j),
                                     std::forward_as_tuple(q));
                    quotes[i][j] = Handle<Quote>(q);
                }
            }

            // FIXME: Same change as in ored/market/basecorrelationcurve.cpp
            if (nt == 1) {
                terms.push_back(terms[0] + 1 * Days); // arbitrary, but larger than the first term
                for (Size i = 0; i < nd; ++i)
                    quotes[i].push_back(quotes[i][0]);
            }
            DayCounter dc = ore::data::parseDayCounter(parameters->baseCorrelationDayCounter(bcName));
            boost::shared_ptr<BilinearBaseCorrelationTermStructure> bcp =
                boost::make_shared<BilinearBaseCorrelationTermStructure>(
                    wrapper->settlementDays(), wrapper->calendar(), wrapper->businessDayConvention(), terms,
                    parameters->baseCorrelationDetachmentPoints(), quotes, dc);

            bcp->enableExtrapolation(wrapper->allowsExtrapolation());
            Handle<BilinearBaseCorrelationTermStructure> bch(bcp);
            baseCorrelations_.insert(
                pair<pair<string, string>, Handle<BaseCorrelationTermStructure<BilinearInterpolation>>>(
                    make_pair(Market::defaultConfiguration, bcName), bch));
        }
        DLOG("Base correlations built for " << bcName);
    }
    LOG("base correlations done");

    LOG("building CPI Indices...");
    for (const auto& ci : parameters->cpiIndices()) {

        DLOG("adding " << ci << " base CPI price");
        Handle<ZeroInflationIndex> zeroInflationIndex = initMarket->zeroInflationIndex(ci, configuration);
        Period obsLag = zeroInflationIndex->zeroInflationTermStructure()->observationLag();
        Date fixingDate = zeroInflationIndex->zeroInflationTermStructure()->baseDate();
        Real baseCPI = zeroInflationIndex->fixing(fixingDate);

        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(baseCPI));
        Handle<Quote> qh(q);

        boost::shared_ptr<InflationIndex> inflationIndex =
            boost::dynamic_pointer_cast<InflationIndex>(*zeroInflationIndex);
        Handle<InflationIndexObserver> inflObserver(
            boost::make_shared<InflationIndexObserver>(inflationIndex, qh, obsLag));

        baseCpis_.insert(pair<pair<string, string>, Handle<InflationIndexObserver>>(
            make_pair(Market::defaultConfiguration, ci), inflObserver));
        simData_.emplace(std::piecewise_construct, std::forward_as_tuple(RiskFactorKey::KeyType::CPIIndex, ci),
                         std::forward_as_tuple(q));
    }
    LOG("CPI Indices done");

    LOG("building zero inflation curves...");
    for (const auto& zic : parameters->zeroInflationIndices()) {
        LOG("building " << zic << " zero inflation curve");

        Handle<ZeroInflationIndex> inflationIndex = initMarket->zeroInflationIndex(zic, configuration);
        Handle<ZeroInflationTermStructure> inflationTs = inflationIndex->zeroInflationTermStructure();
        vector<string> keys(parameters->zeroInflationTenors(zic).size());

        string ccy = inflationIndex->currency().code();
        Handle<YieldTermStructure> yts = discountCurve(ccy, configuration);

        Date date0 = asof_ - inflationTs->observationLag();
        DayCounter dc = ore::data::parseDayCounter(parameters->zeroInflationDayCounter(zic));
        vector<Date> quoteDates;
        vector<Time> zeroCurveTimes(1, -dc.yearFraction(inflationPeriod(date0, inflationTs->frequency()).first, asof_));
        vector<Handle<Quote>> quotes;
        QL_REQUIRE(parameters->zeroInflationTenors(zic).front() > 0 * Days,
                   "zero inflation tenors must not include t=0");

        for (auto& tenor : parameters->zeroInflationTenors(zic)) {
            Date inflDate = inflationPeriod(date0 + tenor, inflationTs->frequency()).first;
            zeroCurveTimes.push_back(dc.yearFraction(asof_, inflDate));
            quoteDates.push_back(asof_ + tenor);
        }

        for (Size i = 1; i < zeroCurveTimes.size(); i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(inflationTs->zeroRate(quoteDates[i - 1])));
            Handle<Quote> qh(q);
            if (i == 1) {
                // add the zero rate at first tenor to the T0 time, to ensure flat interpolation of T1 rate
                // for time t T0 < t < T1
                quotes.push_back(qh);
            }
            quotes.push_back(qh);
            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::ZeroInflationCurve, zic, i - 1),
                             std::forward_as_tuple(q));
            LOG("ScenarioSimMarket index curve " << zic << " zeroRate[" << i << "]=" << q->value());
        }

        // FIXME: Settlement days set to zero - needed for floating term structure implementation
        boost::shared_ptr<ZeroInflationTermStructure> zeroCurve;
        dc = ore::data::parseDayCounter(parameters->zeroInflationDayCounter(zic));
        zeroCurve =
            boost::shared_ptr<ZeroInflationCurveObserverMoving<Linear>>(new ZeroInflationCurveObserverMoving<Linear>(
                0, inflationIndex->fixingCalendar(), dc, inflationTs->observationLag(), inflationTs->frequency(),
                inflationTs->indexIsInterpolated(), yts, zeroCurveTimes, quotes, inflationTs->seasonality()));

        Handle<ZeroInflationTermStructure> its(zeroCurve);
        its->enableExtrapolation();
        boost::shared_ptr<ZeroInflationIndex> i =
            ore::data::parseZeroInflationIndex(zic, false, Handle<ZeroInflationTermStructure>(its));
        Handle<ZeroInflationIndex> zh(i);
        zeroInflationIndices_.insert(
            pair<pair<string, string>, Handle<ZeroInflationIndex>>(make_pair(Market::defaultConfiguration, zic), zh));

        LOG("building " << zic << " zero inflation curve done");
    }
    LOG("zero inflation curves done");

    LOG("building yoy inflation curves...");
    for (const auto& yic : parameters->yoyInflationIndices()) {

        Handle<YoYInflationIndex> yoyInflationIndex = initMarket->yoyInflationIndex(yic, configuration);
        Handle<YoYInflationTermStructure> yoyInflationTs = yoyInflationIndex->yoyInflationTermStructure();
        vector<string> keys(parameters->yoyInflationTenors(yic).size());

        string ccy = yoyInflationIndex->currency().code();
        Handle<YieldTermStructure> yts = initMarket->discountCurve(ccy, configuration);

        Date date0 = asof_ - yoyInflationTs->observationLag();
        DayCounter dc = ore::data::parseDayCounter(parameters->yoyInflationDayCounter(yic));
        vector<Date> quoteDates;
        vector<Time> yoyCurveTimes(1,
                                   -dc.yearFraction(inflationPeriod(date0, yoyInflationTs->frequency()).first, asof_));
        vector<Handle<Quote>> quotes;
        QL_REQUIRE(parameters->yoyInflationTenors(yic).front() > 0 * Days,
                   "zero inflation tenors must not include t=0");

        for (auto& tenor : parameters->yoyInflationTenors(yic)) {
            Date inflDate = inflationPeriod(date0 + tenor, yoyInflationTs->frequency()).first;
            yoyCurveTimes.push_back(dc.yearFraction(asof_, inflDate));
            quoteDates.push_back(asof_ + tenor);
        }

        for (Size i = 1; i < yoyCurveTimes.size(); i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(yoyInflationTs->yoyRate(quoteDates[i - 1])));
            Handle<Quote> qh(q);
            if (i == 1) {
                // add the zero rate at first tenor to the T0 time, to ensure flat interpolation of T1 rate
                // for time t T0 < t < T1
                quotes.push_back(qh);
            }
            quotes.push_back(qh);
            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::YoYInflationCurve, yic, i - 1),
                             std::forward_as_tuple(q));
            LOG("ScenarioSimMarket index curve " << yic << " zeroRate[" << i << "]=" << q->value());
        }

        boost::shared_ptr<YoYInflationTermStructure> yoyCurve;
        // Note this is *not* a floating term structure, it is only suitable for sensi runs
        // TODO: floating
        yoyCurve =
            boost::shared_ptr<YoYInflationCurveObserverMoving<Linear>>(new YoYInflationCurveObserverMoving<Linear>(
                0, yoyInflationIndex->fixingCalendar(), dc, yoyInflationTs->observationLag(),
                yoyInflationTs->frequency(), yoyInflationTs->indexIsInterpolated(), yts, yoyCurveTimes, quotes,
                yoyInflationTs->seasonality()));

        Handle<YoYInflationTermStructure> its(yoyCurve);
        its->enableExtrapolation();
        boost::shared_ptr<YoYInflationIndex> i(yoyInflationIndex->clone(its));
        Handle<YoYInflationIndex> zh(i);
        yoyInflationIndices_.insert(
            pair<pair<string, string>, Handle<YoYInflationIndex>>(make_pair(Market::defaultConfiguration, yic), zh));
    }
    LOG("yoy inflation curves done");
    
    LOG("building commodity spots");
    for (const auto& name : parameters->commodityNames()) {
        Real spot = initMarket->commoditySpot(name, configuration)->value();
        DLOG("adding " << name << " commodity spot price");
        boost::shared_ptr<SimpleQuote> q = boost::make_shared<SimpleQuote>(spot);
        commoditySpots_.emplace(piecewise_construct, 
            forward_as_tuple(Market::defaultConfiguration, name), forward_as_tuple(q));
        simData_.emplace(piecewise_construct, 
            forward_as_tuple(RiskFactorKey::KeyType::CommoditySpot, name), forward_as_tuple(q));
    }
    LOG("commodity spots done");

    LOG("building commodity curves");
    for (const string& name : parameters->commodityNames()) {
        
        LOG("building commodity curve for " << name);
        
        // Time zero initial market commodity curve
        Handle<PriceTermStructure> initialCommodityCurve = 
            initMarket->commodityPriceCurve(name, configuration);
        bool allowsExtrapolation = initialCommodityCurve->allowsExtrapolation();

        // Get prices at specified simulation tenors from time 0 market curve and place in quotes
        vector<Period> simulationTenors = parameters->commodityCurveTenors(name);
        DayCounter commodityCurveDayCounter = parseDayCounter(parameters->commodityCurveDayCounter(name));
        vector<Time> times(simulationTenors.size());
        vector<Handle<Quote>> quotes(simulationTenors.size());

        for (Size i = 0; i < simulationTenors.size(); i++) {
            times[i] = commodityCurveDayCounter.yearFraction(asof_, asof_ + simulationTenors[i]);
            Real price = initialCommodityCurve->price(times[i], allowsExtrapolation);
            boost::shared_ptr<SimpleQuote> quote = boost::make_shared<SimpleQuote>(price);
            quotes[i] = Handle<Quote>(quote);
            
            // If we are simulating commodities, add the quote to simData_
            if (parameters->commodityCurveSimulate()) {
                simData_.emplace(piecewise_construct,
                    forward_as_tuple(RiskFactorKey::KeyType::CommodityCurve, name, i),
                    forward_as_tuple(quote));
            }
        }

        // Create a commodity price curve with simulation tenors as pillars and store
        // Hard-coded linear interpolation here - may need to make this more dynamic
        Handle<PriceTermStructure> simCommodityCurve(boost::make_shared<InterpolatedPriceCurve<Linear>>(
            times, quotes, commodityCurveDayCounter));
        simCommodityCurve->enableExtrapolation(allowsExtrapolation);

        commodityCurves_.emplace(piecewise_construct,
            forward_as_tuple(Market::defaultConfiguration, name),
            forward_as_tuple(simCommodityCurve));
    }
    LOG("commodity curves done");

    LOG("building commodity volatilities");
    for (const auto& name : parameters->commodityVolNames()) {
        // Get initial base volatility structure
        Handle<BlackVolTermStructure> baseVol = initMarket->commodityVolatility(name, configuration);

        Handle<BlackVolTermStructure> newVol;
        if (parameters->commodityVolSimulate()) {
            Handle<Quote> spot = commoditySpot(name, configuration);
            const vector<Real>& moneyness = parameters->commodityVolMoneyness(name);
            QL_REQUIRE(!moneyness.empty(), "Commodity volatility moneyness for " << name << " should have at least one element");
            const vector<Period>& expiries = parameters->commodityVolExpiries(name);
            QL_REQUIRE(!expiries.empty(), "Commodity volatility expiries for " << name << " should have at least one element");

            // Create surface of quotes
            vector<vector<Handle<Quote>>> quotes(moneyness.size(), vector<Handle<Quote>>(expiries.size()));
            vector<Time> expiryTimes(expiries.size());
            Size index = 0;
            DayCounter dayCounter = baseVol->dayCounter();

            for (Size i = 0; i < quotes.size(); i++) {
                Real strike = moneyness[i] * spot->value();
                for (Size j = 0; j < quotes[0].size(); j++) {
                    if (i == 0) expiryTimes[j] = dayCounter.yearFraction(asof_, asof_ + expiries[j]);
                    boost::shared_ptr<SimpleQuote> quote = boost::make_shared<SimpleQuote>(
                        baseVol->blackVol(asof_ + expiries[j], strike));
                    simData_.emplace(piecewise_construct, 
                        forward_as_tuple(RiskFactorKey::KeyType::CommodityVolatility, name, index++),
                        forward_as_tuple(quote));
                    quotes[i][j] = Handle<Quote>(quote);
                }
            }

            // Create volatility structure
            if (moneyness.size() == 1) {
                // We have a term structure of volatilities with no strike dependence
                LOG("Simulating commodity volatilites for " << name << " using BlackVarianceCurve3.");
                newVol = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceCurve3>(
                    0, NullCalendar(), baseVol->businessDayConvention(), dayCounter, expiryTimes, quotes[0]));
            } else {
                // We have a volatility surface
                LOG("Simulating commodity volatilites for " << name << " using BlackVarianceSurfaceMoneynessSpot.");
                bool stickyStrike = true;
                newVol = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceSurfaceMoneynessSpot>(
                    baseVol->calendar(), spot, expiryTimes, moneyness, quotes, dayCounter, stickyStrike));
            }

        } else {
            string decayModeString = parameters->equityVolDecayMode();
            DLOG("Deterministic commodity volatilities with decay mode " << decayModeString << " for " << name);
            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
            // Copy what was done for equity here
            // May need to revisit when looking at commodity RFE
            newVol = Handle<BlackVolTermStructure>(boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                baseVol, 0, NullCalendar(), decayMode, StickyStrike));
        }

        newVol->enableExtrapolation(baseVol->allowsExtrapolation());

        commodityVols_.emplace(piecewise_construct, forward_as_tuple(Market::defaultConfiguration, name),
            forward_as_tuple(newVol));
        
        DLOG("Commodity volatility curve built for " << name);
    }
    LOG("commodity volatilities done");

    LOG("building base scenario");
    baseScenario_ = boost::make_shared<SimpleScenario>(initMarket->asofDate(), "BASE", 1.0);
    for (auto const& data : simData_) {
        baseScenario_->add(data.first, data.second->value());
    }
    LOG("building base scenario done");
}

void ScenarioSimMarket::applyScenario(const boost::shared_ptr<Scenario>& scenario) {
    const vector<RiskFactorKey>& keys = scenario->keys();

    Size count = 0;
    for (const auto& key : keys) {
        // Loop through the scenario keys and check which keys are present in simData_,
        // adding to the count when a match is identified
        // Then check that the count=simData_.size - this ensures that simData_ is a valid
        // subset of the scenario - fails is a member of simData is not present in the 
        // scenario
        auto it = simData_.find(key);
        if (it == simData_.end()) {
            ALOG("simulation data point missing for key " << key);
        } else {
            if (filter_->allow(key))
                it->second->setValue(scenario->get(key));
            count++;
        }
    }

    if (count != simData_.size()) {
        ALOG("mismatch between scenario and sim data size, " << count << " vs " << simData_.size());
        for (auto it : simData_) {
            if (!scenario->has(it.first))
                ALOG("Key " << it.first << " missing in scenario");
        }
        QL_FAIL("mismatch between scenario and sim data size, exit.");
    }

    // update market asof date
    asof_ = scenario->asof();
}

void ScenarioSimMarket::reset() {
    auto filterBackup = filter_;
    // no filter
    filter_ = boost::make_shared<ScenarioFilter>();
    // reset eval date
    Settings::instance().evaluationDate() = baseScenario_->asof();
    // reset numeraire
    numeraire_ = baseScenario_->getNumeraire();
    // reset term structures
    applyScenario(baseScenario_);
    // see the comment in update() for why this is necessary...
    if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
        boost::shared_ptr<QuantLib::Observable> obs = QuantLib::Settings::instance().evaluationDate();
        obs->notifyObservers();
    }
    // reset fixing manager
    fixingManager_->reset();
    // restore the filter
    filter_ = filterBackup;
}

void ScenarioSimMarket::update(const Date& d) {
    // DLOG("ScenarioSimMarket::update called with Date " << QuantLib::io::iso_date(d));
    QL_REQUIRE(scenarioGenerator_ != nullptr, "ScenarioSimMarket::update: no scenario generator set");

    ObservationMode::Mode om = ObservationMode::instance().mode();
    if (om == ObservationMode::Mode::Disable)
        ObservableSettings::instance().disableUpdates(false);
    else if (om == ObservationMode::Mode::Defer)
        ObservableSettings::instance().disableUpdates(true);

    boost::shared_ptr<Scenario> scenario = scenarioGenerator_->next(d);
    QL_REQUIRE(scenario->asof() == d, "Invalid Scenario date " << scenario->asof() << ", expected " << d);

    numeraire_ = scenario->getNumeraire();

    if (d != Settings::instance().evaluationDate())
        Settings::instance().evaluationDate() = d;
    else if (om == ObservationMode::Mode::Unregister) {
        // Due to some of the notification chains having been unregistered,
        // it is possible that some lazy objects might be missed in the case
        // that the evaluation date has not been updated. Therefore, we
        // manually kick off an observer notification from this level.
        // We have unit regression tests in OREAnalyticsTestSuite to ensure
        // the various ObservationMode settings return the anticipated results.
        boost::shared_ptr<QuantLib::Observable> obs = QuantLib::Settings::instance().evaluationDate();
        obs->notifyObservers();
    }

    applyScenario(scenario);

    // Observation Mode - key to update these before fixings are set
    if (om == ObservationMode::Mode::Disable) {
        refresh();
        ObservableSettings::instance().enableUpdates();
    } else if (om == ObservationMode::Mode::Defer) {
        ObservableSettings::instance().enableUpdates();
    }

    // Apply fixings as historical fixings. Must do this before we populate ASD
    fixingManager_->update(d);

    if (asd_) {
        // add additional scenario data to the given container, if required
        for (auto i : parameters_->additionalScenarioDataIndices()) {
            boost::shared_ptr<QuantLib::Index> index;
            try {
                index = *iborIndex(i);
            } catch (...) {
            }
            try {
                index = *swapIndex(i);
            } catch (...) {
            }
            QL_REQUIRE(index != nullptr, "ScenarioSimMarket::update() index " << i << " not found in sim market");
            asd_->set(index->fixing(d), AggregationScenarioDataType::IndexFixing, i);
        }

        for (auto c : parameters_->additionalScenarioDataCcys()) {
            if (c != parameters_->baseCcy())
                asd_->set(fxSpot(c + parameters_->baseCcy())->value(), AggregationScenarioDataType::FXSpot, c);
        }

        asd_->set(numeraire_, AggregationScenarioDataType::Numeraire);

        asd_->next();
    }

    // DLOG("ScenarioSimMarket::update done");
}

bool ScenarioSimMarket::isSimulated(const RiskFactorKey::KeyType& factor) const {
    return std::find(nonSimulatedFactors_.begin(), nonSimulatedFactors_.end(), factor) == nonSimulatedFactors_.end();
}

} // namespace analytics
} // namespace ore
