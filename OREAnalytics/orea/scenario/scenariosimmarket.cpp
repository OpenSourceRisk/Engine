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
#include <ql/instruments/makecapfloor.hpp>
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

#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <qle/indexes/inflationindexobserver.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/blackvariancesurfacestddevs.hpp>
#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
#include <qle/termstructures/dynamiccpivolatilitystructure.hpp>
#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/termstructures/strippedyoyinflationoptionletvol.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/yoyinflationcurveobservermoving.hpp>
#include <qle/termstructures/zeroinflationcurveobservermoving.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace std;

typedef QuantLib::BaseCorrelationTermStructure<QuantLib::BilinearInterpolation> BilinearBaseCorrelationTermStructure;

namespace {
// Utility function that is in catch blocks below
void processException(bool continueOnError, const std::exception& e) {
    if (continueOnError) {
        ALOG("skipping this object: " << e.what());
    } else {
        QL_FAIL(e.what());
    }
}
} // namespace

namespace ore {
namespace analytics {

RiskFactorKey::KeyType yieldCurveRiskFactor(const ore::data::YieldCurveType y) {

    if (y == ore::data::YieldCurveType::Discount) {
        return RiskFactorKey::KeyType::DiscountCurve;
    } else if (y == ore::data::YieldCurveType::Yield) {
        return RiskFactorKey::KeyType::YieldCurve;
    } else if (y == ore::data::YieldCurveType::EquityDividend) {
        return RiskFactorKey::KeyType::DividendYield;
    } else {
        QL_FAIL("yieldCurveType not supported");
    }
}

ore::data::YieldCurveType riskFactorYieldCurve(const RiskFactorKey::KeyType rf) {

    if (rf == RiskFactorKey::KeyType::DiscountCurve) {
        return ore::data::YieldCurveType::Discount;
    } else if (rf == RiskFactorKey::KeyType::YieldCurve) {
        return ore::data::YieldCurveType::Yield;
    } else if (rf == RiskFactorKey::KeyType::DividendYield) {
        return ore::data::YieldCurveType::EquityDividend;
    } else {
        QL_FAIL("RiskFactorKey::KeyType not supported");
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
                                      const RiskFactorKey::KeyType rf, const string& key, const vector<Period>& tenors,
                                      const string& dayCounter, bool simulate, const string& interpolation) {
    Handle<YieldTermStructure> wrapper = initMarket->yieldCurve(riskFactorYieldCurve(rf), key, configuration);
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
    std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>> simDataTmp;
    for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(wrapper->discount(yieldCurveDates[i + 1])));
        Handle<Quote> qh(q);
        quotes.push_back(qh);

        // Check if the risk factor is simulated before adding it
        if (simulate) {
            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(rf, key, i), std::forward_as_tuple(q));
            DLOG("ScenarioSimMarket yield curve " << key << " discount[" << i << "]=" << q->value());
        }
    }

    boost::shared_ptr<YieldTermStructure> yieldCurve;

    if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
        yieldCurve = boost::shared_ptr<YieldTermStructure>(
            new QuantExt::InterpolatedDiscountCurve(yieldCurveTimes, quotes, 0, TARGET(), dc));
    } else {        
        if (interpolation == "LinearZero") {
            yieldCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurveLinearZero(yieldCurveTimes, quotes, dc));
        } else if (interpolation == "LogLinear") {
            yieldCurve = boost::shared_ptr<YieldTermStructure>(
                new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, dc));
        }
        else {
			QL_FAIL("Interpolation \"" << interpolation
                                               << "\" in simulation not recognized. Please provide either LinearZero "
                                                  "or LogLinear in simulation.xml");
        }
    }

    Handle<YieldTermStructure> ych(yieldCurve);
    if (wrapper->allowsExtrapolation())
        ych->enableExtrapolation();
    yieldCurves_.insert(pair<tuple<string, ore::data::YieldCurveType, string>, Handle<YieldTermStructure>>(
        make_tuple(Market::defaultConfiguration, riskFactorYieldCurve(rf), key), ych));
    simData_.insert(simDataTmp.begin(), simDataTmp.end());
}

ScenarioSimMarket::ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                                     const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
                                     const Conventions& conventions, const std::string& configuration,
                                     const CurveConfigurations& curveConfigs,
                                     const TodaysMarketParameters& todaysMarketParams, const bool continueOnError)
    : ScenarioSimMarket(initMarket, parameters, conventions, boost::make_shared<FixingManager>(initMarket->asofDate()),
                        configuration, curveConfigs, todaysMarketParams, continueOnError) {}

