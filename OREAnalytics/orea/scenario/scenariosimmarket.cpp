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
#include <ql/time/daycounters/actualactual.hpp>

#include <ql/experimental/credit/basecorrelationstructure.hpp>

#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>
#include <qle/termstructures/strippedoptionletadapter2.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/zeroinflationcurveobserver.hpp>
#include <qle/termstructures/yoyinflationcurveobserver.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

#include <boost/timer.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

typedef QuantLib::BaseCorrelationTermStructure<QuantLib::BilinearInterpolation> BilinearBaseCorrelationTermStructure;

namespace ore {
namespace analytics {

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

ScenarioSimMarket::ScenarioSimMarket(boost::shared_ptr<ScenarioGenerator>& scenarioGenerator,
                                     boost::shared_ptr<Market>& initMarket,
                                     boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
                                     Conventions conventions, const std::string& configuration)
    : SimMarket(conventions), scenarioGenerator_(scenarioGenerator), parameters_(parameters) {

    LOG("building ScenarioSimMarket...");
    asof_ = initMarket->asofDate();
    LOG("AsOf " << QuantLib::io::iso_date(asof_));

    // Build fixing manager
    fixingManager_ = boost::make_shared<FixingManager>(asof_);

    // constructing fxSpots_
    LOG("building FX triangulation..");
    for (const auto& ccyPair : parameters->fxCcyPairs()) {
        LOG("adding " << ccyPair << " FX rates");
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(initMarket->fxSpot(ccyPair, configuration)->value()));
        Handle<Quote> qh(q);
        fxSpots_[Market::defaultConfiguration].addQuote(ccyPair, qh);
        simData_.emplace(std::piecewise_construct, std::forward_as_tuple(RiskFactorKey::KeyType::FXSpot, ccyPair),
                         std::forward_as_tuple(q));
    }
    LOG("FX triangulation done");

