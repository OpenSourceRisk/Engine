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
#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <qle/termstructures/proxyoptionletvolatility.hpp>
#include <qle/termstructures/proxyswaptionvolatility.hpp>
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
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>
#include <qle/indexes/inflationindexobserver.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/instruments/makeoiscapfloor.hpp>
#include <qle/termstructures/blackvariancesurfacestddevs.hpp>
#include <qle/termstructures/blackvolconstantspread.hpp>
#include <qle/termstructures/credit/spreadedbasecorrelationcurve.hpp>
#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
#include <qle/termstructures/dynamiccpivolatilitystructure.hpp>
#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/spreadedblackvolatilitycurve.hpp>
#include <qle/termstructures/spreadedblackvolatilitysurfacemoneyness.hpp>
#include <qle/termstructures/spreadedcorrelationcurve.hpp>
#include <qle/termstructures/spreadedcpivolatilitysurface.hpp>
#include <qle/termstructures/spreadeddiscountcurve.hpp>
#include <qle/termstructures/spreadedinflationcurve.hpp>
#include <qle/termstructures/spreadedoptionletvolatility2.hpp>
#include <qle/termstructures/spreadedpricetermstructure.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <qle/termstructures/spreadedswaptionvolatility.hpp>
#include <qle/termstructures/spreadedyoyvolsurface.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/termstructures/strippedyoyinflationoptionletvol.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/yoyinflationcurveobservermoving.hpp>
#include <qle/termstructures/zeroinflationcurveobservermoving.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace std;

namespace {

// Utility function that is in catch blocks below
void processException(bool continueOnError, const std::exception& e, const std::string& curveId = "",
                      ore::analytics::RiskFactorKey::KeyType keyType = ore::analytics::RiskFactorKey::KeyType::None,
                      const bool simDataWritten = false) {
    string curve;
    if (keyType != ore::analytics::RiskFactorKey::KeyType::None)
        curve = to_string(keyType) + "/";
    curve += curveId;

    std::string message = "skipping this object in scenario sim market";
    if (!curve.empty()) {
        message += " (scenario data was ";
        if (!simDataWritten)
            message += "not ";
        message += "written for this object.)";
    }
    if (continueOnError) {
        std::string exceptionMessage = e.what();
        /* We do not log a structured curve error message, if the exception message indicates that the problem
           already occurred in the inti market. In this case we have already logged a structured error there. */
        if (boost::starts_with(exceptionMessage, "did not find object ")) {
            ALOG("CurveID: " << curve << ": " << message << ": " << exceptionMessage);
        } else {
            ALOG(StructuredCurveErrorMessage(curve, message, exceptionMessage));
        }
    } else {
        QL_FAIL("Object with CurveID '" << curve << "' failed to build in scenario sim market: " << e.what());
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

void checkDayCounterConsistency(const std::string& curveId, const DayCounter& initCurveDayCounter,
                                const DayCounter& simCurveDayCounter) {
    if (initCurveDayCounter != simCurveDayCounter) {
        std::string initDcName = initCurveDayCounter.empty() ? "(empty)" : initCurveDayCounter.name();
        std::string ssmDcName = simCurveDayCounter.empty() ? "(empty)" : simCurveDayCounter.name();
        ALOG("inconsistent day counters: when using spreaded curves in scenario sim market, the init curve day counter"
             "(" +
             initDcName + ") should be equal to the ssm day counter (" + ssmDcName +
             "), continuing anyway, please consider fixing this in either the initial market or ssm "
             "configuration");
    }
}

boost::shared_ptr<YieldTermStructure>
makeYieldCurve(const std::string& curveId, const bool spreaded, const Handle<YieldTermStructure>& initMarketTs,
               const std::vector<Real>& yieldCurveTimes, const std::vector<Handle<Quote>>& quotes, const DayCounter& dc,
               const Calendar& cal, const std::string& interpolation, const std::string& extrapolation) {
    if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister && !spreaded) {
        return boost::shared_ptr<YieldTermStructure>(boost::make_shared<QuantExt::InterpolatedDiscountCurve>(
            yieldCurveTimes, quotes, 0, cal, dc,
            interpolation == "LogLinear" ? QuantExt::InterpolatedDiscountCurve::Interpolation::logLinear
                                         : QuantExt::InterpolatedDiscountCurve::Interpolation::linearZero,
            extrapolation == "FlatZero" ? QuantExt::InterpolatedDiscountCurve::Extrapolation::flatZero
                                        : QuantExt::InterpolatedDiscountCurve::Extrapolation::flatFwd));
    } else {
        if (spreaded) {
            checkDayCounterConsistency(curveId, initMarketTs->dayCounter(), dc);
            return boost::make_shared<QuantExt::SpreadedDiscountCurve>(
                initMarketTs, yieldCurveTimes, quotes,
                interpolation == "LogLinear" ? QuantExt::SpreadedDiscountCurve::Interpolation::logLinear
                                             : QuantExt::SpreadedDiscountCurve::Interpolation::linearZero,
                extrapolation == "FlatZero" ? SpreadedDiscountCurve::Extrapolation::flatZero
                                            : SpreadedDiscountCurve::Extrapolation::flatFwd);
        } else {
            return boost::make_shared<QuantExt::InterpolatedDiscountCurve2>(
                yieldCurveTimes, quotes, dc,
                interpolation == "LogLinear" ? QuantExt::InterpolatedDiscountCurve2::Interpolation::logLinear
                                             : QuantExt::InterpolatedDiscountCurve2::Interpolation::linearZero,
                extrapolation == "FlatZero" ? InterpolatedDiscountCurve2::Extrapolation::flatZero
                                            : InterpolatedDiscountCurve2::Extrapolation::flatFwd);
        }
    }
}

} // namespace

void ScenarioSimMarket::writeSimData(std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>>& simDataTmp,
                                     std::map<RiskFactorKey, Real>& absoluteSimDataTmp) {
    simData_.insert(simDataTmp.begin(), simDataTmp.end());
    absoluteSimData_.insert(absoluteSimDataTmp.begin(), absoluteSimDataTmp.end());
    simDataTmp.clear();
    absoluteSimDataTmp.clear();
}

void ScenarioSimMarket::addYieldCurve(const boost::shared_ptr<Market>& initMarket, const std::string& configuration,
                                      const RiskFactorKey::KeyType rf, const string& key, const vector<Period>& tenors,
                                      bool& simDataWritten, bool simulate, bool spreaded) {
    Handle<YieldTermStructure> wrapper = (riskFactorYieldCurve(rf) == ore::data::YieldCurveType::Discount)
                                             ? initMarket->discountCurve(key, configuration)
                                             : initMarket->yieldCurve(riskFactorYieldCurve(rf), key, configuration);
    QL_REQUIRE(!wrapper.empty(), "yield curve not provided for " << key);
    QL_REQUIRE(tenors.front() > 0 * Days, "yield curve tenors must not include t=0");
    // include today

    // constructing yield curves
    DayCounter dc = wrapper->dayCounter();
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
    std::map<RiskFactorKey, Real> absoluteSimDataTmp;
    for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
        Real val = wrapper->discount(yieldCurveDates[i + 1]);
        DLOG("ScenarioSimMarket yield curve " << rf << " " << key << " discount[" << i << "]=" << val);
        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(spreaded ? 1.0 : val));
        Handle<Quote> qh(q);
        quotes.push_back(qh);

        // Check if the risk factor is simulated before adding it
        if (simulate) {
            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(rf, key, i), std::forward_as_tuple(q));
            // if generating spreaded scenarios, add the absolute value as well
            if (spreaded) {
                absoluteSimDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(rf, key, i),
                                           std::forward_as_tuple(val));
            }
        }
    }

    writeSimData(simDataTmp, absoluteSimDataTmp);
    simDataWritten = true;

    boost::shared_ptr<YieldTermStructure> yieldCurve =
        makeYieldCurve(key, spreaded, wrapper, yieldCurveTimes, quotes, dc, TARGET(), parameters_->interpolation(),
                       parameters_->extrapolation());

    Handle<YieldTermStructure> ych(yieldCurve);
    if (wrapper->allowsExtrapolation())
        ych->enableExtrapolation();
    yieldCurves_.insert(pair<tuple<string, ore::data::YieldCurveType, string>, Handle<YieldTermStructure>>(
        make_tuple(Market::defaultConfiguration, riskFactorYieldCurve(rf), key), ych));
}

ScenarioSimMarket::ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                                     const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
                                     const std::string& configuration, const CurveConfigurations& curveConfigs,
                                     const TodaysMarketParameters& todaysMarketParams, const bool continueOnError,
                                     const bool useSpreadedTermStructures, const bool cacheSimData,
                                     const bool allowPartialScenarios, const IborFallbackConfig& iborFallbackConfig,
                                     const bool handlePseudoCurrencies)
    : ScenarioSimMarket(initMarket, parameters, boost::make_shared<FixingManager>(initMarket->asofDate()),
                        configuration, curveConfigs, todaysMarketParams, continueOnError, useSpreadedTermStructures,
                        cacheSimData, allowPartialScenarios, iborFallbackConfig, handlePseudoCurrencies) {}