ScenarioSimMarket::ScenarioSimMarket(
    const boost::shared_ptr<Market>& initMarket, const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
    const Conventions& conventions, const boost::shared_ptr<FixingManager>& fixingManager,
    const std::string& configuration, const ore::data::CurveConfigurations& curveConfigs,
    const ore::data::TodaysMarketParameters& todaysMarketParams, const bool continueOnError)
    : SimMarket(conventions), parameters_(parameters), fixingManager_(fixingManager),
      filter_(boost::make_shared<ScenarioFilter>()) {

    LOG("building ScenarioSimMarket...");
    asof_ = initMarket->asofDate();
    LOG("AsOf " << QuantLib::io::iso_date(asof_));

    // Sort parameters so they get processed in correct order
    map<RiskFactorKey::KeyType, pair<bool, set<string>>> params;
    params.insert(parameters->parameters().begin(), parameters->parameters().end());

    for (const auto& param : params) {
        try {
            std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>> simDataTmp;

            switch (param.first) {
            case RiskFactorKey::KeyType::FXSpot:
                for (const auto& name : param.second.second) {
                    try {
                        // constructing fxSpots_
                        LOG("adding " << name << " FX rates");
                        boost::shared_ptr<SimpleQuote> q(
                            new SimpleQuote(initMarket->fxSpot(name, configuration)->value()));
                        Handle<Quote> qh(q);
                        fxSpots_[Market::defaultConfiguration].addQuote(name, qh);
                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(q));
                        }
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::DiscountCurve:
            case RiskFactorKey::KeyType::YieldCurve:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " yield curve..");
                        vector<Period> tenors = parameters->yieldCurveTenors(name);
                        addYieldCurve(initMarket, configuration, param.first, name, tenors,
                                      parameters->yieldCurveDayCounter(name), param.second.first, parameters->interpolation());
                        LOG("building " << name << " yield curve done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::IndexCurve:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " index curve");
                        std::vector<string> indexTokens;
                        split(indexTokens, name, boost::is_any_of("-"));
                        Handle<IborIndex> index;
                        if (indexTokens[1] == "GENERIC") {
                            // If we have a generic curve build the index using the index currency's discount curve
                            // no need to check for a convention based ibor index in this case
                            index = Handle<IborIndex>(
                                parseIborIndex(name, initMarket->discountCurve(indexTokens[0], configuration)));
                        } else {
                            index = initMarket->iborIndex(name, configuration);
                        }
                        QL_REQUIRE(!index.empty(), "index object for " << name << " not provided");
                        Handle<YieldTermStructure> wrapperIndex = index->forwardingTermStructure();
                        QL_REQUIRE(!wrapperIndex.empty(), "no termstructure for index " << name);
                        vector<string> keys(parameters->yieldCurveTenors(name).size());

                        DayCounter dc = ore::data::parseDayCounter(
                            parameters->yieldCurveDayCounter(name)); // used to convert YieldCurve Periods to Times
                        vector<Time> yieldCurveTimes(1, 0.0);        // include today
                        vector<Date> yieldCurveDates(1, asof_);
                        QL_REQUIRE(parameters->yieldCurveTenors(name).front() > 0 * Days,
                                   "yield curve tenors must not include t=0");
                        for (auto& tenor : parameters->yieldCurveTenors(name)) {
                            yieldCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
                            yieldCurveDates.push_back(asof_ + tenor);
                        }

                        // include today
                        vector<Handle<Quote>> quotes;
                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
                        quotes.push_back(Handle<Quote>(q));

                        for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
                            boost::shared_ptr<SimpleQuote> q(
                                new SimpleQuote(wrapperIndex->discount(yieldCurveDates[i + 1])));
                            Handle<Quote> qh(q);
                            quotes.push_back(qh);

                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name, i),
                                               std::forward_as_tuple(q));

                            DLOG("ScenarioSimMarket index curve " << name << " discount[" << i << "]=" << q->value());
                        }
                        // FIXME interpolation fixed to linear, added to xml??
                        boost::shared_ptr<YieldTermStructure> indexCurve;
                        if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
                            indexCurve = boost::shared_ptr<YieldTermStructure>(new QuantExt::InterpolatedDiscountCurve(
                                yieldCurveTimes, quotes, 0, index->fixingCalendar(), dc));
                        } else {							
                            if (parameters->interpolation() == "LinearZero") {
								indexCurve = boost::shared_ptr<YieldTermStructure>(
                                    new QuantExt::InterpolatedDiscountCurveLinearZero(yieldCurveTimes, quotes, dc));
                            } else if (parameters->interpolation() == "LogLinear") {
                                indexCurve = boost::shared_ptr<YieldTermStructure>(
                                    new QuantExt::InterpolatedDiscountCurve2(yieldCurveTimes, quotes, dc));
                            } else {
                                QL_FAIL("Interpolation \""
                                        << parameters->interpolation()
                                        << "\" in simulation not recognized. Please provide either LinearZero "
                                           "or LogLinear in simulation.xml");
                            }                            
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
                        iborIndices_.insert(pair<pair<string, string>, Handle<IborIndex>>(
                            make_pair(Market::defaultConfiguration, name), ih));
                        LOG("building " << name << " index curve done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::EquitySpot:
                for (const auto& name : param.second.second) {
                    try {
                        // building equity spots
                        LOG("adding " << name << " equity spot...");
                        Real spotVal = initMarket->equitySpot(name, configuration)->value();
                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(spotVal));
                        Handle<Quote> qh(q);
                        equitySpots_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), qh));
                        simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                           std::forward_as_tuple(q));
                        LOG("adding " << name << " equity spot done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::DividendYield:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " equity dividend yield curve..");
                        vector<Period> tenors = parameters->equityDividendTenors(name);
                        addYieldCurve(initMarket, configuration, param.first, name, tenors,
                                      parameters->yieldCurveDayCounter(name), param.second.first);
                        LOG("building " << name << " equity dividend yield curve done");

                        // Equity spots and Yield/Index curves added first so we can now build equity index
                        // First get Forecast Curve
                        string forecastCurve;
                        if (curveConfigs.hasEquityCurveConfig(name)) {
                            // From the equity config, get the currency and forecast curve of the equity
                            auto eqVolConfig = curveConfigs.equityCurveConfig(name);
                            string forecastName = eqVolConfig->forecastingCurve();
                            string eqCcy = eqVolConfig->currency();
                            // Build a YieldCurveSpec and extract the yieldCurveSpec name
                            YieldCurveSpec ycspec(eqCcy, forecastName);
                            forecastCurve = ycspec.name();
                            TLOG("Got forecast curve '" << forecastCurve << "' from equity curve config for " << name);
                        }

                        // Get the nominal term structure from this scenario simulation market
                        Handle<YieldTermStructure> forecastTs =
                            getYieldCurve(forecastCurve, todaysMarketParams, Market::defaultConfiguration);
                        Handle<EquityIndex> curve = initMarket->equityCurve(name, configuration);

                        // If forecast term structure is empty, fall back on this scenario simulation market's discount
                        // curve
                        if (forecastTs.empty()) {
                            string ccy = curve->currency().code();
                            TLOG("Falling back on the discount curve for currency '"
                                 << ccy << "', the currency of inflation index '" << name << "'");
                            forecastTs = discountCurve(ccy);
                        }
                        boost::shared_ptr<EquityIndex> ei(
                            curve->clone(equitySpot(name, configuration), forecastTs,
                                         yieldCurve(YieldCurveType::EquityDividend, name, configuration)));
                        Handle<EquityIndex> eh(ei);
                        equityCurves_.insert(pair<pair<string, string>, Handle<EquityIndex>>(
                            make_pair(Market::defaultConfiguration, name), eh));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::SecuritySpread:
                for (const auto& name : param.second.second) {
                    try {
                        DLOG("Adding security spread " << name << " from configuration " << configuration);
                        // we have a security spread for each security, so no try-catch block required
                        boost::shared_ptr<SimpleQuote> spreadQuote(
                            new SimpleQuote(initMarket->securitySpread(name, configuration)->value()));
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(spreadQuote));
                        }
                        securitySpreads_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(spreadQuote)));

                        DLOG("Adding security recovery rate " << name << " from configuration " << configuration);
                        // security recovery rates are optional, so we need a try-catch block
                        try {
                            boost::shared_ptr<SimpleQuote> recoveryQuote(
                                new SimpleQuote(initMarket->recoveryRate(name, configuration)->value()));
                            // TODO this comes from the default curves section in the parameters,
                            // do we want to specify the simulation of security recovery rates separately?
                            if (parameters->simulateRecoveryRates()) {
                                simDataTmp.emplace(std::piecewise_construct,
                                                   std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                                                   std::forward_as_tuple(recoveryQuote));
                            }
                            recoveryRates_.insert(pair<pair<string, string>, Handle<Quote>>(
                                make_pair(Market::defaultConfiguration, name), Handle<Quote>(recoveryQuote)));
                        } catch (const std::exception& e) {
                            // security recovery rates are optional, therefore we never throw
                            ALOG("skipping this object: " << e.what());
                        }
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::SwaptionVolatility:
            case RiskFactorKey::KeyType::YieldVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        // set parameters for swaption resp. yield vols
                        RelinkableHandle<SwaptionVolatilityStructure> wrapper;
                        vector<Period> optionTenors, underlyingTenors;
                        vector<Real> strikeSpreads;
                        string shortSwapIndexBase = "", swapIndexBase = "";
                        bool isCube, isAtm, simulateAtmOnly;
                        if (param.first == RiskFactorKey::KeyType::SwaptionVolatility) {
                            LOG("building " << name << " swaption volatility curve...");
                            wrapper.linkTo(*initMarket->swaptionVol(name, configuration));
                            shortSwapIndexBase = initMarket->shortSwapIndexBase(name, configuration);
                            swapIndexBase = initMarket->swapIndexBase(name, configuration);
                            isCube = parameters->swapVolIsCube(name);
                            optionTenors = parameters->swapVolExpiries(name);
                            underlyingTenors = parameters->swapVolTerms(name);
                            strikeSpreads = parameters->swapVolStrikeSpreads(name);
                            simulateAtmOnly = parameters->simulateSwapVolATMOnly();
                        } else {
                            LOG("building " << name << " yield volatility curve...");
                            wrapper.linkTo(*initMarket->yieldVol(name, configuration));
                            isCube = false;
                            optionTenors = parameters->yieldVolExpiries();
                            underlyingTenors = parameters->yieldVolTerms();
                            strikeSpreads = {0.0};
                            simulateAtmOnly = true;
                        }
                        LOG("Initial market " << name << " yield volatility type = " << wrapper->volatilityType());

                        // Check if underlying market surface is atm or smile
                        isAtm = boost::dynamic_pointer_cast<SwaptionVolatilityMatrix>(*wrapper) != nullptr ||
                                boost::dynamic_pointer_cast<ConstantSwaptionVolatility>(*wrapper) != nullptr;

                        Handle<SwaptionVolatilityStructure> svp;
                        if (param.second.first) {
                            LOG("Simulating yield vols for ccy " << name);
                            DLOG("YieldVol T0  source is atm     : " << (isAtm ? "True" : "False"));
                            DLOG("YieldVol ssm target is cube    : " << (isCube ? "True" : "False"));
                            DLOG("YieldVol simulate atm only     : " << (simulateAtmOnly ? "True" : "False"));
                            if (simulateAtmOnly) {
                                QL_REQUIRE(strikeSpreads.size() == 1 && close_enough(strikeSpreads[0], 0),
                                           "for atmOnly strikeSpreads must be {0.0}");
                            }
                            boost::shared_ptr<QuantLib::SwaptionVolatilityCube> cube;
                            if (isCube && !isAtm) {
                                boost::shared_ptr<SwaptionVolCubeWithATM> tmp =
                                    boost::dynamic_pointer_cast<SwaptionVolCubeWithATM>(*wrapper);
                                QL_REQUIRE(tmp, "swaption cube missing")
                                cube = tmp->cube();
                            }
                            vector<vector<Handle<Quote>>> quotes, atmQuotes;
                            quotes.resize(optionTenors.size() * underlyingTenors.size(),
                                          vector<Handle<Quote>>(strikeSpreads.size(), Handle<Quote>()));
                            atmQuotes.resize(optionTenors.size(),
                                             std::vector<Handle<Quote>>(underlyingTenors.size(), Handle<Quote>()));
                            vector<vector<Real>> shift(optionTenors.size(), vector<Real>(underlyingTenors.size(), 0.0));
                            Size atmSlice = std::find_if(strikeSpreads.begin(), strikeSpreads.end(),
                                                         [](const Real s) { return close_enough(s, 0.0); }) -
                                            strikeSpreads.begin();
                            QL_REQUIRE(atmSlice < strikeSpreads.size(),
                                       "could not find atm slice (strikeSpreads do not contain 0.0)");

                            // convert to normal if
                            // a) we have a swaption (i.e. not a yield) volatility and
                            // b) the T0 term structure is not normal
                            // c) we are not in the situation of simulating ATM only and having a non-normal cube in T0,
                            //    since in this case the T0 structure is dynamically used to determine the sim market
                            //    vols
                            bool convertToNormal = wrapper->volatilityType() != Normal &&
                                                   param.first == RiskFactorKey::KeyType::SwaptionVolatility &&
                                                   (!simulateAtmOnly || isAtm);
                            DLOG("T0 ts is normal             : " << (wrapper->volatilityType() == Normal ? "True"
                                                                                                          : "False"));
                            DLOG("Have swaption vol           : "
                                 << (param.first == RiskFactorKey::KeyType::SwaptionVolatility ? "True" : "False"));
                            DLOG("Will convert to normal vol  : " << (convertToNormal ? "True" : "False"));

                            // Set up a vol converter, and create if vol type is not normal
                            SwaptionVolatilityConverter* converter = nullptr;
                            if (convertToNormal) {
                                Handle<SwapIndex> swapIndex = initMarket->swapIndex(swapIndexBase, configuration);
                                Handle<SwapIndex> shortSwapIndex =
                                    initMarket->swapIndex(shortSwapIndexBase, configuration);
                                converter = new SwaptionVolatilityConverter(asof_, *wrapper, *swapIndex,
                                                                            *shortSwapIndex, Normal);
                            }

                            for (Size k = 0; k < strikeSpreads.size(); ++k) {
                                for (Size i = 0; i < optionTenors.size(); ++i) {
                                    for (Size j = 0; j < underlyingTenors.size(); ++j) {
                                        Real strike = Null<Real>();
                                        if (!simulateAtmOnly && cube)
                                            strike = cube->atmStrike(optionTenors[i], underlyingTenors[j]) +
                                                     strikeSpreads[k];
                                        Real vol;
                                        if (convertToNormal) {
                                            // if not a normal volatility use the converted to convert to normal at
                                            // given point
                                            vol = converter->convert(wrapper->optionDateFromTenor(optionTenors[i]),
                                                                     underlyingTenors[j], strikeSpreads[k],
                                                                     wrapper->dayCounter(), Normal);
                                        } else {
                                            vol =
                                                wrapper->volatility(optionTenors[i], underlyingTenors[j], strike, true);
                                        }
                                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));

                                        Size index = i * underlyingTenors.size() * strikeSpreads.size() +
                                                     j * strikeSpreads.size() + k;

                                        simDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, index),
                                                           std::forward_as_tuple(q));
                                        auto tmp = Handle<Quote>(q);
                                        quotes[i * underlyingTenors.size() + j][k] = tmp;
                                        if (k == atmSlice) {
                                            atmQuotes[i][j] = tmp;
                                            shift[i][j] =
                                                !convertToNormal && wrapper->volatilityType() == ShiftedLognormal
                                                    ? wrapper->shift(optionTenors[i], underlyingTenors[j])
                                                    : 0.0;
                                        }
                                    }
                                }
                            }
                            bool flatExtrapolation = true; // FIXME: get this from curve configuration
                            VolatilityType volType = convertToNormal ? Normal : wrapper->volatilityType();
                            DayCounter dc = ore::data::parseDayCounter(parameters->swapVolDayCounter(name));
                            Handle<SwaptionVolatilityStructure> atm(boost::make_shared<SwaptionVolatilityMatrix>(
                                wrapper->calendar(), wrapper->businessDayConvention(), optionTenors, underlyingTenors,
                                atmQuotes, dc, flatExtrapolation, volType, shift));
                            if (simulateAtmOnly) {
                                if (isAtm) {
                                    svp = atm;
                                } else {
                                    // floating reference date matrix in sim market
                                    // if we have a cube, we keep the vol spreads constant under scenarios
                                    // notice that cube is from todaysmarket, so it has a fixed reference date, which
                                    // means that we keep the smiles constant in terms of vol spreads when moving
                                    // forward in time; notice also that the volatility will be "sticky strike", i.e. it
                                    // will not react to changes in the ATM level
                                    svp = Handle<SwaptionVolatilityStructure>(
                                        boost::make_shared<SwaptionVolatilityConstantSpread>(atm, wrapper));
                                }
                            } else {
                                if (isCube) {
                                    boost::shared_ptr<SwaptionVolatilityCube> tmp(new QuantExt::SwaptionVolCube2(
                                        atm, optionTenors, underlyingTenors, strikeSpreads, quotes,
                                        *initMarket->swapIndex(swapIndexBase, configuration),
                                        *initMarket->swapIndex(shortSwapIndexBase, configuration), false,
                                        flatExtrapolation, false));
                                    svp = Handle<SwaptionVolatilityStructure>(
                                        boost::make_shared<SwaptionVolCubeWithATM>(tmp));
                                } else {
                                    svp = atm;
                                }
                            }
                        } else {
                            string decayModeString = parameters->swapVolDecayMode();
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            LOG("Dynamic (" << wrapper->volatilityType() << ") yield vols (" << decayModeString
                                            << ") for qualifier " << name);
                            if (isCube)
                                WLOG("Only ATM slice is considered from init market's cube");
                            boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svolp =
                                boost::make_shared<QuantExt::DynamicSwaptionVolatilityMatrix>(
                                    *wrapper, 0, NullCalendar(), decayMode);
                            svp = Handle<SwaptionVolatilityStructure>(svolp);
                        }

                        svp->enableExtrapolation(); // FIXME

                        LOG("Simulaton market " << name << " yield volatility type = " << svp->volatilityType());

                        if (param.first == RiskFactorKey::KeyType::SwaptionVolatility) {
                            swaptionCurves_.insert(pair<pair<string, string>, Handle<SwaptionVolatilityStructure>>(
                                make_pair(Market::defaultConfiguration, name), svp));
                            swaptionIndexBases_.insert(pair<pair<string, string>, pair<string, string>>(
                                make_pair(Market::defaultConfiguration, name),
                                make_pair(shortSwapIndexBase, swapIndexBase)));
                            swaptionIndexBases_.insert(pair<pair<string, string>, pair<string, string>>(
                                make_pair(Market::defaultConfiguration, name),
                                make_pair(swapIndexBase, swapIndexBase)));
                        } else {
                            yieldVolCurves_.insert(pair<pair<string, string>, Handle<SwaptionVolatilityStructure>>(
                                make_pair(Market::defaultConfiguration, name), svp));
                        }
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::OptionletVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " cap/floor volatility curve...");
                        Handle<OptionletVolatilityStructure> wrapper = initMarket->capFloorVol(name, configuration);

                        LOG("Initial market cap/floor volatility type = " << wrapper->volatilityType());

                        Handle<OptionletVolatilityStructure> hCapletVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            LOG("Simulating Cap/Floor Optionlet vols for ccy " << name);

                            // Try to get the ibor index that the cap floor structure relates to
                            // We use this to convert Period to Date below to sample from `wrapper`
                            boost::shared_ptr<IborIndex> iborIndex;
                            Date spotDate;
                            Calendar capCalendar;
                            string strIborIndex;
                            Natural settleDays = 0;
                            if (curveConfigs.hasCapFloorVolCurveConfig(name)) {
                                // From the cap floor config, get the ibor index name
                                // (we do not support convention based indices there)
                                auto config = curveConfigs.capFloorVolCurveConfig(name);
                                settleDays = config->settleDays();
                                strIborIndex = config->iborIndex();
                                if (tryParseIborIndex(strIborIndex, iborIndex)) {
                                    capCalendar = iborIndex->fixingCalendar();
                                    Natural settlementDays = iborIndex->fixingDays();
                                    spotDate = capCalendar.adjust(asof_);
                                    spotDate = capCalendar.advance(spotDate, settlementDays * Days);
                                }
                            }

                            vector<Period> optionTenors = parameters->capFloorVolExpiries(name);
                            vector<Date> optionDates(optionTenors.size());

                            vector<Real> strikes = parameters->capFloorVolStrikes(name);
                            bool isAtm = false;
                            // Strikes may be empty here which means that an ATM curve has been configured
                            if (strikes.empty()) {
                                QL_REQUIRE(
                                    parameters->capFloorVolIsAtm(name),
                                    "Strikes for "
                                        << name
                                        << " is empty in simulation parameters so expected its ATM flag to be true");
                                strikes = {0.0};
                                isAtm = true;
                            }

                            vector<vector<Handle<Quote>>> quotes(
                                optionTenors.size(), vector<Handle<Quote>>(strikes.size(), Handle<Quote>()));

                            for (Size i = 0; i < optionTenors.size(); ++i) {

                                if (iborIndex) {
                                    // If we ask for cap pillars at tenors t_i for i = 1,...,N, we should attempt to
                                    // place the optionlet pillars at the fixing date of the last optionlet in the cap
                                    // with tenor t_i
                                    QL_REQUIRE(optionTenors[i] > iborIndex->tenor(),
                                               "The cap floor tenor must be greater than the ibor index tenor");
                                    boost::shared_ptr<CapFloor> capFloor =
                                        MakeCapFloor(CapFloor::Cap, optionTenors[i], iborIndex, 0.0, 0 * Days);
                                    optionDates[i] = capFloor->lastFloatingRateCoupon()->fixingDate();
                                    DLOG("Option [tenor, date] pair is [" << optionTenors[i] << ", "
                                                                          << io::iso_date(optionDates[i]) << "]");
                                } else {
                                    optionDates[i] = wrapper->optionDateFromTenor(optionTenors[i]);
                                }

                                // If ATM, use initial market's discount curve and ibor index to calculate ATM rate
                                Rate strike = Null<Rate>();
                                if (isAtm) {
                                    QL_REQUIRE(!strIborIndex.empty(), "Expected cap floor vol curve config for "
                                                                          << name << " to have an ibor index name");
                                    initMarket->iborIndex(strIborIndex, configuration);
                                    boost::shared_ptr<CapFloor> cap = MakeCapFloor(
                                        CapFloor::Cap, optionTenors[i],
                                        *initMarket->iborIndex(strIborIndex, configuration), 0.0, 0 * Days);
                                    strike = cap->atmRate(**initMarket->discountCurve(name, configuration));
                                }

                                for (Size j = 0; j < strikes.size(); ++j) {
                                    strike = isAtm ? strike : strikes[j];
                                    Real vol =
                                        wrapper->volatility(optionDates[i], strike, wrapper->allowsExtrapolation());
                                    DLOG("Vol at [date, strike] pair [" << optionDates[i] << ", " << std::fixed
                                                                        << std::setprecision(4) << strike << "] is "
                                                                        << std::setprecision(12) << vol);
                                    boost::shared_ptr<SimpleQuote> q = boost::make_shared<SimpleQuote>(vol);
                                    Size index = i * strikes.size() + j;
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, index),
                                                       std::forward_as_tuple(q));
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }

                            DayCounter dc = ore::data::parseDayCounter(parameters->capFloorVolDayCounter(name));

                            // FIXME: Works as of today only, i.e. for sensitivity/scenario analysis.
                            // TODO: Build floating reference date StrippedOptionlet class for MC path generators
                            boost::shared_ptr<StrippedOptionlet> optionlet = boost::make_shared<StrippedOptionlet>(
                                settleDays, wrapper->calendar(), wrapper->businessDayConvention(), iborIndex,
                                optionDates, strikes, quotes, dc, wrapper->volatilityType(), wrapper->displacement());

                            hCapletVol = Handle<OptionletVolatilityStructure>(
                                boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                                    optionlet));
                        } else {
                            string decayModeString = parameters->capFloorVolDecayMode();
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            boost::shared_ptr<OptionletVolatilityStructure> capletVol =
                                boost::make_shared<DynamicOptionletVolatilityStructure>(*wrapper, 0, NullCalendar(),
                                                                                        decayMode);
                            hCapletVol = Handle<OptionletVolatilityStructure>(capletVol);
                        }

                        hCapletVol->enableExtrapolation();
                        capFloorCurves_.emplace(std::piecewise_construct,
                                                std::forward_as_tuple(Market::defaultConfiguration, name),
                                                std::forward_as_tuple(hCapletVol));

                        LOG("Simulaton market cap/floor volatility type = " << hCapletVol->volatilityType());
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::SurvivalProbability:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " default curve..");
                        Handle<DefaultProbabilityTermStructure> wrapper = initMarket->defaultCurve(name, configuration);
                        vector<Handle<Quote>> quotes;

                        QL_REQUIRE(parameters->defaultTenors(name).front() > 0 * Days,
                                   "default curve tenors must not include t=0");

                        vector<Date> dates(1, asof_);

                        for (Size i = 0; i < parameters->defaultTenors(name).size(); i++) {
                            dates.push_back(asof_ + parameters->defaultTenors(name)[i]);
                        }

                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
                        quotes.push_back(Handle<Quote>(q));
                        for (Size i = 0; i < dates.size() - 1; i++) {
                            Probability prob = wrapper->survivalProbability(dates[i + 1], true);
                            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(prob));
                            // Check if the risk factor is simulated before adding it
                            if (param.second.first) {
                                simDataTmp.emplace(std::piecewise_construct,
                                                   std::forward_as_tuple(param.first, name, i),
                                                   std::forward_as_tuple(q));
                                DLOG("ScenarioSimMarket default curve " << name << " survival[" << i << "]=" << prob);
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
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::RecoveryRate:
                for (const auto& name : param.second.second) {
                    try {
                        DLOG("Adding security recovery rate " << name << " from configuration " << configuration);
                        boost::shared_ptr<SimpleQuote> rrQuote(
                            new SimpleQuote(initMarket->recoveryRate(name, configuration)->value()));
                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                                               std::forward_as_tuple(rrQuote));
                        }
                        recoveryRates_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(rrQuote)));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CDSVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << "  cds vols..");
                        Handle<BlackVolTermStructure> wrapper = initMarket->cdsVol(name, configuration);
                        Handle<BlackVolTermStructure> cvh;
                        if (param.second.first) {
                            LOG("Simulating CDS Vols for " << name);
                            vector<Handle<Quote>> quotes;
                            vector<Time> times;
                            for (Size i = 0; i < parameters->cdsVolExpiries().size(); i++) {
                                Date date = asof_ + parameters->cdsVolExpiries()[i];
                                Volatility vol = wrapper->blackVol(date, Null<Real>(), true);
                                times.push_back(wrapper->timeFromReference(date));
                                boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                                if (parameters->simulateCdsVols()) {
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, i),
                                                       std::forward_as_tuple(q));
                                }
                                quotes.emplace_back(q);
                            }

                            DayCounter dc = ore::data::parseDayCounter(parameters->cdsVolDayCounter(name));
                            boost::shared_ptr<BlackVolTermStructure> cdsVolCurve(new BlackVarianceCurve3(
                                0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes, false));

                            cvh = Handle<BlackVolTermStructure>(cdsVolCurve);
                        } else {
                            string decayModeString = parameters->cdsVolDecayMode();
                            LOG("Deterministic CDS Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // currently only curves (i.e. strike indepdendent) CDS volatility structures are
                            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
                            // strike stickyness, since then no yield term structures and no fx spot are required
                            // that define the ATM level
                            cvh = Handle<BlackVolTermStructure>(
                                boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode, StickyStrike));
                        }

                        if (wrapper->allowsExtrapolation())
                            cvh->enableExtrapolation();
                        cdsVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
                            make_pair(Market::defaultConfiguration, name), cvh));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::FXVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        Handle<BlackVolTermStructure> wrapper = initMarket->fxVol(name, configuration);
                        Handle<Quote> spot = fxSpot(name);
                        QL_REQUIRE(name.length() == 6, "invalid ccy pair length");
                        string forCcy = name.substr(0, 3);
                        string domCcy = name.substr(3, 3);

                        // Get the yield curve IDs from the FX volatility configuration
                        // They may still be empty
                        string foreignTsId;
                        string domesticTsId;
                        if (curveConfigs.hasFxVolCurveConfig(name)) {
                            auto fxVolConfig = curveConfigs.fxVolCurveConfig(name);
                            foreignTsId = fxVolConfig->fxForeignYieldCurveID();
                            TLOG("Got foreign term structure '" << foreignTsId
                                                                << "' from FX volatility curve config for " << name);
                            domesticTsId = fxVolConfig->fxDomesticYieldCurveID();
                            TLOG("Got domestic term structure '" << domesticTsId
                                                                 << "' from FX volatility curve config for " << name);
                        }
                        Handle<BlackVolTermStructure> fvh;

                        if (param.second.first) {
                            LOG("Simulating FX Vols for " << name);
                            Size n = parameters->fxVolExpiries().size();
                            Size m;
                            if (parameters->useMoneyness(name)) {
                                m = parameters->fxVolMoneyness(name).size();
                            } else {
                                m = parameters->fxVolStdDevs(name).size();
                            }
                            vector<vector<Handle<Quote>>> quotes(m, vector<Handle<Quote>>(n, Handle<Quote>()));
                            Calendar cal = wrapper->calendar();
                            if (cal.empty()) {
                                cal = NullCalendar();
                            }
                            // FIXME hardcoded in todaysmarket
                            DayCounter dc = ore::data::parseDayCounter(parameters->fxVolDayCounter(name));
                            vector<Time> times;
                            vector<Date> dates;

                            // Attempt to get the relevant yield curves from the initial market
                            Handle<YieldTermStructure> forTS =
                                getYieldCurve(foreignTsId, todaysMarketParams, configuration, initMarket);
                            TLOG("Foreign term structure '" << foreignTsId << "' from t_0 market is "
                                                            << (forTS.empty() ? "empty" : "not empty"));
                            Handle<YieldTermStructure> domTS =
                                getYieldCurve(domesticTsId, todaysMarketParams, configuration, initMarket);
                            TLOG("Domestic term structure '" << domesticTsId << "' from t_0 market is "
                                                             << (domTS.empty() ? "empty" : "not empty"));

                            // If either term structure is empty, fall back on the initial market's discount curves
                            if (forTS.empty() || domTS.empty()) {
                                TLOG("Falling back on the discount curves for " << forCcy << " and " << domCcy
                                                                                << " from t_0 market");
                                forTS = initMarket->discountCurve(forCcy, configuration);
                                domTS = initMarket->discountCurve(domCcy, configuration);
                            }

                            // get vol matrix to feed to surface
                            if (parameters->useMoneyness(name) ||
                                !(parameters->fxVolIsSurface(name))) { // if moneyness or ATM
                                for (Size i = 0; i < n; i++) {
                                    Date date = asof_ + parameters->fxVolExpiries()[i];

                                    times.push_back(wrapper->timeFromReference(date));

                                    for (Size j = 0; j < m; j++) {
                                        Size idx = j * n + i;
                                        Real mon = parameters->fxVolMoneyness(name)[j]; // 0 if ATM

                                        // strike (assuming forward prices)
                                        Real k = spot->value() * mon * forTS->discount(date) / domTS->discount(date);
                                        Volatility vol = wrapper->blackVol(date, k, true);
                                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                                        simDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, idx),
                                                           std::forward_as_tuple(q));
                                        quotes[j][i] = Handle<Quote>(q);
                                    }
                                }
                            } else { // if stdDevPoints

                                // times (for fwds)
                                for (Size i = 0; i < n; i++) {
                                    Date date = asof_ + parameters->fxVolExpiries()[i];
                                    times.push_back(wrapper->timeFromReference(date));
                                    dates.push_back(date);
                                }

                                // forwards
                                vector<Real> fwds;
                                vector<Real> atmVols;
                                for (Size i = 0; i < parameters->fxVolExpiries().size(); i++) {
                                    fwds.push_back(spot->value() * forTS->discount(times[i]) /
                                                   domTS->discount(times[i]));
                                    atmVols.push_back(wrapper->blackVol(dates[i], spot->value()));
                                    DLOG("atmVol(s) is " << atmVols.back() << " on date " << dates[i]);
                                }

                                // interpolations
                                Interpolation forwardCurve =
                                    Linear().interpolate(times.begin(), times.end(), fwds.begin());
                                Interpolation atmVolCurve =
                                    Linear().interpolate(times.begin(), times.end(), atmVols.begin());

                                // populate quotes
                                BlackVarianceSurfaceStdDevs::populateVolMatrix(
                                    wrapper, quotes, parameters->fxVolExpiries(), parameters->fxVolStdDevs(name),
                                    forwardCurve, atmVolCurve);

                                // sort out simDataTemp
                                for (Size i = 0; i < parameters->fxVolExpiries().size(); i++) {
                                    for (Size j = 0; j < parameters->fxVolStdDevs(name).size(); j++) {
                                        Size idx = j * n + i;
                                        boost::shared_ptr<Quote> q = quotes[j][i].currentLink();
                                        boost::shared_ptr<SimpleQuote> sq = boost::dynamic_pointer_cast<SimpleQuote>(q);
                                        QL_REQUIRE(sq, "Quote is not a SimpleQuote"); // why do we need this?
                                        simDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, idx),
                                                           std::forward_as_tuple(sq));
                                    }
                                }
                            }

                            // build surface
                            boost::shared_ptr<BlackVolTermStructure> fxVolCurve;
                            if (parameters->fxVolIsSurface(name)) {

                                // Attempt to get the relevant yield curves from this scenario simulation market
                                Handle<YieldTermStructure> forTS =
                                    getYieldCurve(foreignTsId, todaysMarketParams, Market::defaultConfiguration);
                                TLOG("Foreign term structure '" << foreignTsId << "' from sim market is "
                                                                << (forTS.empty() ? "empty" : "not empty"));
                                Handle<YieldTermStructure> domTS =
                                    getYieldCurve(domesticTsId, todaysMarketParams, Market::defaultConfiguration);
                                TLOG("Domestic term structure '" << domesticTsId << "' from sim market is "
                                                                 << (domTS.empty() ? "empty" : "not empty"));

                                // If either term structure is empty, fall back on this scenario simulation market's
                                // discount curves
                                if (forTS.empty() || domTS.empty()) {
                                    TLOG("Falling back on the discount curves for " << forCcy << " and " << domCcy
                                                                                    << " from sim market");
                                    forTS = discountCurve(forCcy);
                                    domTS = discountCurve(domCcy);
                                }
                                bool stickyStrike = true;
                                bool flatExtrapolation = true; // flat extrapolation of strikes at far ends.

                                if (parameters->useMoneyness(name)) { // moneyness
                                    fxVolCurve = boost::shared_ptr<BlackVolTermStructure>(
                                        new BlackVarianceSurfaceMoneynessForward(
                                            cal, spot, times, parameters->fxVolMoneyness(name), quotes, dc, forTS,
                                            domTS, stickyStrike, flatExtrapolation));
                                } else { // standard deviations
                                    fxVolCurve =
                                        boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceSurfaceStdDevs(
                                            cal, spot, times, parameters->fxVolStdDevs(name), quotes, dc, forTS, domTS,
                                            stickyStrike, flatExtrapolation));
                                }

                            } else {
                                fxVolCurve = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve3(
                                    0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes[0], false));
                            }
                            fvh = Handle<BlackVolTermStructure>(fxVolCurve);

                        } else {
                            string decayModeString = parameters->fxVolDecayMode();
                            LOG("Deterministic FX Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // currently only curves (i.e. strike indepdendent) FX volatility structures are
                            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
                            // strike stickyness, since then no yield term structures and no fx spot are required
                            // that define the ATM level - to be revisited when FX surfaces are supported
                            fvh = Handle<BlackVolTermStructure>(
                                boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode, StickyStrike));
                        }

                        fvh->enableExtrapolation();
                        fxVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
                            make_pair(Market::defaultConfiguration, name), fvh));

                        // build inverted surface
                        QL_REQUIRE(name.size() == 6, "Invalid Ccy pair " << name);
                        string reverse = name.substr(3) + name.substr(0, 3);
                        Handle<QuantLib::BlackVolTermStructure> ifvh(
                            boost::make_shared<BlackInvertedVolTermStructure>(fvh));
                        ifvh->enableExtrapolation();
                        fxVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
                            make_pair(Market::defaultConfiguration, reverse), ifvh));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::EquityVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        Handle<BlackVolTermStructure> wrapper = initMarket->equityVol(name, configuration);

                        Handle<BlackVolTermStructure> evh;

                        if (param.second.first) {
                            Handle<Quote> spot = equitySpots_[make_pair(Market::defaultConfiguration, name)];
                            Size n = parameters->equityVolMoneyness().size();
                            Size m = parameters->equityVolExpiries().size();
                            vector<vector<Handle<Quote>>> quotes(n, vector<Handle<Quote>>(m, Handle<Quote>()));
                            vector<Time> times(m);
                            Calendar cal = wrapper->calendar();
                            DayCounter dc = ore::data::parseDayCounter(parameters->equityVolDayCounter(name));
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
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, idx),
                                                       std::forward_as_tuple(q));
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }
                            boost::shared_ptr<BlackVolTermStructure> eqVolCurve;
                            if (!parameters->simulateEquityVolATMOnly()) {
                                LOG("Simulating EQ Vols (BlackVarianceSurfaceMoneyness) for " << name);
                                // If true, the strikes are fixed, if false they move with the spot handle
                                // Should probably be false, but some people like true for sensi runs.
                                bool stickyStrike = true;

                                eqVolCurve =
                                    boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceSurfaceMoneynessSpot(
                                        cal, spot, times, parameters->equityVolMoneyness(), quotes, dc, stickyStrike));
                                eqVolCurve->enableExtrapolation();
                            } else {
                                LOG("Simulating EQ Vols (BlackVarianceCurve3) for " << name);
                                eqVolCurve = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve3(
                                    0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes[0], false));
                            }

                            // if we have a surface but are only simulating atm vols we wrap the atm curve and the full
                            // t0 surface
                            if (parameters->equityVolIsSurface() && parameters->simulateEquityVolATMOnly()) {
                                LOG("Simulating EQ Vols (EquityVolatilityConstantSpread) for " << name);
                                evh = Handle<BlackVolTermStructure>(boost::make_shared<EquityVolatilityConstantSpread>(
                                    Handle<BlackVolTermStructure>(eqVolCurve), wrapper));
                            } else {
                                evh = Handle<BlackVolTermStructure>(eqVolCurve);
                            }
                        } else {
                            string decayModeString = parameters->equityVolDecayMode();
                            DLOG("Deterministic EQ Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // currently only curves (i.e. strike indepdendent) EQ volatility structures are
                            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
                            // strike stickyness, since then no yield term structures and no EQ spot are required
                            // that define the ATM level - to be revisited when EQ surfaces are supported
                            evh = Handle<BlackVolTermStructure>(
                                boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode, StickyStrike));
                        }
                        if (wrapper->allowsExtrapolation())
                            evh->enableExtrapolation();
                        equityVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
                            make_pair(Market::defaultConfiguration, name), evh));
                        DLOG("EQ volatility curve built for " << name);
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::BaseCorrelation:
                for (const auto& name : param.second.second) {
                    try {
                        Handle<BaseCorrelationTermStructure<BilinearInterpolation>> wrapper =
                            initMarket->baseCorrelation(name, configuration);
                        if (!param.second.first)
                            baseCorrelations_.insert(
                                pair<pair<string, string>, Handle<BaseCorrelationTermStructure<BilinearInterpolation>>>(
                                    make_pair(Market::defaultConfiguration, name), wrapper));
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
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, i * nt + j),
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
                            DayCounter dc = ore::data::parseDayCounter(parameters->baseCorrelationDayCounter(name));
                            boost::shared_ptr<BilinearBaseCorrelationTermStructure> bcp =
                                boost::make_shared<BilinearBaseCorrelationTermStructure>(
                                    wrapper->settlementDays(), wrapper->calendar(), wrapper->businessDayConvention(),
                                    terms, parameters->baseCorrelationDetachmentPoints(), quotes, dc);

                            bcp->enableExtrapolation(wrapper->allowsExtrapolation());
                            Handle<BilinearBaseCorrelationTermStructure> bch(bcp);
                            baseCorrelations_.insert(
                                pair<pair<string, string>, Handle<BaseCorrelationTermStructure<BilinearInterpolation>>>(
                                    make_pair(Market::defaultConfiguration, name), bch));
                        }
                        DLOG("Base correlations built for " << name);
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CPIIndex:
                for (const auto& name : param.second.second) {
                    try {
                        DLOG("adding " << name << " base CPI price");
                        Handle<ZeroInflationIndex> zeroInflationIndex =
                            initMarket->zeroInflationIndex(name, configuration);
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
                            make_pair(Market::defaultConfiguration, name), inflObserver));
                        simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                           std::forward_as_tuple(q));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::ZeroInflationCurve:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " zero inflation curve");

                        Handle<ZeroInflationIndex> inflationIndex = initMarket->zeroInflationIndex(name, configuration);
                        Handle<ZeroInflationTermStructure> inflationTs = inflationIndex->zeroInflationTermStructure();
                        vector<string> keys(parameters->zeroInflationTenors(name).size());

                        Date date0 = asof_ - inflationTs->observationLag();
                        DayCounter dc = ore::data::parseDayCounter(parameters->zeroInflationDayCounter(name));
                        vector<Date> quoteDates;
                        vector<Time> zeroCurveTimes(
                            1, -dc.yearFraction(inflationPeriod(date0, inflationTs->frequency()).first, asof_));
                        vector<Handle<Quote>> quotes;
                        QL_REQUIRE(parameters->zeroInflationTenors(name).front() > 0 * Days,
                                   "zero inflation tenors must not include t=0");

                        for (auto& tenor : parameters->zeroInflationTenors(name)) {
                            Date inflDate = inflationPeriod(date0 + tenor, inflationTs->frequency()).first;
                            zeroCurveTimes.push_back(dc.yearFraction(asof_, inflDate));
                            quoteDates.push_back(asof_ + tenor);
                        }

                        for (Size i = 1; i < zeroCurveTimes.size(); i++) {
                            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(inflationTs->zeroRate(quoteDates[i - 1])));
                            Handle<Quote> qh(q);
                            if (i == 1) {
                                // add the zero rate at first tenor to the T0 time, to ensure flat interpolation of T1
                                // rate for time t T0 < t < T1
                                quotes.push_back(qh);
                            }
                            quotes.push_back(qh);
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(param.first, name, i - 1),
                                               std::forward_as_tuple(q));
                            DLOG("ScenarioSimMarket index curve " << name << " zeroRate[" << i << "]=" << q->value());
                        }

                        // Get the configured nominal term structure from this scenario sim market if possible
                        // 1) Look for zero inflation curve configuration ID in zero inflation curves of todays market
                        string zeroInflationConfigId;
                        if (todaysMarketParams.hasConfiguration(configuration) &&
                            todaysMarketParams.hasMarketObject(MarketObject::ZeroInflationCurve)) {
                            auto m = todaysMarketParams.mapping(MarketObject::ZeroInflationCurve, configuration);
                            auto it = m.find(name);
                            if (it != m.end()) {
                                string zeroInflationSpecId = it->second;
                                TLOG("Got spec ID " << zeroInflationSpecId << " for zero inflation index " << name);
                                auto zeroInflationSpec = parseCurveSpec(zeroInflationSpecId);
                                QL_REQUIRE(zeroInflationSpec->baseType() == CurveSpec::CurveType::Inflation,
                                           "Expected the curve "
                                               << "spec type for " << zeroInflationSpecId << " to be 'Inflation'");
                                zeroInflationConfigId = zeroInflationSpec->curveConfigID();
                            }
                        }

                        // 2) Get the nominal term structure ID from the zero inflation curve configuration
                        string nominalTsId;
                        if (!zeroInflationConfigId.empty() &&
                            curveConfigs.hasInflationCurveConfig(zeroInflationConfigId)) {
                            auto zeroInflationConfig = curveConfigs.inflationCurveConfig(zeroInflationConfigId);
                            nominalTsId = zeroInflationConfig->nominalTermStructure();
                            TLOG("Got nominal term structure ID '" << nominalTsId << "' from config with ID '"
                                                                   << zeroInflationConfigId << "'");
                        }

                        // 3) Get the nominal term structure from this scenario simulation market
                        Handle<YieldTermStructure> nominalTs =
                            getYieldCurve(nominalTsId, todaysMarketParams, Market::defaultConfiguration);
                        TLOG("Nominal term structure '" << nominalTsId << "' from sim market is "
                                                        << (nominalTs.empty() ? "empty" : "not empty"));

                        // If nominal term structure is empty, fall back on this scenario simulation market's discount
                        // curve
                        if (nominalTs.empty()) {
                            string ccy = inflationIndex->currency().code();
                            TLOG("Falling back on the discount curve for currency '"
                                 << ccy << "', the currency of inflation index '" << name << "'");
                            nominalTs = discountCurve(ccy);
                        }

                        // FIXME: Settlement days set to zero - needed for floating term structure implementation
                        boost::shared_ptr<ZeroInflationTermStructure> zeroCurve;
                        dc = ore::data::parseDayCounter(parameters->zeroInflationDayCounter(name));
                        zeroCurve = boost::shared_ptr<ZeroInflationCurveObserverMoving<Linear>>(
                            new ZeroInflationCurveObserverMoving<Linear>(
                                0, inflationIndex->fixingCalendar(), dc, inflationTs->observationLag(),
                                inflationTs->frequency(), inflationTs->indexIsInterpolated(), nominalTs, zeroCurveTimes,
                                quotes, inflationTs->seasonality()));

                        Handle<ZeroInflationTermStructure> its(zeroCurve);
                        its->enableExtrapolation();
                        boost::shared_ptr<ZeroInflationIndex> i =
                            ore::data::parseZeroInflationIndex(name, false, Handle<ZeroInflationTermStructure>(its));
                        Handle<ZeroInflationIndex> zh(i);
                        zeroInflationIndices_.insert(pair<pair<string, string>, Handle<ZeroInflationIndex>>(
                            make_pair(Market::defaultConfiguration, name), zh));

                        LOG("building " << name << " zero inflation curve done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " zero inflation cap/floor volatility curve...");
                        Handle<CPIVolatilitySurface> wrapper =
                            initMarket->cpiInflationCapFloorVolatilitySurface(name, configuration);
                        Handle<ZeroInflationIndex> zeroInflationIndex =
                            initMarket->zeroInflationIndex(name, configuration);
                        // LOG("Initial market zero inflation cap/floor volatility type = " <<
                        // wrapper->volatilityType());

                        Handle<CPIVolatilitySurface> hCpiVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            LOG("Simulating zero inflation cap/floor vols for index name " << name);
                            vector<Period> optionTenors = parameters->zeroInflationCapFloorVolExpiries(name);
                            vector<Date> optionDates(optionTenors.size());
                            vector<Real> strikes = parameters->zeroInflationCapFloorVolStrikes(name);
                            vector<vector<Handle<Quote>>> quotes(
                                optionTenors.size(), vector<Handle<Quote>>(strikes.size(), Handle<Quote>()));
                            for (Size i = 0; i < optionTenors.size(); ++i) {
                                optionDates[i] = wrapper->optionDateFromTenor(optionTenors[i]);
                                for (Size j = 0; j < strikes.size(); ++j) {
                                    Real vol =
                                        wrapper->volatility(optionTenors[i], strikes[j], wrapper->observationLag(),
                                                            wrapper->allowsExtrapolation());
                                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                                    Size index = i * strikes.size() + j;
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, index),
                                                       std::forward_as_tuple(q));
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }
                            DayCounter dc =
                                ore::data::parseDayCounter(parameters->zeroInflationCapFloorVolDayCounter(name));
                            boost::shared_ptr<InterpolatedCPIVolatilitySurface<Bilinear>> interpolatedCpiVol =
                                boost::make_shared<InterpolatedCPIVolatilitySurface<Bilinear>>(
                                    optionTenors, strikes, quotes, zeroInflationIndex.currentLink(),
                                    wrapper->settlementDays(), wrapper->calendar(), wrapper->businessDayConvention(),
                                    wrapper->dayCounter(), wrapper->observationLag());
                            boost::shared_ptr<CPIVolatilitySurface> cpiVol(interpolatedCpiVol);
                            hCpiVol = Handle<CPIVolatilitySurface>(cpiVol);

                            // Check that we have correctly copied today's market vol structure into the sim market
                            // structure
                            for (Size i = 0; i < optionTenors.size(); ++i) {
                                for (Size j = 0; j < strikes.size(); ++j) {
                                    Date d = optionDates[i];
                                    Real vol1 = wrapper->volatility(d, strikes[j]);
                                    Real vol2 = hCpiVol->volatility(d, strikes[j]);
                                    // DLOG("CPI Vol Check " << i << " " << optionTenors[i] << " " << j << " "
                                    //                       << std::setprecision(4) << strikes[j] << " "
                                    //                       << std::setprecision(6) << vol1 << " " << vol2 << " "
                                    //                       << vol2 - vol1);
                                    QL_REQUIRE(
                                        close_enough(vol1 - vol2, 0.0),
                                        "Simulation market CPI vol does not match today's market CPI vol for expiry "
                                            << optionTenors[i] << " and strike " << strikes[j]);
                                }
                            }

                        } else {
                            // string decayModeString = parameters->zeroInflationCapFloorVolDecayMode();
                            // ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            // boost::shared_ptr<CPIVolatilitySurface> cpiVol =
                            //     boost::make_shared<QuantExt::DynamicCPIVolatilitySurface>(*wrapper, decayMode);
                            // hCpiVol = Handle<CPIVolatilitySurface>(cpiVol);#
                            // FIXME
                            hCpiVol = wrapper;
                        }
                        if (wrapper->allowsExtrapolation())
                            hCpiVol->enableExtrapolation();
                        cpiInflationCapFloorVolatilitySurfaces_.emplace(
                            std::piecewise_construct, std::forward_as_tuple(Market::defaultConfiguration, name),
                            std::forward_as_tuple(hCpiVol));

                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::YoYInflationCurve:
                for (const auto& name : param.second.second) {
                    try {
                        Handle<YoYInflationIndex> yoyInflationIndex =
                            initMarket->yoyInflationIndex(name, configuration);
                        Handle<YoYInflationTermStructure> yoyInflationTs =
                            yoyInflationIndex->yoyInflationTermStructure();
                        vector<string> keys(parameters->yoyInflationTenors(name).size());

                        Date date0 = asof_ - yoyInflationTs->observationLag();
                        DayCounter dc = ore::data::parseDayCounter(parameters->yoyInflationDayCounter(name));
                        vector<Date> quoteDates;
                        vector<Time> yoyCurveTimes(
                            1, -dc.yearFraction(inflationPeriod(date0, yoyInflationTs->frequency()).first, asof_));
                        vector<Handle<Quote>> quotes;
                        QL_REQUIRE(parameters->yoyInflationTenors(name).front() > 0 * Days,
                                   "zero inflation tenors must not include t=0");

                        for (auto& tenor : parameters->yoyInflationTenors(name)) {
                            Date inflDate = inflationPeriod(date0 + tenor, yoyInflationTs->frequency()).first;
                            yoyCurveTimes.push_back(dc.yearFraction(asof_, inflDate));
                            quoteDates.push_back(asof_ + tenor);
                        }

                        for (Size i = 1; i < yoyCurveTimes.size(); i++) {
                            boost::shared_ptr<SimpleQuote> q(
                                new SimpleQuote(yoyInflationTs->yoyRate(quoteDates[i - 1])));
                            Handle<Quote> qh(q);
                            if (i == 1) {
                                // add the zero rate at first tenor to the T0 time, to ensure flat interpolation of T1
                                // rate for time t T0 < t < T1
                                quotes.push_back(qh);
                            }
                            quotes.push_back(qh);
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(param.first, name, i - 1),
                                               std::forward_as_tuple(q));
                            DLOG("ScenarioSimMarket index curve " << name << " zeroRate[" << i << "]=" << q->value());
                        }

                        // Get the configured nominal term structure from this scenario sim market if possible
                        // 1) Look for yoy inflation curve configuration ID in yoy inflation curves of todays market
                        string yoyInflationConfigId;
                        if (todaysMarketParams.hasConfiguration(configuration) &&
                            todaysMarketParams.hasMarketObject(MarketObject::YoYInflationCurve)) {
                            auto m = todaysMarketParams.mapping(MarketObject::YoYInflationCurve, configuration);
                            auto it = m.find(name);
                            if (it != m.end()) {
                                string yoyInflationSpecId = it->second;
                                TLOG("Got spec ID " << yoyInflationSpecId << " for yoy inflation index " << name);
                                auto yoyInflationSpec = parseCurveSpec(yoyInflationSpecId);
                                QL_REQUIRE(yoyInflationSpec->baseType() == CurveSpec::CurveType::Inflation,
                                           "Expected the curve "
                                               << "spec type for " << yoyInflationSpecId << " to be 'Inflation'");
                                yoyInflationConfigId = yoyInflationSpec->curveConfigID();
                            }
                        }

                        // 2) Get the nominal term structure ID from the yoy inflation curve configuration
                        string nominalTsId;
                        if (!yoyInflationConfigId.empty() &&
                            curveConfigs.hasInflationCurveConfig(yoyInflationConfigId)) {
                            auto yoyInflationConfig = curveConfigs.inflationCurveConfig(yoyInflationConfigId);
                            nominalTsId = yoyInflationConfig->nominalTermStructure();
                            TLOG("Got nominal term structure ID '" << nominalTsId << "' from config with ID '"
                                                                   << yoyInflationConfigId << "'");
                        }

                        // 3) Get the nominal term structure from this scenario simulation market
                        Handle<YieldTermStructure> nominalTs =
                            getYieldCurve(nominalTsId, todaysMarketParams, Market::defaultConfiguration);
                        TLOG("Nominal term structure '" << nominalTsId << "' from sim market is "
                                                        << (nominalTs.empty() ? "empty" : "not empty"));

                        // If nominal term structure is empty, fall back on this scenario simulation market's discount
                        // curve
                        if (nominalTs.empty()) {
                            string ccy = yoyInflationIndex->currency().code();
                            TLOG("Falling back on the discount curve for currency '"
                                 << ccy << "', the currency of inflation index '" << name << "'");
                            nominalTs = discountCurve(ccy);
                        }

                        boost::shared_ptr<YoYInflationTermStructure> yoyCurve;
                        // Note this is *not* a floating term structure, it is only suitable for sensi runs
                        // TODO: floating
                        yoyCurve = boost::shared_ptr<YoYInflationCurveObserverMoving<Linear>>(
                            new YoYInflationCurveObserverMoving<Linear>(
                                0, yoyInflationIndex->fixingCalendar(), dc, yoyInflationTs->observationLag(),
                                yoyInflationTs->frequency(), yoyInflationTs->indexIsInterpolated(), nominalTs,
                                yoyCurveTimes, quotes, yoyInflationTs->seasonality()));

                        Handle<YoYInflationTermStructure> its(yoyCurve);
                        its->enableExtrapolation();
                        boost::shared_ptr<YoYInflationIndex> i(yoyInflationIndex->clone(its));
                        Handle<YoYInflationIndex> zh(i);
                        yoyInflationIndices_.insert(pair<pair<string, string>, Handle<YoYInflationIndex>>(
                            make_pair(Market::defaultConfiguration, name), zh));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building " << name << " yoy inflation cap/floor volatility curve...");
                        Handle<QuantExt::YoYOptionletVolatilitySurface> wrapper =
                            initMarket->yoyCapFloorVol(name, configuration);
                        LOG("Initial market "
                            << name << " yoy inflation cap/floor volatility type = " << wrapper->volatilityType());
                        Handle<QuantExt::YoYOptionletVolatilitySurface> hYoYCapletVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            LOG("Simulating yoy inflation optionlet vols for index name " << name);
                            vector<Period> optionTenors = parameters->yoyInflationCapFloorVolExpiries(name);
                            vector<Date> optionDates(optionTenors.size());
                            vector<Real> strikes = parameters->yoyInflationCapFloorVolStrikes(name);
                            vector<vector<Handle<Quote>>> quotes(
                                optionTenors.size(), vector<Handle<Quote>>(strikes.size(), Handle<Quote>()));
                            for (Size i = 0; i < optionTenors.size(); ++i) {
                                optionDates[i] = wrapper->yoyVolSurface()->optionDateFromTenor(optionTenors[i]);
                                for (Size j = 0; j < strikes.size(); ++j) {
                                    Real vol =
                                        wrapper->volatility(optionTenors[i], strikes[j], wrapper->observationLag(),
                                                            wrapper->allowsExtrapolation());
                                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
                                    Size index = i * strikes.size() + j;
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, index),
                                                       std::forward_as_tuple(q));
                                    quotes[i][j] = Handle<Quote>(q);
                                    TLOG("ScenarioSimMarket yoy cf vol " << name << " tenor #" << i << " strike #" << j
                                                                         << " " << vol);
                                }
                            }
                            DayCounter dc =
                                ore::data::parseDayCounter(parameters->yoyInflationCapFloorVolDayCounter(name));
                            boost::shared_ptr<StrippedYoYInflationOptionletVol> yoyoptionlet =
                                boost::make_shared<StrippedYoYInflationOptionletVol>(
                                    0, wrapper->yoyVolSurface()->calendar(),
                                    wrapper->yoyVolSurface()->businessDayConvention(), dc, wrapper->observationLag(),
                                    wrapper->yoyVolSurface()->frequency(),
                                    wrapper->yoyVolSurface()->indexIsInterpolated(), optionDates, strikes, quotes,
                                    wrapper->volatilityType(), wrapper->displacement());
                            boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyoptionletvolsurface =
                                boost::make_shared<QuantExt::YoYOptionletVolatilitySurface>(
                                    yoyoptionlet, wrapper->volatilityType(), wrapper->displacement());
                            hYoYCapletVol = Handle<QuantExt::YoYOptionletVolatilitySurface>(yoyoptionletvolsurface);
                        } else {
                            string decayModeString = parameters->yoyInflationCapFloorVolDecayMode();
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            boost::shared_ptr<QuantExt::DynamicYoYOptionletVolatilitySurface> yoyCapletVol =
                                boost::make_shared<QuantExt::DynamicYoYOptionletVolatilitySurface>(*wrapper, decayMode);
                            hYoYCapletVol = Handle<QuantExt::YoYOptionletVolatilitySurface>(yoyCapletVol);
                        }
                        if (wrapper->allowsExtrapolation())
                            hYoYCapletVol->enableExtrapolation();
                        yoyCapFloorVolSurfaces_.emplace(std::piecewise_construct,
                                                        std::forward_as_tuple(Market::defaultConfiguration, name),
                                                        std::forward_as_tuple(hYoYCapletVol));
                        LOG("Simulaton market yoy inflation cap/floor volatility type = "
                            << hYoYCapletVol->volatilityType());
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CommodityCurve:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("building commodity curve for " << name);

                        // Time zero initial market commodity curve
                        Handle<PriceTermStructure> initialCommodityCurve =
                            initMarket->commodityPriceCurve(name, configuration);
                        bool allowsExtrapolation = initialCommodityCurve->allowsExtrapolation();

                        // Get the configured simulation tenors. Simulation tenors being empty at this point means
                        // that we wish to use the pillar date points from the t_0 market PriceTermStructure.
                        vector<Period> simulationTenors = parameters->commodityCurveTenors(name);
                        DayCounter commodityCurveDayCounter =
                            parseDayCounter(parameters->commodityCurveDayCounter(name));
                        if (simulationTenors.empty()) {
                            simulationTenors.reserve(initialCommodityCurve->pillarDates().size());
                            for (const Date& d : initialCommodityCurve->pillarDates()) {
                                QL_REQUIRE(d >= asof_, "Commodity curve pillar date (" << io::iso_date(d)
                                                                                       << ") must be after as of ("
                                                                                       << io::iso_date(asof_) << ").");
                                simulationTenors.push_back(Period(d - asof_, Days));
                            }

                            // It isn't great to be updating parameters here. However, actual tenors are requested
                            // downstream from parameters and they need to be populated.
                            parameters->setCommodityCurveTenors(name, simulationTenors);
                        }

                        // Get prices at specified simulation times from time 0 market curve and place in quotes
                        vector<Handle<Quote>> quotes(simulationTenors.size());
                        for (Size i = 0; i < simulationTenors.size(); i++) {
                            Date d = asof_ + simulationTenors[i];
                            Real price = initialCommodityCurve->price(d, allowsExtrapolation);
                            boost::shared_ptr<SimpleQuote> quote = boost::make_shared<SimpleQuote>(price);
                            quotes[i] = Handle<Quote>(quote);

                            // If we are simulating commodities, add the quote to simData_
                            if (param.second.first) {
                                simDataTmp.emplace(piecewise_construct, forward_as_tuple(param.first, name, i),
                                                   forward_as_tuple(quote));
                            }
                        }

                        // Create a commodity price curve with simulation tenors as pillars and store
                        // Hard-coded linear flat interpolation here - may need to make this more dynamic
                        Handle<PriceTermStructure> simCommodityCurve(
                            boost::make_shared<InterpolatedPriceCurve<LinearFlat>>(
                                simulationTenors, quotes, commodityCurveDayCounter, initialCommodityCurve->currency()));
                        simCommodityCurve->enableExtrapolation(allowsExtrapolation);

                        commodityCurves_.emplace(piecewise_construct,
                                                 forward_as_tuple(Market::defaultConfiguration, name),
                                                 forward_as_tuple(simCommodityCurve));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CommodityVolatility:
                for (const auto& name : param.second.second) {
                    try {
                        // Get initial base volatility structure
                        Handle<BlackVolTermStructure> baseVol = initMarket->commodityVolatility(name, configuration);

                        Handle<BlackVolTermStructure> newVol;
                        if (param.second.first) {
                            Handle<Quote> spot(boost::make_shared<SimpleQuote>(
                                initMarket->commodityPriceCurve(name, configuration)->price(0)));
                            const vector<Real>& moneyness = parameters->commodityVolMoneyness(name);
                            QL_REQUIRE(!moneyness.empty(), "Commodity volatility moneyness for "
                                                               << name << " should have at least one element");
                            const vector<Period>& expiries = parameters->commodityVolExpiries(name);
                            QL_REQUIRE(!expiries.empty(), "Commodity volatility expiries for "
                                                              << name << " should have at least one element");

                            // Create surface of quotes
                            vector<vector<Handle<Quote>>> quotes(moneyness.size(),
                                                                 vector<Handle<Quote>>(expiries.size()));
                            vector<Time> expiryTimes(expiries.size());
                            Size index = 0;
                            DayCounter dayCounter = baseVol->dayCounter();

                            for (Size i = 0; i < quotes.size(); i++) {
                                Real strike = moneyness[i] * spot->value();
                                for (Size j = 0; j < quotes[0].size(); j++) {
                                    if (i == 0)
                                        expiryTimes[j] = dayCounter.yearFraction(asof_, asof_ + expiries[j]);
                                    boost::shared_ptr<SimpleQuote> quote =
                                        boost::make_shared<SimpleQuote>(baseVol->blackVol(asof_ + expiries[j], strike));
                                    simDataTmp.emplace(piecewise_construct,
                                                       forward_as_tuple(param.first, name, index++),
                                                       forward_as_tuple(quote));
                                    quotes[i][j] = Handle<Quote>(quote);
                                }
                            }

                            // Create volatility structure
                            if (moneyness.size() == 1) {
                                // We have a term structure of volatilities with no strike dependence
                                LOG("Simulating commodity volatilites for " << name << " using BlackVarianceCurve3.");
                                newVol = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceCurve3>(
                                    0, NullCalendar(), baseVol->businessDayConvention(), dayCounter, expiryTimes,
                                    quotes[0], false));
                            } else {
                                // We have a volatility surface
                                LOG("Simulating commodity volatilites for "
                                    << name << " using BlackVarianceSurfaceMoneynessSpot.");
                                bool stickyStrike = true;
                                bool flatExtrapMoneyness = true;
                                newVol =
                                    Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceSurfaceMoneynessSpot>(
                                        baseVol->calendar(), spot, expiryTimes, moneyness, quotes, dayCounter,
                                        stickyStrike, flatExtrapMoneyness));
                            }

                        } else {
                            string decayModeString = parameters->commodityVolDecayMode();
                            DLOG("Deterministic commodity volatilities with decay mode " << decayModeString << " for "
                                                                                         << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            // Copy what was done for equity here
                            // May need to revisit when looking at commodity RFE
                            newVol = Handle<BlackVolTermStructure>(
                                boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    baseVol, 0, NullCalendar(), decayMode, StickyStrike));
                        }

                        newVol->enableExtrapolation(baseVol->allowsExtrapolation());

                        commodityVols_.emplace(piecewise_construct,
                                               forward_as_tuple(Market::defaultConfiguration, name),
                                               forward_as_tuple(newVol));

                        DLOG("Commodity volatility curve built for " << name);
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::Correlation:
                for (const auto& name : param.second.second) {
                    try {
                        LOG("Adding correlations for " << name << " from configuration " << configuration);

                        // Look for '&' first
                        // see todaysmarket.cpp for similar logic
                        string delim;
                        if (name.find('&') != std::string::npos)
                            delim = "&";
                        else
                            // otherwise fall back on old behavior
                            delim = ":";

                        vector<string> tokens;
                        boost::split(tokens, name, boost::is_any_of(delim));
                        QL_REQUIRE(tokens.size() == 2, "not a valid correlation pair: " << name);
                        pair<string, string> pair = std::make_pair(tokens[0], tokens[1]);

                        boost::shared_ptr<QuantExt::CorrelationTermStructure> corr;
                        Handle<QuantExt::CorrelationTermStructure> baseCorr =
                            initMarket->correlationCurve(pair.first, pair.second, configuration);

                        Handle<QuantExt::CorrelationTermStructure> ch;
                        if (param.second.first) {
                            Size n = parameters->correlationStrikes().size();
                            Size m = parameters->correlationExpiries().size();
                            vector<vector<Handle<Quote>>> quotes(n, vector<Handle<Quote>>(m, Handle<Quote>()));
                            vector<Time> times(m);
                            Calendar cal = baseCorr->calendar();
                            DayCounter dc =
                                ore::data::parseDayCounter(parameters->correlationDayCounter(pair.first, pair.second));

                            for (Size i = 0; i < n; i++) {
                                Real strike = parameters->correlationStrikes()[i];

                                for (Size j = 0; j < m; j++) {
                                    // Index is expiries then strike TODO: is this the best?
                                    Size idx = i * m + j;
                                    times[j] = dc.yearFraction(asof_, asof_ + parameters->correlationExpiries()[j]);
                                    Real correlation =
                                        baseCorr->correlation(asof_ + parameters->correlationExpiries()[j], strike);
                                    boost::shared_ptr<SimpleQuote> q(new SimpleQuote(correlation));
                                    simDataTmp.emplace(
                                        std::piecewise_construct,
                                        std::forward_as_tuple(RiskFactorKey::KeyType::Correlation, name, idx),
                                        std::forward_as_tuple(q));
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }

                            if (n == 1 && m == 1) {
                                ch = Handle<QuantExt::CorrelationTermStructure>(boost::make_shared<FlatCorrelation>(
                                    baseCorr->settlementDays(), cal, quotes[0][0], dc));
                            } else if (n == 1) {
                                ch = Handle<QuantExt::CorrelationTermStructure>(
                                    boost::make_shared<InterpolatedCorrelationCurve<Linear>>(times, quotes[0], dc,
                                                                                             cal));
                            } else {
                                QL_FAIL("only atm or flat correlation termstructures currently supported");
                            }

                            ch->enableExtrapolation(baseCorr->allowsExtrapolation());
                        } else {
                            ch = Handle<QuantExt::CorrelationTermStructure>(*baseCorr);
                        }

                        correlationCurves_[make_tuple(Market::defaultConfiguration, pair.first, pair.second)] = ch;
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CPR:
                for (const auto& name : param.second.second) {
                    try {
                        DLOG("Adding cpr " << name << " from configuration " << configuration);
                        boost::shared_ptr<SimpleQuote> cprQuote(
                            new SimpleQuote(initMarket->cpr(name, configuration)->value()));
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(cprQuote));
                        }
                        cprs_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(cprQuote)));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e);
                    }
                }
                break;

            case RiskFactorKey::KeyType::None:
                WLOG("RiskFactorKey None not yet implemented");
                break;
            }

            simData_.insert(simDataTmp.begin(), simDataTmp.end());
        } catch (const std::exception& e) {
            ALOG("ScenarioSimMarket::ScenarioSimMarket() top level catch " << e.what());
            processException(continueOnError, e);
        }
    }

    // swap indices
    LOG("building swap indices...");
    for (const auto& it : parameters->swapIndices()) {
        const string& indexName = it.first;
        const string& discounting = it.second;
        LOG("Adding swap index " << indexName << " with discounting index " << discounting);

        try {
            addSwapIndex(indexName, discounting, Market::defaultConfiguration);
        } catch (const std::exception& e) {
            processException(continueOnError, e);
        }
        LOG("Adding swap index " << indexName << " done.");
    }

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
            if (filter_->allow(key)) {
                it->second->setValue(scenario->get(key));
            }
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