    // constructing discount yield curves
    LOG("building discount yield curves...");
    for (const auto& ccy : parameters->ccys()) {
        LOG("building " << ccy << " discount yield curve..");
        Handle<YieldTermStructure> wrapper = initMarket->discountCurve(ccy, configuration);
        QL_REQUIRE(!wrapper.empty(), "discount curve for currency " << ccy << " not provided");
        // include today

        // constructing discount yield curves
        DayCounter dc = wrapper->dayCounter(); // used to convert YieldCurve Periods to Times
        vector<Time> yieldCurveTimes(1, 0.0);  // include today
        vector<Date> yieldCurveDates(1, asof_);
        QL_REQUIRE(parameters->yieldCurveTenors(ccy).front() > 0 * Days, "yield curve tenors must not include t=0");
        for (auto& tenor : parameters->yieldCurveTenors(ccy)) {
            yieldCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
            yieldCurveDates.push_back(asof_ + tenor);
        }

        vector<Handle<Quote>> quotes;
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
        quotes.push_back(Handle<Quote>(q));
        vector<Real> discounts(yieldCurveTimes.size());
        for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(wrapper->discount(yieldCurveDates[i + 1])));
            Handle<Quote> qh(q);
            quotes.push_back(qh);

            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::DiscountCurve, ccy, i),
                             std::forward_as_tuple(q));

            LOG("ScenarioSimMarket yield curve " << ccy << " discount[" << i << "]=" << q->value());
        }

        boost::shared_ptr<YieldTermStructure> discountCurve;

        if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
            discountCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve(yieldCurveTimes, quotes, 0, TARGET(), wrapper->dayCounter()));
        } else {
            discountCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, wrapper->dayCounter()));
        }

        Handle<YieldTermStructure> dch(discountCurve);
        if (wrapper->allowsExtrapolation())
            dch->enableExtrapolation();
        discountCurves_.insert(
            pair<pair<string, string>, Handle<YieldTermStructure>>(make_pair(Market::defaultConfiguration, ccy), dch));
        LOG("building " << ccy << " discount yield curve done");
    }
    LOG("discount yield curves done");

    LOG("building benchmark yield curves...");
    for (const auto& name : parameters->yieldCurveNames()) {
        LOG("building benchmark yield curve name " << name);
        Handle<YieldTermStructure> wrapper = initMarket->yieldCurve(name, configuration);
        QL_REQUIRE(!wrapper.empty(), "yield curve for name " << name << " not provided");

        DayCounter dc = wrapper->dayCounter(); // used to convert YieldCurve Periods to Times
        vector<Time> yieldCurveTimes(1, 0.0);  // include today
        vector<Date> yieldCurveDates(1, asof_);
        QL_REQUIRE(parameters->yieldCurveTenors(name).front() > 0 * Days, "yield curve tenors must not include t=0");
        for (auto& tenor : parameters->yieldCurveTenors(name)) {
            yieldCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
            yieldCurveDates.push_back(asof_ + tenor);
        }

        // include today
        vector<Handle<Quote>> quotes;
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
        quotes.push_back(Handle<Quote>(q));
        vector<Real> discounts(yieldCurveTimes.size());
        for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(wrapper->discount(yieldCurveDates[i + 1])));
            Handle<Quote> qh(q);
            quotes.push_back(qh);

            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::YieldCurve, name, i),
                             std::forward_as_tuple(q));

            LOG("ScenarioSimMarket yield curve name " << name << " discount[" << i << "]=" << q->value());
        }

        boost::shared_ptr<YieldTermStructure> yieldCurve;

        if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
            yieldCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve(yieldCurveTimes, quotes, 0, TARGET(), wrapper->dayCounter()));
        } else {
            yieldCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, wrapper->dayCounter()));
        }

        Handle<YieldTermStructure> dch(yieldCurve);
        if (wrapper->allowsExtrapolation())
            dch->enableExtrapolation();
        yieldCurves_.insert(
            pair<pair<string, string>, Handle<YieldTermStructure>>(make_pair(Market::defaultConfiguration, name), dch));
        LOG("building benchmark yield curve " << name << " done");
    }
    LOG("benchmark yield curves done");

    // building security spreads
    LOG("building security spreads...");
    for (const auto& name : parameters->securities()) {
        boost::shared_ptr<Quote> spreadQuote(new SimpleQuote(initMarket->securitySpread(name, configuration)->value()));
        securitySpreads_.insert(pair<pair<string, string>, Handle<Quote>>(make_pair(Market::defaultConfiguration, name),
                                                                          Handle<Quote>(spreadQuote)));
    }

    // constructing index curves
    LOG("building index curves...");
    for (const auto& ind : parameters->indices()) {
        LOG("building " << ind << " index curve");
        Handle<IborIndex> index = initMarket->iborIndex(ind, configuration);
        QL_REQUIRE(!index.empty(), "index object for " << ind << " not provided");
        Handle<YieldTermStructure> wrapperIndex = index->forwardingTermStructure();
        QL_REQUIRE(!wrapperIndex.empty(), "no termstructure for index " << ind);
        vector<string> keys(parameters->yieldCurveTenors(ind).size());

        DayCounter dc = wrapperIndex->dayCounter(); // used to convert YieldCurve Periods to Times
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
            indexCurve = boost::shared_ptr<YieldTermStructure>(new QuantExt::InterpolatedDiscountCurve(
                yieldCurveTimes, quotes, 0, index->fixingCalendar(), wrapperIndex->dayCounter()));
        } else {
            indexCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, wrapperIndex->dayCounter()));
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

        bool isMatrix = boost::dynamic_pointer_cast<SwaptionVolatilityMatrix>(*wrapper) != nullptr;
        // bool isConstant = boost::dynamic_pointer_cast<ConstantSwaptionVolatility>(*wrapper) != nullptr;
        bool isCube = boost::dynamic_pointer_cast<QuantExt::SwaptionVolCubeWithATM>(*wrapper) != nullptr;

        // If swaption volatility type is not Normal, convert to Normal for the simulation
        if (wrapper->volatilityType() != Normal) {
            // FIXME we should support constant swaption vols here as well (or build a matrix
            // always in todays market even if only one vol point is given?)
            if (isMatrix) {
                // Get swap index associated with this volatility structure
                string swapIndexName = initMarket->swapIndexBase(ccy, configuration);
                Handle<SwapIndex> swapIndex = initMarket->swapIndex(swapIndexName, configuration);

                // Set up swaption volatility converter
                SwaptionVolatilityConverter converter(asof_, *wrapper, *swapIndex, Normal);
                wrapper.linkTo(converter.convert());

                LOG("Converting swaption volatilities in configuration " << configuration << " with currency " << ccy
                                                                         << " to normal swaption volatilities");
            } else {
                // Only support conversions for swaption volatility matrices
                // FIXME we should also convert cubes
                LOG("Swaption volatility for ccy " << ccy << " is not a matrix so it is not converted to Normal");
            }
        }

        Handle<SwaptionVolatilityStructure> svp;
        if (parameters->simulateSwapVols()) {
            LOG("Simulating (" << wrapper->volatilityType() << ") Swaption vols for ccy " << ccy);
            vector<Period> optionTenors = parameters->swapVolExpiries();
            vector<Period> swapTenors = parameters->swapVolTerms();
            vector<vector<Handle<Quote>>> quotes(optionTenors.size(),
                                                 vector<Handle<Quote>>(swapTenors.size(), Handle<Quote>()));
            vector<vector<Real>> shift(optionTenors.size(), vector<Real>(swapTenors.size(), 0.0));
            for (Size i = 0; i < optionTenors.size(); ++i) {
                for (Size j = 0; j < swapTenors.size(); ++j) {
                    Real vol = wrapper->volatility(optionTenors[i], swapTenors[j], Null<Real>()); // ATM
                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                    Size index = i * swapTenors.size() + j;
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::SwaptionVolatility, ccy, index),
                                     std::forward_as_tuple(q));
                    quotes[i][j] = Handle<Quote>(q);
                    shift[i][j] = wrapper->volatilityType() == ShiftedLognormal
                                      ? wrapper->shift(optionTenors[i], swapTenors[j])
                                      : 0.0;
                }
            }
            bool flatExtrapolation = true; // FIXME: get this from curve configuration
            VolatilityType volType = wrapper->volatilityType();
            // floating reference date matrix in sim market
            boost::shared_ptr<SwaptionVolatilityStructure> svolp(new SwaptionVolatilityMatrix(
                wrapper->calendar(), wrapper->businessDayConvention(), optionTenors, swapTenors, quotes,
                wrapper->dayCounter(), flatExtrapolation, volType, shift));
            // if we have a cube, we keep the vol spreads constant under scenarios
            // notice that cube is from todaysmarket, so it has a fixed reference date, which means that
            // we keep the smiles constant in terms of vol spreads when moving forward in time;
            // notice also that the volatility will be "sticky strike", i.e. it will not react to
            // changes in the ATM level
            if (isCube) {
                svp = Handle<SwaptionVolatilityStructure>(boost::make_shared<SwaptionVolatilityConstantSpread>(
                    Handle<SwaptionVolatilityStructure>(svolp), wrapper));
            } else {
                svp = Handle<SwaptionVolatilityStructure>(svolp);
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

        string shortSwapIndexBase = initMarket->shortSwapIndexBase(ccy, configuration);
        string swapIndexBase = initMarket->swapIndexBase(ccy, configuration);
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
            // FIXME: Works as of today only, i.e. for sensitivity/scenario analysis.
            // TODO: Build floating reference date StrippedOptionlet class for MC path generators
            boost::shared_ptr<StrippedOptionlet> optionlet = boost::make_shared<StrippedOptionlet>(
                0, // FIXME: settlement days
                wrapper->calendar(), wrapper->businessDayConvention(),
                boost::shared_ptr<IborIndex>(), // FIXME: required for ATM vol calculation
                optionDates, strikes, quotes, wrapper->dayCounter(), wrapper->volatilityType(),
                wrapper->displacement());
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

        // FIXME riskmarket uses SurvivalProbabilityCurve but this isn't added to ore
        boost::shared_ptr<DefaultProbabilityTermStructure> defaultCurve(
            new QuantExt::SurvivalProbabilityCurve<Linear>(dates, quotes, wrapper->dayCounter(), wrapper->calendar()));
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
            boost::shared_ptr<BlackVolTermStructure> cdsVolCurve(new BlackVarianceCurve3(
                0, NullCalendar(), wrapper->businessDayConvention(), wrapper->dayCounter(), times, quotes));

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

        Handle<BlackVolTermStructure> fvh;

        if (parameters->simulateFXVols()) {
            LOG("Simulating FX Vols (BlackVarianceCurve3) for " << ccyPair);

            vector<Handle<Quote>> quotes;
            vector<Time> times;
            for (Size i = 0; i < parameters->fxVolExpiries().size(); i++) {
                Date date = asof_ + parameters->fxVolExpiries()[i];
                Volatility vol = wrapper->blackVol(date, Null<Real>(), true);
                times.push_back(wrapper->timeFromReference(date));
                boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                simData_.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(RiskFactorKey::KeyType::FXVolatility, ccyPair, i),
                                 std::forward_as_tuple(q));
                quotes.emplace_back(q);
            }

            boost::shared_ptr<BlackVolTermStructure> fxVolCurve(new BlackVarianceCurve3(
                0, NullCalendar(), wrapper->businessDayConvention(), wrapper->dayCounter(), times, quotes));

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

        if (wrapper->allowsExtrapolation())
            fvh->enableExtrapolation();
        fxVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
            make_pair(Market::defaultConfiguration, ccyPair), fvh));

        // build inverted surface
        QL_REQUIRE(ccyPair.size() == 6, "Invalid Ccy pair " << ccyPair);
        string reverse = ccyPair.substr(3) + ccyPair.substr(0, 3);
        Handle<QuantLib::BlackVolTermStructure> ifvh(boost::make_shared<BlackInvertedVolTermStructure>(fvh));
        if (fvh->allowsExtrapolation())
            ifvh->enableExtrapolation();
        fxVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
            make_pair(Market::defaultConfiguration, reverse), ifvh));
    }
    LOG("fx volatilities done");

    // building equity spots
    LOG("building equity spots...");
    for (const auto& eqName : parameters->equityNames()) {
        Real spotVal = initMarket->equitySpot(eqName, configuration)->value();
        DLOG("adding " << eqName << " equity spot price");
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(spotVal));
        Handle<Quote> qh(q);
        equitySpots_.insert(
            pair<pair<string, string>, Handle<Quote>>(make_pair(Market::defaultConfiguration, eqName), qh));
        simData_.emplace(std::piecewise_construct, std::forward_as_tuple(RiskFactorKey::KeyType::EquitySpot, eqName),
                         std::forward_as_tuple(q));
    }
    LOG("equity spots done");

    // building equity dividend yield curves
    LOG("building equity dividend yield curves...");
    for (const auto& eqName : parameters->equityNames()) {
        DLOG("building " << eqName << " equity dividend yield curve..");
        Handle<YieldTermStructure> wrapper = initMarket->equityDividendCurve(eqName, configuration);
        vector<Handle<Quote>> quotes;
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
        quotes.push_back(Handle<Quote>(q));

        DayCounter dc = wrapper->dayCounter();
        vector<Time> equityCurveTimes(1, 0.0);
        vector<Date> equityCurveDates(1, asof_);
        if (parameters->equityNames().size() > 0) {
            QL_REQUIRE(parameters->equityTenors(eqName).size() > 0, "Equity curve tenor grid not defined");
            QL_REQUIRE(parameters->equityTenors(eqName).front() > 0 * Days, "equity curve tenors must not include t=0");
            for (auto& tenor : parameters->equityTenors(eqName)) {
                equityCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
                equityCurveDates.push_back(asof_ + tenor);
            }
        }

        for (Size i = 0; i < equityCurveTimes.size() - 1; i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(wrapper->discount(equityCurveTimes[i + 1])));
            Handle<Quote> qh(q);
            quotes.push_back(qh);
            simData_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(RiskFactorKey::KeyType::DividendYield, eqName, i),
                             std::forward_as_tuple(q));
        }
        boost::shared_ptr<YieldTermStructure> eqdivCurve;
        if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
            eqdivCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve(equityCurveTimes, quotes, 0, TARGET(), wrapper->dayCounter()));
        } else {
            eqdivCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve2(equityCurveTimes, quotes, wrapper->dayCounter()));
        }
        Handle<YieldTermStructure> eqdiv_h(eqdivCurve);
        if (wrapper->allowsExtrapolation())
            eqdiv_h->enableExtrapolation();

        equityDividendCurves_.insert(pair<pair<string, string>, Handle<YieldTermStructure>>(
            make_pair(Market::defaultConfiguration, eqName), eqdiv_h));
        DLOG("building " << eqName << " equity dividend yield curve done");
    }
    LOG("equity dividend yield curves done");

    // building eq volatilities
    LOG("building eq volatilities...");
    for (const auto& equityName : parameters->equityVolNames()) {
        Handle<BlackVolTermStructure> wrapper = initMarket->equityVol(equityName, configuration);

        Handle<BlackVolTermStructure> evh;

        if (parameters->simulateEquityVols()) {
            if (parameters->equityVolIsSurface() == false) {
                LOG("Simulating EQ Vols (BlackVarianceCurve3) for " << equityName);
                vector<Handle<Quote>> quotes;
                vector<Time> times;
                for (Size i = 0; i < parameters->equityVolExpiries().size(); i++) {
                    Date date = asof_ + parameters->equityVolExpiries()[i];
                    Volatility vol = wrapper->blackVol(date, Null<Real>(), true);
                    times.push_back(wrapper->timeFromReference(date));
                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                    simData_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(RiskFactorKey::KeyType::EquityVolatility, equityName, i),
                                     std::forward_as_tuple(q));
                    quotes.emplace_back(q);
                }
                boost::shared_ptr<BlackVolTermStructure> eqVolCurve(new BlackVarianceCurve3(
                    0, NullCalendar(), wrapper->businessDayConvention(), wrapper->dayCounter(), times, quotes));
                evh = Handle<BlackVolTermStructure>(eqVolCurve);
            } else {
                LOG("Simulating EQ Vols (BlackVarianceSurfaceMoneyness) for " << equityName);
                Handle<Quote> spot = equitySpots_[make_pair(Market::defaultConfiguration, equityName)];
                Size n = parameters->equityVolMoneyness().size();
                Size m = parameters->equityVolExpiries().size();
                vector<vector<Handle<Quote>>> quotes(n, vector<Handle<Quote>>(m, Handle<Quote>()));
                for (Size i = 0; i < n; i++) {
                    Real mon = parameters->equityVolMoneyness()[i];

                    // strike
                    Real k = spot->value() * mon;

                    for (Size j = 0; j < m; j++) {
                        Period p = parameters->equityVolExpiries()[j];

                        // Index is expires then moneyness. TODO: is this the best?
                        Size idx = i * m + j;

                        Volatility vol = wrapper->blackVol(asof_ + p, k);
                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                        simData_.emplace(std::piecewise_construct,
                                         std::forward_as_tuple(RiskFactorKey::KeyType::EquityVolatility, equityName, idx),
                                         std::forward_as_tuple(q));
                        quotes[i][j] = Handle<Quote>(q);
                    }
                }

                Calendar cal = wrapper->calendar();
                DayCounter dc = wrapper->dayCounter();
                vector<Time> times;
                for (Period p : parameters->equityVolExpiries())
                    times.push_back(dc.yearFraction(asof_, asof_ + p));

                // If true, the strikes are fixed, if false they move with the spot handle
                // Should probably be false, but some people like true for sensi runs.
                bool stickyStrike = true;

                boost::shared_ptr<BlackVolTermStructure> eqVolCurve(
                    new BlackVarianceSurfaceMoneyness(cal, spot, times, parameters->equityVolMoneyness(), quotes, dc, stickyStrike));
                eqVolCurve->enableExtrapolation();
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

            boost::shared_ptr<BilinearBaseCorrelationTermStructure> bcp =
                boost::make_shared<BilinearBaseCorrelationTermStructure>(
                    wrapper->settlementDays(), wrapper->calendar(), wrapper->businessDayConvention(), terms,
                    parameters->baseCorrelationDetachmentPoints(), quotes, wrapper->dayCounter());

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
    //TODO: do we need anything here?

    LOG("CPI Indices done");

    LOG("building zero inflation curves...");
    for (const auto& zic : parameters->zeroInflationIndices()) {
        LOG("building " << zic << " zero inflation curve");

        Handle<ZeroInflationIndex> inflationIndex = initMarket->zeroInflationIndex(zic, configuration);
        Handle<ZeroInflationTermStructure> inflationTs = inflationIndex->zeroInflationTermStructure();
        vector<string> keys(parameters->zeroInflationTenors(zic).size());
        BusinessDayConvention bdc = ModifiedFollowing;

       // vector<Time> zeroCurveTimes;       // include today
        vector<Date> zeroCurveDates;
        zeroCurveDates.push_back(asof_ - inflationTs->observationLag());
        vector<Handle<Quote>> quotes;
        QL_REQUIRE(parameters->zeroInflationTenors(zic).front() > 0 * Days, "zero inflation tenors must not include t=0");
        for (auto& tenor : parameters->zeroInflationTenors(zic)) {
           // zeroCurveTimes.push_back(inflationTs->dayCounter().yearFraction(asof_, asof_ + tenor));
            zeroCurveDates.push_back(asof_ + tenor);
        }
        
        boost::shared_ptr<SimpleQuote> q0(new SimpleQuote(inflationTs->zeroRate(zeroCurveDates[1])));
        Handle<Quote> qh0(q0);
        quotes.push_back(qh0);

        for (Size i = 1; i < zeroCurveDates.size(); i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(inflationTs->zeroRate(zeroCurveDates[i])));
            Handle<Quote> qh(q);
            quotes.push_back(qh);

            simData_.emplace(std::piecewise_construct,
                std::forward_as_tuple(RiskFactorKey::KeyType::ZeroInflationCurve, zic, i-1),
                std::forward_as_tuple(q));
            LOG("ScenarioSimMarket index curve " << zic << " zeroRate[" << i << "]=" << q->value());
        }

        boost::shared_ptr<ZeroInflationTermStructure> zeroCurve;

        // Note this is *not* a floating term structure, it is only suitable for sensi runs
        // TODO: floating
        zeroCurve = boost::shared_ptr<ZeroInflationCurveObserver<Linear>> (new ZeroInflationCurveObserver<Linear>(
            asof_, inflationIndex->fixingCalendar(), inflationTs->dayCounter(), inflationTs->observationLag(), inflationTs->frequency(),
            inflationTs->indexIsInterpolated(), inflationTs->nominalTermStructure(), zeroCurveDates, quotes, inflationTs->seasonality()));

        Handle<ZeroInflationTermStructure> its(zeroCurve);
        boost::shared_ptr<ZeroInflationIndex> i = ore::data::parseZeroInflationIndex(zic, false, Handle<ZeroInflationTermStructure>(its));
        Handle<ZeroInflationIndex> zh(i);
        zeroInflationIndices_.insert(pair<pair<string, string>, Handle<ZeroInflationIndex>>
            (make_pair(Market::defaultConfiguration, zic), zh));

        LOG("building " << zic << " zero inflation curve done");   
    }
    LOG("zero inflation curves done");

    LOG("building yoy inflation curves...");
    for (const auto& yic : parameters->yoyInflationIndices()) {

        Handle<YoYInflationIndex> yoyInflationIndex = initMarket->yoyInflationIndex(yic, configuration);
        Handle<YoYInflationTermStructure> yoyInflationTs = yoyInflationIndex->yoyInflationTermStructure();
        vector<string> keys(parameters->yoyInflationTenors(yic).size());
        BusinessDayConvention bdc = ModifiedFollowing;

        vector<Date> yoyCurveDates;
        yoyCurveDates.push_back(asof_ - yoyInflationTs->observationLag());
        vector<Handle<Quote>> quotes;
        QL_REQUIRE(parameters->yoyInflationTenors(yic).front() > 0 * Days, "yoy inflation tenors must not include t=0");
        for (auto& tenor : parameters->yoyInflationTenors(yic)) {
            yoyCurveDates.push_back(asof_ + tenor);
        }

        boost::shared_ptr<SimpleQuote> q0(new SimpleQuote(yoyInflationTs->yoyRate(yoyCurveDates[1])));
        Handle<Quote> qh0(q0);
        quotes.push_back(qh0);

        for (Size i = 1; i < yoyCurveDates.size(); i++) {
            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(yoyInflationTs->yoyRate(yoyCurveDates[i])));
            Handle<Quote> qh(q);
            quotes.push_back(qh);

            simData_.emplace(std::piecewise_construct,
                std::forward_as_tuple(RiskFactorKey::KeyType::YoYInflationCurve, yic, i-1),
                std::forward_as_tuple(q));

            LOG("ScenarioSimMarket yoy inflation index curve " << yic << " yoyRate[" << i << "]=" << q->value());
        }

        boost::shared_ptr<YoYInflationTermStructure> yoyCurve;

        // Note this is *not* a floating term structure, it is only suitable for sensi runs
        // TODO: floating
        yoyCurve = boost::shared_ptr<YoYInflationCurveObserver<Linear>>(new YoYInflationCurveObserver<Linear>(
            asof_, yoyInflationIndex->fixingCalendar(), yoyInflationTs->dayCounter(), yoyInflationTs->observationLag(), yoyInflationTs->frequency(),
            yoyInflationTs->indexIsInterpolated(), yoyInflationTs->nominalTermStructure(), yoyCurveDates, quotes, yoyInflationTs->seasonality()));

        Handle<YoYInflationTermStructure> its(yoyCurve);
        boost::shared_ptr<YoYInflationIndex> i(yoyInflationIndex->clone(its));
        Handle<YoYInflationIndex> zh(i);
        yoyInflationIndices_.insert(pair<pair<string, string>, Handle<YoYInflationIndex>>
            (make_pair(Market::defaultConfiguration, yic), zh));
    }

    LOG("yoy inflation curves done");
}