ScenarioSimMarket::ScenarioSimMarket(
    const boost::shared_ptr<Market>& initMarket, const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
    const boost::shared_ptr<FixingManager>& fixingManager, const std::string& configuration,
    const ore::data::CurveConfigurations& curveConfigs, const ore::data::TodaysMarketParameters& todaysMarketParams,
    const bool continueOnError, const bool useSpreadedTermStructures, const bool cacheSimData,
    const bool allowPartialScenarios, const IborFallbackConfig& iborFallbackConfig, const bool handlePseudoCurrencies)
    : SimMarket(handlePseudoCurrencies), parameters_(parameters), fixingManager_(fixingManager),
      filter_(boost::make_shared<ScenarioFilter>()), useSpreadedTermStructures_(useSpreadedTermStructures),
      cacheSimData_(cacheSimData), allowPartialScenarios_(allowPartialScenarios),
      iborFallbackConfig_(iborFallbackConfig) {

    LOG("building ScenarioSimMarket...");
    asof_ = initMarket->asofDate();
    DLOG("AsOf " << QuantLib::io::iso_date(asof_));

    // check ssm parameters
    QL_REQUIRE(parameters_->interpolation() == "LogLinear" || parameters_->interpolation() == "LinearZero",
               "ScenarioSimMarket: Interpolation (" << parameters_->interpolation()
                                                    << ") must be set to 'LogLinear' or 'LinearZero'");
    QL_REQUIRE(parameters_->extrapolation() == "FlatZero" || parameters_->extrapolation() == "FlatFwd",
               "ScenarioSimMarket: YieldCurves / Extrapolation ('" << parameters_->extrapolation()
                                                                   << "') must be set to 'FlatZero' or 'FlatFwd'");
    QL_REQUIRE(parameters_->defaultCurveExtrapolation() == "FlatZero" ||
                   parameters_->defaultCurveExtrapolation() == "FlatFwd",
               "ScenarioSimMarket: DefaultCurves / Extrapolation ('" << parameters_->extrapolation()
                                                                     << "') must be set to 'FlatZero' or 'FlatFwd'");

    for (const auto& param : parameters->parameters()) {
        try {
            // we populate the temp containers for each curve and write the result to the global
            // containers only if the set of data points is complete for this curve
            std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>> simDataTmp;
            std::map<RiskFactorKey, Real> absoluteSimDataTmp;

	    boost::timer::cpu_timer timer;

            switch (param.first) {
            case RiskFactorKey::KeyType::FXSpot: {
		std::map<std::string, Handle<Quote>> fxQuotes;
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        // constructing fxSpots_
                        DLOG("adding " << name << " FX rates");
			auto q = boost::make_shared<SimpleQuote>(initMarket->fxSpot(name, configuration)->value());
                        auto qh = Handle<Quote>(q);
                        fxQuotes[name] = qh;
                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(q));
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
		fx_ = boost::make_shared<FXTriangulation>(fxQuotes);
                break;
            }

            case RiskFactorKey::KeyType::DiscountCurve:
            case RiskFactorKey::KeyType::YieldCurve:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building " << name << " yield curve..");
                        vector<Period> tenors = parameters->yieldCurveTenors(name);
                        addYieldCurve(initMarket, configuration, param.first, name, tenors, simDataWritten,
                                      param.second.first, useSpreadedTermStructures_);
                        DLOG("building " << name << " yield curve done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::IndexCurve: {
                // make sure we built overnight indices first, so that we can build ibor fallback indices
                // that depend on them
                std::vector<std::string> indices;
                for (auto const& i : param.second.second) {
                    bool isOn = false;
                    try {
                        isOn = boost::dynamic_pointer_cast<OvernightIndex>(*initMarket->iborIndex(i, configuration)) !=
                               nullptr;
                    } catch (...) {
                    }
                    if (isOn)
                        indices.insert(indices.begin(), i);
                    else
                        indices.push_back(i);
                }
                // loop over sorted indices and build them
                for (const auto& name : indices) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building " << name << " index curve");
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

                        DayCounter dc = wrapperIndex->dayCounter();
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
                            Real val = wrapperIndex->discount(yieldCurveDates[i + 1]);
                            boost::shared_ptr<SimpleQuote> q(new SimpleQuote(useSpreadedTermStructures_ ? 1.0 : val));
                            Handle<Quote> qh(q);
                            quotes.push_back(qh);

                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name, i),
                                               std::forward_as_tuple(q));
                            if (useSpreadedTermStructures_) {
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, i),
                                                           std::forward_as_tuple(val));
                            }
                            // FIXME where do we check whether the risk factor is simulated?
                            DLOG("ScenarioSimMarket index curve " << name << " discount[" << i << "]=" << val);
                        }

                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;

                        boost::shared_ptr<YieldTermStructure> indexCurve = makeYieldCurve(
                            name, useSpreadedTermStructures_, wrapperIndex, yieldCurveTimes, quotes, dc,
                            index->fixingCalendar(), parameters_->interpolation(), parameters_->extrapolation());

                        Handle<YieldTermStructure> ich(indexCurve);
                        if (wrapperIndex->allowsExtrapolation())
                            ich->enableExtrapolation();

                        boost::shared_ptr<IborIndex> i = index->clone(ich);
                        if (iborFallbackConfig_.isIndexReplaced(name, asof_)) {
                            // handle ibor fallback indices
                            auto fallbackData = iborFallbackConfig_.fallbackData(name);
                            auto f = iborIndices_.find(make_pair(Market::defaultConfiguration, fallbackData.rfrIndex));
                            QL_REQUIRE(f != iborIndices_.end(),
                                       "Could not build ibor fallback index '"
                                           << name << "', because rfr index '" << fallbackData.rfrIndex
                                           << "' is not present in scenario sim market, is the rfr index in the "
                                              "scenario sim market parameters?");
                            auto rfrInd = boost::dynamic_pointer_cast<OvernightIndex>(*f->second);
                            QL_REQUIRE(rfrInd != nullptr,
                                       "Could not cast '"
                                           << fallbackData.rfrIndex
                                           << "' to overnight index when building the ibor fallback index '" << name
                                           << "'");
			    if (auto original = boost::dynamic_pointer_cast<OvernightIndex>(i))
			        i = boost::make_shared<QuantExt::FallbackOvernightIndex>(
				    original, rfrInd, fallbackData.spread, fallbackData.switchDate,
				    iborFallbackConfig_.useRfrCurveInSimulationMarket());
                            else
			        i = boost::make_shared<QuantExt::FallbackIborIndex>(
                                    i, rfrInd, fallbackData.spread, fallbackData.switchDate,
				    iborFallbackConfig_.useRfrCurveInSimulationMarket());
                            DLOG("built ibor fall back index '"
                                 << name << "' with rfr index '" << fallbackData.rfrIndex << "', spread "
                                 << fallbackData.spread << ", use rfr curve in scen sim market: " << std::boolalpha
                                 << iborFallbackConfig_.useRfrCurveInSimulationMarket());
                        }
                        iborIndices_.insert(
                            make_pair(make_pair(Market::defaultConfiguration, name), Handle<IborIndex>(i)));
                        DLOG("building " << name << " index curve done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;
            }

            case RiskFactorKey::KeyType::EquitySpot:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        // building equity spots
                        DLOG("adding " << name << " equity spot...");
                        Real spotVal = initMarket->equitySpot(name, configuration)->value();
                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(spotVal));
                        Handle<Quote> qh(q);
                        equitySpots_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), qh));
                        simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                           std::forward_as_tuple(q));
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                        DLOG("adding " << name << " equity spot done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::DividendYield:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building " << name << " equity dividend yield curve..");
                        vector<Period> tenors = parameters->equityDividendTenors(name);
                        addYieldCurve(initMarket, configuration, param.first, name, tenors, simDataWritten,
                                      param.second.first, useSpreadedTermStructures_);
                        DLOG("building " << name << " equity dividend yield curve done");

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
                                 << ccy << "' for equity forecast curve '" << name << "'");
                            forecastTs = discountCurve(ccy);
                        }
                        boost::shared_ptr<EquityIndex> ei(
                            curve->clone(equitySpot(name, configuration), forecastTs,
                                         yieldCurve(YieldCurveType::EquityDividend, name, configuration)));
                        Handle<EquityIndex> eh(ei);
                        equityCurves_.insert(pair<pair<string, string>, Handle<EquityIndex>>(
                            make_pair(Market::defaultConfiguration, name), eh));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::SecuritySpread:
                for (const auto& name : param.second.second) {
                    // security spreads and recovery rates are optional
                    try {
                        DLOG("Adding security spread " << name << " from configuration " << configuration);
                        boost::shared_ptr<SimpleQuote> spreadQuote(
                            new SimpleQuote(initMarket->securitySpread(name, configuration)->value()));
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(spreadQuote));
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        securitySpreads_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(spreadQuote)));
                    } catch (const std::exception& e) {
                        DLOG("skipping this object: " << e.what());
                    }

                    try {
                        DLOG("Adding security recovery rate " << name << " from configuration " << configuration);
                        boost::shared_ptr<SimpleQuote> recoveryQuote(
                            new SimpleQuote(initMarket->recoveryRate(name, configuration)->value()));
                        // TODO this comes from the default curves section in the parameters,
                        // do we want to specify the simulation of security recovery rates separately?
                        if (parameters->simulateRecoveryRates()) {
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                                               std::forward_as_tuple(recoveryQuote));
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        recoveryRates_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(recoveryQuote)));
                    } catch (const std::exception& e) {
                        DLOG("skipping this object: " << e.what());
                    }
                }
                break;

            case RiskFactorKey::KeyType::SwaptionVolatility:
            case RiskFactorKey::KeyType::YieldVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        // set parameters for swaption resp. yield vols
                        RelinkableHandle<SwaptionVolatilityStructure> wrapper;
                        vector<Period> optionTenors, underlyingTenors;
                        vector<Real> strikeSpreads;
                        string shortSwapIndexBase = "", swapIndexBase = "";
                        bool isCube, isAtm, simulateAtmOnly;
                        if (param.first == RiskFactorKey::KeyType::SwaptionVolatility) {
                            DLOG("building " << name << " swaption volatility curve...");
                            wrapper.linkTo(*initMarket->swaptionVol(name, configuration));
                            shortSwapIndexBase = initMarket->shortSwapIndexBase(name, configuration);
                            swapIndexBase = initMarket->swapIndexBase(name, configuration);
                            isCube = parameters->swapVolIsCube(name);
                            optionTenors = parameters->swapVolExpiries(name);
                            underlyingTenors = parameters->swapVolTerms(name);
                            strikeSpreads = parameters->swapVolStrikeSpreads(name);
                            simulateAtmOnly = parameters->simulateSwapVolATMOnly();
                        } else {
                            DLOG("building " << name << " yield volatility curve...");
                            wrapper.linkTo(*initMarket->yieldVol(name, configuration));
                            isCube = false;
                            optionTenors = parameters->yieldVolExpiries();
                            underlyingTenors = parameters->yieldVolTerms();
                            strikeSpreads = {0.0};
                            simulateAtmOnly = true;
                        }
                        DLOG("Initial market " << name << " yield volatility type = " << wrapper->volatilityType());

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
                                QL_REQUIRE(tmp, "swaption cube missing");
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
                            // d) we do not use spreaded term structures, in which case we keep the original T0
                            //    term structure in any case
                            bool convertToNormal = wrapper->volatilityType() != Normal &&
                                                   param.first == RiskFactorKey::KeyType::SwaptionVolatility &&
                                                   (!simulateAtmOnly || isAtm) && !useSpreadedTermStructures_;
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
                                        boost::shared_ptr<SimpleQuote> q(
                                            new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : vol));

                                        Size index = i * underlyingTenors.size() * strikeSpreads.size() +
                                                     j * strikeSpreads.size() + k;

                                        simDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, index),
                                                           std::forward_as_tuple(q));
                                        if (useSpreadedTermStructures_) {
                                            absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                       std::forward_as_tuple(param.first, name, index),
                                                                       std::forward_as_tuple(vol));
                                        }
                                        auto tmp = Handle<Quote>(q);
                                        quotes[i * underlyingTenors.size() + j][k] = tmp;
                                        if (k == atmSlice) {
                                            atmQuotes[i][j] = tmp;
                                            shift[i][j] =
                                                !convertToNormal && wrapper->volatilityType() == ShiftedLognormal
                                                    ? wrapper->shift(optionTenors[i], underlyingTenors[j])
                                                    : 0.0;
                                            DLOG("AtmVol at " << optionTenors.at(i) << "/" << underlyingTenors.at(j)
                                                              << " is " << vol << ", shift is " << shift[i][j]
                                                              << ", (name,index) = (" << name << "," << index << ")");
                                        } else {
                                            DLOG("SmileVol at " << optionTenors.at(i) << "/" << underlyingTenors.at(j)
                                                                << "/" << strikeSpreads.at(k) << " is " << vol
                                                                << ", (name,index) = (" << name << "," << index << ")");
                                        }
                                    }
                                }
                            }
                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;
                            bool flatExtrapolation = true; // FIXME: get this from curve configuration
                            VolatilityType volType = convertToNormal ? Normal : wrapper->volatilityType();
                            DayCounter dc = wrapper->dayCounter();
                
			    if (useSpreadedTermStructures_) {
                                bool stickyStrike = parameters_->swapVolSmileDynamics(name) == "StickyStrike";
                                boost::shared_ptr<SwapIndex> swapIndex, shortSwapIndex;
                                boost::shared_ptr<SwapIndex> simSwapIndex, simShortSwapIndex;
                                if (!stickyStrike) {
                                    if (addSwapIndexToSsm(swapIndexBase, continueOnError)) {
                                        simSwapIndex = *this->swapIndex(swapIndexBase, configuration);
                                    }
                                    if (addSwapIndexToSsm(shortSwapIndexBase, continueOnError)) {
                                        simShortSwapIndex = *this->swapIndex(shortSwapIndexBase, configuration);
                                    }
                                    if (simSwapIndex == nullptr || simShortSwapIndex == nullptr)
                                        stickyStrike = true;
                                }
                                if(!swapIndexBase.empty())
                                    swapIndex = *initMarket->swapIndex(swapIndexBase, configuration);
                                if(!shortSwapIndexBase.empty())
                                    shortSwapIndex = *initMarket->swapIndex(shortSwapIndexBase, configuration);
                                svp =
                                    Handle<SwaptionVolatilityStructure>(boost::make_shared<SpreadedSwaptionVolatility>(
                                        wrapper, optionTenors, underlyingTenors, strikeSpreads, quotes, swapIndex,
                                        shortSwapIndex, simSwapIndex, simShortSwapIndex, !stickyStrike));
                            } else {
                                Handle<SwaptionVolatilityStructure> atm;
                                atm = Handle<SwaptionVolatilityStructure>(boost::make_shared<SwaptionVolatilityMatrix>(
                                    wrapper->calendar(), wrapper->businessDayConvention(), optionTenors,
                                    underlyingTenors, atmQuotes, dc, flatExtrapolation, volType, shift));
                                atm->enableExtrapolation(); // see below for svp, take this from T0 config?
                                if (simulateAtmOnly) {
                                    if (isAtm) {
                                        svp = atm;
                                    } else {
                                        // floating reference date matrix in sim market
                                        // if we have a cube, we keep the vol spreads constant under scenarios
                                        // notice that cube is from todaysmarket, so it has a fixed reference date,
                                        // which means that we keep the smiles constant in terms of vol spreads when
                                        // moving forward in time; notice also that the volatility will be "sticky
                                        // strike", i.e. it will not react to changes in the ATM level
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
                            }
                        } else {
                            string decayModeString = parameters->swapVolDecayMode();
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            DLOG("Dynamic (" << wrapper->volatilityType() << ") yield vols (" << decayModeString
                                            << ") for qualifier " << name);

                            QL_REQUIRE(!boost::dynamic_pointer_cast<ProxySwaptionVolatility>(*wrapper),
                                "DynamicSwaptionVolatilityMatrix does not support ProxySwaptionVolatility surface");

                            boost::shared_ptr<SwaptionVolatilityStructure> atmSlice;
                            if (isAtm)
                                atmSlice = *wrapper;
                            else {
                                auto c = boost::dynamic_pointer_cast<SwaptionVolCubeWithATM>(*wrapper);
                                QL_REQUIRE(c, "internal error - expected swaption cube to be SwaptionVolCubeWithATM.");
                                atmSlice = *c->cube()->atmVol();
                            }

                            if (isCube)
                                WLOG("Only ATM slice is considered from init market's cube");
                            boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svolp =
                                boost::make_shared<QuantExt::DynamicSwaptionVolatilityMatrix>(
                                    atmSlice, 0, NullCalendar(), decayMode);
                            svp = Handle<SwaptionVolatilityStructure>(svolp);
                        }

                        svp->enableExtrapolation(); // FIXME

                        DLOG("Simulation market " << name << " yield volatility type = " << svp->volatilityType());

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
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::OptionletVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building " << name << " cap/floor volatility curve...");
                        Handle<OptionletVolatilityStructure> wrapper = initMarket->capFloorVol(name, configuration);
                        auto [iborIndexName, rateComputationPeriod] =
                            initMarket->capFloorVolIndexBase(name, configuration);
                        boost::shared_ptr<IborIndex> iborIndex =
                            iborIndexName.empty() ? nullptr : parseIborIndex(iborIndexName);

                        LOG("Initial market cap/floor volatility type = " << wrapper->volatilityType());

                        Handle<OptionletVolatilityStructure> hCapletVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            LOG("Simulating Cap/Floor Optionlet vols for key " << name);

                            // Try to get the ibor index that the cap floor structure relates to
                            // We use this to convert Period to Date below to sample from `wrapper`
                            Natural settleDays = 0;
                            bool isOis = false;
                            Calendar iborCalendar;
                            Size onSettlementDays = 0;

                            // get the curve config for the index, or if not available for its ccy
                            boost::shared_ptr<CapFloorVolatilityCurveConfig> config;
                            if (curveConfigs.hasCapFloorVolCurveConfig(name)) {
                                config = curveConfigs.capFloorVolCurveConfig(name);
                            } else {
                                if (iborIndex && curveConfigs.hasCapFloorVolCurveConfig(iborIndex->currency().code())) {
                                    config = curveConfigs.capFloorVolCurveConfig(iborIndex->currency().code());
                                }
                            }

			    // get info from the config if we have one
                            if (config) {
                                settleDays = config->settleDays();
				onSettlementDays = config->onCapSettlementDays();
                            }

			    // derive info from the ibor index
                            if (iborIndex) {
                                iborCalendar = iborIndex->fixingCalendar();
                                isOis = boost::dynamic_pointer_cast<OvernightIndex>(iborIndex) != nullptr;
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

			    DLOG("cap floor use adjusted option pillars = " << std::boolalpha << parameters_->capFloorVolAdjustOptionletPillars());
			    DLOG("have ibor index = " << std::boolalpha << (iborIndex != nullptr));

                            for (Size i = 0; i < optionTenors.size(); ++i) {

                                if (parameters_->capFloorVolAdjustOptionletPillars() && iborIndex) {
                                    // If we ask for cap pillars at tenors t_i for i = 1,...,N, we should attempt to
                                    // place the optionlet pillars at the fixing date of the last optionlet in the cap
                                    // with tenor t_i, if capFloorVolAdjustOptionletPillars is true.
				    if(isOis) {
                                        Leg capFloor =
                                            MakeOISCapFloor(
                                                CapFloor::Cap, optionTenors[i],
                                                boost::dynamic_pointer_cast<QuantLib::OvernightIndex>(iborIndex),
                                                rateComputationPeriod, 0.0)
                                                .withTelescopicValueDates(true)
                                                .withSettlementDays(onSettlementDays);
                                        if (capFloor.empty()) {
                                            optionDates[i] = asof_ + 1;
                                        } else {
                                            auto lastCoupon = boost::dynamic_pointer_cast<
                                                QuantExt::CappedFlooredOvernightIndexedCoupon>(capFloor.back());
                                            QL_REQUIRE(lastCoupon, "SSM internal error, could not cast to "
                                                                   "CappedFlooredOvernightIndexedCoupon "
                                                                   "when building optionlet vol for '"
                                                                       << name << "' (index=" << iborIndex->name()
                                                                       << ")");
                                            optionDates[i] =
                                                std::max(asof_ + 1, lastCoupon->underlying()->fixingDates().front());
                                        }
                                    } else {
                                        boost::shared_ptr<CapFloor> capFloor =
                                            MakeCapFloor(CapFloor::Cap, optionTenors[i], iborIndex, 0.0, 0 * Days);
                                        if (capFloor->floatingLeg().empty()) {
                                            optionDates[i] = asof_ + 1;
                                        } else {
                                            optionDates[i] =
                                                std::max(asof_ + 1, capFloor->lastFloatingRateCoupon()->fixingDate());
                                        }
                                    }
                                    QL_REQUIRE(i == 0 || optionDates[i] > optionDates[i - 1],
                                               "SSM: got non-increasing option dates "
                                                   << optionDates[i - 1] << ", " << optionDates[i] << " for tenors "
                                                   << optionTenors[i - 1] << ", " << optionTenors[i] << " for index "
                                                   << iborIndex->name());
                                } else {
                                    // Otherwise, just place the optionlet pillars at the configured tenors.
                                    optionDates[i] = wrapper->optionDateFromTenor(optionTenors[i]);
                                    if (iborCalendar != Calendar()) {
                                        // In case the original cap floor surface has the incorrect calendar configured.
                                        optionDates[i] = iborCalendar.adjust(optionDates[i]);
                                    }
                                }

                                DLOG("Option [tenor, date] pair is [" << optionTenors[i] << ", "
                                                                      << io::iso_date(optionDates[i]) << "]");

                                // If ATM, use initial market's discount curve and ibor index to calculate ATM rate
                                Rate strike = Null<Rate>();
                                if (isAtm) {
                                    QL_REQUIRE(iborIndex != nullptr,
                                               "SSM: Expected ibor index for key "
                                                   << name << " from the key or a curve config for a ccy");
                                    auto t0_iborIndex = *initMarket->iborIndex(
                                        IndexNameTranslator::instance().oreName(iborIndex->name()), configuration);
                                    if (parameters_->capFloorVolUseCapAtm()) {
                                        QL_REQUIRE(!isOis, "SSM: capFloorVolUseCapATM not supported for OIS indices ("
                                                               << t0_iborIndex->name() << ")");
                                        boost::shared_ptr<CapFloor> cap =
                                            MakeCapFloor(CapFloor::Cap, optionTenors[i], t0_iborIndex, 0.0, 0 * Days);
                                        strike = cap->atmRate(**initMarket->discountCurve(name, configuration));
                                    } else {
                                        if (isOis) {
                                            Leg capFloor =
                                                MakeOISCapFloor(CapFloor::Cap, optionTenors[i],
                                                                boost::dynamic_pointer_cast<OvernightIndex>(t0_iborIndex),
                                                                rateComputationPeriod, 0.0)
                                                    .withTelescopicValueDates(true)
                                                    .withSettlementDays(onSettlementDays);
                                            if (capFloor.empty()) {
                                                strike = t0_iborIndex->fixing(optionDates[i]);
                                            } else {
                                                auto lastCoupon =
                                                    boost::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(
                                                        capFloor.back());
                                                QL_REQUIRE(lastCoupon, "SSM internal error, could not cast to "
                                                                       "CappedFlooredOvernightIndexedCoupon "
                                                                       "when building optionlet vol for '"
                                                                           << name << "', index=" << t0_iborIndex->name());
                                                strike = lastCoupon->underlying()->rate();
                                            }
                                        } else {
                                            strike = t0_iborIndex->fixing(optionDates[i]);
                                        }
                                    }
                                }

                                for (Size j = 0; j < strikes.size(); ++j) {
                                    strike = isAtm ? strike : strikes[j];
                                    Real vol =
                                        wrapper->volatility(optionDates[i], strike, true);
                                    DLOG("Vol at [date, strike] pair [" << optionDates[i] << ", " << std::fixed
                                                                        << std::setprecision(4) << strike << "] is "
                                                                        << std::setprecision(12) << vol);
                                    boost::shared_ptr<SimpleQuote> q =
                                        boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
                                    Size index = i * strikes.size() + j;
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, index),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, index),
                                                                   std::forward_as_tuple(vol));
                                    }
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;

                            DayCounter dc = wrapper->dayCounter();

                            if (useSpreadedTermStructures_) {
                                hCapletVol = Handle<OptionletVolatilityStructure>(
                                    boost::make_shared<QuantExt::SpreadedOptionletVolatility2>(wrapper, optionDates,
                                                                                               strikes, quotes));
                            } else {
                                // FIXME: Works as of today only, i.e. for sensitivity/scenario analysis.
                                // TODO: Build floating reference date StrippedOptionlet class for MC path generators
                                boost::shared_ptr<StrippedOptionlet> optionlet = boost::make_shared<StrippedOptionlet>(
                                    settleDays, wrapper->calendar(), wrapper->businessDayConvention(), iborIndex,
                                    optionDates, strikes, quotes, dc, wrapper->volatilityType(),
                                    wrapper->displacement());

                                hCapletVol = Handle<OptionletVolatilityStructure>(
                                    boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                                        optionlet));
                            }
                        } else {
                            string decayModeString = parameters->capFloorVolDecayMode();
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            QL_REQUIRE(!boost::dynamic_pointer_cast<ProxyOptionletVolatility>(*wrapper), 
                                "DynamicOptionletVolatilityStructure does not support ProxyOptionletVolatility surface.");

                            boost::shared_ptr<OptionletVolatilityStructure> capletVol =
                                boost::make_shared<DynamicOptionletVolatilityStructure>(*wrapper, 0, NullCalendar(),
                                                                                        decayMode);
                            hCapletVol = Handle<OptionletVolatilityStructure>(capletVol);
                        }

                        hCapletVol->enableExtrapolation();
                        capFloorCurves_.emplace(std::piecewise_construct,
                                                std::forward_as_tuple(Market::defaultConfiguration, name),
                                                std::forward_as_tuple(hCapletVol));
                        capFloorIndexBase_.emplace(
                            std::piecewise_construct, std::forward_as_tuple(Market::defaultConfiguration, name),
                            std::forward_as_tuple(std::make_pair(iborIndexName, rateComputationPeriod)));

                        LOG("Simulation market cap/floor volatility type = " << hCapletVol->volatilityType());
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::SurvivalProbability:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building " << name << " default curve..");
                        auto wrapper = initMarket->defaultCurve(name, configuration);
                        vector<Handle<Quote>> quotes;

                        QL_REQUIRE(parameters->defaultTenors(name).front() > 0 * Days,
                                   "default curve tenors must not include t=0");

                        vector<Date> dates(1, asof_);
                        vector<Real> times(1, 0.0);

                        DayCounter dc = wrapper->curve()->dayCounter();
            
                        for (Size i = 0; i < parameters->defaultTenors(name).size(); i++) {
                            dates.push_back(asof_ + parameters->defaultTenors(name)[i]);
                            times.push_back(dc.yearFraction(asof_, dates.back()));
                        }

                        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
                        quotes.push_back(Handle<Quote>(q));
                        for (Size i = 0; i < dates.size() - 1; i++) {
                            Probability prob = wrapper->curve()->survivalProbability(dates[i + 1], true);
                            boost::shared_ptr<SimpleQuote> q =
                                boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : prob);
                            // Check if the risk factor is simulated before adding it
                            if (param.second.first) {
                                simDataTmp.emplace(std::piecewise_construct,
                                                   std::forward_as_tuple(param.first, name, i),
                                                   std::forward_as_tuple(q));
                                DLOG("ScenarioSimMarket default curve " << name << " survival[" << i << "]=" << prob);
                                if (useSpreadedTermStructures_) {
                                    absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(param.first, name, i),
                                                               std::forward_as_tuple(prob));
                                }
                            }
                            Handle<Quote> qh(q);
                            quotes.push_back(qh);
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                        Calendar cal = ore::data::parseCalendar(parameters->defaultCurveCalendar(name));
                        Handle<DefaultProbabilityTermStructure> defaultCurve;
                        if (useSpreadedTermStructures_) {
                            defaultCurve = Handle<DefaultProbabilityTermStructure>(
                                boost::make_shared<QuantExt::SpreadedSurvivalProbabilityTermStructure>(
                                    wrapper->curve(), times, quotes,
                                    parameters->defaultCurveExtrapolation() == "FlatZero"
                                        ? QuantExt::SpreadedSurvivalProbabilityTermStructure::Extrapolation::flatZero
                                        : QuantExt::SpreadedSurvivalProbabilityTermStructure::Extrapolation::flatFwd));
                        } else {
                            defaultCurve = Handle<DefaultProbabilityTermStructure>(
                                boost::make_shared<QuantExt::SurvivalProbabilityCurve<LogLinear>>(
                                    dates, quotes, dc, cal, std::vector<Handle<Quote>>(), std::vector<Date>(),
                                    LogLinear(),
                                    parameters->defaultCurveExtrapolation() == "FlatZero"
                                        ? QuantExt::SurvivalProbabilityCurve<LogLinear>::Extrapolation::flatZero
                                        : QuantExt::SurvivalProbabilityCurve<LogLinear>::Extrapolation::flatFwd));
                        }
                        defaultCurve->enableExtrapolation();
                        defaultCurves_.insert(pair<pair<string, string>, Handle<CreditCurve>>(
                            make_pair(Market::defaultConfiguration, name),
                            Handle<CreditCurve>(boost::make_shared<CreditCurve>(
                                defaultCurve, wrapper->rateCurve(), wrapper->recovery(), wrapper->refData()))));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::RecoveryRate:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
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
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                        recoveryRates_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(rrQuote)));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CDSVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building " << name << "  cds vols..");
                        Handle<QuantExt::CreditVolCurve> wrapper = initMarket->cdsVol(name, configuration);
                        Handle<QuantExt::CreditVolCurve> cvh;
                        bool stickyStrike = parameters_->cdsVolSmileDynamics(name) == "StickyStrike";
                        if (param.second.first) {
                            LOG("Simulating CDS Vols for " << name);
                            vector<Handle<Quote>> quotes;
                            vector<Volatility> vols;
                            vector<Time> times;
                            vector<Date> expiryDates;
                            DayCounter dc = wrapper->dayCounter();
                            for (Size i = 0; i < parameters->cdsVolExpiries().size(); i++) {
                                Date date = asof_ + parameters->cdsVolExpiries()[i];
                                expiryDates.push_back(date);
                                // hardcoded, single term 5y
                                Volatility vol = wrapper->volatility(date, 5.0, Null<Real>(), wrapper->type());
                                vols.push_back(vol);
                                times.push_back(dc.yearFraction(asof_, date));
                                boost::shared_ptr<SimpleQuote> q =
                                    boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
                                if (parameters->simulateCdsVols()) {
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, i),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, i),
                                                                   std::forward_as_tuple(vol));
                                    }
                                }
                                quotes.emplace_back(q);
                            }
                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;
                            if (useSpreadedTermStructures_ ||
                                (!useSpreadedTermStructures_ && parameters->simulateCdsVolATMOnly())) {
                                std::vector<Handle<Quote>> spreads;
                                if (parameters_->simulateCdsVolATMOnly()) {
                                    for (size_t i = 0; i < quotes.size(); i++) {
                                        Handle<Quote> atmVol(boost::make_shared<SimpleQuote>(quotes[i]->value()));
                                        Handle<Quote> quote(boost::make_shared <
                                                            CompositeQuote<std::minus<double>>>(quotes[i], atmVol,
                                                                                               std::minus<double>()));
                                        spreads.push_back(quote);
                                    }
                                } else {
                                    spreads = quotes;
                                }
                                std::vector<QuantLib::Period> simTerms;
                                std::vector<Handle<CreditCurve>> simTermCurves;
                                if (curveConfigs.hasCdsVolCurveConfig(name)) {
                                    // get the term curves from the curve config if possible
                                    auto cc = curveConfigs.cdsVolCurveConfig(name);
                                    simTerms = cc->terms();
                                    for (auto const& c : cc->termCurves())
                                        simTermCurves.push_back(defaultCurve(parseCurveSpec(c)->curveConfigID()));
                                } else {
                                    // assume the default curve names follow the naming convention volName_5Y
                                    simTerms = wrapper->terms();
                                    for (auto const& t : simTerms) {
                                        simTermCurves.push_back(defaultCurve(name + "_" + ore::data::to_string(t)));
                                    }
                                }
                                cvh = Handle<CreditVolCurve>(boost::make_shared<SpreadedCreditVolCurve>(
                                    wrapper, expiryDates, spreads, !stickyStrike, simTerms, simTermCurves));
                            } else {
                                // TODO support strike and term dependence
                                cvh = Handle<CreditVolCurve>(boost::make_shared<CreditVolCurveWrapper>(
                                    Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceCurve3>(
                                        0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes,
                                        false))));
                            }
                        } else {
                            string decayModeString = parameters->cdsVolDecayMode();
                            LOG("Deterministic CDS Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // TODO support strike and term dependence, hardcoded term 5y
                            cvh = Handle<CreditVolCurve>(
                                boost::make_shared<CreditVolCurveWrapper>(Handle<BlackVolTermStructure>(
                                    boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                        Handle<BlackVolTermStructure>(
                                            boost::make_shared<BlackVolFromCreditVolWrapper>(wrapper, 5.0)),
                                        0, NullCalendar(), decayMode,
                                        stickyStrike ? StickyStrike : StickyLogMoneyness))));
                        }

                        if (wrapper->allowsExtrapolation())
                            cvh->enableExtrapolation();
                        cdsVols_.insert(pair<pair<string, string>, Handle<QuantExt::CreditVolCurve>>(
                            make_pair(make_pair(Market::defaultConfiguration, name), cvh)));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::FXVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
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

                        bool stickyStrike = parameters_->fxVolSmileDynamics(name) == "StickyStrike";

                        if (param.second.first) {
                            LOG("Simulating FX Vols for " << name);
                            auto& expiries = parameters->fxVolExpiries(name);
                            Size m = expiries.size();
                            Calendar cal = wrapper->calendar();
                            if (cal.empty()) {
                                cal = NullCalendar();
                            }
                            DayCounter dc = wrapper->dayCounter();
                            vector<vector<Handle<Quote>>> quotes;
                            vector<Time> times(m);
                            vector<Date> dates(m);

                            // Attempt to get the relevant yield curves from the initial market
                            Handle<YieldTermStructure> initForTS =
                                getYieldCurve(foreignTsId, todaysMarketParams, configuration, initMarket);
                            TLOG("Foreign term structure '" << foreignTsId << "' from t_0 market is "
                                                            << (initForTS.empty() ? "empty" : "not empty"));
                            Handle<YieldTermStructure> initDomTS =
                                getYieldCurve(domesticTsId, todaysMarketParams, configuration, initMarket);
                            TLOG("Domestic term structure '" << domesticTsId << "' from t_0 market is "
                                                             << (initDomTS.empty() ? "empty" : "not empty"));

                            // fall back on discount curves
                            if (initForTS.empty() || initDomTS.empty()) {
                                TLOG("Falling back on the discount curves for " << forCcy << " and " << domCcy
                                                                                << " from t_0 market");
                                initForTS = initMarket->discountCurve(forCcy, configuration);
                                initDomTS = initMarket->discountCurve(domCcy, configuration);
                            }

                            // Attempt to get the relevant yield curves from this scenario simulation market
                            Handle<YieldTermStructure> forTS =
                                getYieldCurve(foreignTsId, todaysMarketParams, Market::defaultConfiguration);
                            TLOG("Foreign term structure '" << foreignTsId << "' from sim market is "
                                                            << (forTS.empty() ? "empty" : "not empty"));
                            Handle<YieldTermStructure> domTS =
                                getYieldCurve(domesticTsId, todaysMarketParams, Market::defaultConfiguration);
                            TLOG("Domestic term structure '" << domesticTsId << "' from sim market is "
                                                             << (domTS.empty() ? "empty" : "not empty"));

                            // fall back on discount curves
                            if (forTS.empty() || domTS.empty()) {
                                TLOG("Falling back on the discount curves for " << forCcy << " and " << domCcy
                                                                                << " from sim market");
                                forTS = discountCurve(forCcy);
                                domTS = discountCurve(domCcy);
                            }

                            for (Size k = 0; k < m; k++) {
                                dates[k] = asof_ + expiries[k];
                                times[k] = wrapper->timeFromReference(dates[k]);
                            }

                            boost::shared_ptr<BlackVolTermStructure> fxVolCurve;
                            if (parameters->fxVolIsSurface(name)) {
                                vector<Real> strikes;
                                strikes = parameters->fxUseMoneyness(name) ? parameters->fxVolMoneyness(name)
                                                                           : parameters->fxVolStdDevs(name);
                                Size n = strikes.size();
                                quotes.resize(n, vector<Handle<Quote>>(m, Handle<Quote>()));

                                // hardcode this for now
                                bool flatExtrapolation = true;

                                // get vol matrix to feed to surface
                                if (parameters->fxUseMoneyness(name)) { // if moneyness
                                    for (Size j = 0; j < m; j++) {
                                        for (Size i = 0; i < n; i++) {
                                            Real mon = strikes[i];
                                            // strike (assuming forward prices)
                                            Real k = spot->value() * mon * initForTS->discount(dates[j]) /
                                                     initDomTS->discount(dates[j]);
                                            Size idx = i * m + j;

                                            Volatility vol = wrapper->blackVol(dates[j], k, true);
                                            boost::shared_ptr<SimpleQuote> q(
                                                new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : vol));
                                            simDataTmp.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(param.first, name, idx),
                                                               std::forward_as_tuple(q));
                                            if (useSpreadedTermStructures_) {
                                                absoluteSimDataTmp.emplace(
                                                    std::piecewise_construct,
                                                    std::forward_as_tuple(param.first, name, idx),
                                                    std::forward_as_tuple(q->value()));
                                            }
                                            quotes[i][j] = Handle<Quote>(q);
                                        }
                                    }
                                    writeSimData(simDataTmp, absoluteSimDataTmp);
                                    simDataWritten = true;
                                    // build the surface
                                    if (useSpreadedTermStructures_) {
                                        fxVolCurve = boost::make_shared<SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                            Handle<BlackVolTermStructure>(wrapper), spot, times,
                                            parameters->fxVolMoneyness(name), quotes,
                                            Handle<Quote>(boost::make_shared<SimpleQuote>(spot->value())), initForTS,
                                            initDomTS, forTS, domTS, stickyStrike);
                                    } else {
                                        fxVolCurve = boost::make_shared<BlackVarianceSurfaceMoneynessForward>(
                                            cal, spot, times, parameters->fxVolMoneyness(name), quotes, dc, forTS,
                                            domTS, stickyStrike, flatExtrapolation);
                                    }
                                } else { // if stdDevPoints
                                    // forwards
                                    vector<Real> fwds;
                                    vector<Real> atmVols;
                                    for (Size i = 0; i < m; i++) {
                                        Real k = spot->value() * initForTS->discount(dates[i]) / initDomTS->discount(dates[i]);
                                        fwds.push_back(k);
                                        atmVols.push_back(wrapper->blackVol(dates[i], k));
                                        DLOG("on date " << dates[i] << ": fwd = " << fwds.back()
                                                        << ", atmVol = " << atmVols.back());
                                    }

                                    // interpolations
                                    Interpolation forwardCurve =
                                        Linear().interpolate(times.begin(), times.end(), fwds.begin());
                                    Interpolation atmVolCurve =
                                        Linear().interpolate(times.begin(), times.end(), atmVols.begin());

                                    // populate quotes
                                    vector<vector<Handle<Quote>>> absQuotes(n,
                                                                            vector<Handle<Quote>>(m, Handle<Quote>()));
                                    BlackVarianceSurfaceStdDevs::populateVolMatrix(wrapper, absQuotes, times,
                                                                                   parameters->fxVolStdDevs(name),
                                                                                   forwardCurve, atmVolCurve);
                                    if (useSpreadedTermStructures_) {
                                        for (Size i = 0; i < n; ++i)
                                            for (Size j = 0; j < m; ++j)
                                                quotes[i][j] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));
                                    } else {
                                        quotes = absQuotes;
                                    }

                                    // sort out simDataTemp
                                    for (Size i = 0; i < m; i++) {
                                        for (Size j = 0; j < n; j++) {
                                            Size idx = j * m + i;
                                            boost::shared_ptr<Quote> q = quotes[j][i].currentLink();
                                            boost::shared_ptr<SimpleQuote> sq =
                                                boost::dynamic_pointer_cast<SimpleQuote>(q);
                                            simDataTmp.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(param.first, name, idx),
                                                               std::forward_as_tuple(sq));
                                            if (useSpreadedTermStructures_) {
                                                absoluteSimDataTmp.emplace(
                                                    std::piecewise_construct,
                                                    std::forward_as_tuple(param.first, name, idx),
                                                    std::forward_as_tuple(absQuotes[j][i]->value()));
                                            }
                                        }
                                    }
                                    writeSimData(simDataTmp, absoluteSimDataTmp);
                                    simDataWritten = true;

                                    // set up a FX Index
                                    Handle<FxIndex> fxInd = fxIndex(name);

                                    if (parameters->fxUseMoneyness(name)) { // moneyness
                                    } else {                                // standard deviations
                                        if (useSpreadedTermStructures_) {
                                            fxVolCurve = boost::make_shared<SpreadedBlackVolatilitySurfaceStdDevs>(
                                                Handle<BlackVolTermStructure>(wrapper), spot, times,
                                                parameters->fxVolStdDevs(name), quotes,
                                                Handle<Quote>(boost::make_shared<SimpleQuote>(spot->value())),
                                                initForTS, initDomTS, forTS, domTS, stickyStrike);
                                        } else {
                                            fxVolCurve = boost::make_shared<BlackVarianceSurfaceStdDevs>(
                                                cal, spot, times, parameters->fxVolStdDevs(name), quotes, dc,
                                                fxInd.currentLink(), stickyStrike, flatExtrapolation);
                                        }
                                    }
                                }                            
                            } else { // not a surface - case for ATM or simulateATMOnly
                                quotes.resize(1, vector<Handle<Quote>>(m, Handle<Quote>()));
                                // Only need ATM quotes in this case
                                for (Size j = 0; j < m; j++) {
                                    // Index is expires then moneyness.
                                    Size idx = j;
                                    Real f =
                                        spot->value() * initForTS->discount(dates[j]) / initDomTS->discount(dates[j]);
                                    Volatility vol = wrapper->blackVol(dates[j], f);
                                    boost::shared_ptr<SimpleQuote> q(
                                        new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : vol));
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, idx),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, idx),
                                                                   std::forward_as_tuple(vol));
                                    }
                                    quotes[0][j] = Handle<Quote>(q);
                                }

                                writeSimData(simDataTmp, absoluteSimDataTmp);
                                simDataWritten = true;

                                if (useSpreadedTermStructures_) {
                                    // if simulate atm only is false, we use the ATM slice from the wrapper only
                                    // the smile dynamics is sticky strike here always (if t0 is a surface)
                                    fxVolCurve = boost::make_shared<SpreadedBlackVolatilityCurve>(
                                        Handle<BlackVolTermStructure>(wrapper), times, quotes[0],
                                        !parameters->simulateFxVolATMOnly());
                                } else {
                                    LOG("ATM FX Vols (BlackVarianceCurve3) for " << name);
                                    boost::shared_ptr<BlackVolTermStructure> atmCurve;
                                    atmCurve = boost::make_shared<BlackVarianceCurve3>(
                                        0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes[0], false);
                                    // if we have a surface but are only simulating atm vols we wrap the atm curve and
                                    // the full t0 surface
                                    if (parameters->simulateFxVolATMOnly()) {
                                        LOG("Simulating FX Vols (FXVolatilityConstantSpread) for " << name);
                                        fxVolCurve = boost::make_shared<BlackVolatilityConstantSpread>(
                                            Handle<BlackVolTermStructure>(atmCurve), wrapper);
                                    } else {
                                        fxVolCurve = atmCurve;
                                    }
                                }
                            }
                            fvh = Handle<BlackVolTermStructure>(fxVolCurve);

                        } else {
                            string decayModeString = parameters->fxVolDecayMode();
                            LOG("Deterministic FX Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // currently only curves (i.e. strike independent) FX volatility structures are
                            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
                            // strike stickiness, since then no yield term structures and no fx spot are required
                            // that define the ATM level - to be revisited when FX surfaces are supported
                            fvh = Handle<BlackVolTermStructure>(
                                boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode,
                                    stickyStrike ? StickyStrike : StickyLogMoneyness));
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
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::EquityVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        Handle<BlackVolTermStructure> wrapper = initMarket->equityVol(name, configuration);
                        Handle<BlackVolTermStructure> evh;

                        bool stickyStrike = parameters_->equityVolSmileDynamics(name) == "StickyStrike";
                        if (param.second.first) {
                            auto eqCurve = equityCurve(name, Market::defaultConfiguration);
                            Handle<Quote> spot = eqCurve->equitySpot();
                            auto expiries = parameters->equityVolExpiries(name);

                            Size m = expiries.size();
                            vector<vector<Handle<Quote>>> quotes;
                            vector<Time> times(m);
                            vector<Date> dates(m);
                            Calendar cal;
                            if (curveConfigs.hasEquityVolCurveConfig(name)) {
                                auto cfg = curveConfigs.equityVolCurveConfig(name);
                                if (cfg->calendar().empty())
                                    cal = parseCalendar(cfg->ccy());
                                else
                                    cal = parseCalendar(cfg->calendar());
                            }
                            if (cal.empty() || cal == NullCalendar()) {
                                // take the equity curves calendar - this at least ensures fixings align
                                cal = eqCurve->fixingCalendar();
                            }
                            DayCounter dc = wrapper->dayCounter();

                            for (Size k = 0; k < m; k++) {
                                dates[k] = cal.advance(asof_, expiries[k]);
                                times[k] = dc.yearFraction(asof_, dates[k]);
                            }

                            boost::shared_ptr<BlackVolTermStructure> eqVolCurve;

                            if (parameters->equityVolIsSurface(name)) {
                                vector<Real> strikes;
                                strikes = parameters->equityUseMoneyness(name)
                                              ? parameters->equityVolMoneyness(name)
                                              : parameters->equityVolStandardDevs(name);
                                Size n = strikes.size();
                                quotes.resize(n, vector<Handle<Quote>>(m, Handle<Quote>()));

                                if (parameters->equityUseMoneyness(name)) { // moneyness surface
                                    for (Size j = 0; j < m; j++) {
                                        for (Size i = 0; i < n; i++) {
                                            Real mon = strikes[i];
                                            // strike (assuming forward prices)
                                            Real k = eqCurve->forecastFixing(dates[j]) * mon;
                                            Size idx = i * m + j;
                                            Volatility vol = wrapper->blackVol(dates[j], k);
                                            boost::shared_ptr<SimpleQuote> q(
                                                new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : vol));
                                            simDataTmp.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(param.first, name, idx),
                                                               std::forward_as_tuple(q));
                                            if (useSpreadedTermStructures_) {
                                                absoluteSimDataTmp.emplace(
                                                    std::piecewise_construct,
                                                    std::forward_as_tuple(param.first, name, idx),
                                                    std::forward_as_tuple(vol));
                                            }
                                            quotes[i][j] = Handle<Quote>(q);
                                        }
                                    }
                                    writeSimData(simDataTmp, absoluteSimDataTmp);
                                    simDataWritten = true;
                                    LOG("Simulating EQ Vols (BlackVarianceSurfaceMoneyness) for " << name);
                                    
                                    if (useSpreadedTermStructures_) {
                                        eqVolCurve = boost::make_shared<SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                            Handle<BlackVolTermStructure>(wrapper), spot, times,
                                            parameters->equityVolMoneyness(name), quotes,
                                            Handle<Quote>(boost::make_shared<SimpleQuote>(spot->value())),
                                            initMarket->equityCurve(name, configuration)->equityDividendCurve(),
                                            initMarket->equityCurve(name, configuration)->equityForecastCurve(),
                                            eqCurve->equityDividendCurve(), eqCurve->equityForecastCurve(),
                                            stickyStrike);
                                    } else {
                                        // FIXME should that be Forward, since we read the vols at fwd moneyness above?
                                        eqVolCurve = boost::make_shared<BlackVarianceSurfaceMoneynessSpot>(
                                            cal, spot, times, parameters->equityVolMoneyness(name), quotes, dc,
                                            stickyStrike);
                                    }
                                    eqVolCurve->enableExtrapolation();

                                } else { // standard deviations surface
                                    // forwards
                                    vector<Real> fwds;
                                    vector<Real> atmVols;
                                    for (Size i = 0; i < expiries.size(); i++) {
                                        auto eqForward = eqCurve->forecastFixing(dates[i]);
                                        fwds.push_back(eqForward);
                                        atmVols.push_back(wrapper->blackVol(dates[i], eqForward));
                                        DLOG("on date " << dates[i] << ": fwd = " << fwds.back()
                                                        << ", atmVol = " << atmVols.back());
                                    }

                                    // interpolations
                                    Interpolation forwardCurve =
                                        Linear().interpolate(times.begin(), times.end(), fwds.begin());
                                    Interpolation atmVolCurve =
                                        Linear().interpolate(times.begin(), times.end(), atmVols.begin());

                                    // populate quotes
                                    vector<vector<Handle<Quote>>> absQuotes(n,
                                                                            vector<Handle<Quote>>(m, Handle<Quote>()));
                                    BlackVarianceSurfaceStdDevs::populateVolMatrix(wrapper, absQuotes, times, strikes,
                                                                                   forwardCurve, atmVolCurve);
                                    if (useSpreadedTermStructures_) {
                                        for (Size i = 0; i < n; ++i)
                                            for (Size j = 0; j < m; ++j)
                                                quotes[i][j] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));
                                    } else {
                                        quotes = absQuotes;
                                    }

                                    // add to simDataTemp
                                    for (Size i = 0; i < m; i++) {
                                        for (Size j = 0; j < n; j++) {
                                            Size idx = j * m + i;
                                            boost::shared_ptr<Quote> q = quotes[j][i].currentLink();
                                            boost::shared_ptr<SimpleQuote> sq =
                                                boost::dynamic_pointer_cast<SimpleQuote>(q);
                                            QL_REQUIRE(sq, "Quote is not a SimpleQuote"); // why do we need this?
                                            simDataTmp.emplace(std::piecewise_construct,
                                                               std::forward_as_tuple(param.first, name, idx),
                                                               std::forward_as_tuple(sq));
                                            if (useSpreadedTermStructures_) {
                                                absoluteSimDataTmp.emplace(
                                                    std::piecewise_construct,
                                                    std::forward_as_tuple(param.first, name, idx),
                                                    std::forward_as_tuple(absQuotes[j][i]->value()));
                                            }
                                        }
                                    }
                                    writeSimData(simDataTmp, absoluteSimDataTmp);
                                    simDataWritten = true;
                                    bool flatExtrapolation = true; // flat extrapolation of strikes at far ends.
                                    if (useSpreadedTermStructures_) {
                                        eqVolCurve = boost::make_shared<SpreadedBlackVolatilitySurfaceStdDevs>(
                                            Handle<BlackVolTermStructure>(wrapper), spot, times,
                                            parameters->equityVolStandardDevs(name), quotes,
                                            Handle<Quote>(boost::make_shared<SimpleQuote>(spot->value())),
                                            initMarket->equityCurve(name, configuration)->equityDividendCurve(),
                                            initMarket->equityCurve(name, configuration)->equityForecastCurve(),
                                            eqCurve->equityDividendCurve(), eqCurve->equityForecastCurve(),
                                            stickyStrike);
                                    } else {
                                        eqVolCurve = boost::make_shared<BlackVarianceSurfaceStdDevs>(
                                            cal, spot, times, parameters->equityVolStandardDevs(name), quotes, dc,
                                            eqCurve.currentLink(), stickyStrike, flatExtrapolation);
                                    }
                                }
                            } else { // not a surface - case for ATM or simulateATMOnly
                                quotes.resize(1, vector<Handle<Quote>>(m, Handle<Quote>()));
                                // Only need ATM quotes in this case
                                for (Size j = 0; j < m; j++) {
                                    // Index is expires then moneyness. TODO: is this the best?
                                    Size idx = j;
                                    auto eqForward = eqCurve->fixing(dates[j]);
                                    Volatility vol = wrapper->blackVol(dates[j], eqForward);
                                    boost::shared_ptr<SimpleQuote> q(
                                        new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : vol));
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, idx),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, idx),
                                                                   std::forward_as_tuple(vol));
                                    }
                                    quotes[0][j] = Handle<Quote>(q);
                                }

                                writeSimData(simDataTmp, absoluteSimDataTmp);
                                simDataWritten = true;

                                if (useSpreadedTermStructures_) {
                                    // if simulate atm only is false, we use the ATM slice from the wrapper only
                                    // the smile dynamics is sticky strike here always (if t0 is a surface)
                                    eqVolCurve = boost::make_shared<SpreadedBlackVolatilityCurve>(
                                        Handle<BlackVolTermStructure>(wrapper), times, quotes[0],
                                        !parameters->simulateEquityVolATMOnly());
                                } else {
                                    LOG("ATM EQ Vols (BlackVarianceCurve3) for " << name);
                                    boost::shared_ptr<BlackVolTermStructure> atmCurve;
                                    atmCurve = boost::make_shared<BlackVarianceCurve3>(0, NullCalendar(),
                                                                                       wrapper->businessDayConvention(),
                                                                                       dc, times, quotes[0], false);
                                    // if we have a surface but are only simulating atm vols we wrap the atm curve and
                                    // the full t0 surface
                                    if (parameters->simulateEquityVolATMOnly()) {
                                        LOG("Simulating EQ Vols (EquityVolatilityConstantSpread) for " << name);
                                        eqVolCurve = boost::make_shared<BlackVolatilityConstantSpread>(
                                            Handle<BlackVolTermStructure>(atmCurve), wrapper);
                                    } else {
                                        eqVolCurve = atmCurve;
                                    }
                                }
                            }
                            evh = Handle<BlackVolTermStructure>(eqVolCurve);

                        } else {
                            string decayModeString = parameters->equityVolDecayMode();
                            DLOG("Deterministic EQ Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // currently only curves (i.e. strike independent) EQ volatility structures are
                            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
                            // strike stickiness, since then no yield term structures and no EQ spot are required
                            // that define the ATM level - to be revisited when EQ surfaces are supported
                            evh = Handle<BlackVolTermStructure>(
                                boost::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode,
                                    stickyStrike ? StickyStrike : StickyLogMoneyness));
                        }
                        if (wrapper->allowsExtrapolation())
                            evh->enableExtrapolation();
                        equityVols_.insert(pair<pair<string, string>, Handle<BlackVolTermStructure>>(
                            make_pair(Market::defaultConfiguration, name), evh));
                        DLOG("EQ volatility curve built for " << name);
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::BaseCorrelation:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        Handle<QuantExt::BaseCorrelationTermStructure> wrapper =
                            initMarket->baseCorrelation(name, configuration);
                        if (!param.second.first)
                            baseCorrelations_.insert(
                                pair<pair<string, string>, Handle<QuantExt::BaseCorrelationTermStructure>>(
                                    make_pair(Market::defaultConfiguration, name), wrapper));
                        else {
                            Size nd = parameters->baseCorrelationDetachmentPoints().size();
                            Size nt = parameters->baseCorrelationTerms().size();
                            vector<vector<Handle<Quote>>> quotes(nd, vector<Handle<Quote>>(nt));
                            vector<Period> terms(nt);
                            vector<double> detachmentPoints(nd);
                            for (Size i = 0; i < nd; ++i) {
                                Real lossLevel = parameters->baseCorrelationDetachmentPoints()[i];
                                detachmentPoints[i] = lossLevel;
                                for (Size j = 0; j < nt; ++j) {
                                    Period term = parameters->baseCorrelationTerms()[j];
                                    if (i == 0)
                                        terms[j] = term;
                                    Real bc = wrapper->correlation(asof_ + term, lossLevel, true); // extrapolate
                                    boost::shared_ptr<SimpleQuote> q =
                                        boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : bc);
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, i * nt + j),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, i * nt + j),
                                                                   std::forward_as_tuple(bc));
                                    }
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;

                            //
                            if (nt == 1) {                           
                                terms.push_back(terms[0] + 1 * terms[0].units()); // arbitrary, but larger than the first term
                                for (Size i = 0; i < nd; ++i)
                                    quotes[i].push_back(quotes[i][0]);
                            }

                            if (nd == 1) {
                                quotes.push_back(vector<Handle<Quote>>(terms.size()));
                                for (Size j = 0; j < terms.size(); ++j)
                                    quotes[1][j] = quotes[0][j];

                                if (detachmentPoints[0] < 1.0 && !QuantLib::close_enough(detachmentPoints[0], 1.0)) {
                                    detachmentPoints.push_back(1.0);
                                } else {
                                    detachmentPoints.insert(detachmentPoints.begin(), 0.01); // arbitrary, but larger than then 0 and less than 1.0
                                }
                            }
                            
                            boost::shared_ptr<QuantExt::BaseCorrelationTermStructure> bcp;
                            if (useSpreadedTermStructures_) {
                                bcp = boost::make_shared<QuantExt::SpreadedBaseCorrelationCurve>(
                                    wrapper, terms, detachmentPoints, quotes);
                                bcp->enableExtrapolation(wrapper->allowsExtrapolation());
                            } else {
                                DayCounter dc = wrapper->dayCounter();
                                bcp = boost::make_shared<InterpolatedBaseCorrelationTermStructure<Bilinear>>(
                                    wrapper->settlementDays(), wrapper->calendar(), wrapper->businessDayConvention(),
                                    terms, detachmentPoints, quotes, dc);

                                bcp->enableExtrapolation(wrapper->allowsExtrapolation());
                            }
                            Handle<QuantExt::BaseCorrelationTermStructure> bch(bcp);
                            baseCorrelations_.insert(
                                pair<pair<string, string>, Handle<QuantExt::BaseCorrelationTermStructure>>(
                                    make_pair(Market::defaultConfiguration, name), bch));
                        }
                        DLOG("Base correlations built for " << name);
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CPIIndex:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
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
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::ZeroInflationCurve:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building " << name << " zero inflation curve");

                        Handle<ZeroInflationIndex> inflationIndex = initMarket->zeroInflationIndex(name, configuration);
                        Handle<ZeroInflationTermStructure> inflationTs = inflationIndex->zeroInflationTermStructure();
                        vector<string> keys(parameters->zeroInflationTenors(name).size());

                        Date date0 = asof_ - inflationTs->observationLag();
                        DayCounter dc = inflationTs->dayCounter();
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
                            Real rate = inflationTs->zeroRate(quoteDates[i - 1]);
                            if (inflationTs->hasSeasonality()) {
                                Date fixingDate = quoteDates[i - 1] - inflationTs->observationLag();
                                rate = inflationTs->seasonality()->deseasonalisedZeroRate(fixingDate,                                 
                                    rate, *inflationTs.currentLink());
                            }
                            auto q = boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : rate);
                            if (i == 1) {
                                // add the zero rate at first tenor to the T0 time, to ensure flat interpolation of T1
                                // rate for time t T0 < t < T1
                                quotes.push_back(Handle<Quote>(q));
                            }
                            quotes.push_back(Handle<Quote>(q));
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(param.first, name, i - 1),
                                               std::forward_as_tuple(q));
                            if (useSpreadedTermStructures_)
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, i - 1),
                                                           std::forward_as_tuple(rate));
                            DLOG("ScenarioSimMarket zero inflation curve " << name << " zeroRate[" << i
                                                                           << "]=" << rate);
                        }

                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                                                
                        // FIXME: Settlement days set to zero - needed for floating term structure implementation
                        boost::shared_ptr<ZeroInflationTermStructure> zeroCurve;
                        if (useSpreadedTermStructures_) {
                            zeroCurve =
                                boost::make_shared<SpreadedZeroInflationCurve>(inflationTs, zeroCurveTimes, quotes);
                        } else {
                            zeroCurve = boost::make_shared<ZeroInflationCurveObserverMoving<Linear>>(
                                0, inflationIndex->fixingCalendar(), dc, inflationTs->observationLag(),
                                inflationTs->frequency(), false, zeroCurveTimes, quotes,
                                inflationTs->seasonality());
                        }

                        Handle<ZeroInflationTermStructure> its(zeroCurve);
                        its->enableExtrapolation();
                        boost::shared_ptr<ZeroInflationIndex> i =
                            parseZeroInflationIndex(name, Handle<ZeroInflationTermStructure>(its));
                        Handle<ZeroInflationIndex> zh(i);
                        zeroInflationIndices_.insert(pair<pair<string, string>, Handle<ZeroInflationIndex>>(
                            make_pair(Market::defaultConfiguration, name), zh));

                        LOG("building " << name << " zero inflation curve done");
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building " << name << " zero inflation cap/floor volatility curve...");
                        Handle<QuantLib::CPIVolatilitySurface> wrapper =
                            initMarket->cpiInflationCapFloorVolatilitySurface(name, configuration);
                        Handle<ZeroInflationIndex> zeroInflationIndex =
                            initMarket->zeroInflationIndex(name, configuration);
                        // LOG("Initial market zero inflation cap/floor volatility type = " <<
                        // wrapper->volatilityType());

                        Handle<QuantLib::CPIVolatilitySurface> hCpiVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            LOG("Simulating zero inflation cap/floor vols for index name " << name);

                            DayCounter dc = wrapper->dayCounter();
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
                                    auto q = boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
                                    Size index = i * strikes.size() + j;
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, index),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, index),
                                                                   std::forward_as_tuple(vol));
                                    }
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;



                            if (useSpreadedTermStructures_) {
                                auto surface = boost::dynamic_pointer_cast<QuantExt::CPIVolatilitySurface>(wrapper.currentLink());
                                QL_REQUIRE(surface,
                                           "Internal error, todays market should build QuantExt::CPIVolatiltiySurface "
                                           "instead of QuantLib::CPIVolatilitySurface");
                                hCpiVol = Handle<QuantLib::CPIVolatilitySurface>(
                                    boost::make_shared<SpreadedCPIVolatilitySurface>(
                                        Handle<QuantExt::CPIVolatilitySurface>(surface), optionDates, strikes, quotes));
                            } else {
                                auto surface =
                                    boost::dynamic_pointer_cast<QuantExt::CPIVolatilitySurface>(wrapper.currentLink());
                                QL_REQUIRE(surface,
                                           "Internal error, todays market should build QuantExt::CPIVolatiltiySurface "
                                           "instead of QuantLib::CPIVolatilitySurface");
                                hCpiVol = Handle<QuantLib::CPIVolatilitySurface>(
                                    boost::make_shared<InterpolatedCPIVolatilitySurface<Bilinear>>(
                                        optionTenors, strikes, quotes, zeroInflationIndex.currentLink(),
                                        wrapper->settlementDays(), wrapper->calendar(),
                                        wrapper->businessDayConvention(), wrapper->dayCounter(),
                                        wrapper->observationLag(), surface->capFloorStartDate(), Bilinear(),
                                        surface->volatilityType(), surface->displacement()));
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
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::YoYInflationCurve:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        Handle<YoYInflationIndex> yoyInflationIndex =
                            initMarket->yoyInflationIndex(name, configuration);
                        Handle<YoYInflationTermStructure> yoyInflationTs =
                            yoyInflationIndex->yoyInflationTermStructure();
                        vector<string> keys(parameters->yoyInflationTenors(name).size());

                        Date date0 = asof_ - yoyInflationTs->observationLag();
                        DayCounter dc = yoyInflationTs->dayCounter();
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
                            Real rate = yoyInflationTs->yoyRate(quoteDates[i - 1]);
                            auto q = boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : rate);
                            if (i == 1) {
                                // add the zero rate at first tenor to the T0 time, to ensure flat interpolation of T1
                                // rate for time t T0 < t < T1
                                quotes.push_back(Handle<Quote>(q));
                            }
                            quotes.push_back(Handle<Quote>(q));
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(param.first, name, i - 1),
                                               std::forward_as_tuple(q));
                            if (useSpreadedTermStructures_)
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name, i - 1),
                                                           std::forward_as_tuple(rate));
                            DLOG("ScenarioSimMarket yoy inflation curve " << name << " yoyRate[" << i << "]=" << rate);
                        }

                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                                                
                        boost::shared_ptr<YoYInflationTermStructure> yoyCurve;
                        // Note this is *not* a floating term structure, it is only suitable for sensi runs
                        // TODO: floating
                        if (useSpreadedTermStructures_) {
                            yoyCurve =
                                boost::make_shared<SpreadedYoYInflationCurve>(yoyInflationTs, yoyCurveTimes, quotes);
                        } else {
                            yoyCurve = boost::make_shared<YoYInflationCurveObserverMoving<Linear>>(
                                0, yoyInflationIndex->fixingCalendar(), dc, yoyInflationTs->observationLag(),
                                yoyInflationTs->frequency(), yoyInflationIndex->interpolated(), yoyCurveTimes,
                                quotes, yoyInflationTs->seasonality());
                        }

                        Handle<YoYInflationTermStructure> its(yoyCurve);
                        its->enableExtrapolation();
                        boost::shared_ptr<YoYInflationIndex> i(yoyInflationIndex->clone(its));
                        Handle<YoYInflationIndex> zh(i);
                        yoyInflationIndices_.insert(pair<pair<string, string>, Handle<YoYInflationIndex>>(
                            make_pair(Market::defaultConfiguration, name), zh));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
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
                                optionDates[i] = wrapper->optionDateFromTenor(optionTenors[i]);
                                for (Size j = 0; j < strikes.size(); ++j) {
                                    Real vol =
                                        wrapper->volatility(optionTenors[i], strikes[j], wrapper->observationLag(),
                                                            wrapper->allowsExtrapolation());
                                    boost::shared_ptr<SimpleQuote> q(
                                        new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : vol));
                                    Size index = i * strikes.size() + j;
                                    simDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name, index),
                                                       std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                                   std::forward_as_tuple(param.first, name, index),
                                                                   std::forward_as_tuple(vol));
                                    }
                                    quotes[i][j] = Handle<Quote>(q);
                                    TLOG("ScenarioSimMarket yoy cf vol " << name << " tenor #" << i << " strike #" << j
                                                                         << " " << vol);
                                }
                            }
                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;

                            DayCounter dc = wrapper->dayCounter();
                
                            boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyoptionletvolsurface;
                            if (useSpreadedTermStructures_) {
                                yoyoptionletvolsurface = boost::make_shared<QuantExt::SpreadedYoYVolatilitySurface>(
                                    wrapper, optionDates, strikes, quotes);
                            } else {
                                yoyoptionletvolsurface = boost::make_shared<StrippedYoYInflationOptionletVol>(
                                    0, wrapper->calendar(), wrapper->businessDayConvention(), dc,
                                    wrapper->observationLag(), wrapper->frequency(), wrapper->indexIsInterpolated(),
                                    optionDates, strikes, quotes, wrapper->volatilityType(), wrapper->displacement());
                            }
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
                        LOG("Simulation market yoy inflation cap/floor volatility type = "
                            << hYoYCapletVol->volatilityType());
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CommodityCurve:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building commodity curve for " << name);

                        // Time zero initial market commodity curve
                        Handle<PriceTermStructure> initialCommodityCurve =
                            initMarket->commodityPriceCurve(name, configuration);
                        bool allowsExtrapolation = initialCommodityCurve->allowsExtrapolation();

                        // Get the configured simulation tenors. Simulation tenors being empty at this point means
                        // that we wish to use the pillar date points from the t_0 market PriceTermStructure.
                        vector<Period> simulationTenors = parameters->commodityCurveTenors(name);
                        DayCounter commodityCurveDayCounter = initialCommodityCurve->dayCounter();
                        if (simulationTenors.empty()) {
                            DLOG("simulation tenors are empty, use "
                                 << initialCommodityCurve->pillarDates().size()
                                 << " pillar dates from T0 curve to build ssm curve.");
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
                        } else {
                            DLOG("using " << simulationTenors.size() << " simulation tenors.");
                        }

                        // Get prices at specified simulation times from time 0 market curve and place in quotes
                        vector<Handle<Quote>> quotes(simulationTenors.size());
                        for (Size i = 0; i < simulationTenors.size(); i++) {
                            Date d = asof_ + simulationTenors[i];
                            Real price = initialCommodityCurve->price(d, allowsExtrapolation);
			    TLOG("Commodity curve: price at " << io::iso_date(d) << " is " << price);
                            // if we simulate the factors and use spreaded ts, the quote should be zero
                            boost::shared_ptr<SimpleQuote> quote = boost::make_shared<SimpleQuote>(
                                param.second.first && useSpreadedTermStructures_ ? 0.0 : price);
                            quotes[i] = Handle<Quote>(quote);

                            // If we are simulating commodities, add the quote to simData_
                            if (param.second.first) {
                                simDataTmp.emplace(piecewise_construct, forward_as_tuple(param.first, name, i),
                                                   forward_as_tuple(quote));
                                if (useSpreadedTermStructures_)
                                    absoluteSimDataTmp.emplace(piecewise_construct,
                                                               forward_as_tuple(param.first, name, i),
                                                               forward_as_tuple(price));
                            }
                        }

                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;

                        Handle<PriceTermStructure> pts;
                        if (param.second.first && useSpreadedTermStructures_) {
                            // Created spreaded commodity price curve if we simulate commodities and spreads should be
                            // used
                            vector<Real> simulationTimes;
                            for (auto const& t : simulationTenors) {
                                simulationTimes.push_back(commodityCurveDayCounter.yearFraction(asof_, asof_ + t));
                            }
                            if (simulationTimes.front() != 0.0) {
                                simulationTimes.insert(simulationTimes.begin(), 0.0);
                                quotes.insert(quotes.begin(), quotes.front());
                            }
                            pts = Handle<PriceTermStructure>(boost::make_shared<SpreadedPriceTermStructure>(
                                initialCommodityCurve, simulationTimes, quotes));
                        } else {
                            // Create a commodity price curve with simulation tenors as pillars and store
                            // Hard-coded linear flat interpolation here - may need to make this more dynamic
                            pts = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<LinearFlat>>(
                                simulationTenors, quotes, commodityCurveDayCounter, initialCommodityCurve->currency()));
                        }
                        pts->enableExtrapolation(allowsExtrapolation);

                        Handle<CommodityIndex> commIdx(parseCommodityIndex(name, false, pts));
                        commodityIndices_.emplace(piecewise_construct,
                                                  forward_as_tuple(Market::defaultConfiguration, name),
                                                  forward_as_tuple(commIdx));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CommodityVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("building commodity volatility for " << name);

                        // Get initial base volatility structure
                        Handle<BlackVolTermStructure> baseVol = initMarket->commodityVolatility(name, configuration);

                        Handle<BlackVolTermStructure> newVol;
                        bool stickyStrike = parameters_->commodityVolSmileDynamics(name) == "StickyStrike";
                        if (param.second.first) {

                            // Check and reorg moneyness and/or expiries to simplify subsequent code.
                            vector<Real> moneyness = parameters->commodityVolMoneyness(name);
                            QL_REQUIRE(!moneyness.empty(), "Commodity volatility moneyness for "
                                                               << name << " should have at least one element.");
                            sort(moneyness.begin(), moneyness.end());
                            auto mIt = unique(moneyness.begin(), moneyness.end(),
                                              [](const Real& x, const Real& y) { return close(x, y); });
                            QL_REQUIRE(mIt == moneyness.end(),
                                       "Commodity volatility moneyness values for " << name << " should be unique.");

                            vector<Period> expiries = parameters->commodityVolExpiries(name);
                            QL_REQUIRE(!expiries.empty(), "Commodity volatility expiries for "
                                                              << name << " should have at least one element.");
                            sort(expiries.begin(), expiries.end());
                            auto eIt = unique(expiries.begin(), expiries.end());
                            QL_REQUIRE(eIt == expiries.end(),
                                       "Commodity volatility expiries for " << name << " should be unique.");

                            // Get this scenario simulation market's commodity price curve. An exception is expected
                            // if there is no commodity curve but there is a commodity volatility.
                            const auto& priceCurve = *commodityPriceCurve(name, configuration);

                            // More than one moneyness implies a surface. If we have a surface, we will build a
                            // forward surface below which requires two yield term structures, one for the commodity
                            // price currency and another that recovers the commodity forward prices. We don't want
                            // the commodity prices changing with changes in the commodity price currency yield curve
                            // so we take a copy here - it will work for sticky strike false also.
                            bool isSurface = moneyness.size() > 1;
                            Handle<YieldTermStructure> yts;
                            Handle<YieldTermStructure> priceYts;

                            if (isSurface) {

                                vector<Date> dates{asof_};
                                vector<Real> dfs{1.0};

                                auto discCurve = discountCurve(priceCurve->currency().code(), configuration);
                                for (const auto& expiry : expiries) {
                                    auto d = asof_ + expiry;
                                    if (d == asof_)
                                        continue;
                                    dates.push_back(d);
                                    dfs.push_back(discCurve->discount(d, true));
                                }

                                auto ytsPtr = boost::make_shared<DiscountCurve>(dates, dfs, discCurve->dayCounter());
                                ytsPtr->enableExtrapolation();
                                yts = Handle<YieldTermStructure>(ytsPtr);
                                priceYts = Handle<YieldTermStructure>(
                                    boost::make_shared<PriceTermStructureAdapter>(priceCurve, ytsPtr));
                                priceYts->enableExtrapolation();
                            }

                            // Create surface of quotes, rows are moneyness, columns are expiries.
                            using QuoteRow = vector<Handle<Quote>>;
                            using QuoteMatrix = vector<QuoteRow>;
                            QuoteMatrix quotes(moneyness.size(), QuoteRow(expiries.size()));

                            // Calculate up front the expiry times, dates and forward prices.
                            vector<Date> expiryDates(expiries.size());
                            vector<Time> expiryTimes(expiries.size());
                            vector<Real> forwards(expiries.size());
                            // TODO: do we want to use the base vol dc or - as elsewhere - a dc specified in the ssm
                            // parameters?
                            DayCounter dayCounter = baseVol->dayCounter();
                            for (Size j = 0; j < expiries.size(); ++j) {
                                Date d = asof_ + expiries[j];
                                expiryDates[j] = d;
                                expiryTimes[j] = dayCounter.yearFraction(asof_, d);
                                forwards[j] = priceCurve->price(d);
                            }

                            // Store the quotes.
                            Size index = 0;
                            for (Size i = 0; i < moneyness.size(); ++i) {
                                for (Size j = 0; j < expiries.size(); ++j) {
                                    Real strike = moneyness[i] * forwards[j];
                                    auto vol = baseVol->blackVol(expiryDates[j], strike);
                                    auto quote =
                                        boost::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
                                    simDataTmp.emplace(piecewise_construct, forward_as_tuple(param.first, name, index),
                                                       forward_as_tuple(quote));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(piecewise_construct,
                                                                   forward_as_tuple(param.first, name, index),
                                                                   forward_as_tuple(vol));
                                    }
                                    quotes[i][j] = Handle<Quote>(quote);
                                    ++index;
                                }
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;

                            // Create volatility structure
                            if (!isSurface) {
                                DLOG("Ssm comm vol for " << name << " uses BlackVarianceCurve3.");
                                if (useSpreadedTermStructures_) {
                                    newVol =
                                        Handle<BlackVolTermStructure>(boost::make_shared<SpreadedBlackVolatilityCurve>(
                                            Handle<BlackVolTermStructure>(baseVol), expiryTimes, quotes[0], true));
                                } else {
                                    newVol = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceCurve3>(
                                        0, NullCalendar(), baseVol->businessDayConvention(), dayCounter, expiryTimes,
                                        quotes[0], false));
                                }
                            } else {
			        DLOG("Ssm comm vol for " << name << " uses BlackVarianceSurfaceMoneynessSpot.");

                                bool flatExtrapMoneyness = true;
                                Handle<Quote> spot(boost::make_shared<SimpleQuote>(priceCurve->price(0)));
                                if (useSpreadedTermStructures_) {
                                    // get init market curves to populate sticky ts in vol surface ctor
                                    Handle<YieldTermStructure> initMarketYts =
                                        initMarket->discountCurve(priceCurve->currency().code(), configuration);
                                    Handle<QuantExt::PriceTermStructure> priceCurve =
                                        initMarket->commodityPriceCurve(name, configuration);
                                    Handle<YieldTermStructure> initMarketPriceYts(
                                        boost::make_shared<PriceTermStructureAdapter>(*priceCurve, *initMarketYts));
                                    // create vol surface
                                    newVol = Handle<BlackVolTermStructure>(
                                        boost::make_shared<SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                            Handle<BlackVolTermStructure>(baseVol), spot, expiryTimes, moneyness,
                                            quotes, Handle<Quote>(boost::make_shared<SimpleQuote>(spot->value())),
                                            initMarketPriceYts, initMarketYts, priceYts, yts, stickyStrike));
                                } else {
                                    newVol = Handle<BlackVolTermStructure>(
                                        boost::make_shared<BlackVarianceSurfaceMoneynessForward>(
                                            baseVol->calendar(), spot, expiryTimes, moneyness, quotes, dayCounter,
                                            priceYts, yts, stickyStrike, flatExtrapMoneyness));
                                }
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
                                    baseVol, 0, NullCalendar(), decayMode,
                                    stickyStrike ? StickyStrike : StickyLogMoneyness));
                        }

                        newVol->enableExtrapolation(baseVol->allowsExtrapolation());
                        commodityVols_.emplace(piecewise_construct,
                                               forward_as_tuple(Market::defaultConfiguration, name),
                                               forward_as_tuple(newVol));

                        DLOG("Commodity volatility curve built for " << name);
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::Correlation:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        LOG("Adding correlations for " << name << " from configuration " << configuration);

                        vector<string> tokens = getCorrelationTokens(name);
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
                            DayCounter dc = baseCorr->dayCounter();
                            
                            for (Size i = 0; i < n; i++) {
                                Real strike = parameters->correlationStrikes()[i];

                                for (Size j = 0; j < m; j++) {
                                    // Index is expiries then strike TODO: is this the best?
                                    Size idx = i * m + j;
                                    times[j] = dc.yearFraction(asof_, asof_ + parameters->correlationExpiries()[j]);
                                    Real correlation =
                                        baseCorr->correlation(asof_ + parameters->correlationExpiries()[j], strike);
                                    boost::shared_ptr<SimpleQuote> q(
                                        new SimpleQuote(useSpreadedTermStructures_ ? 0.0 : correlation));
                                    simDataTmp.emplace(
                                        std::piecewise_construct,
                                        std::forward_as_tuple(RiskFactorKey::KeyType::Correlation, name, idx),
                                        std::forward_as_tuple(q));
                                    if (useSpreadedTermStructures_) {
                                        absoluteSimDataTmp.emplace(
                                            std::piecewise_construct,
                                            std::forward_as_tuple(RiskFactorKey::KeyType::Correlation, name, idx),
                                            std::forward_as_tuple(correlation));
                                    }
                                    quotes[i][j] = Handle<Quote>(q);
                                }
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp);
                            simDataWritten = true;

                            if (n == 1 && m == 1) {
                                if (useSpreadedTermStructures_) {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(
                                        boost::make_shared<QuantExt::SpreadedCorrelationCurve>(baseCorr, times,
                                                                                               quotes[0]));
                                } else {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(boost::make_shared<FlatCorrelation>(
                                        baseCorr->settlementDays(), cal, quotes[0][0], dc));
                                }
                            } else if (n == 1) {
                                if (useSpreadedTermStructures_) {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(
                                        boost::make_shared<QuantExt::SpreadedCorrelationCurve>(baseCorr, times,
                                                                                               quotes[0]));
                                } else {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(
                                        boost::make_shared<InterpolatedCorrelationCurve<Linear>>(times, quotes[0], dc,
                                                                                                 cal));
                                }
                            } else {
                                QL_FAIL("only atm or flat correlation termstructures currently supported");
                            }

                            ch->enableExtrapolation(baseCorr->allowsExtrapolation());
                        } else {
                            ch = Handle<QuantExt::CorrelationTermStructure>(*baseCorr);
                        }

                        correlationCurves_[make_tuple(Market::defaultConfiguration, pair.first, pair.second)] = ch;
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::CPR:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("Adding cpr " << name << " from configuration " << configuration);
                        boost::shared_ptr<SimpleQuote> cprQuote(
                            new SimpleQuote(initMarket->cpr(name, configuration)->value()));
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(cprQuote));
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp);
                        simDataWritten = true;
                        cprs_.insert(pair<pair<string, string>, Handle<Quote>>(
                            make_pair(Market::defaultConfiguration, name), Handle<Quote>(cprQuote)));
                    } catch (const std::exception& e) {
                        processException(continueOnError, e, name, param.first, simDataWritten);
                    }
                }
                break;

            case RiskFactorKey::KeyType::SurvivalWeight:
                // nothing to do, these are written to asd
                break;

            case RiskFactorKey::KeyType::CreditState:
                // nothing to do, these are written to asd
                break;

            case RiskFactorKey::KeyType::None:
                WLOG("RiskFactorKey None not yet implemented");
                break;
            }

            if (!param.second.second.empty()) {
                LOG("built " << std::left << std::setw(25) << param.first << std::right << std::setw(10)
                             << param.second.second.size() << std::setprecision(3) << std::setw(15)
                             << static_cast<double>(timer.elapsed().wall) / 1E6 << " ms");
            }

        } catch (const std::exception& e) {
            ALOG("ScenarioSimMarket::ScenarioSimMarket() top level catch " << e.what());
            processException(continueOnError, e);
        }
    }

    // swap indices
    DLOG("building swap indices...");
    for (const auto& it : parameters->swapIndices()) {
        addSwapIndexToSsm(it.first, continueOnError);
    }

    LOG("building base scenario");
    baseScenario_ = boost::make_shared<SimpleScenario>(initMarket->asofDate(), "BASE", 1.0);
    if (!useSpreadedTermStructures_) {
        for (auto const& data : simData_) {
            baseScenario_->add(data.first, data.second->value());
        }
        baseScenarioAbsolute_ = baseScenario_;
    } else {
        baseScenarioAbsolute_ = boost::make_shared<SimpleScenario>(initMarket->asofDate(), "BASE", 1.0);
        for (auto const& data : simData_) {
            baseScenario_->add(data.first, data.second->value());
            baseScenarioAbsolute_->add(data.first, data.second->value());
        }
        for (auto const& data : absoluteSimData_) {
            baseScenarioAbsolute_->add(data.first, data.second);
        }
    }
    LOG("building base scenario done");
}