Handle<YieldTermStructure> ScenarioSimMarket::getYieldCurve(const string& yieldSpecId,
                                                            const TodaysMarketParameters& todaysMarketParams,
                                                            const string& configuration,
                                                            const boost::shared_ptr<Market>& market) const {

    // If yield spec ID is "", return empty Handle
    if (yieldSpecId.empty())
        return Handle<YieldTermStructure>();

    if (todaysMarketParams.hasConfiguration(configuration)) {
        // Look for yield spec ID in index curves of todays market
        if (todaysMarketParams.hasMarketObject(MarketObject::IndexCurve)) {
            for (const auto& indexMapping : todaysMarketParams.mapping(MarketObject::IndexCurve, configuration)) {
                if (indexMapping.second == yieldSpecId) {
                    if (market) {
                        return market->iborIndex(indexMapping.first, configuration)->forwardingTermStructure();
                    } else {
                        return iborIndex(indexMapping.first, configuration)->forwardingTermStructure();
                    }
                }
            }
        }

        // Look for yield spec ID in yield curves of todays market
        if (todaysMarketParams.hasMarketObject(MarketObject::YieldCurve)) {
            for (const auto& yieldMapping : todaysMarketParams.mapping(MarketObject::YieldCurve, configuration)) {
                if (yieldMapping.second == yieldSpecId) {
                    if (market) {
                        return market->yieldCurve(yieldMapping.first, configuration);
                    } else {
                        return yieldCurve(yieldMapping.first, configuration);
                    }
                }
            }
        }

        // Look for yield spec ID in discount curves of todays market
        if (todaysMarketParams.hasMarketObject(MarketObject::DiscountCurve)) {
            for (const auto& discountMapping : todaysMarketParams.mapping(MarketObject::DiscountCurve, configuration)) {
                if (discountMapping.second == yieldSpecId) {
                    if (market) {
                        return market->discountCurve(discountMapping.first, configuration);
                    } else {
                        return discountCurve(discountMapping.first, configuration);
                    }
                }
            }
        }
    }

    // If yield spec ID still has not been found, return empty Handle
    return Handle<YieldTermStructure>();
}

} // namespace analytics
} // namespace ore