void ScenarioSimMarket::update(const Date& d) {
    // DLOG("ScenarioSimMarket::update called with Date " << QuantLib::io::iso_date(d));

    ObservationMode::Mode om = ObservationMode::instance().mode();
    if (om == ObservationMode::Mode::Disable)
        ObservableSettings::instance().disableUpdates(false);
    else if (om == ObservationMode::Mode::Defer)
        ObservableSettings::instance().disableUpdates(true);

    boost::shared_ptr<Scenario> scenario = scenarioGenerator_->next(d);

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

    const vector<RiskFactorKey>& keys = scenario->keys();

    Size count = 0;
    bool missingPoint = false;
    for (const auto& key : keys) {
        // TODO: Is this really an error?
        auto it = simData_.find(key);
        if (it == simData_.end()) {
            ALOG("simulation data point missing for key " << key);
            missingPoint = true;
        } else {
            // LOG("simulation data point found for key " << key);
            it->second->setValue(scenario->get(key));
            count++;
        }
    }
    QL_REQUIRE(!missingPoint, "simulation data points missing from scenario, exit.");

    if (count != simData_.size()) {
        ALOG("mismatch between scenario and sim data size, " << count << " vs " << simData_.size());
        for (auto it : simData_) {
            if (!scenario->has(it.first))
                ALOG("Key " << it.first << " missing in scenario");
        }
        QL_FAIL("mismatch between scenario and sim data size, exit.");
    }

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
        for (auto i : parameters_->additionalScenarioDataIndices())
            asd_->set(iborIndex(i)->fixing(d), AggregationScenarioDataType::IndexFixing, i);

        for (auto c : parameters_->additionalScenarioDataCcys()) {
            if (c != parameters_->baseCcy())
                asd_->set(fxSpot(c + parameters_->baseCcy())->value(), AggregationScenarioDataType::FXSpot, c);
        }

        asd_->set(numeraire_, AggregationScenarioDataType::Numeraire);

        asd_->next();
    }

    // DLOG("ScenarioSimMarket::update done");
}
} // namespace analytics
} // namespace ore