bool ScenarioSimMarket::addSwapIndexToSsm(const std::string& indexName, const bool continueOnError) {
    auto dsc = parameters_->swapIndices().find(indexName);
    if (dsc == parameters_->swapIndices().end()) {
        return false;
    }
    DLOG("Adding swap index " << indexName << " with discounting index " << dsc->second);
    try {
        addSwapIndex(indexName, dsc->second, Market::defaultConfiguration);
    DLOG("Adding swap index " << indexName << " done.");
    return true;
    } catch (const std::exception& e) {
        processException(continueOnError, e, indexName);
    return false;
    }
}

void ScenarioSimMarket::reset() {
    auto filterBackup = filter_;
    // no filter
    filter_ = boost::make_shared<ScenarioFilter>();
    // reset eval date
    Settings::instance().evaluationDate() = baseScenario_->asof();
    // reset numeraire and label
    numeraire_ = baseScenario_->getNumeraire();
    label_ = baseScenario_->label();
    // delete the sim data cache
    cachedSimData_.clear();
    cachedSimDataActive_.clear();
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

void ScenarioSimMarket::applyScenario(const boost::shared_ptr<Scenario>& scenario) {

    currentScenario_ = scenario;

    // apply scenario based on cached indices for simData_ for a SimpleScenario
    // this assumes that all scenarios have an identical key structure in their data map

    if (cacheSimData_) {
        if (auto s = boost::dynamic_pointer_cast<SimpleScenario>(scenario)) {

            // fill cache

            if (cachedSimData_.empty()) {
                Size count = 0;
                for (auto const& d : s->data()) {
                    auto it = simData_.find(d.first);
                    if (it == simData_.end()) {
                        WLOG("simulation data point missing for key " << d.first);
                        cachedSimData_.push_back(boost::shared_ptr<SimpleQuote>());
                        cachedSimDataActive_.push_back(false);
                    } else {
                        ++count;
                        cachedSimData_.push_back(it->second);
                        cachedSimDataActive_.push_back(filter_->allow(d.first));
                    }
                }
                if (count != simData_.size() && !allowPartialScenarios_) {
                    ALOG("mismatch between scenario and sim data size, " << count << " vs " << simData_.size());
                    for (auto it : simData_) {
                        if (!scenario->has(it.first))
                            WLOG("Key " << it.first << " missing in scenario");
                    }
                    QL_FAIL("mismatch between scenario and sim data size, exit.");
                }
            }

            // apply scenario data according to cached indices

            Size i = 0;
            for (auto const& q : s->data()) {
                if (cachedSimDataActive_[i])
                    cachedSimData_[i]->setValue(q.second);
                ++i;
            }

            return;
        }
    }

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
            WLOG("simulation data point missing for key " << key);
        } else {
            if (filter_->allow(key)) {
                it->second->setValue(scenario->get(key));
            }
            count++;
        }
    }

    if (count != simData_.size() && !allowPartialScenarios_) {
        ALOG("mismatch between scenario and sim data size, " << count << " vs " << simData_.size());
        for (auto it : simData_) {
            if (!scenario->has(it.first))
                ALOG("Key " << it.first << " missing in scenario");
        }
        QL_FAIL("mismatch between scenario and sim data size, exit.");
    }
}

void ScenarioSimMarket::preUpdate() {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    if (om == ObservationMode::Mode::Disable)
        ObservableSettings::instance().disableUpdates(false);
    else if (om == ObservationMode::Mode::Defer)
        ObservableSettings::instance().disableUpdates(true);
}

void ScenarioSimMarket::updateDate(const Date& d) {
    ObservationMode::Mode om = ObservationMode::instance().mode();
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
}

void ScenarioSimMarket::updateScenario(const Date& d) {
    QL_REQUIRE(scenarioGenerator_ != nullptr, "ScenarioSimMarket::update: no scenario generator set");
    auto scenario = scenarioGenerator_->next(d);
    QL_REQUIRE(scenario->asof() == d,
               "Invalid Scenario date " << scenario->asof() << ", expected " << d);
    numeraire_ = scenario->getNumeraire();
    label_ = scenario->label();
    applyScenario(scenario);
}

void ScenarioSimMarket::postUpdate(const Date& d, bool withFixings) {
    ObservationMode::Mode om = ObservationMode::instance().mode();

    // Observation Mode - key to update these before fixings are set
    if (om == ObservationMode::Mode::Disable) {
        refresh();
        ObservableSettings::instance().enableUpdates();
    } else if (om == ObservationMode::Mode::Defer) {
        ObservableSettings::instance().enableUpdates();
    }

    // Apply fixings as historical fixings. Must do this before we populate ASD
    if (withFixings)
        fixingManager_->update(d);
}

void ScenarioSimMarket::updateAsd(const Date& d) {
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
            if (auto fb = boost::dynamic_pointer_cast<FallbackIborIndex>(index)) {
                // proxy fallback ibor index by its rfr index's fixing
                index = fb->rfrIndex();
            }
            asd_->set(index->fixing(index->fixingCalendar().adjust(d)), AggregationScenarioDataType::IndexFixing, i);
        }

        for (auto c : parameters_->additionalScenarioDataCcys()) {
            if (c != parameters_->baseCcy())
                asd_->set(fxSpot(c + parameters_->baseCcy())->value(), AggregationScenarioDataType::FXSpot, c);
        }

        for (Size i = 0; i < parameters_->additionalScenarioDataNumberOfCreditStates(); ++i) {
            RiskFactorKey key(RiskFactorKey::KeyType::CreditState, std::to_string(i));
            QL_REQUIRE(currentScenario_->has(key), "scenario does not have key " << key);
            asd_->set(currentScenario_->get(key), AggregationScenarioDataType::CreditState, std::to_string(i));
        }

        for (const auto& n : parameters_->additionalScenarioDataSurvivalWeights()) {
            RiskFactorKey key(RiskFactorKey::KeyType::SurvivalWeight, n);
            QL_REQUIRE(currentScenario_->has(key), "scenario does not have key " << key);
            asd_->set(currentScenario_->get(key), AggregationScenarioDataType::SurvivalWeight, n);
            RiskFactorKey rrKey(RiskFactorKey::KeyType::RecoveryRate, n);
            QL_REQUIRE(currentScenario_->has(rrKey), "scenario does not have key " << key);
            asd_->set(currentScenario_->get(rrKey), AggregationScenarioDataType::RecoveryRate, n);
        }

        asd_->set(numeraire_, AggregationScenarioDataType::Numeraire);

        asd_->next();
    }
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
    } else if (configuration != Market::defaultConfiguration) {
        // try to fall back on default configuration
        return getYieldCurve(yieldSpecId, todaysMarketParams, Market::defaultConfiguration);
    }

    // If yield spec ID still has not been found, return empty Handle
    return Handle<YieldTermStructure>();
}

} // namespace analytics
} // namespace ore
