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
#include <orea/scenario/deltascenario.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenarioutilities.hpp>
#include <orea/scenario/simplescenario.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/inflationcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>
#include <qle/indexes/inflationindexobserver.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/instruments/makeoiscapfloor.hpp>
#include <qle/quotes/derivedquote.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>
#include <qle/termstructures/blackvariancecurve3.hpp>
#include <qle/termstructures/blackvariancesurfacestddevs.hpp>
#include <qle/termstructures/blackvolconstantspread.hpp>
#include <qle/termstructures/commoditybasispricecurvewrapper.hpp>
#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <qle/termstructures/credit/spreadedbasecorrelationcurve.hpp>
#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
#include <qle/termstructures/dynamiccpivolatilitystructure.hpp>
#include <qle/termstructures/dynamicoptionletvolatilitystructure.hpp>
#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>
#include <qle/termstructures/dynamicyoyoptionletvolatilitystructure.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>
#include <qle/termstructures/interpolateddiscountcurve.hpp>
#include <qle/termstructures/interpolateddiscountcurve2.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>
#include <qle/termstructures/proxyswaptionvolatility.hpp>
#include <qle/termstructures/sabrstrippedoptionletadapter.hpp>
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
#include <qle/termstructures/strippedoptionlet.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/termstructures/strippedoptionletbasebumped.hpp>
#include <qle/termstructures/strippedyoyinflationoptionletvol.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/swaptionsabrcube.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/yoyinflationcurveobservermoving.hpp>
#include <qle/termstructures/zeroinflationcurveobservermoving.hpp>

#include <ql/instruments/makecapfloor.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/interpolations/forwardflatinterpolation.hpp>
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
#include <ql/quotes/compositequote.hpp>
#include <ql/quotes/derivedquote.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace std;

namespace {

// Utility function that is in catch blocks below
void processException(const std::exception& e, const std::string& curveId = "",
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
    std::string exceptionMessage = e.what();
    /* We do not log a structured curve error message, if the exception message indicates that the problem
       already occurred in the initial market. In this case we have already logged a structured error there. */
    if (boost::starts_with(exceptionMessage, "did not find object ")) {
        ALOG("CurveID: " << curve << ": " << message << ": " << exceptionMessage);
    } else {
        StructuredCurveErrorMessage(curve, message, exceptionMessage).log();
    }
}

template <typename TimeInterpolator>
bool createSabrAdapter(
    RelinkableHandle<OptionletVolatilityStructure> rhOvs,
    ext::shared_ptr<QuantLib::StrippedOptionletBase> optionlet,
    const vector<Handle<Quote>>& bumpQuotes,
    const vector<Time>& bumpTimes,
    ext::shared_ptr<OptionletVolatilityStructure> initMktOvs,
    const string& name,
    const ext::shared_ptr<IborIndex>& initMktIndex,
    const ext::shared_ptr<IborIndex>& ssmIndex,
    const Period& rateCompPeriod)
{
    QL_REQUIRE(initMktOvs, "createSabrAdapter: initial market optionlet is null in for name " << name);
    if (auto sabr = ext::dynamic_pointer_cast<SabrStrippedOptionletAdapter<TimeInterpolator>>(initMktOvs)) {

        auto baseVol = sabr->optionletBase();
        QL_REQUIRE(baseVol, "createSabrAdapter: base optionlet is null for name " << name);

        SabrStrippedOptionletAdapterBase::ModelParamData modelParameters;
        if (!optionlet) {
            // If optionlet is not provided, the intention is to take the initial market SABR's StrippedOptionletBase 
            // and use it to create a new StrippedOptionletBase with bumps. Its structure will match the initial market
            // SABR's StrippedOptionletBase, so we can use its initial model parameters directly.
            optionlet = ext::make_shared<StrippedOptionletBaseBumped>(baseVol, bumpQuotes, bumpTimes);
            modelParameters = sabr->initialModelParameters();
        } else {
            // If optionlet is provided, it can be a completely new set of optionlet quotes configured via SSM option 
            // tenors. We therefore need to populate the initial model parameters for the new optionlet's fixing times.
            // We map them to the last available data in the initial market SABR's initialModelParameters.
            const auto& baseFixingTimes = baseVol->optionletFixingTimes();
            const auto& fixingTimes = optionlet->optionletFixingTimes();
            const auto& initialModelParameters = sabr->initialModelParameters();
            auto nModelParams = initialModelParameters.size();
            if (nModelParams > 1) {
                for (const auto& fixingTime : fixingTimes) {
                    auto it = std::upper_bound(baseFixingTimes.begin(), baseFixingTimes.end(), fixingTime);
                    if (it != baseFixingTimes.begin())
                        --it;
                    Size idx = std::distance(baseFixingTimes.begin(), it);
                    QL_REQUIRE(idx < nModelParams, "createSabrAdapter: index, " << idx << ", into initial market"
                        " optionlet fixing times does not align with number of parameters " << nModelParams <<
                        " for name " << name);
                    modelParameters.push_back(initialModelParameters[idx]);
                }
            } else if (nModelParams == 1) {
                modelParameters.push_back(initialModelParameters[0]);
            }
        }

        // Create our SSM SABR surface.
        auto ssmSabr = ext::make_shared<SabrStrippedOptionletAdapter<TimeInterpolator>>(optionlet, sabr->modelVariant(),
            TimeInterpolator(), sabr->volatilityType(), sabr->displacement(), sabr->modelDisplacement(),
            modelParameters, sabr->maxCalibrationAttempts(), sabr->exitEarlyErrorThreshold(),
            sabr->maxAcceptableError(), initMktIndex, rateCompPeriod, sabr->residualCorrection(), ssmIndex);

        // Trigger calibration and then amend parameters for response to updates.
        using PVPC = QuantExt::ParametricVolatility::ParameterCalibration;
        SabrParametricVolatility::SliceParamInfo sspi {
            {Null<Real>(), PVPC::Implied}, // alpha implied.
            {Null<Real>(), PVPC::Fixed},   // beta fixed at its initially calibrated value on each slice.
            {Null<Real>(), PVPC::Fixed},   // nu fixed at its initially calibrated value on each slice.
            {Null<Real>(), PVPC::Fixed},   // rho fixed at its initially calibrated value on each slice.
        };
        ssmSabr->amendModelParameters(sspi);

        // Update the SSM optionlet volatility structure handle.
        rhOvs.linkTo(ssmSabr);

        return true;
    }
    return false;
}

template <class... TimeInterpolators>
bool tryCreateSabrAdapter(
    RelinkableHandle<OptionletVolatilityStructure> rhOvs,
    ext::shared_ptr<QuantLib::StrippedOptionletBase> optionlet,
    const vector<Handle<Quote>>& bumpQuotes,
    const vector<Time>& bumpTimes,
    ext::shared_ptr<OptionletVolatilityStructure> initMktOvs,
    const string& name,
    const ext::shared_ptr<IborIndex>& initMktIndex,
    const ext::shared_ptr<IborIndex>& ssmIndex,
    const Period& rateCompPeriod)
{
    return (createSabrAdapter<TimeInterpolators>(rhOvs, optionlet, bumpQuotes, bumpTimes, initMktOvs, name,
        initMktIndex, ssmIndex, rateCompPeriod) || ...);
}

// Helper function to sort and check uniqueness. Can be used below with strikes or expiries for example.
template <class T, class Equal = std::equal_to<T>>
void sortCheckUnique(vector<T>& values, const std::string& msgPrefix, const std::string& name, Equal eq = Equal()) {
    QL_REQUIRE(!values.empty(), msgPrefix << " for " << name << " should have at least one element.");
    std::sort(values.begin(), values.end());
    auto it = std::unique(values.begin(), values.end(), eq);
    QL_REQUIRE(it == values.end(), msgPrefix << " for " << name << " should be unique.");
}

//! Helper function to extract tenors from curve if no sim tenors are given
std::vector<QuantLib::Period>
simTenorsFromPriceCurve(const QuantLib::Handle<QuantExt::PriceTermStructure>& initialCurve,
                        const QuantLib::Date& asof) {
    std::vector<QuantLib::Period> simulationTenors;
    simulationTenors.reserve(initialCurve->pillarDates().size());
    for (const Date& d : initialCurve->pillarDates()) {
        QL_REQUIRE(d >= asof,
                   "Curve pillar date (" << io::iso_date(d) << ") must be after as of (" << io::iso_date(asof) << ").");
        simulationTenors.push_back(Period(d - asof, Days));
    }
    return simulationTenors;
}

QuantLib::ext::shared_ptr<QuantExt::PriceTermStructure> makeInterpolatedPriceCurve(
    const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Handle<QuantLib::Quote>>& quotes,
    const QuantLib::DayCounter& dayCounter, const QuantLib::Currency& currency, const std::string& interpolation) {
    if (interpolation == "Linear")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantExt::LinearFlat>>(tenors, quotes,
                                                                                                  dayCounter, currency);
    else if (interpolation == "Cubic")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantExt::CubicFlat>>(tenors, quotes,
                                                                                                 dayCounter, currency);
    else if (interpolation == "BackwardFlat")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantLib::BackwardFlat>>(
            tenors, quotes, dayCounter, currency);
    else if (interpolation == "ForwardFlat")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantLib::ForwardFlat>>(
            tenors, quotes, dayCounter, currency);
    else if (interpolation == "LinearFlat")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantExt::LinearFlat>>(tenors, quotes,
                                                                                                  dayCounter, currency);
    else if (interpolation == "CubicFlat")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantExt::CubicFlat>>(tenors, quotes,
                                                                                                 dayCounter, currency);
    else if (interpolation == "LogLinear")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantLib::LogLinear>>(tenors, quotes,
                                                                                                 dayCounter, currency);
    else if (interpolation == "LogLinearFlat")
        return QuantLib::ext::make_shared<QuantExt::InterpolatedPriceCurve<QuantExt::LogLinearFlat>>(
            tenors, quotes, dayCounter, currency);
    else
        QL_FAIL("makeInterpolatedPriceCurve: interpolation '" << interpolation << "' not recognised.");
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

QuantLib::ext::shared_ptr<YieldTermStructure>
makeYieldCurve(const std::string& curveId, const bool spreaded, const Handle<YieldTermStructure>& initMarketTs,
               const std::vector<Real>& yieldCurveTimes, const std::vector<Handle<Quote>>& quotes, const DayCounter& dc,
               const Calendar& cal, const std::string& interpolation, const std::string& extrapolation,
               const YieldCurveRollDown yieldCurveRollDown) {
    if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister && !spreaded) {
        return QuantLib::ext::shared_ptr<YieldTermStructure>(QuantLib::ext::make_shared<QuantExt::InterpolatedDiscountCurve>(
            yieldCurveTimes, quotes, 0, cal, dc,
            interpolation == "LogLinear" ? QuantExt::InterpolatedDiscountCurve::Interpolation::logLinear
                                         : QuantExt::InterpolatedDiscountCurve::Interpolation::linearZero,
            extrapolation == "FlatZero" ? QuantExt::InterpolatedDiscountCurve::Extrapolation::flatZero
                                        : QuantExt::InterpolatedDiscountCurve::Extrapolation::flatFwd));
    } else {
        if (spreaded) {
            checkDayCounterConsistency(curveId, initMarketTs->dayCounter(), dc);
            auto sdc = QuantLib::ext::make_shared<QuantExt::SpreadedDiscountCurve>(
                initMarketTs, yieldCurveTimes, quotes,
                interpolation == "LogLinear" ? QuantExt::SpreadedDiscountCurve::Interpolation::logLinear
                                             : QuantExt::SpreadedDiscountCurve::Interpolation::linearZero,
                extrapolation == "FlatZero" ? SpreadedDiscountCurve::Extrapolation::flatZero
                                            : SpreadedDiscountCurve::Extrapolation::flatFwd,
                yieldCurveRollDown);
            sdc->setAdjustReferenceDate(false);
            return sdc;
        } else {
            auto idc = QuantLib::ext::make_shared<QuantExt::InterpolatedDiscountCurve2>(
                yieldCurveTimes, quotes, dc,
                interpolation == "LogLinear" ? QuantExt::InterpolatedDiscountCurve2::Interpolation::logLinear
                                             : QuantExt::InterpolatedDiscountCurve2::Interpolation::linearZero,
                extrapolation == "FlatZero" ? InterpolatedDiscountCurve2::Extrapolation::flatZero
                                            : InterpolatedDiscountCurve2::Extrapolation::flatFwd);
            idc->setAdjustReferenceDate(false);
            return idc;
        }
    }
}

} // namespace

void ScenarioSimMarket::writeSimData(std::map<RiskFactorKey, QuantLib::ext::shared_ptr<SimpleQuote>>& simDataTmp,
                                     std::map<RiskFactorKey, Real>& absoluteSimDataTmp,
                                     const RiskFactorKey::KeyType keyType, const std::string& name,
                                     const std::vector<std::vector<Real>>& coordinates) {
    simData_.insert(simDataTmp.begin(), simDataTmp.end());
    absoluteSimData_.insert(absoluteSimDataTmp.begin(), absoluteSimDataTmp.end());
    coordinatesData_.insert(std::make_tuple(keyType, name, coordinates));
    simDataTmp.clear();
    absoluteSimDataTmp.clear();
}

void ScenarioSimMarket::addYieldCurve(const QuantLib::ext::shared_ptr<Market>& initMarket, const std::string& configuration,
                                      const RiskFactorKey::KeyType rf, const string& key, const vector<Period>& tenors,
                                      bool& simDataWritten, bool simulate, bool spreaded) {
    Handle<YieldTermStructure> wrapper = (riskFactorYieldCurve(rf) == ore::data::YieldCurveType::Discount)
                                             ? initMarket->discountCurve(key, configuration)
                                             : initMarket->yieldCurve(riskFactorYieldCurve(rf), key, configuration);
    QL_REQUIRE(!wrapper.empty(), "yield curve not provided for " << key);
    QL_REQUIRE(!tenors.empty(), "yield curve tenors must not be empty");
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
    QuantLib::ext::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
    quotes.push_back(Handle<Quote>(q));
    vector<Real> discounts(yieldCurveTimes.size());
    std::map<RiskFactorKey, QuantLib::ext::shared_ptr<SimpleQuote>> simDataTmp;
    std::map<RiskFactorKey, Real> absoluteSimDataTmp;
    for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
        Real val = wrapper->discount(yieldCurveDates[i + 1]);
        DLOG("ScenarioSimMarket yield curve " << rf << " " << key << " discount[" << i << "]=" << val);
        QuantLib::ext::shared_ptr<SimpleQuote> q(new SimpleQuote(spreaded ? 1.0 : val));
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

    writeSimData(simDataTmp, absoluteSimDataTmp, rf, key,
                 {std::vector<Real>(std::next(yieldCurveTimes.begin(), 1), yieldCurveTimes.end())});
    simDataWritten = true;

    QuantLib::ext::shared_ptr<YieldTermStructure> yieldCurve =
        makeYieldCurve(key, spreaded, wrapper, yieldCurveTimes, quotes, dc, TARGET(), parameters_->interpolation(),
                       parameters_->extrapolation(), parseYieldCurveRollDown(parameters_->yieldCurveRollDown()));

    Handle<YieldTermStructure> ych(yieldCurve);
    if (wrapper->allowsExtrapolation())
        ych->enableExtrapolation();
    yieldCurves_.insert(make_pair(make_tuple(Market::defaultConfiguration, riskFactorYieldCurve(rf), key), ych));
}

ScenarioSimMarket::ScenarioSimMarket(
    const QuantLib::ext::shared_ptr<Market>& initMarket,
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& parameters, const std::string& configuration,
    const ore::data::CurveConfigurations& curveConfigs, const ore::data::TodaysMarketParameters& todaysMarketParams,
    const bool continueOnError, const bool useSpreadedTermStructures, const bool cacheSimData,
    const bool allowPartialScenarios, const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
    const bool handlePseudoCurrencies, const QuantLib::ext::shared_ptr<Scenario>& offSetScenario)
    : SimMarket(handlePseudoCurrencies), parameters_(parameters), filter_(QuantLib::ext::make_shared<ScenarioFilter>()),
      useSpreadedTermStructures_(useSpreadedTermStructures), cacheSimData_(cacheSimData),
      allowPartialScenarios_(allowPartialScenarios), iborFallbackConfig_(iborFallbackConfig),
      offsetScenario_(offSetScenario) {

    LOG("building ScenarioSimMarket...");
    asof_ = initMarket->asofDate();
    DLOG("AsOf " << QuantLib::io::iso_date(asof_));

    // Create the build context in case we want to move logic out of the case statements e.g. createBondFutureVol.
    BuildContext bc {
        initMarket,
        configuration,
        curveConfigs,
        todaysMarketParams,
        continueOnError
    };

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

    bool gotException = false;
    for (const auto& param : parameters->parameters()) {
        try {
            // we populate the temp containers for each curve and write the result to the global
            // containers only if the set of data points is complete for this curve
            std::map<RiskFactorKey, QuantLib::ext::shared_ptr<SimpleQuote>> simDataTmp;
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
                        Real v = initMarket->fxSpot(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : v);
                        if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x * v; };
                            fxQuotes[name] = Handle<Quote>(
                                QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(Handle<Quote>(q), m));
                        } else {
                            fxQuotes[name] = Handle<Quote>(q);
                        }
                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(q));
                            if(useSpreadedTermStructures_) {
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name),
                                                           std::forward_as_tuple(v));
                            }
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {});
                        simDataWritten = true;
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                fx_ = QuantLib::ext::make_shared<FXTriangulation>(fxQuotes);
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
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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
                        isOn = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(*initMarket->iborIndex(i, configuration)) !=
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
                        QL_REQUIRE(!parameters->yieldCurveTenors(name).empty(),
                                   "yield curve tenors must not be empty");
                        QL_REQUIRE(parameters->yieldCurveTenors(name).front() > 0 * Days,
                                   "yield curve tenors must not include t=0");
                        for (auto& tenor : parameters->yieldCurveTenors(name)) {
                            yieldCurveTimes.push_back(dc.yearFraction(asof_, asof_ + tenor));
                            yieldCurveDates.push_back(asof_ + tenor);
                        }

                        // include today
                        vector<Handle<Quote>> quotes;
                        QuantLib::ext::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
                        quotes.push_back(Handle<Quote>(q));

                        for (Size i = 0; i < yieldCurveTimes.size() - 1; i++) {
                            Real val = wrapperIndex->discount(yieldCurveDates[i + 1]);
                            QuantLib::ext::shared_ptr<SimpleQuote> q(new SimpleQuote(useSpreadedTermStructures_ ? 1.0 : val));
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

                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name,
                                     {std::vector<Real>(std::next(yieldCurveTimes.begin(), 1), yieldCurveTimes.end())});
                        simDataWritten = true;

                        QuantLib::ext::shared_ptr<YieldTermStructure> indexCurve = makeYieldCurve(
                            name, useSpreadedTermStructures_, wrapperIndex, yieldCurveTimes, quotes, dc,
                            index->fixingCalendar(), parameters_->interpolation(), parameters_->extrapolation(),
                            parseYieldCurveRollDown(parameters_->yieldCurveRollDown()));

                        Handle<YieldTermStructure> ich(indexCurve);
                        if (wrapperIndex->allowsExtrapolation())
                            ich->enableExtrapolation();

                        // unpack original index, if i is a fallback index itself
                        if (auto f = QuantLib::ext::dynamic_pointer_cast<FallbackOvernightIndex>(*index))
                            index = Handle<IborIndex>(f->originalIndex());
                        else if (auto f = QuantLib::ext::dynamic_pointer_cast<FallbackIborIndex>(*index))
                            index = Handle<IborIndex>(f->originalIndex());

                        QuantLib::ext::shared_ptr<IborIndex> i = index->clone(ich);

                        if (iborFallbackConfig_ && iborFallbackConfig_->isIndexReplaced(name, asof_)) {
                            // handle ibor fallback indices
                            auto fallbackData = iborFallbackConfig_->fallbackData(name);
                            auto f = iborIndices_.find(make_pair(Market::defaultConfiguration, fallbackData.rfrIndex));
                            QL_REQUIRE(f != iborIndices_.end(),
                                       "Could not build ibor fallback index '"
                                           << name << "', because rfr index '" << fallbackData.rfrIndex
                                           << "' is not present in scenario sim market, is the rfr index in the "
                                              "scenario sim market parameters?");
                            auto rfrInd = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(*f->second);
                            QL_REQUIRE(rfrInd != nullptr,
                                       "Could not cast '"
                                           << fallbackData.rfrIndex
                                           << "' to overnight index when building the ibor fallback index '" << name
                                           << "'");
                            if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(i))
                                i = QuantLib::ext::make_shared<QuantExt::FallbackOvernightIndex>(
                                    on, rfrInd, fallbackData.spread, fallbackData.switchDate,
                                    iborFallbackConfig_->useRfrCurveInSimulationMarket());
                            else
                                i = QuantLib::ext::make_shared<QuantExt::FallbackIborIndex>(
                                                i, rfrInd, fallbackData.spread, fallbackData.switchDate,
                                                iborFallbackConfig_->useRfrCurveInSimulationMarket());
                            DLOG("built ibor fall back index '"
                                 << name << "' with rfr index '" << fallbackData.rfrIndex << "', spread "
                                 << fallbackData.spread << ", use rfr curve in scen sim market: " << std::boolalpha << iborFallbackConfig_->useRfrCurveInSimulationMarket());
                        }
                        iborIndices_.insert(
                            make_pair(make_pair(Market::defaultConfiguration, name), Handle<IborIndex>(i)));
                        DLOG("building " << name << " index curve done");
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : spotVal);
                        if(useSpreadedTermStructures_) {
                            auto m = [spotVal](Real x) { return x * spotVal; };
                            equitySpots_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name),
                                          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                              Handle<Quote>(q), m))));
                        } else {
                            equitySpots_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }
                        simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                           std::forward_as_tuple(q));
                        if(useSpreadedTermStructures_) {
                            absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name),
                                                       std::forward_as_tuple(spotVal));
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {});
                        simDataWritten = true;
                        DLOG("adding " << name << " equity spot done");
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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
                            auto eqConfig = curveConfigs.equityCurveConfig(name);
                            string forecastName = eqConfig->forecastingCurve();
                            string eqCcy = eqConfig->currency();
                            // Build a YieldCurveSpec and extract the yieldCurveSpec name
                            YieldCurveSpec ycspec(eqCcy, forecastName);
                            forecastCurve = ycspec.name();
                            TLOG("Got forecast curve '" << forecastCurve << "' from equity curve config for " << name);
                        }

                        // Get the nominal term structure from this scenario simulation market
                        Handle<YieldTermStructure> forecastTs =
                            getYieldCurve(forecastCurve, todaysMarketParams, Market::defaultConfiguration);
                        Handle<EquityIndex2> curve = initMarket->equityCurve(name, configuration);

                        // If forecast term structure is empty, fall back on this scenario simulation market's discount
                        // curve
                        if (forecastTs.empty()) {
                            string ccy = curve->currency().code();
                            TLOG("Falling back on the discount curve for currency '"
                                 << ccy << "' for equity forecast curve '" << name << "'");
                            forecastTs = discountCurve(ccy);
                        }
                        QuantLib::ext::shared_ptr<EquityIndex2> ei(
                            curve->clone(equitySpot(name, configuration), forecastTs,
                                         yieldCurve(YieldCurveType::EquityDividend, name, configuration), curve->announcedDividendCurve()));
                        Handle<EquityIndex2> eh(ei);
                        equityCurves_.insert(make_pair(make_pair(Market::defaultConfiguration, name), eh));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::SecuritySpread:
                for (const auto& name : param.second.second) {
                    // security spreads and recovery rates are optional
                    try {
                        DLOG("Adding security spread " << name << " from configuration " << configuration);
                        Real v = initMarket->securitySpread(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : v);
                        if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x + v; };
                            securitySpreads_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name),
                                          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                              Handle<Quote>(q), m))));
                        } else {
                            securitySpreads_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(q));
                            if(useSpreadedTermStructures_) {
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name),
                                                           std::forward_as_tuple(v));
                            }
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {});

                    } catch (const std::exception& e) {
                        DLOG("skipping this object: " << e.what());
                    }

                    try {
                        DLOG("Adding security recovery rate " << name << " from configuration " << configuration);
                        Real v = initMarket->recoveryRate(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : v);
                        if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x * v; };
                            recoveryRates_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name),
                                          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                              Handle<Quote>(q), m))));
                        } else {
                            recoveryRates_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }

                        // TODO this comes from the default curves section in the parameters,
                        // do we want to specify the simulation of security recovery rates separately?
                        if (parameters->simulateRecoveryRates()) {
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                                               std::forward_as_tuple(q));
                            if (useSpreadedTermStructures_) {
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name),
                                                           std::forward_as_tuple(v));
                            }
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, RiskFactorKey::KeyType::RecoveryRate, name, {});
                    } catch (const std::exception& e) {
                        DLOG("skipping this object: " << e.what());
                    }

                    try {
                        DLOG("Adding security conversion factor " << name << " from configuration " << configuration);
                        Real v = initMarket->conversionFactor(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : v);
                        if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x * v; };
                            conversionFactors_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name),
                                          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                              Handle<Quote>(q), m))));
                        } else {
                            conversionFactors_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }

                        // Add the future price also here.
                        StructuredSecurityId ssid{ name };
                        string futureContract = ssid.futureContract();
                        auto futurePriceKey = std::pair{ Market::defaultConfiguration, futureContract };
                        if (!securityPrices_.contains(futurePriceKey)) {
                            Real futurePx = initMarket->securityPrice(futureContract, configuration)->value();
                            auto futureQt = ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : futurePx);
                            if (useSpreadedTermStructures_) {
                                auto m = [futurePx](Real x) { return x * futurePx; };
                                auto derQt = ext::make_shared<DerivedQuote<decltype(m)>>(Handle<Quote>(futureQt), m);
                                securityPrices_[futurePriceKey] = Handle<Quote>(derQt);
                            } else {
                                securityPrices_[futurePriceKey] = Handle<Quote>(futureQt);
                            }
                        }

                    } catch (const std::exception& e) {
                        DLOG("skipping this object: " << e.what());
                    }

                    try {
                        DLOG("Adding security price " << name << " from configuration " << configuration);
                        Real v = initMarket->securityPrice(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : v);
                        if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x * v; };
                            securityPrices_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name),
                                          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                              Handle<Quote>(q), m))));
                        } else {
                            securityPrices_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }
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
                        string shortSwapIndexBase, swapIndexBase, smileDynamics, decayMode;
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
                            smileDynamics = parameters->swapVolSmileDynamics(name);
                            decayMode = parameters->swapVolDecayMode();
                        } else {
                            DLOG("building " << name << " yield volatility curve...");
                            wrapper.linkTo(*initMarket->yieldVol(name, configuration));
                            isCube = false;
                            optionTenors = parameters->yieldVolExpiries();
                            underlyingTenors = parameters->yieldVolTerms();
                            strikeSpreads = {0.0};
                            simulateAtmOnly = true;
                            smileDynamics = parameters->yieldVolSmileDynamics(name);
                            decayMode = parameters->yieldVolDecayMode();
                        }
                        DLOG("Initial market " << name << " yield volatility type = " << wrapper->volatilityType());

                        bool stickySabr = parseStickyness(smileDynamics) == Stickyness::StickySABR;
                        auto proxy = stickySabr || !useSpreadedTermStructures_ ?
                            QuantLib::ext::dynamic_pointer_cast<ProxySwaptionVolatility>(*wrapper) : nullptr;
                        if (proxy) {
                            DLOG("Detected ProxySwaptionVolatility for " << name);
                            wrapper.linkTo(*proxy->baseVol());
                        }

                        // Check if underlying market surface is atm or smile
                        isAtm = QuantLib::ext::dynamic_pointer_cast<SwaptionVolatilityMatrix>(*wrapper) != nullptr ||
                                QuantLib::ext::dynamic_pointer_cast<ConstantSwaptionVolatility>(*wrapper) != nullptr;

                        DLOG("YieldVol T0  source is atm     : " << (isAtm ? "True" : "False"));
                        DLOG("YieldVol ssm target is cube    : " << (isCube ? "True" : "False"));

                        Handle<SwaptionVolatilityStructure> svp;
                        if (param.second.first) {
                            DLOG("Simulating yield vols for ccy " << name);
                            DLOG("YieldVol simulate atm only     : " << (simulateAtmOnly ? "True" : "False"));
                            bool stickyStrike = parseStickyness(smileDynamics) == Stickyness::StickyStrike;

                            if (simulateAtmOnly) {
                                QL_REQUIRE(strikeSpreads.size() == 1 && close_enough(strikeSpreads[0], 0),
                                           "for atmOnly strikeSpreads must be {0.0}");
                            }
                            QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityCube> cube;
                            if ((isCube && !isAtm) || stickySabr) {
                                QuantLib::ext::shared_ptr<SwaptionVolCubeWithATM> tmp =
                                    QuantLib::ext::dynamic_pointer_cast<SwaptionVolCubeWithATM>(*wrapper);
                                QL_REQUIRE(tmp, "swaption cube missing");
                                cube = tmp->cube();
                            }
                            // For stickySabr only - we don't simulate vol at these strikeSpreads,
                            // but we will need the T0 volSpreads when constructing SABR cube
                            vector<Real> strikeSpreadsSabr;
                            vector<vector<Handle<Quote>>> volSpreadsSabr;
                            auto sabrCube = QuantLib::ext::dynamic_pointer_cast<SwaptionSabrCube>(cube);
                            if (stickySabr) {
                                QL_REQUIRE(sabrCube, "StickySABR simulation requires a SABR cube in the initial market");
                                QL_REQUIRE(simulateAtmOnly, "StickySABR simulation requires simulateAtmOnly=true");

                                strikeSpreadsSabr = sabrCube->strikeSpreads();
                                volSpreadsSabr.resize(optionTenors.size() * underlyingTenors.size(),
                                                      vector<Handle<Quote>>(strikeSpreadsSabr.size(), Handle<Quote>()));
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

                            QuantLib::ext::shared_ptr<SwapIndex> swapIndex, shortSwapIndex;
                            if (convertToNormal) {
                                swapIndex = *initMarket->swapIndex(swapIndexBase, configuration);
                                shortSwapIndex = *initMarket->swapIndex(shortSwapIndexBase, configuration);
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
                                            vol = QuantExt::convertSwaptionVolatility(
                                                asof_, optionTenors[i], underlyingTenors[j], swapIndex, shortSwapIndex,
                                                wrapper->dayCounter(), strikeSpreads[k],
                                                wrapper->volatility(optionTenors[i], underlyingTenors[j], strike, true),
                                                wrapper->volatilityType(),
                                                wrapper->shift(optionTenors[i], underlyingTenors[j]),
                                                QuantLib::VolatilityType::Normal, 0.0);
                                        } else {
                                            vol =
                                                wrapper->volatility(optionTenors[i], underlyingTenors[j], strike, true);
                                        }
                                        QuantLib::ext::shared_ptr<SimpleQuote> q(
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
                            for (Size k = 0; k < strikeSpreadsSabr.size(); ++k) {
                                for (Size i = 0, idx = 0; i < optionTenors.size(); ++i) {
                                    for (Size j = 0; j < underlyingTenors.size(); ++j, ++idx) {
                                        Real strike = cube->atmStrike(optionTenors[i], underlyingTenors[j]) +
                                                      strikeSpreadsSabr[k];
                                        Real vol;
                                        if (convertToNormal) {
                                            vol = QuantExt::convertSwaptionVolatility(
                                                asof_, optionTenors[i], underlyingTenors[j], swapIndex, shortSwapIndex,
                                                wrapper->dayCounter(), strikeSpreadsSabr[k],
                                                wrapper->volatility(optionTenors[i], underlyingTenors[j], strike, true),
                                                wrapper->volatilityType(),
                                                wrapper->shift(optionTenors[i], underlyingTenors[j]),
                                                QuantLib::VolatilityType::Normal, 0.0);
                                        } else {
                                            vol =
                                                wrapper->volatility(optionTenors[i], underlyingTenors[j], strike, true);
                                        }
                                        Real atmVol = useSpreadedTermStructures_ ?
                                            absoluteSimDataTmp.at(RiskFactorKey(param.first, name, idx)) :
                                            atmQuotes.at(i).at(j)->value();
                                        Real volSpread = vol - atmVol;
                                        auto q = QuantLib::ext::make_shared<SimpleQuote>(volSpread);
                                        volSpreadsSabr[idx][k] = Handle<Quote>(q);
                                        DLOG("VolSpread at " << optionTenors.at(i) << "/" << underlyingTenors.at(j)
                                                             << "/" << strikeSpreadsSabr.at(k) << " is " << volSpread
                                                             << ", name = " << name << ")");
                                    }
                                }
                            }

                            std::vector<std::vector<Real>> coordinates(3);
                            for (Size i = 0; i < optionTenors.size(); ++i) {
                                coordinates[0].push_back(
                                    wrapper->timeFromReference(wrapper->optionDateFromTenor(optionTenors[i])));
                            }
                            for (Size j = 0; j < underlyingTenors.size(); ++j) {
                                coordinates[1].push_back(wrapper->swapLength(underlyingTenors[j]));
                            }
                            for (Size k = 0; k < strikeSpreads.size(); ++k) {
                                coordinates[2].push_back(strikeSpreads[k]);
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, coordinates);
                            simDataWritten = true;
                            bool flatExtrapolation = true; // FIXME: get this from curve configuration
                            VolatilityType volType = convertToNormal ? Normal : wrapper->volatilityType();
                            DayCounter dc = wrapper->dayCounter();
                
                            if (useSpreadedTermStructures_) {
                                QuantLib::ext::shared_ptr<SwapIndex> swapIndex, shortSwapIndex;
                                QuantLib::ext::shared_ptr<SwapIndex> simSwapIndex, simShortSwapIndex;
                                if (!swapIndexBase.empty()) {
                                    try {
                                        addSwapIndexToSsm(swapIndexBase);
                                        simSwapIndex = *this->swapIndex(swapIndexBase, configuration);
                                    } catch (const std::exception& e) {
                                        processException(e, name, param.first, simDataWritten);
                                        gotException = true;
                                    }
                                }
                                if (!shortSwapIndexBase.empty()) {
                                    try {
                                        addSwapIndexToSsm(shortSwapIndexBase);
                                        simShortSwapIndex = *this->swapIndex(shortSwapIndexBase, configuration);
                                    } catch (const std::exception& e) {
                                        processException(e, name, param.first, simDataWritten);
                                        gotException = true;
                                    }
                                }
                                if (!swapIndexBase.empty())
                                    swapIndex = *initMarket->swapIndex(swapIndexBase, configuration);
                                if(!shortSwapIndexBase.empty())
                                    shortSwapIndex = *initMarket->swapIndex(shortSwapIndexBase, configuration);
                                if (stickySabr) {
                                    DLOG("Linking to SABR cube atm vol surface for sim market");
                                    wrapper.linkTo(*sabrCube->atmVol());
                                }
                                svp = Handle<SwaptionVolatilityStructure>(
                                    QuantLib::ext::make_shared<SpreadedSwaptionVolatility>(
                                        wrapper, optionTenors, underlyingTenors, strikeSpreads, quotes, swapIndex,
                                        shortSwapIndex, simSwapIndex, simShortSwapIndex, !stickyStrike, parseDecayMode(decayMode),
                                        parseYieldCurveRollDown(parameters_->yieldCurveRollDown())));
                                svp->setAdjustReferenceDate(false);
                            } else {
                                Handle<SwaptionVolatilityStructure> atm;
                                atm = Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<SwaptionVolatilityMatrix>(
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
                                            QuantLib::ext::make_shared<SwaptionVolatilityConstantSpread>(atm, wrapper));
                                    }
                                } else {
                                    if (isCube) {
                                        QuantLib::ext::shared_ptr<SwaptionVolatilityCube> tmp;
                                        tmp = QuantLib::ext::make_shared<SwaptionVolCube2>(
                                            atm, optionTenors, underlyingTenors, strikeSpreads, quotes,
                                            *initMarket->swapIndex(swapIndexBase, configuration),
                                            *initMarket->swapIndex(shortSwapIndexBase, configuration), false,
                                            flatExtrapolation, false);
                                        tmp->setAdjustReferenceDate(false);
                                        svp = Handle<SwaptionVolatilityStructure>(
                                            QuantLib::ext::make_shared<SwaptionVolCubeWithATM>(tmp, !stickySabr));
                                    } else {
                                        svp = atm;
                                    }
                                }
                            }
                            if (stickySabr) {
                                DLOG("Rebuilding SABR cube for sim market with simulated ATM vols for " << name);

                                std::map<std::pair<Period, Period>, 
                                            std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>
                                    modelParameters;
                                auto initialModelParameters = sabrCube->initialModelParameters();

                                // If optionTenors/underlyingTenors in sim market are not in the initialModelParameters,
                                // we need to map them to the last available tenor in initialModelParameters
                                if (!initialModelParameters.empty()) {
                                    std::set<Period> modelOptionTenorsSet;
                                    std::set<Period> modelSwapTenorsSet;
                                    for (const auto& [key, value] : initialModelParameters) {
                                        modelOptionTenorsSet.insert(key.first);
                                        modelSwapTenorsSet.insert(key.second);
                                    }
                                    for (auto optionTenor : optionTenors) {
                                        auto i0 = modelOptionTenorsSet.upper_bound(optionTenor);
                                        if (i0 != modelOptionTenorsSet.begin())
                                            --i0;
                                        for (auto underlyingTenor : underlyingTenors) {
                                            if (auto m = initialModelParameters.find(std::make_pair(optionTenor, underlyingTenor));
                                                m != initialModelParameters.end()) {
                                                modelParameters[std::make_pair(optionTenor, underlyingTenor)] = m->second;
                                            } else {
                                                auto j0 = modelSwapTenorsSet.upper_bound(underlyingTenor);
                                                if (j0 != modelSwapTenorsSet.begin())
                                                    --j0;
                                                modelParameters[std::make_pair(optionTenor, underlyingTenor)] =
                                                    initialModelParameters.at(std::make_pair(*i0, *j0));
                                            }
                                        }
                                    }
                                }
                                QuantLib::ext::shared_ptr<SwaptionVolatilityCube> tmp;
                                tmp = QuantLib::ext::make_shared<SwaptionSabrCube>(
                                    svp, optionTenors, underlyingTenors,
                                    optionTenors, underlyingTenors,
                                    strikeSpreadsSabr,
                                    volSpreadsSabr,
                                    *initMarket->swapIndex(swapIndexBase, configuration),
                                    *initMarket->swapIndex(shortSwapIndexBase, configuration),
                                    sabrCube->modelVariant(), sabrCube->volatilityType(),
                                    modelParameters,
                                    sabrCube->modelShift(), sabrCube->outputShift(),
                                    sabrCube->maxCalibrationAttempts(),
                                    sabrCube->exitEarlyErrorThreshold(),
                                    sabrCube->maxAcceptableError(), stickySabr);
                                tmp->setAdjustReferenceDate(false);
                                svp = Handle<SwaptionVolatilityStructure>(
                                    QuantLib::ext::make_shared<SwaptionVolCubeWithATM>(tmp, true));
                            }
                            if (proxy) {
                                DLOG("Wrapping simulated vol structure with ProxySwaptionVolatility for " << name);
                                svp->setAdjustReferenceDate(false);
                                svp->enableExtrapolation(); // FIXME
                                svp = Handle<SwaptionVolatilityStructure>(
                                    QuantLib::ext::make_shared<ProxySwaptionVolatility>(svp,
                                                                                        proxy->baseSwapIndexBase(),
                                                                                        proxy->baseShortSwapIndexBase(),
                                                                                        *initMarket->swapIndex(swapIndexBase, configuration),
                                                                                        *initMarket->swapIndex(shortSwapIndexBase, configuration)));
                            }
                        } else {
                            DLOG("Dynamic (" << wrapper->volatilityType() << ") yield vols (" << decayMode
                                             << ") for qualifier " << name);

                            QL_REQUIRE(!QuantLib::ext::dynamic_pointer_cast<ProxySwaptionVolatility>(*wrapper),
                                "DynamicSwaptionVolatilityMatrix does not support ProxySwaptionVolatility surface");

                            QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atmSlice;
                            if (isAtm)
                                atmSlice = *wrapper;
                            else {
                                auto c = QuantLib::ext::dynamic_pointer_cast<SwaptionVolCubeWithATM>(*wrapper);
                                QL_REQUIRE(c, "internal error - expected swaption cube to be SwaptionVolCubeWithATM.");
                                atmSlice = *c->cube()->atmVol();
                            }

                            if (isCube)
                                WLOG("Only ATM slice is considered from init market's cube");
                            QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svolp =
                                QuantLib::ext::make_shared<QuantExt::DynamicSwaptionVolatilityMatrix>(
                                    atmSlice, 0, NullCalendar(), parseDecayMode(decayMode));
                            svp = Handle<SwaptionVolatilityStructure>(svolp);
                        }
                        svp->setAdjustReferenceDate(false);
                        svp->enableExtrapolation(); // FIXME

                        DLOG("Simulation market " << name << " yield volatility type = " << svp->volatilityType());

                        if (param.first == RiskFactorKey::KeyType::SwaptionVolatility) {
                            swaptionCurves_.insert(make_pair(make_pair(Market::defaultConfiguration, name), svp));
                            swaptionIndexBases_.insert(make_pair(make_pair(Market::defaultConfiguration, name),
                                                                 make_pair(shortSwapIndexBase, swapIndexBase)));
                            swaptionIndexBases_.insert(make_pair(make_pair(Market::defaultConfiguration, name),
                                                                 make_pair(swapIndexBase, swapIndexBase)));
                        } else {
                            yieldVolCurves_.insert(make_pair(make_pair(Market::defaultConfiguration, name), svp));
                        }
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::OptionletVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        createOptionletVol(param.first, name, param.second.first, simDataWritten, bc);
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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

                        QL_REQUIRE(!parameters->defaultTenors(name).empty(),
                                   "default curve tenors must not be empty");
                        QL_REQUIRE(parameters->defaultTenors(name).front() > 0 * Days,
                                   "default curve tenors must not include t=0");

                        vector<Date> dates(1, asof_);
                        vector<Real> times(1, 0.0);

                        DayCounter dc = wrapper->curve()->dayCounter();
            
                        for (Size i = 0; i < parameters->defaultTenors(name).size(); i++) {
                            dates.push_back(asof_ + parameters->defaultTenors(name)[i]);
                            times.push_back(dc.yearFraction(asof_, dates.back()));
                        }

                        QuantLib::ext::shared_ptr<SimpleQuote> q(new SimpleQuote(1.0));
                        quotes.push_back(Handle<Quote>(q));
                        for (Size i = 0; i < dates.size() - 1; i++) {
                            Probability prob = wrapper->curve()->survivalProbability(dates[i + 1], true);
                            QuantLib::ext::shared_ptr<SimpleQuote> q =
                                QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : prob);
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
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name,
                                     {std::vector<Real>(std::next(times.begin(), 1), times.end())});
                        simDataWritten = true;
                        Calendar cal = ore::data::parseCalendar(parameters->defaultCurveCalendar(name));
                        Handle<DefaultProbabilityTermStructure> defaultCurve;
                        if (useSpreadedTermStructures_) {
                            defaultCurve = Handle<DefaultProbabilityTermStructure>(
                                QuantLib::ext::make_shared<QuantExt::SpreadedSurvivalProbabilityTermStructure>(
                                    wrapper->curve(), times, quotes,
                                    parameters->defaultCurveExtrapolation() == "FlatZero"
                                        ? QuantExt::SpreadedSurvivalProbabilityTermStructure::Extrapolation::flatZero
                                        : QuantExt::SpreadedSurvivalProbabilityTermStructure::Extrapolation::flatFwd,
                                    parseYieldCurveRollDown(parameters_->yieldCurveRollDown())));
                            defaultCurve->setAdjustReferenceDate(false);
                        } else {
                            defaultCurve = Handle<DefaultProbabilityTermStructure>(
                                QuantLib::ext::make_shared<QuantExt::SurvivalProbabilityCurve<LogLinear>>(
                                    dates, quotes, dc, cal, std::vector<Handle<Quote>>(), std::vector<Date>(),
                                    LogLinear(),
                                    parameters->defaultCurveExtrapolation() == "FlatZero"
                                        ? QuantExt::SurvivalProbabilityCurve<LogLinear>::Extrapolation::flatZero
                                        : QuantExt::SurvivalProbabilityCurve<LogLinear>::Extrapolation::flatFwd));
                        }
                        defaultCurve->setAdjustReferenceDate(false);
                        defaultCurve->enableExtrapolation();
                        defaultCurves_.insert(make_pair(
                            make_pair(Market::defaultConfiguration, name),
                            Handle<CreditCurve>(QuantLib::ext::make_shared<CreditCurve>(
                                defaultCurve, wrapper->rateCurve(), wrapper->recovery(), wrapper->refData()))));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::RecoveryRate:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("Adding recovery rate " << name << " from configuration " << configuration);
                        Real v = initMarket->recoveryRate(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 1.0 : v);
                        if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x * v; };
                            recoveryRates_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name),
                                          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                              Handle<Quote>(q), m))));
                        } else {
                            recoveryRates_.insert(
                                make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }
                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                                               std::forward_as_tuple(q));
                            if(useSpreadedTermStructures_) {
                                absoluteSimDataTmp.emplace(
                                    std::piecewise_construct,
                                    std::forward_as_tuple(RiskFactorKey::KeyType::RecoveryRate, name),
                                    std::forward_as_tuple(v));
                            }
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {});
                        simDataWritten = true;
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::CDSVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building " << name << "  cds vols..");
                        Handle<QuantExt::CreditVolCurve> wrapper = initMarket->cdsVol(name, configuration);
                        Handle<QuantExt::CreditVolCurve> cvh;
                        bool stickyStrike =
                            parseStickyness(parameters_->cdsVolSmileDynamics(name)) == Stickyness::StickyStrike;
                        if (param.second.first) {
                            DLOG("Simulating CDS Vols for " << name);
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
                                QuantLib::ext::shared_ptr<SimpleQuote> q =
                                    QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
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
                            writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {times});
                            simDataWritten = true;
                            if (useSpreadedTermStructures_ ||
                                (!useSpreadedTermStructures_ && parameters->simulateCdsVolATMOnly())) {
                                std::vector<Handle<Quote>> spreads;
                                if (parameters_->simulateCdsVolATMOnly()) {
                                    for (size_t i = 0; i < quotes.size(); i++) {
                                        Handle<Quote> atmVol(QuantLib::ext::make_shared<SimpleQuote>(quotes[i]->value()));
                                        Handle<Quote> quote(QuantLib::ext::make_shared <
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
                                cvh = Handle<CreditVolCurve>(QuantLib::ext::make_shared<SpreadedCreditVolCurve>(
                                    wrapper, expiryDates, spreads, !stickyStrike, simTerms, simTermCurves,
                                    parseDecayMode(parameters->cdsVolDecayMode())));
                                cvh->setAdjustReferenceDate(false);
                            } else {
                                // TODO support strike and term dependence
                                cvh = Handle<CreditVolCurve>(QuantLib::ext::make_shared<CreditVolCurveWrapper>(
                                    Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<BlackVarianceCurve3>(
                                        0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes,
                                        false))));
                            }
                        } else {
                            string decayModeString = parameters->cdsVolDecayMode();
                            DLOG("Deterministic CDS Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // TODO support strike and term dependence, hardcoded term 5y
                            cvh = Handle<CreditVolCurve>(
                                QuantLib::ext::make_shared<CreditVolCurveWrapper>(Handle<BlackVolTermStructure>(
                                    QuantLib::ext::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                        Handle<BlackVolTermStructure>(
                                            QuantLib::ext::make_shared<BlackVolFromCreditVolWrapper>(wrapper, 5.0)),
                                        0, NullCalendar(), decayMode,
                                        stickyStrike ? StickyStrike : StickyMoneyness))));
                        }
                        cvh->setAdjustReferenceDate(false);
                        if (wrapper->allowsExtrapolation())
                            cvh->enableExtrapolation();
                        cdsVols_.insert(make_pair(make_pair(Market::defaultConfiguration, name), cvh));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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

                        bool stickyStrike =
                            parseStickyness(parameters_->fxVolSmileDynamics(name)) == Stickyness::StickyStrike;

                        if (param.second.first) {
                            DLOG("Simulating FX Vols for " << name);
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

                            QuantLib::ext::shared_ptr<BlackVolTermStructure> fxVolCurve;
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
                                            QuantLib::ext::shared_ptr<SimpleQuote> q(
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
                                    writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {strikes, times});
                                    simDataWritten = true;
                                    // build the surface
                                    if (useSpreadedTermStructures_) {
                                        fxVolCurve =
                                            QuantLib::ext::make_shared<SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                                Handle<BlackVolTermStructure>(wrapper), spot, times,
                                                parameters->fxVolMoneyness(name), quotes,
                                                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot->value())),
                                                initForTS, initDomTS, forTS, domTS, stickyStrike,
                                                parseDecayMode(parameters->fxVolDecayMode()));
                                        fxVolCurve->setAdjustReferenceDate(false);
                                    } else {
                                        fxVolCurve = QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessForward>(
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
                                                quotes[i][j] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
                                    } else {
                                        quotes = absQuotes;
                                    }

                                    // sort out simDataTemp
                                    for (Size i = 0; i < m; i++) {
                                        for (Size j = 0; j < n; j++) {
                                            Size idx = j * m + i;
                                            QuantLib::ext::shared_ptr<Quote> q = quotes[j][i].currentLink();
                                            QuantLib::ext::shared_ptr<SimpleQuote> sq =
                                                QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(q);
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
                                    writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {strikes, times});
                                    simDataWritten = true;

                                    // set up a FX Index
                                    Handle<FxIndex> fxInd = fxIndex(name);

                                    if (parameters->fxUseMoneyness(name)) { // moneyness
                                        if (useSpreadedTermStructures_) {
                                            fxVolCurve = QuantLib::ext::make_shared<
                                                SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                                Handle<BlackVolTermStructure>(wrapper), spot, times,
                                                parameters->fxVolMoneyness(name), quotes,
                                                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot->value())),
                                                initForTS, initDomTS, forTS, domTS, stickyStrike,
                                                parseDecayMode(parameters->fxVolDecayMode()));
                                            fxVolCurve->setAdjustReferenceDate(false);
                                        } else {
                                            fxVolCurve =
                                                QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessForward>(
                                                    cal, spot, times, parameters->fxVolStdDevs(name), quotes, dc,
                                                    fxInd->sourceCurve(), fxInd->targetCurve(), stickyStrike,
                                                    flatExtrapolation);
                                        }
                                    } else {                                // standard deviations
                                        if (useSpreadedTermStructures_) {
                                            fxVolCurve =
                                                QuantLib::ext::make_shared<SpreadedBlackVolatilitySurfaceStdDevs>(
                                                    Handle<BlackVolTermStructure>(wrapper), spot, times,
                                                    parameters->fxVolStdDevs(name), quotes,
                                                    Handle<Quote>(
                                                        QuantLib::ext::make_shared<SimpleQuote>(spot->value())),
                                                    initForTS, initDomTS, forTS, domTS, stickyStrike,
                                                    parseDecayMode(parameters->fxVolDecayMode()));
                                            fxVolCurve->setAdjustReferenceDate(false);
                                        } else {
                                            fxVolCurve = QuantLib::ext::make_shared<BlackVarianceSurfaceStdDevs>(
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
                                    QuantLib::ext::shared_ptr<SimpleQuote> q(
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

                                writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {times});
                                simDataWritten = true;

                                if (useSpreadedTermStructures_) {
                                    // if simulate atm only is false, we use the ATM slice from the wrapper only
                                    // the smile dynamics is sticky strike here always (if t0 is a surface)
                                    fxVolCurve = QuantLib::ext::make_shared<SpreadedBlackVolatilityCurve>(
                                        Handle<BlackVolTermStructure>(wrapper), times, quotes[0],
                                        !parameters->simulateFxVolATMOnly(),
                                        parseDecayMode(parameters->fxVolDecayMode()));
                                    fxVolCurve->setAdjustReferenceDate(false);
                                } else {
                                    DLOG("ATM FX Vols (BlackVarianceCurve3) for " << name);
                                    QuantLib::ext::shared_ptr<BlackVolTermStructure> atmCurve;
                                    atmCurve = QuantLib::ext::make_shared<BlackVarianceCurve3>(
                                        0, NullCalendar(), wrapper->businessDayConvention(), dc, times, quotes[0], false);
                                    // if we have a surface but are only simulating atm vols we wrap the atm curve and
                                    // the full t0 surface
                                    if (parameters->simulateFxVolATMOnly()) {
                                        DLOG("Simulating FX Vols (FXVolatilityConstantSpread) for " << name);
                                        fxVolCurve = QuantLib::ext::make_shared<BlackVolatilityConstantSpread>(
                                            Handle<BlackVolTermStructure>(atmCurve), wrapper);
                                    } else {
                                        fxVolCurve = atmCurve;
                                    }
                                }
                            }
                            fvh = Handle<BlackVolTermStructure>(fxVolCurve);

                        } else {
                            string decayModeString = parameters->fxVolDecayMode();
                            DLOG("Deterministic FX Vols with decay mode " << decayModeString << " for " << name);
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);

                            // currently only curves (i.e. strike independent) FX volatility structures are
                            // supported, so we use a) the more efficient curve tag and b) a hard coded sticky
                            // strike stickiness, since then no yield term structures and no fx spot are required
                            // that define the ATM level - to be revisited when FX surfaces are supported
                            fvh = Handle<BlackVolTermStructure>(
                                QuantLib::ext::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode,
                                    stickyStrike ? StickyStrike : StickyMoneyness));
                        }

                        fvh->setAdjustReferenceDate(false);
                        fvh->enableExtrapolation();
                        fxVols_.insert(make_pair(make_pair(Market::defaultConfiguration, name), fvh));

                        // build inverted surface
                        QL_REQUIRE(name.size() == 6, "Invalid Ccy pair " << name);
                        string reverse = name.substr(3) + name.substr(0, 3);
                        Handle<QuantLib::BlackVolTermStructure> ifvh(
                            QuantLib::ext::make_shared<BlackInvertedVolTermStructure>(fvh));
                        ifvh->enableExtrapolation();
                        fxVols_.insert(make_pair(make_pair(Market::defaultConfiguration, reverse), ifvh));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::EquityVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        Handle<BlackVolTermStructure> wrapper = initMarket->equityVol(name, configuration);
                        Handle<BlackVolTermStructure> evh;

                        bool stickyStrike =
                            parseStickyness(parameters_->equityVolSmileDynamics(name)) == Stickyness::StickyStrike;
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

                            QuantLib::ext::shared_ptr<BlackVolTermStructure> eqVolCurve;

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
                                            QuantLib::ext::shared_ptr<SimpleQuote> q(
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
                                    writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {strikes, times});
                                    simDataWritten = true;
                                    DLOG("Simulating EQ Vols (BlackVarianceSurfaceMoneyness) for " << name);
                                    
                                    if (useSpreadedTermStructures_) {
                                        eqVolCurve =
                                            QuantLib::ext::make_shared<SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                                Handle<BlackVolTermStructure>(wrapper), spot, times,
                                                parameters->equityVolMoneyness(name), quotes,
                                                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot->value())),
                                                initMarket->equityCurve(name, configuration)->equityDividendCurve(),
                                                initMarket->equityCurve(name, configuration)->equityForecastCurve(),
                                                eqCurve->equityDividendCurve(), eqCurve->equityForecastCurve(),
                                                stickyStrike, parseDecayMode(parameters->equityVolDecayMode()));
                                        eqVolCurve->setAdjustReferenceDate(false);
                                    } else {
                                        eqVolCurve = QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessForward>(
                                            cal, spot, times, parameters->equityVolMoneyness(name), quotes, dc,
                                            eqCurve->equityDividendCurve(), eqCurve->equityForecastCurve(),
                                            stickyStrike, true);
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
                                                quotes[i][j] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
                                    } else {
                                        quotes = absQuotes;
                                    }

                                    // add to simDataTemp
                                    for (Size i = 0; i < m; i++) {
                                        for (Size j = 0; j < n; j++) {
                                            Size idx = j * m + i;
                                            QuantLib::ext::shared_ptr<Quote> q = quotes[j][i].currentLink();
                                            QuantLib::ext::shared_ptr<SimpleQuote> sq =
                                                QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(q);
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
                                    writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {strikes, times});
                                    simDataWritten = true;
                                    bool flatExtrapolation = true; // flat extrapolation of strikes at far ends.
                                    if (useSpreadedTermStructures_) {
                                        eqVolCurve = QuantLib::ext::make_shared<SpreadedBlackVolatilitySurfaceStdDevs>(
                                            Handle<BlackVolTermStructure>(wrapper), spot, times,
                                            parameters->equityVolStandardDevs(name), quotes,
                                            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot->value())),
                                            initMarket->equityCurve(name, configuration)->equityDividendCurve(),
                                            initMarket->equityCurve(name, configuration)->equityForecastCurve(),
                                            eqCurve->equityDividendCurve(), eqCurve->equityForecastCurve(),
                                            stickyStrike, parseDecayMode(parameters->equityVolDecayMode()));
                                        eqVolCurve->setAdjustReferenceDate(false);
                                    } else {
                                        eqVolCurve = QuantLib::ext::make_shared<BlackVarianceSurfaceStdDevs>(
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
                                    QuantLib::ext::shared_ptr<SimpleQuote> q(
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

                                writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {times});
                                simDataWritten = true;

                                if (useSpreadedTermStructures_) {
                                    // if simulate atm only is false, we use the ATM slice from the wrapper only
                                    // the smile dynamics is sticky strike here always (if t0 is a surface)
                                    eqVolCurve = QuantLib::ext::make_shared<SpreadedBlackVolatilityCurve>(
                                        Handle<BlackVolTermStructure>(wrapper), times, quotes[0],
                                        !parameters->simulateEquityVolATMOnly(),
                                        parseDecayMode(parameters->equityVolDecayMode()));
                                } else {
                                    DLOG("ATM EQ Vols (BlackVarianceCurve3) for " << name);
                                    QuantLib::ext::shared_ptr<BlackVolTermStructure> atmCurve;
                                    atmCurve = QuantLib::ext::make_shared<BlackVarianceCurve3>(0, NullCalendar(),
                                                                                       wrapper->businessDayConvention(),
                                                                                       dc, times, quotes[0], false);
                                    // if we have a surface but are only simulating atm vols we wrap the atm curve and
                                    // the full t0 surface
                                    if (parameters->simulateEquityVolATMOnly()) {
                                        DLOG("Simulating EQ Vols (EquityVolatilityConstantSpread) for " << name);
                                        eqVolCurve = QuantLib::ext::make_shared<BlackVolatilityConstantSpread>(
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
                                QuantLib::ext::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    wrapper, 0, NullCalendar(), decayMode,
                                    stickyStrike ? StickyStrike : StickyMoneyness));
                        }

                        evh->setAdjustReferenceDate(false);
                        if (wrapper->allowsExtrapolation())
                            evh->enableExtrapolation();
                        equityVols_.insert(make_pair(make_pair(Market::defaultConfiguration, name), evh));
                        DLOG("EQ volatility curve built for " << name);
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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
                            baseCorrelations_.insert(make_pair(make_pair(Market::defaultConfiguration, name), wrapper));
                        else {
                            std::vector<Real> times;
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
                                    times.push_back(wrapper->timeFromReference(asof_ + term));
                                    Real bc = wrapper->correlation(asof_ + term, lossLevel, true); // extrapolate
                                    QuantLib::ext::shared_ptr<SimpleQuote> q =
                                        QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : bc);
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

                            writeSimData(
                                simDataTmp, absoluteSimDataTmp, param.first, name,
                                {parameters->baseCorrelationDetachmentPoints(), times});
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
                            
                            QuantLib::ext::shared_ptr<QuantExt::BaseCorrelationTermStructure> bcp;
                            if (useSpreadedTermStructures_) {
                                bcp = QuantLib::ext::make_shared<QuantExt::SpreadedBaseCorrelationCurve>(
                                    wrapper, terms, detachmentPoints, quotes);
                                bcp->enableExtrapolation(wrapper->allowsExtrapolation());
                            } else {
                                DayCounter dc = wrapper->dayCounter();
                                bcp = QuantLib::ext::make_shared<InterpolatedBaseCorrelationTermStructure<Bilinear>>(
                                    wrapper->settlementDays(), wrapper->calendar(), wrapper->businessDayConvention(),
                                    terms, detachmentPoints, quotes, dc);

                                bcp->enableExtrapolation(wrapper->allowsExtrapolation());
                            }
                            bcp->setAdjustReferenceDate(false);
                            Handle<QuantExt::BaseCorrelationTermStructure> bch(bcp);
                            baseCorrelations_.insert(make_pair(make_pair(Market::defaultConfiguration, name), bch));
                        }
                        DLOG("Base correlations built for " << name);
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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
                        auto zits = zeroInflationIndex->zeroInflationTermStructure();
                        int simLag = simulationLag(zits);
                        Date fixingDate = zits->baseDate();
                        Real baseCPI = zeroInflationIndex->fixing(fixingDate);

                        auto q = QuantLib::ext::make_shared<SimpleQuote>(baseCPI);
                        if(useSpreadedTermStructures_) {
                            auto m = [baseCPI](Real x) { return x * baseCPI; };
                            Handle<InflationIndexObserver> inflObserver(
                                QuantLib::ext::make_shared<InflationIndexObserver>(
                                    zeroInflationIndex,
                                    Handle<Quote>(
                                        QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(Handle<Quote>(q), m)),
                                    simLag));
                            baseCpis_.insert(make_pair(make_pair(Market::defaultConfiguration, name), inflObserver));
                        } else {
                            Handle<InflationIndexObserver> inflObserver(
                                QuantLib::ext::make_shared<InflationIndexObserver>(zeroInflationIndex, Handle<Quote>(q),
                                                                                   simLag));
                            baseCpis_.insert(make_pair(make_pair(Market::defaultConfiguration, name), inflObserver));
                        }
                        simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                           std::forward_as_tuple(q));
                        if(useSpreadedTermStructures_) {
                            absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(param.first, name),
                                                       std::forward_as_tuple(baseCPI));
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {});
                        simDataWritten = true;
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::ZeroInflationCurve:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        Handle<ZeroInflationIndex> inflationIndex = initMarket->zeroInflationIndex(name, configuration);
                        auto observationLegs = initMarket->zeroInflationObservationLags(name, configuration);
                        QL_REQUIRE(!observationLegs.empty(),
                                   "Zero inflation index " << name << " has no observation legs defined");
                        auto obsLag = observationLegs.rbegin()->second; // take the longest lag as the main lag for simulation,
                        
                        Handle<ZeroInflationTermStructure> inflationTs = inflationIndex->zeroInflationTermStructure();
                        vector<string> keys(parameters->zeroInflationTenors(name).size());

                        Date date0 = inflationTs->baseDate();
                        DayCounter dc = inflationTs->dayCounter();
                        vector<Time> zeroCurveTimes(
                            1, -dc.yearFraction(inflationPeriod(date0, inflationTs->frequency()).first, asof_));
                        vector<Handle<Quote>> quotes;
                        QL_REQUIRE(!parameters->zeroInflationTenors(name).empty(),
                                   "zero inflation tenors must not be empty");
                        QL_REQUIRE(parameters->zeroInflationTenors(name).front() > 0 * Days,
                                   "zero inflation tenors must not include t=0");
                        DLOG("ScenarioSimMarket building zero inflation curve for " << name << " with base date " << date0
                                                                           << " and obs lag " << obsLag);
                        for (auto& tenor : parameters->zeroInflationTenors(name)) {
                            Date inflDate = inflationPeriod(asof_ + tenor - obsLag, inflationTs->frequency()).first;
                            DLOG("ScenarioSimMarket zero inflation curve " << name << " inflation date: " << inflDate);
                            zeroCurveTimes.push_back(dc.yearFraction(asof_, inflDate));
                        }

                        for (Size i = 1; i < zeroCurveTimes.size(); i++) {
                            Real rate = inflationTs->zeroRate(zeroCurveTimes[i]);
                            auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : rate);
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

                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name,
                                     {std::vector<Real>(std::next(zeroCurveTimes.begin(), 1), zeroCurveTimes.end())});
                        simDataWritten = true;
                                                
                        // FIXME: Settlement days set to zero - needed for floating term structure implementation
                        QuantLib::ext::shared_ptr<ZeroInflationTermStructure> zeroCurve;
                        if (useSpreadedTermStructures_) {
                            zeroCurve = QuantLib::ext::make_shared<SpreadedZeroInflationCurve>(inflationTs,
                                                                                               zeroCurveTimes, quotes);
                            zeroCurve->setAdjustReferenceDate(false);
                        } else {
                            int simLag = simulationLag(inflationTs);
                            // Quotes are build with first time to be (baseDate), need to 0 Days tenors here
                            vector<Period> tenors(1, 0 * Days);
                            tenors.insert(tenors.end(), parameters->zeroInflationTenors(name).begin(),
                                          parameters->zeroInflationTenors(name).end());
                            zeroCurve = QuantLib::ext::make_shared<ZeroInflationCurveObserverMoving<Linear>>(
                                0, inflationIndex->fixingCalendar(), dc, simLag, obsLag,
                                inflationTs->frequency(), false, tenors, quotes, inflationTs->seasonality());
                        }

                        Handle<ZeroInflationTermStructure> its(zeroCurve);
                        its->setAdjustReferenceDate(false);
                        its->enableExtrapolation();
                        QuantLib::ext::shared_ptr<ZeroInflationIndex> i =
                            parseZeroInflationIndex(name, Handle<ZeroInflationTermStructure>(its));
                        Handle<ZeroInflationIndex> zh(i);
                        zeroInflationIndices_.insert(make_pair(make_pair(Market::defaultConfiguration, name), zh));
                        zeroInflationObservationLags_.insert(
                            make_pair(make_pair(Market::defaultConfiguration, name), observationLegs));
                        DLOG("building " << name << " zero inflation curve done");
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building " << name << " zero inflation cap/floor volatility curve...");
                        Handle<QuantLib::CPIVolatilitySurface> wrapper =
                            initMarket->cpiInflationCapFloorVolatilitySurface(name, configuration);
                        Handle<ZeroInflationIndex> zeroInflationIndex =
                            initMarket->zeroInflationIndex(name, configuration);
                        // LOG("Initial market zero inflation cap/floor volatility type = " <<
                        // wrapper->volatilityType());

                        Handle<QuantLib::CPIVolatilitySurface> hCpiVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            DLOG("Simulating zero inflation cap/floor vols for index name " << name);

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
                                    auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
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

                            std::vector<std::vector<Real>> coordinates(2);
                            for (Size i = 0; i < optionTenors.size(); ++i) {
                                coordinates[0].push_back(
                                    wrapper->timeFromReference(wrapper->optionDateFromTenor(optionTenors[i])));
                            }
                            for (Size j = 0; j < strikes.size(); ++j) {
                                coordinates[1].push_back(strikes[j]);
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, coordinates);
                            simDataWritten = true;



                            if (useSpreadedTermStructures_) {
                                auto surface = QuantLib::ext::dynamic_pointer_cast<QuantExt::CPIVolatilitySurface>(wrapper.currentLink());
                                QL_REQUIRE(surface,
                                           "Internal error, todays market should build QuantExt::CPIVolatiltiySurface "
                                           "instead of QuantLib::CPIVolatilitySurface");
                                hCpiVol = Handle<QuantLib::CPIVolatilitySurface>(
                                    QuantLib::ext::make_shared<SpreadedCPIVolatilitySurface>(
                                        Handle<QuantExt::CPIVolatilitySurface>(surface), optionDates, strikes, quotes));
                                hCpiVol->setAdjustReferenceDate(false);
                            } else {
                                auto surface =
                                    QuantLib::ext::dynamic_pointer_cast<QuantExt::CPIVolatilitySurface>(wrapper.currentLink());
                                QL_REQUIRE(surface,
                                           "Internal error, todays market should build QuantExt::CPIVolatiltiySurface "
                                           "instead of QuantLib::CPIVolatilitySurface");
                                hCpiVol = Handle<QuantLib::CPIVolatilitySurface>(
                                    QuantLib::ext::make_shared<InterpolatedCPIVolatilitySurface<Bilinear>>(
                                        optionTenors, strikes, quotes, zeroInflationIndex.currentLink(), false,
                                        wrapper->settlementDays(), wrapper->calendar(),
                                        wrapper->businessDayConvention(), wrapper->dayCounter(),
                                        wrapper->observationLag(), surface->capFloorStartDate(), Bilinear(),
                                        surface->volatilityType(), surface->displacement()));
                            }
                        } else {
                            // string decayModeString = parameters->zeroInflationCapFloorVolDecayMode();
                            // ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            // QuantLib::ext::shared_ptr<CPIVolatilitySurface> cpiVol =
                            //     QuantLib::ext::make_shared<QuantExt::DynamicCPIVolatilitySurface>(*wrapper, decayMode);
                            // hCpiVol = Handle<CPIVolatilitySurface>(cpiVol);#
                            // FIXME
                            hCpiVol = wrapper;
                        }

                        hCpiVol->setAdjustReferenceDate(false);
                        if (wrapper->allowsExtrapolation())
                            hCpiVol->enableExtrapolation();
                        cpiInflationCapFloorVolatilitySurfaces_.emplace(
                            std::piecewise_construct, std::forward_as_tuple(Market::defaultConfiguration, name),
                            std::forward_as_tuple(hCpiVol));

                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
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
                        auto observationLegs = initMarket->yoyInflationObservationLags(name, configuration);
                        QL_REQUIRE(!observationLegs.empty(),
                                   "YoY inflation index " << name << " has no observation legs defined");
                        auto obsLag = observationLegs.rbegin()->second;
                        
                        Date date0 = yoyInflationTs->baseDate();
                        DayCounter dc = yoyInflationTs->dayCounter();
                        
                        vector<Time> yoyCurveTimes(
                            1, -dc.yearFraction(inflationPeriod(date0, yoyInflationTs->frequency()).first, asof_));
                        vector<Handle<Quote>> quotes;
                        QL_REQUIRE(!parameters->yoyInflationTenors(name).empty(),
                                   "yoy inflation tenors must not be empty");
                        QL_REQUIRE(parameters->yoyInflationTenors(name).front() > 0 * Days,
                                   "yoy inflation tenors must not include t=0");

                        for (auto& tenor : parameters->yoyInflationTenors(name)) {
                            Date inflDate = inflationPeriod(asof_ + tenor - obsLag, yoyInflationTs->frequency()).first;
                            yoyCurveTimes.push_back(dc.yearFraction(asof_, inflDate));
                        }

                        for (Size i = 1; i < yoyCurveTimes.size(); i++) {
                            Real rate = yoyInflationTs->yoyRate(yoyCurveTimes[i]);
                            auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : rate);
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

                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name,
                                     {std::vector<Real>(std::next(yoyCurveTimes.begin(), 1), yoyCurveTimes.end())});
                        simDataWritten = true;
                                                
                        QuantLib::ext::shared_ptr<YoYInflationTermStructure> yoyCurve;
                        // Note this is *not* a floating term structure, it is only suitable for sensi runs
                        // TODO: floating
                        if (useSpreadedTermStructures_) {
                            yoyCurve =
                                QuantLib::ext::make_shared<SpreadedYoYInflationCurve>(yoyInflationTs, yoyCurveTimes, quotes);
                            yoyCurve->setAdjustReferenceDate(false);
                        } else {
                            int simLag = simulationLag(yoyInflationTs);
                            vector<Period> tenors(1, 0 * Days);
                            tenors.insert(tenors.end(), parameters->yoyInflationTenors(name).begin(),
                                          parameters->yoyInflationTenors(name).end());
                            yoyCurve = QuantLib::ext::make_shared<YoYInflationCurveObserverMoving<Linear>>(
                                0, yoyInflationIndex->fixingCalendar(), dc, simLag, obsLag,
                                yoyInflationTs->frequency(), yoyInflationIndex->interpolated(), tenors,
                                quotes, yoyInflationTs->seasonality());
                        }
                        yoyCurve->setAdjustReferenceDate(false);
                        Handle<YoYInflationTermStructure> its(yoyCurve);
                        its->enableExtrapolation();
                        QuantLib::ext::shared_ptr<YoYInflationIndex> i(yoyInflationIndex->clone(its));
                        Handle<YoYInflationIndex> zh(i);
                        yoyInflationIndices_.insert(make_pair(make_pair(Market::defaultConfiguration, name), zh));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building " << name << " yoy inflation cap/floor volatility curve...");
                        Handle<QuantExt::YoYOptionletVolatilitySurface> wrapper =
                            initMarket->yoyCapFloorVol(name, configuration);
                        DLOG("Initial market "
                            << name << " yoy inflation cap/floor volatility type = " << wrapper->volatilityType());
                        Handle<QuantExt::YoYOptionletVolatilitySurface> hYoYCapletVol;

                        // Check if the risk factor is simulated before adding it
                        if (param.second.first) {
                            DLOG("Simulating yoy inflation optionlet vols for index name " << name);
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
                                    QuantLib::ext::shared_ptr<SimpleQuote> q(
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

                            std::vector<std::vector<Real>> coordinates(2);
                            for (Size i = 0; i < optionTenors.size(); ++i) {
                                coordinates[0].push_back(
                                    wrapper->timeFromReference(wrapper->optionDateFromTenor(optionTenors[i])));
                            }
                            for (Size j = 0; j < strikes.size(); ++j) {
                                coordinates[1].push_back(strikes[j]);
                            }

                            writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, coordinates);
                            simDataWritten = true;

                            DayCounter dc = wrapper->dayCounter();
                
                            QuantLib::ext::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyoptionletvolsurface;
                            if (useSpreadedTermStructures_) {
                                yoyoptionletvolsurface = QuantLib::ext::make_shared<QuantExt::SpreadedYoYVolatilitySurface>(
                                    wrapper, optionDates, strikes, quotes);
                                yoyoptionletvolsurface->setAdjustReferenceDate(false);
                            } else {
                                yoyoptionletvolsurface = QuantLib::ext::make_shared<StrippedYoYInflationOptionletVol>(
                                    0, wrapper->calendar(), wrapper->businessDayConvention(), dc,
                                    wrapper->observationLag(), wrapper->frequency(), wrapper->indexIsInterpolated(),
                                    optionDates, strikes, quotes, wrapper->volatilityType(), wrapper->displacement());
                            }
                            hYoYCapletVol = Handle<QuantExt::YoYOptionletVolatilitySurface>(yoyoptionletvolsurface);
                        } else {
                            string decayModeString = parameters->yoyInflationCapFloorVolDecayMode();
                            ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
                            QuantLib::ext::shared_ptr<QuantExt::DynamicYoYOptionletVolatilitySurface> yoyCapletVol =
                                QuantLib::ext::make_shared<QuantExt::DynamicYoYOptionletVolatilitySurface>(*wrapper, decayMode);
                            hYoYCapletVol = Handle<QuantExt::YoYOptionletVolatilitySurface>(yoyCapletVol);
                        }
                        hYoYCapletVol->setAdjustReferenceDate(false);
                        if (wrapper->allowsExtrapolation())
                            hYoYCapletVol->enableExtrapolation();
                        yoyCapFloorVolSurfaces_.emplace(std::piecewise_construct,
                                                        std::forward_as_tuple(Market::defaultConfiguration, name),
                                                        std::forward_as_tuple(hYoYCapletVol));
                        DLOG("Simulation market yoy inflation cap/floor volatility type = "
                            << hYoYCapletVol->volatilityType());
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::CommodityCurve: {

                std::vector<std::string> curveNames;
                std::vector<std::string> basisCurves;
                for (const auto& name : param.second.second) {
                    try {
                        Handle<PriceTermStructure> initialCommodityCurve =
                            initMarket->commodityPriceCurve(name, configuration);
                        QuantLib::ext::shared_ptr<CommodityBasisPriceTermStructure> basisCurve =
                            QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityBasisPriceTermStructure>(
                                initialCommodityCurve.currentLink());
                        if (basisCurve != nullptr) {
                            basisCurves.push_back(name);
                        } else {
                            curveNames.push_back(name);
                        }
                    } catch (...) {
                        curveNames.push_back(name);
                    }
                }
                curveNames.insert(curveNames.end(), basisCurves.begin(), basisCurves.end());

                for (const auto& name : curveNames) {

                    bool simDataWritten = false;
                    try {
                        DLOG("building commodity curve for " << name);

                        // Time zero initial market commodity curve
                        Handle<PriceTermStructure> initialCommodityCurve =
                            initMarket->commodityPriceCurve(name, configuration);

                        bool allowsExtrapolation = initialCommodityCurve->allowsExtrapolation();

                        // Get the configured simulation tenors. Simulation tenors being empty at this point means
                        // that we wish to use the pillar date points from the t_0 market PriceTermStructure.
                        vector<Period> simulationTenors = parameters->commodityCurveTenors(name);
                        if (simulationTenors.empty()){
                            DLOG("simulation tenors are empty, use pillar dates from T0 curve to build ssm curve.");
                            simulationTenors = simTenorsFromPriceCurve(initialCommodityCurve, asof_);
                            // It isn't great to be updating parameters here. However, actual tenors are requested
                            // downstream from parameters and they need to be populated.
                            parameters->setCommodityCurveTenors(name, simulationTenors);
                        }

                        // Get prices at specified simulation times from time 0 market curve and place in quotes
                        vector<Handle<Quote>> quotes(simulationTenors.size());
                        vector<Real> times;
                        for (Size i = 0; i < simulationTenors.size(); i++) {
                            Date d = asof_ + simulationTenors[i];
                            Real price = initialCommodityCurve->price(d, allowsExtrapolation);
                            times.push_back(initialCommodityCurve->timeFromReference(d));
                            TLOG("Commodity curve: price at " << io::iso_date(d) << " is " << price);
                            // if we simulate the factors and use spreaded ts, the quote should be zero
                            QuantLib::ext::shared_ptr<SimpleQuote> quote = QuantLib::ext::make_shared<SimpleQuote>(
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

                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {times});
                        simDataWritten = true;
                        QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve;

                        if (param.second.first && useSpreadedTermStructures_) {
                            vector<Real> simulationTimes;
                            for (auto const& t : simulationTenors) {
                                simulationTimes.push_back(
                                    initialCommodityCurve->dayCounter().yearFraction(asof_, asof_ + t));
                            }
                            if (simulationTimes.front() != 0.0) {
                                simulationTimes.insert(simulationTimes.begin(), 0.0);
                                quotes.insert(quotes.begin(), quotes.front());
                            }
                            // Created spreaded commodity price curve if we simulate commodities and spreads should be
                            // used
                            priceCurve = QuantLib::ext::make_shared<SpreadedPriceTermStructure>(
                                initialCommodityCurve, simulationTimes, quotes,
                                parsePriceCurveRollDown(parameters->commodityCurveRollDown()),
                                parameters->commodityCurveInterpolation(name));
                            priceCurve->setAdjustReferenceDate(false);
                        } else {
                            priceCurve = makeInterpolatedPriceCurve(
                                simulationTenors, quotes, initialCommodityCurve->dayCounter(),
                                initialCommodityCurve->currency(), parameters->commodityCurveInterpolation(name));
                        }

                        auto orgBasisCurve =
                            QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityBasisPriceTermStructure>(
                                initialCommodityCurve.currentLink());

                        Handle<PriceTermStructure> pts;  
                        if (orgBasisCurve == nullptr) {
                            pts = Handle<PriceTermStructure>(priceCurve);
                        } else {
                            auto baseIndex = commodityIndices_.find(
                                {Market::defaultConfiguration, orgBasisCurve->baseIndex()->underlyingName()});
                            QL_REQUIRE(baseIndex != commodityIndices_.end(),
                                       "Internal error in scenariosimmarket: couldn't find underlying base curve '"
                                           << orgBasisCurve->baseIndex()->underlyingName()
                                           << "' while building commodity basis curve '" << name << "'");
                            pts = Handle<PriceTermStructure>(QuantLib::ext::make_shared<CommodityBasisPriceCurveWrapper>(
                                orgBasisCurve, baseIndex->second.currentLink(), priceCurve));
                        } 

                        pts->setAdjustReferenceDate(false);
                        pts->enableExtrapolation(allowsExtrapolation);

                        Handle<CommodityIndex> commIdx(parseCommodityIndex(name, false, pts));
                        commodityIndices_.emplace(piecewise_construct,
                                                  forward_as_tuple(Market::defaultConfiguration, name),
                                                  forward_as_tuple(commIdx));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;
            }
            case RiskFactorKey::KeyType::CommodityVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("building commodity volatility for " << name);
                        
                        QuantLib::ext::shared_ptr<CommodityVolatilityConfig> volConfig;
                        if (curveConfigs.hasCommodityVolatilityConfig(name))
                            volConfig = curveConfigs.commodityVolatilityConfig(name);

                        // Get initial base volatility structure
                        Handle<BlackVolTermStructure> baseVol = initMarket->commodityVolatility(name, configuration);

                        Handle<BlackVolTermStructure> newVol;
                        bool stickyStrike =
                            parseStickyness(parameters_->commodityVolSmileDynamics(name)) == Stickyness::StickyStrike;

                        if (param.second.first) {
                            DLOG("Simulating commodity volatilities for index name " << name
                                                                                 << " with smile dynamics "
                                                                                 << parameters_->commodityVolSmileDynamics(name));
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
                            // Check if we have a calendar spread vol surface (naming convention:
                            // <name>_CALENDAR_SPREAD_<Offset>)
                            QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve;
                            bool isCalendarSpreadVolSurface = volConfig && volConfig->instrumentType() ==
                                                         MarketDatum::InstrumentType::COMMODITY_CALENDAR_SPREAD_OPTION;
                            if (isCalendarSpreadVolSurface) {
                                DLOG("Commodity volatility surface " << name << " is configured as calendar spread vol surface");
                                priceCurve = getCalendarSpreadPriceCurve(this, name, configuration,
                                    volConfig->calendarSpreadOffset(), volConfig->futureConventionsId());
                            } else {
                                priceCurve = *commodityPriceCurve(name, configuration);
                            }
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

                                auto ytsPtr = QuantLib::ext::make_shared<DiscountCurve>(dates, dfs, discCurve->dayCounter());
                                ytsPtr->enableExtrapolation();
                                yts = Handle<YieldTermStructure>(ytsPtr);
                                priceYts = Handle<YieldTermStructure>(
                                    QuantLib::ext::make_shared<PriceTermStructureAdapter>(priceCurve, ytsPtr));
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
                                        QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);
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

                            writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {moneyness, expiryTimes});
                            simDataWritten = true;

                            // Create volatility structure
                            if (!isSurface) {
                                DLOG("Ssm comm vol for " << name << " uses BlackVarianceCurve3.");
                                if (useSpreadedTermStructures_) {
                                    // if simulate atm only is false, we use the ATM slice from the wrapper only
                                    // the smile dynamics is sticky strike here always (if t0 is a surface)
                                    newVol = Handle<BlackVolTermStructure>(
                                        QuantLib::ext::make_shared<SpreadedBlackVolatilityCurve>(
                                            Handle<BlackVolTermStructure>(baseVol), expiryTimes, quotes[0],
                                            !parameters->simulateCommodityVolATMOnly(),
                                            parseDecayMode(parameters->commodityVolDecayMode())));
                                    newVol->setAdjustReferenceDate(false);
                                } else {
                                    newVol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<BlackVarianceCurve3>(
                                        0, NullCalendar(), baseVol->businessDayConvention(), dayCounter, expiryTimes,
                                        quotes[0], false, baseVol->volType(), baseVol->shift()));
                                }
                            } else {
                                DLOG("Ssm comm vol for " << name << " uses BlackVarianceSurfaceMoneynessSpot.");

                                bool flatExtrapMoneyness = true;
                                Handle<Quote> spot(
                                    QuantLib::ext::make_shared<DerivedPriceQuote>(Handle<PriceTermStructure>(priceCurve)));
                                if (useSpreadedTermStructures_) {
                                    // get init market curves to populate sticky ts in vol surface ctor
                                    Handle<YieldTermStructure> initMarketYts =
                                        initMarket->discountCurve(priceCurve->currency().code(), configuration);
                                    QuantLib::ext::shared_ptr<PriceTermStructure> initMarketPriceCurve;
                                    if (isCalendarSpreadVolSurface) {
                                        initMarketPriceCurve =
                                            getCalendarSpreadPriceCurve(initMarket.get(), name, configuration,
                                                volConfig->calendarSpreadOffset(), volConfig->futureConventionsId());
                                    } else {
                                        initMarketPriceCurve = *initMarket->commodityPriceCurve(name, configuration);
                                    }
                                    Handle<YieldTermStructure> initMarketPriceYts(
                                        QuantLib::ext::make_shared<PriceTermStructureAdapter>(initMarketPriceCurve, *initMarketYts));
                                    // create vol surface
                                    newVol = Handle<BlackVolTermStructure>(
                                        QuantLib::ext::make_shared<SpreadedBlackVolatilitySurfaceMoneynessForward>(
                                            Handle<BlackVolTermStructure>(baseVol), spot, expiryTimes, moneyness,
                                            quotes, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot->value())),
                                            initMarketPriceYts, initMarketYts, priceYts, yts, stickyStrike));
                                    newVol->setAdjustReferenceDate(false);
                                } else {
                                    newVol = Handle<BlackVolTermStructure>(
                                        QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessForward>(
                                            baseVol->calendar(), spot, expiryTimes, moneyness, quotes, dayCounter,
                                            priceYts, yts, stickyStrike, flatExtrapMoneyness,
                                            BlackVolTimeExtrapolation::FlatVolatility, baseVol->volType(),
                                            baseVol->shift()),
                                        parseDecayMode(parameters->commodityVolDecayMode()));
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
                                QuantLib::ext::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
                                    baseVol, 0, NullCalendar(), decayMode,
                                    stickyStrike ? StickyStrike : StickyMoneyness));
                        }

                        newVol->setAdjustReferenceDate(false);
                        newVol->enableExtrapolation(baseVol->allowsExtrapolation());
                        commodityVols_.emplace(piecewise_construct,
                                               forward_as_tuple(Market::defaultConfiguration, name),
                                               forward_as_tuple(newVol));

                        DLOG("Commodity volatility curve built for " << name);
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::BondFutureVolatility:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        createBondFutureVol(param.first, name, param.second.first, simDataWritten, bc);
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::Correlation:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("Adding correlations for " << name << " from configuration " << configuration);

                        vector<string> tokens = getCorrelationTokens(name);
                        QL_REQUIRE(tokens.size() == 2, "not a valid correlation pair: " << name);
                        pair<string, string> pair = std::make_pair(tokens[0], tokens[1]);

                        QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure> corr;
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
                                    QuantLib::ext::shared_ptr<SimpleQuote> q(
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

                            writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name,
                                         {parameters->correlationStrikes(), times});
                            simDataWritten = true;

                            if (n == 1 && m == 1) {
                                if (useSpreadedTermStructures_) {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(
                                        QuantLib::ext::make_shared<QuantExt::SpreadedCorrelationCurve>(baseCorr, times,
                                                                                               quotes[0]));
                                } else {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<FlatCorrelation>(
                                        baseCorr->settlementDays(), cal, quotes[0][0], dc));
                                }
                            } else if (n == 1) {
                                if (useSpreadedTermStructures_) {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(
                                        QuantLib::ext::make_shared<QuantExt::SpreadedCorrelationCurve>(baseCorr, times,
                                                                                               quotes[0]));
                                } else {
                                    ch = Handle<QuantExt::CorrelationTermStructure>(
                                        QuantLib::ext::make_shared<InterpolatedCorrelationCurve<Linear>>(times, quotes[0], dc,
                                                                                                 cal));
                                }
                            } else {
                                QL_FAIL("only atm or flat correlation termstructures currently supported");
                            }

                            ch->enableExtrapolation(baseCorr->allowsExtrapolation());
                        } else {
                            ch = Handle<QuantExt::CorrelationTermStructure>(*baseCorr);
                        }

                        ch->setAdjustReferenceDate(false);
                        correlationCurves_[make_tuple(Market::defaultConfiguration, pair.first, pair.second)] = ch;
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::CPR:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        DLOG("Adding cpr " << name << " from configuration " << configuration);
                        Real v = initMarket->cpr(name, configuration)->value();
                        auto q = QuantLib::ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : v);
                                                if(useSpreadedTermStructures_) {
                            auto m = [v](Real x) { return x + v; };
                            cprs_.insert(make_pair(make_pair(Market::defaultConfiguration, name),
                                                   Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                                                       Handle<Quote>(q), m))));
                        } else {
                            cprs_.insert(make_pair(make_pair(Market::defaultConfiguration, name), Handle<Quote>(q)));
                        }

                        if (param.second.first) {
                            simDataTmp.emplace(std::piecewise_construct, std::forward_as_tuple(param.first, name),
                                               std::forward_as_tuple(q));
                            if(useSpreadedTermStructures_) {
                                absoluteSimDataTmp.emplace(std::piecewise_construct,
                                                           std::forward_as_tuple(param.first, name),
                                                           std::forward_as_tuple(v));
                            }
                        }
                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {});
                        simDataWritten = true;
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;

            case RiskFactorKey::KeyType::SurvivalWeight:
                // nothing to do, these are written to asd
                break;

            case RiskFactorKey::KeyType::CreditState:
                // nothing to do, these are written to asd
                break;

            case RiskFactorKey::KeyType::Theta:
                // nothing to do, used only for sensi analysis
                break;

            case RiskFactorKey::KeyType::None:
                WLOG("RiskFactorKey None not yet implemented");
                break;

            case RiskFactorKey::KeyType::IntradayPowerCurve:
                for (const auto& name : param.second.second) {
                    bool simDataWritten = false;
                    try {
                        // At the moment only shifts of the day average price, the shape factors will not be shifted
                        DLOG("building intraday power curve for " << name);

                        auto initialIntradayPowerCurve =
                            initMarket->intradayPowerPriceCurve(name, configuration);
                        
                        QL_REQUIRE(!initialIntradayPowerCurve.empty(), "ScenarioSimMarket: Initial curve for " << name << " is empty");
                        auto averageDayPriceCurve = initialIntradayPowerCurve->averageDayPriceCurve();
                        
                        bool allowsExtrapolation = initialIntradayPowerCurve->allowsExtrapolation();

                        // Get the configured simulation tenors. Simulation tenors being empty at this point means
                        // that we wish to use the pillar date points from the t_0 market PriceTermStructure.
                        vector<Period> simulationTenors = parameters->intradayPowerCurveTenors(name);
                        if (simulationTenors.empty()){
                            DLOG("simulation tenors are empty, use pillar dates from T0 curve to build ssm curve.");
                            simulationTenors = simTenorsFromPriceCurve(averageDayPriceCurve, asof_);
                            // It isn't great to be updating parameters here. However, actual tenors are requested
                            // downstream from parameters and they need to be populated.
                            parameters->setIntradayPowerCurveTenors(name, simulationTenors);
                        }
                        // Get prices at specified simulation times from time 0 market curve and place in quotes
                        vector<Handle<Quote>> quotes(simulationTenors.size());
                        vector<Real> times;
                        for (Size i = 0; i < simulationTenors.size(); i++) {
                            Date d = asof_ + simulationTenors[i];
                            Real price = averageDayPriceCurve->price(d, allowsExtrapolation);
                            times.push_back(averageDayPriceCurve->timeFromReference(d));
                            TLOG("Intraday power curve: price at " << io::iso_date(d) << " is " << price);
                            // if we simulate the factors and use spreaded ts, the quote should be zero
                            QuantLib::ext::shared_ptr<SimpleQuote> quote = QuantLib::ext::make_shared<SimpleQuote>(
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

                        writeSimData(simDataTmp, absoluteSimDataTmp, param.first, name, {times});
                        simDataWritten = true;
                        QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve;

                        if (param.second.first && useSpreadedTermStructures_) {
                            vector<Real> simulationTimes;
                            for (auto const& t : simulationTenors) {
                                simulationTimes.push_back(averageDayPriceCurve->dayCounter().yearFraction(asof_, asof_ + t));
                            }
                            if (simulationTimes.front() != 0.0) {
                                simulationTimes.insert(simulationTimes.begin(), 0.0);
                                quotes.insert(quotes.begin(), quotes.front());
                            }
                            // Created spreaded commodity price curve if we simulate commodities and spreads should be
                            // used
                            priceCurve = QuantLib::ext::make_shared<SpreadedPriceTermStructure>(
                                averageDayPriceCurve, simulationTimes, quotes, PriceCurveRollDown::Forward,
                                parameters->intradayPowerCurveInterpolation(name));
                        } else {
                            priceCurve = makeInterpolatedPriceCurve(
                                simulationTenors, quotes, averageDayPriceCurve->dayCounter(),
                                averageDayPriceCurve->currency(), parameters->intradayPowerCurveInterpolation(name));
                        }
                        Handle<IntradayPowerPriceTermStructure> ippts(
                            QuantLib::ext::make_shared<IntradayPowerPriceTermStructure>(
                                QuantLib::Handle<QuantExt::PriceTermStructure>(priceCurve), initialIntradayPowerCurve->intradayShape()));
                        auto powerIndex = parseIntradayPowerIndex(name, false, ippts);
                        intradayPowerIndices_.emplace(piecewise_construct,
                                                  forward_as_tuple(Market::defaultConfiguration, name),
                                                  forward_as_tuple(powerIndex));
                    } catch (const std::exception& e) {
                        processException(e, name, param.first, simDataWritten);
                        gotException = true;
                    }
                }
                break;
            }

            if (!param.second.second.empty()) {
                DLOG("built " << std::left << std::setw(25) << param.first << std::right << std::setw(10)
                             << param.second.second.size() << std::setprecision(3) << std::setw(15)
                             << static_cast<double>(timer.elapsed().wall) / 1E6 << " ms");
            }

        } catch (const std::exception& e) {
            StructuredMessage(ore::data::StructuredMessage::Category::Error,
                                   ore::data::StructuredMessage::Group::Curve, e.what(),
                                   {{"exceptionType", "ScenarioSimMarket top level catch - this should never happen, "
                                                 "contact dev. Results are likely wrong or incomplete."}})
                .log();
            processException(e);
            gotException = true;
        }
    }

    // swap indices
    DLOG("building swap indices...");
    for (const auto& it : parameters->swapIndices()) {
        try {
            addSwapIndexToSsm(it.first);
        } catch (const std::exception& e) {
            processException(e);
            gotException = true;
        }
    }

    QL_REQUIRE(continueOnError || !gotException, "Got at least one exception during SSM build. Aborting since "
                                                 "continueOnError is false. Check structured messages for details.");

    // if specified, modify curves following the curve algebra specs
    applyCurveAlgebra();

    if (offsetScenario_ != nullptr && offsetScenario_->isAbsolute() && !useSpreadedTermStructures_) {
        auto recastedScenario = recastScenario(offsetScenario_, offsetScenario_->coordinates(), coordinatesData_);
        QL_REQUIRE(recastedScenario != nullptr, "ScenarioSimMarke: Offset Scenario couldn't applied");
        for (auto& [key, quote] : simData_) {
            if (recastedScenario->has(key)) {
                quote->setValue(recastedScenario->get(key));
            } else {
                QL_FAIL("ScenarioSimMarket: Offset Scenario doesnt contain key "
                        << key
                        << ". Internal error, possibly an internal error in the recastScenario method, contact dev.");
            }
        }
    } else if (offsetScenario_ != nullptr && offsetScenario_->isAbsolute() && useSpreadedTermStructures_) {
        auto recastedScenario = recastScenario(offsetScenario_, offsetScenario_->coordinates(), coordinatesData_);
        QL_REQUIRE(recastedScenario != nullptr, "ScenarioSimMarke: Offset Scenario couldn't applied");
        for (auto& [key, data] : simData_) {
            if (recastedScenario->has(key)) {
                auto shift = getDifferenceScenario(key.keytype, absoluteSimData_[key], recastedScenario->get(key));
                data->setValue(shift);
                absoluteSimData_[key] = recastedScenario->get(key);
            } else {
                QL_FAIL("ScenarioSimMarket: Offset Scenario doesnt contain key "
                        << key
                        << ". Internal error, possibly an internal error in the recastScenario method, contact dev.");
            }
        }
    } else if (offsetScenario_ != nullptr && !offsetScenario_->isAbsolute() && !useSpreadedTermStructures_) {
        auto recastedScenario = recastScenario(offsetScenario_, offsetScenario_->coordinates(), coordinatesData_);
        QL_REQUIRE(recastedScenario != nullptr, "ScenarioSimMarke: Offset Scenario couldn't applied");
        for (auto& [key, quote] : simData_) {
            if (recastedScenario->has(key)) {
                quote->setValue(addDifferenceToScenario(key.keytype, quote->value(), recastedScenario->get(key)));
            } else {
                QL_FAIL("ScenarioSimMarket: Offset Scenario doesnt contain key "
                        << key
                        << ". Internal error, possibly an internal error in the recastScenario method, contact dev.");
            }
        }
    } else if (offsetScenario_ != nullptr && !offsetScenario_->isAbsolute() && useSpreadedTermStructures_) {
        auto recastedScenario = recastScenario(offsetScenario_, offsetScenario_->coordinates(), coordinatesData_);
        QL_REQUIRE(recastedScenario != nullptr, "ScenarioSimMarke: Offset Scenario couldn't applied");
        for (auto& [key, quote] : simData_) {
            if (recastedScenario->has(key)) {
                quote->setValue(recastedScenario->get(key));
                absoluteSimData_[key] =
                    addDifferenceToScenario(key.keytype, absoluteSimData_[key], recastedScenario->get(key));
            } else {
                QL_FAIL("ScenarioSimMarket: Offset Scenario doesnt contain key "
                        << key
                        << ". Internal error, possibly an internal error in the recastScenario method, contact dev.");
            }
        }
    }

    LOG("building base scenario");
    auto tmp = QuantLib::ext::make_shared<SimpleScenario>(initMarket->asofDate(), "BASE", 1.0);
    if (!useSpreadedTermStructures_) {
        for (auto const& data : simData_) {
            tmp->add(data.first, data.second->value());
        }
        tmp->setAbsolute(true);
        for (auto const& [type, name, coordinates] : coordinatesData_) {
            tmp->setCoordinates(type, name, coordinates);
        }
        baseScenarioAbsolute_ = baseScenario_ = tmp;
    } else {
        auto tmpAbs = QuantLib::ext::make_shared<SimpleScenario>(initMarket->asofDate(), "BASE", 1.0);
        for (auto const& data : simData_) {
            tmp->add(data.first, data.second->value());
        }
        for (auto const& data : absoluteSimData_) {
            tmpAbs->add(data.first, data.second);
        }
        tmp->setAbsolute(false);
        tmpAbs->setAbsolute(true);
        for (auto const& [type, name, coordinates] : coordinatesData_) {
            tmp->setCoordinates(type, name, coordinates);
            tmpAbs->setCoordinates(type, name, coordinates);
        }
        baseScenario_ = tmp;
        baseScenarioAbsolute_ = tmpAbs;
    }
    LOG("building base scenario done");

    if (ScenarioInformation::instance().isEnabled()) {
        scenarioInformationSetter_ = QuantLib::ext::make_shared<QuantExt::ScenarioInformationSetter>();
        scenarioInformationSetter_->setParentScenario(baseScenarioAbsolute_);
        scenarioInformationSetter_->setChildScenario(baseScenarioAbsolute_);
    }
}

void ScenarioSimMarket::addSwapIndexToSsm(const std::string& indexName) {
    auto dsc = parameters_->swapIndices().find(indexName);
    QL_REQUIRE(dsc != parameters_->swapIndices().end(),
               "addSwapIndexToSsm: index '" << indexName << "' not found in ssm parameters.");
    DLOG("Adding swap index " << indexName << " with discounting index " << dsc->second);
    addSwapIndex(indexName, dsc->second, Market::defaultConfiguration);
}

void ScenarioSimMarket::reset() {
    auto filterBackup = filter_;
    // no filter
    filter_ = QuantLib::ext::make_shared<ScenarioFilter>();
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
    // clear delta scenario keys
    diffToBaseKeys_.clear();
    // see the comment in update() for why this is necessary...
    if (ObservationMode::instance().mode() == ObservationMode::Mode::Unregister) {
        QuantLib::ext::shared_ptr<QuantLib::Observable> obs = QuantLib::Settings::instance().evaluationDate();
        obs->notifyObservers();
    }
    // restore the filter
    filter_ = filterBackup;
    // reset asd cache
    cachingAsd_ = true;
    firstAsdDate_ = {};
    asdCacheCounter_ = 0;
    asdCache_ = {};
}

void ScenarioSimMarket::applyScenario(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& s) {

    auto scenario = s;
    if (useSpreadedTermStructures_ && scenario->isAbsolute())
        scenario = absoluteToSpreadedScenario(s, baseScenarioAbsolute_, parameters_);

    currentScenario_ = scenario;

    if (ScenarioInformation::instance().isEnabled()) {
        QuantLib::ext::shared_ptr<QuantExt::Scenario> currentScenarioAbsolute = currentScenario_;
        if (!currentScenario_->isAbsolute())
            currentScenarioAbsolute =
                addDifferenceToScenario(baseScenarioAbsolute_, currentScenario_, baseScenarioAbsolute_->asof());
        scenarioInformationSetter_->setParentScenario(baseScenarioAbsolute_);
        scenarioInformationSetter_->setChildScenario(currentScenarioAbsolute);
    }

    if (auto deltaScenario = QuantLib::ext::dynamic_pointer_cast<DeltaScenario>(scenario)) {

        // 1 handle delta scenario

        /* our assumption is that either all or none of the scenarios we apply are
           delta scenarios or the base scenario */

        if (deltaScenario != nullptr) {
            for (auto const& key : diffToBaseKeys_) {
                auto it = simData_.find(key);
                if (it != simData_.end()) {
                    it->second->setValue(baseScenario_->get(key));
                }
            }
            diffToBaseKeys_.clear();
            auto delta = deltaScenario->delta();
            bool missingPoint = false;
            for (auto const& key : delta->keys()) {
                auto it = simData_.find(key);
                if (it == simData_.end()) {
                    ALOG("simulation data point missing for key " << key);
                    missingPoint = true;
                } else {
                    if (filter_->allow(key)) {
                        it->second->setValue(delta->get(key));
                        diffToBaseKeys_.insert(key);
                    }
                }
            }
            QL_REQUIRE(!missingPoint, "simulation data points missing from scenario, exit.");
        }

    } else if (auto s = QuantLib::ext::dynamic_pointer_cast<SimpleScenario>(scenario); s && cacheSimData_) {

        // 2 handle cached sim data with simple scenario

        /*  apply scenario based on cached indices for simData_ for a SimpleScenario
            the scenario's keysHash() is used to make sure consistent keys are used
            if keysHash() is zero, this check is not effective (for backwards compatibility) */

        // fill cache

        if (cachedSimData_.empty() || s->keysHash() != cachedSimDataKeysHash_) {
            cachedSimData_.clear();
            cachedSimDataKeysHash_ = s->keysHash();
            Size count = 0;
            for (auto const& key : s->keys()) {
                auto it = simData_.find(key);
                if (it == simData_.end()) {
                    WLOG("simulation data point missing for key " << key);
                    cachedSimData_.push_back(QuantLib::ext::shared_ptr<SimpleQuote>());
                    cachedSimDataActive_.push_back(false);
                } else {
                    ++count;
                    cachedSimData_.push_back(it->second);
                    cachedSimDataActive_.push_back(filter_->allow(key));
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
                cachedSimData_[i]->setValue(q);
            ++i;
        }

    } else {

        // 3 all other cases

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

    // set numeraire, label and update date from scenario

    numeraire_ = scenario->getNumeraire();
    label_ = scenario->label();
}

void ScenarioSimMarket::preUpdate() {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    if (om == ObservationMode::Mode::Disable)
        ObservableSettings::instance().disableUpdates(false);
    else if (om == ObservationMode::Mode::Defer)
        ObservableSettings::instance().disableUpdates(true);
}

void ScenarioSimMarket::updateDate(const Date& d) {
    if(d == Null<Date>())
        return;
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
        QuantLib::ext::shared_ptr<QuantLib::Observable> obs = QuantLib::Settings::instance().evaluationDate();
        obs->notifyObservers();
    }
}

Date ScenarioSimMarket::loadNextScenario(const Date& d) {
    QL_REQUIRE(scenarioGenerator_ != nullptr, "ScenarioSimMarket::update: no scenario generator set");
    loadedScenario_ = scenarioGenerator_->next(d);
    return loadedScenario_->asof();
}

void ScenarioSimMarket::applyLoadedScenario() {
    applyScenario(loadedScenario_);
}

void ScenarioSimMarket::postUpdate() {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    // Observation Mode - key to update these before fixings are set
    if (om == ObservationMode::Mode::Disable) {
        refresh();
        ObservableSettings::instance().enableUpdates();
    } else if (om == ObservationMode::Mode::Defer) {
        ObservableSettings::instance().enableUpdates();
    }
}

void ScenarioSimMarket::setAsd(Size cacheCounter) {
    for (Size i = 0; i < asdCache_.indices.size(); ++i) {
        asd_->set(asdCache_.indexRawData[cacheCounter][i],
                  asdCache_.indices[i]->fixing(asdCache_.indexFixingDates[cacheCounter][i]));
    }

    for (Size i = 0; i < asdCache_.fxSpots.size(); ++i) {
        asd_->set(asdCache_.fxSpotRawData[cacheCounter][i], asdCache_.fxSpots[i]->value());
    }

    for (Size i = 0; i < asdCache_.creditStateKeys.size(); ++i) {
        asd_->set(asdCache_.creditStateRawData[cacheCounter][i], currentScenario_->get(asdCache_.creditStateKeys[i]));
    }

    for (Size i = 0; i < asdCache_.survWeightKeys.size(); ++i) {
        asd_->set(asdCache_.survWeightRawData[cacheCounter][i], currentScenario_->get(asdCache_.survWeightKeys[i]));
        asd_->set(asdCache_.rrRawData[cacheCounter][i], currentScenario_->get(asdCache_.rrKeys[i]));
    }

    asd_->set(numeraire_, AggregationScenarioDataType::Numeraire);
}

void ScenarioSimMarket::updateAsd() {

    if (asd_) {

        Date d = Settings::instance().evaluationDate();

        if (cachingAsd_) {

            if (d == firstAsdDate_) {

                cachingAsd_ = false;

            } else {

                // caching on first path

                if (firstAsdDate_ == Date())
                    firstAsdDate_ = d;

                // indices

                if (asdCache_.indices.empty()) {
                    for (auto i : parameters_->additionalScenarioDataIndices()) {
                        QuantLib::ext::shared_ptr<QuantLib::Index> index;
                        try {
                            index = *iborIndex(i);
                        } catch (...) {
                        }
                        try {
                            index = *swapIndex(i);
                        } catch (...) {
                        }
                        QL_REQUIRE(index != nullptr,
                                   "ScenarioSimMarket::update() index " << i << " not found in sim market");
                        if (auto fb = QuantLib::ext::dynamic_pointer_cast<FallbackIborIndex>(index)) {
                            // proxy fallback ibor index by its rfr index's fixing
                            index = fb->rfrIndex();
                        }
                        asdCache_.indices.push_back(index);
                    }
                }

                asdCache_.indexRawData.push_back({});
                asdCache_.indexFixingDates.push_back({});
                for (Size i = 0; i<  parameters_->additionalScenarioDataIndices().size();++i) {
                    asdCache_.indexRawData.back().push_back(asd_->rawData(
                        AggregationScenarioDataType::IndexFixing, parameters_->additionalScenarioDataIndices()[i]));
                    asdCache_.indexFixingDates.back().push_back(asdCache_.indices[i]->fixingCalendar().adjust(d));
                }

                // fx spots

                if (asdCache_.fxSpots.empty()) {
                    for (auto c : parameters_->additionalScenarioDataCcys()) {
                        if (c != parameters_->baseCcy()) {
                            asdCache_.fxSpots.push_back(fxSpot(c + parameters_->baseCcy()));
                        }
                    }
                }

                asdCache_.fxSpotRawData.push_back({});
                for (auto c : parameters_->additionalScenarioDataCcys()) {
                    if (c != parameters_->baseCcy()) {
                        asdCache_.fxSpotRawData.back().push_back(asd_->rawData(AggregationScenarioDataType::FXSpot, c));
                    }
                }

                // credit states

                if (asdCache_.creditStateKeys.empty()) {
                    for (Size i = 0; i < parameters_->additionalScenarioDataNumberOfCreditStates(); ++i) {
                        RiskFactorKey key(RiskFactorKey::KeyType::CreditState, std::to_string(i));
                        QL_REQUIRE(currentScenario_->has(key), "scenario does not have key " << key);
                        asdCache_.creditStateKeys.push_back(key);
                    }
                }

                asdCache_.creditStateRawData.push_back({});
                for (Size i = 0; i < parameters_->additionalScenarioDataNumberOfCreditStates(); ++i) {
                    asdCache_.creditStateRawData.back().push_back(
                        asd_->rawData(AggregationScenarioDataType::CreditState, std::to_string(i)));
                }

                // surv weights and rrs

                if (asdCache_.survWeightKeys.empty()) {
                    for (const auto& n : parameters_->additionalScenarioDataSurvivalWeights()) {
                        RiskFactorKey key(RiskFactorKey::KeyType::SurvivalWeight, n);
                        RiskFactorKey rrKey(RiskFactorKey::KeyType::RecoveryRate, n);
                        QL_REQUIRE(currentScenario_->has(key), "scenario does not have key " << key);
                        QL_REQUIRE(currentScenario_->has(rrKey), "scenario does not have key " << key);
                        asdCache_.survWeightKeys.push_back(key);
                        asdCache_.rrKeys.push_back(rrKey);
                    }
                }

                asdCache_.survWeightRawData.push_back({});
                asdCache_.rrRawData.push_back({});
                for (const auto& n : parameters_->additionalScenarioDataSurvivalWeights()) {
                    asdCache_.survWeightRawData.back().push_back(
                        asd_->rawData(AggregationScenarioDataType::SurvivalWeight, n));
                    asdCache_.rrRawData.back().push_back(asd_->rawData(AggregationScenarioDataType::RecoveryRate, n));
                }

                setAsd(asdCache_.indexRawData.size() - 1);
            }
        }

        if (!cachingAsd_) {

            // cachingAsd_ is false

            if (d == firstAsdDate_) {
                asdCacheCounter_ = 0;
            } else {
                ++asdCacheCounter_;
            }

            setAsd(asdCacheCounter_);
        }

        asd_->next();
    }
}

bool ScenarioSimMarket::isSimulated(const RiskFactorKey::KeyType& factor) const {
    return std::find(nonSimulatedFactors_.begin(), nonSimulatedFactors_.end(), factor) == nonSimulatedFactors_.end();
}

Handle<YieldTermStructure> ScenarioSimMarket::getYieldCurve(const string& yieldSpecId,
                                                            const TodaysMarketParameters& todaysMarketParams,
                                                            const string& configuration,
                                                            const QuantLib::ext::shared_ptr<Market>& market) const {

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

Handle<YieldTermStructure> ScenarioSimMarket::getYieldCurve(const std::string& key) const {
    RiskFactorKey rf = parseRiskFactorKey(key + "/0");
    switch (rf.keytype) {
    case RiskFactorKey::KeyType::DiscountCurve:
        return this->discountCurve(rf.name);
    case RiskFactorKey::KeyType::YieldCurve:
        return this->yieldCurve(rf.name);
    case RiskFactorKey::KeyType::IndexCurve:
        return this->iborIndex(rf.name)->forwardingTermStructure();
    default:
        QL_FAIL("ScenarioSimMarket::getYieldCurve(" << key << "): key type " << rf.keytype
                                                    << " not suitable for yield curves. Internal error. Contact dev.");
    }
}

void ScenarioSimMarket::applyCurveAlgebra() {
    LOG("Applying " << parameters_->curveAlgebraData().data().size() << " curve algebra rules...");
    for (auto const& a : parameters_->curveAlgebraData().data()) {
        DLOG("Processing curve algebra rule for key " << a.key());
        QL_REQUIRE(a.operationType() == "Spreaded",
                   "ScenarioSimMarket::applyCurveAlgebra(): operation type must be 'Spreaded'.");
        auto rfKeyTarget = parseRiskFactorKey(a.key() + "/0");
        switch (rfKeyTarget.keytype) {
        case RiskFactorKey::KeyType::DiscountCurve:
        case RiskFactorKey::KeyType::YieldCurve:
        case RiskFactorKey::KeyType::IndexCurve:
            applyCurveAlgebraSpreadedYieldCurve(a);
            break;
        case RiskFactorKey::KeyType::CommodityCurve:
            applyCurveAlgebraCommodityPriceCurve(a);
            break;
        case RiskFactorKey::KeyType::IntradayPowerCurve:
            applyCurveAlgebraIntradayPowerPriceCurve(a);
            break;
        default:
            QL_FAIL("ScenarioSimMarket::applyCurveAlgebra(): target key type "
                    << rfKeyTarget.keytype
                    << " not supported for curve algebra. Expected: DiscountCurve, YieldCurve, IndexCurve or CommodityCurve. ");
        }
    }
}

void ScenarioSimMarket::applyCurveAlgebraSpreadedYieldCurve(
    const ScenarioSimMarketParameters::CurveAlgebraData::Curve& a) {
    std::vector<Handle<YieldTermStructure>> bases;
    std::vector<double> multiplier;
    for (auto const& arg : a.arguments()) {
        auto v = parseListOfValues(arg);
        bases.push_back(getYieldCurve(v[0]));
        multiplier.push_back(v.size() <= 1 ? 1.0 : parseReal(v[1]));
        DLOG("curve " << a.key() << " is set as spreaded over " << v[0] << ", multiplier " << multiplier.back());
    }
    auto target = getYieldCurve(a.key());
    if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedDiscountCurve2>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<SpreadedDiscountCurve>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else {
        QL_FAIL("ScenarioSimMarket::applyCurveAlgebraSpreadedRateCurve(): target curve could not be cast to one of the "
                "supported curve types. Internal error, contact dev.");
    }
}

void makeCommodityPriceCurveSpreaded(const Handle<PriceTermStructure>& target,
                                     const std::vector<Handle<PriceTermStructure>>& bases,
                                     const std::vector<double>& multiplier) {
    if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<Linear>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<SpreadedPriceTermStructure>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<CommodityBasisPriceCurveWrapper>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<BackwardFlat>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinear>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<Cubic>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LinearFlat>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinearFlat>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<CubicFlat>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<ForwardFlat>>(*target)) {
        c->makeThisCurveSpreaded(bases, multiplier);
    } else {
        QL_FAIL("makeCommodityPriceCurveSpreaded(): target curve could not be cast to one of the "
                "supported curve types. Internal error, contact dev.");
    }
}

void ScenarioSimMarket::applyCurveAlgebraCommodityPriceCurve(
    const ScenarioSimMarketParameters::CurveAlgebraData::Curve& a) {
    std::vector<Handle<PriceTermStructure>> bases;
    std::vector<double> multiplier;
    for (auto const& arg : a.arguments()) {
        auto v = parseListOfValues(arg);
        auto rf = parseRiskFactorKey(v[0] + "/0");
        bases.push_back(commodityIndex(rf.name)->priceCurve());
        multiplier.push_back(v.size() <= 1 ? 1.0 : parseReal(v[1]));
        DLOG("curve " << a.key() << " is set as spreaded over " << v[0] << ", multiplier " << multiplier.back());
    }
    auto rf = parseRiskFactorKey(a.key() + "/0");
    auto target = commodityIndex(rf.name)->priceCurve();
    makeCommodityPriceCurveSpreaded(target, bases, multiplier);
}

void ScenarioSimMarket::applyCurveAlgebraIntradayPowerPriceCurve(const ScenarioSimMarketParameters::CurveAlgebraData::Curve& a) {
    std::vector<Handle<PriceTermStructure>> bases;
    std::vector<double> multiplier;
    for (auto const& arg : a.arguments()) {
        auto v = parseListOfValues(arg);
        auto rf = parseRiskFactorKey(v[0] + "/0");
        QL_REQUIRE(rf.keytype == RiskFactorKey::KeyType::CommodityCurve,
                   "ScenarioSimMarket::applyCurveAlgebraIntradayPowerPriceCurve(): argument curve "
                       << v[0] << " is not of type CommodityCurve. Internal error, contact dev.");
        bases.push_back(commodityIndex(rf.name)->priceCurve());
        multiplier.push_back(v.size() <= 1 ? 1.0 : parseReal(v[1]));
        DLOG("curve " << a.key() << " is set as spreaded over " << v[0] << ", multiplier " << multiplier.back());
    }
    auto rf = parseRiskFactorKey(a.key() + "/0");
    QL_REQUIRE(rf.keytype == RiskFactorKey::KeyType::IntradayPowerCurve,
               "ScenarioSimMarket::applyCurveAlgebraIntradayPowerPriceCurve(): target curve "
                   << a.key() << " is not of type IntradayPowerCurve. Internal error, contact dev.");
    auto target = intradayPowerIndex(rf.name)->priceCurve();
    auto& avgDayPriceCurve = target->averageDayPriceCurve();
    makeCommodityPriceCurveSpreaded(avgDayPriceCurve, bases, multiplier);
}

void ScenarioSimMarket::createBondFutureVol(RiskFactorKey::KeyType rfKeyType, const string& name, bool simulate,
    bool& simDataWritten, const BuildContext& bc) {

    DLOG("ScenarioSimMarket: building bond future volatility for " << name);

    // Containers used below.
    map<RiskFactorKey, ext::shared_ptr<SimpleQuote>> simDataTmp;
    map<RiskFactorKey, Real> absoluteSimDataTmp;

    // We only support an expiry x absolute strike surface here as the implementation was done for CRIF.

    // The new volatility strucuture to be populated.
    Handle<BlackVolTermStructure> newVol;

    // Get initial base volatility structure
    Handle<BlackVolTermStructure> baseVol = bc.initMarket->bondFutureVol(name, bc.configuration);
    bool stickyStrike = parseStickyness(parameters_->commodityVolSmileDynamics(name)) == Stickyness::StickyStrike;

    if (simulate) {
        DLOG("ScenarioSimMarket: simulating bond future volatilities for " << name << " with smile dynamics " <<
            parameters_->commodityVolSmileDynamics(name));
        vector<Real> moneyness = parameters_->bondFutureVolMoneyness(name);
        sortCheckUnique(moneyness, "Bond future volatility moneyness ", name,
            [](Real x, Real y) { return close(x, y); });
        vector<Period> expiries = parameters_->bondFutureVolExpiries(name);
        sortCheckUnique(expiries, "Bond future volatility expiries ", name);

        // Populate expiry times for the new volatility surface below.
        vector<Time> expiryTimes(expiries.size());
        vector<Date> expiryDates(expiries.size());
        DayCounter dayCounter = baseVol->dayCounter();
        for (Size j = 0; j < expiries.size(); ++j) {
            Date d = asof_ + expiries[j];
            expiryDates[j] = d;
            expiryTimes[j] = dayCounter.yearFraction(asof_, d);
        }

        // We set up spot moneyness below.
        // Note name may have a suffix like _CALL or _PUT which we need to strip to get the future contract name.
        string futureName{ futureContractName(name) };
        Handle<Quote> futureQuote = bc.initMarket->securityPrice(futureName, bc.configuration);
        Real futurePrice = futureQuote->value();

        // Populate the quotes for the new surface.
        using QuoteRow = vector<Handle<Quote>>;
        using QuoteMatrix = vector<QuoteRow>;
        QuoteMatrix quotes(moneyness.size(), QuoteRow(expiries.size()));
        Size index = 0;
        for (Size i = 0; i < moneyness.size(); ++i) {
            for (Size j = 0; j < expiries.size(); ++j) {
                Real strike = moneyness[i] * futurePrice;
                auto vol = baseVol->blackVol(expiryDates[j], strike);
                Real quoteValue = useSpreadedTermStructures_ ? 0.0 : vol;
                auto quote = ext::make_shared<SimpleQuote>(quoteValue);
                simDataTmp.emplace(RiskFactorKey{rfKeyType, name, index}, quote);
                if (useSpreadedTermStructures_) {
                    absoluteSimDataTmp.emplace(RiskFactorKey{rfKeyType, name, index}, vol);
                }
                quotes[i][j] = Handle<Quote>(quote);
                ++index;
            }
        }

        // Write the simulation data and update the flag.
        writeSimData(simDataTmp, absoluteSimDataTmp, rfKeyType, name, { moneyness, expiryTimes });
        simDataWritten = true;

        // Create the new volatility surface.
        bool flatExtrapMoneyness = true;
        if (useSpreadedTermStructures_) {
            Handle<YieldTermStructure> emptyYts;
            auto volPtr = QuantLib::ext::make_shared<SpreadedBlackVolatilitySurfaceMoneynessSpot>(
                Handle<BlackVolTermStructure>(baseVol), futureQuote, expiryTimes, moneyness, quotes, futureQuote,
                emptyYts, emptyYts, emptyYts, emptyYts, stickyStrike);
            newVol = Handle<BlackVolTermStructure>(volPtr);
        } else {
            auto volPtr = QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessSpot>(
                baseVol->calendar(), futureQuote, expiryTimes, moneyness, quotes, dayCounter, stickyStrike,
                flatExtrapMoneyness, BlackVolTimeExtrapolation::FlatVolatility, baseVol->volType(), baseVol->shift());
            newVol = Handle<BlackVolTermStructure>(volPtr);
        }

    } else {
        // This is a straight copy from other volatility structures. It will likely never be used for bond future 
        // volatilities but if it is needed, it will need to be reviewed.
        string decayModeString = parameters_->commodityVolDecayMode();
        DLOG("ScenarioSimMarket: deterministic bond future volatilities with decay mode " <<
            decayModeString << " for " << name);
        ReactionToTimeDecay decayMode = parseDecayMode(decayModeString);
        auto stickyness = stickyStrike ? StickyStrike : StickyMoneyness;
        auto volPtr = QuantLib::ext::make_shared<QuantExt::DynamicBlackVolTermStructure<tag::curve>>(
            baseVol, 0, NullCalendar(), decayMode, stickyness);
        newVol = Handle<BlackVolTermStructure>(volPtr);
    }

    newVol->setAdjustReferenceDate(false);
    newVol->enableExtrapolation(baseVol->allowsExtrapolation());
    bondFutureVols_.emplace(std::pair{Market::defaultConfiguration, name}, newVol);

    DLOG("ScenarioSimMarket: bond future volatility built for " << name);
}

void ScenarioSimMarket::createOptionletVol(RiskFactorKey::KeyType rfKeyType, const string& name, bool simulate,
    bool& simDataWritten, const BuildContext& bc) {

    DLOG("ScenarioSimMarket: building cap floor volatility for " << name);

    auto stickyness = parseStickyness(parameters_->capFloorVolSmileDynamics(name));

    // Get IR index name and rate tenor.
    auto indexNameRateCompPeriod = bc.initMarket->capFloorVolIndexBase(name, bc.configuration);
    const auto& [indexName, rateCompPeriod] = indexNameRateCompPeriod;
    ext::shared_ptr<IborIndex> index;
    if (!indexName.empty())
        index = parseIborIndex(indexName);

    // Delegate to helper methods depending on what we are looking for.
    Handle<OptionletVolatilityStructure> ssmOvs;
    if (!simulate) {
        auto baseOvs = bc.initMarket->capFloorVol(name, bc.configuration);
        ssmOvs = createNonSimulatedOptionletVol(*baseOvs, name);
    } else if (stickyness == Stickyness::StickySABR) {

        // Sticky SABR needs to know if the initial market volatility structure is a SABR or a proxy to a SABR.
        const auto& initMktOvs = *bc.initMarket->capFloorVol(name, bc.configuration);
        RelinkableHandle<OptionletVolatilityStructure> baseOvs;
        auto proxy = ext::dynamic_pointer_cast<ProxyOptionletVolatility>(initMktOvs);
        if (proxy)
            baseOvs.linkTo(*proxy->baseVol());
        else
            baseOvs.linkTo(initMktOvs);

        ssmOvs = createStickySabrOptionletVol(rfKeyType, name, simDataWritten, bc,
            index, baseOvs, rateCompPeriod, proxy);

    } else {
        auto baseOvs = bc.initMarket->capFloorVol(name, bc.configuration);
        ssmOvs = createOptionletVol(rfKeyType, name, simDataWritten, bc, index, baseOvs, rateCompPeriod, stickyness);
    }

    // Final steps common to all.
    ssmOvs->setAdjustReferenceDate(false);
    ssmOvs->enableExtrapolation();
    capFloorCurves_.emplace(std::pair{ Market::defaultConfiguration, name }, ssmOvs);
    capFloorIndexBase_.emplace(std::pair{ Market::defaultConfiguration, name }, indexNameRateCompPeriod);

    DLOG("ScenarioSimMarket: cap floor volatility built for " << name);
}

ScenarioSimMarket::CapFloorConventions ScenarioSimMarket::getCapFloorConventions(const string& name,
    const CurveConfigurations& curveConfigs, const ext::shared_ptr<IborIndex>& index) const
{
    CapFloorConventions result;

    // Try to get the relevant cap floor curve configuration.
    ext::shared_ptr<CapFloorVolatilityCurveConfig> config;
    if (curveConfigs.hasCapFloorVolCurveConfig(name)) {
        config = curveConfigs.capFloorVolCurveConfig(name);
    } else if (index) {
        const auto& ccy = index->currency().code();
        if (curveConfigs.hasCapFloorVolCurveConfig(ccy))
            config = curveConfigs.capFloorVolCurveConfig(ccy);
    }

    // If we got a curve configuration above, populate some information from it.
    if (config) {
        result.settleDays = config->settleDays();
        result.onSettlementDays = config->onCapSettlementDays();
    }

    // If we have an IR index, populate some information from it.
    if (index) {
        result.indexCalendar = index->fixingCalendar();
        result.isOis = ext::dynamic_pointer_cast<OvernightIndex>(index) != nullptr;
    }

    return result;
}

vector<Date> ScenarioSimMarket::getOptionDates(const vector<Period>& optionTenors,
    const ext::shared_ptr<IborIndex>& index, const CapFloorConventions& conv,
    const ext::shared_ptr<OptionletVolatilityStructure>& baseOvs, const Period& rateCompPeriod,
    const std::string& name) const
{
    vector<Date> optionDates(optionTenors.size());

    // Deal with the simple case first and return.
    if (!parameters_->capFloorVolAdjustOptionletPillars() || !index) {
        for (Size i = 0; i < optionTenors.size(); ++i) {
            optionDates[i] = baseOvs->optionDateFromTenor(optionTenors[i]);
            if (!conv.indexCalendar.empty())
                optionDates[i] = conv.indexCalendar.adjust(optionDates[i]);
            DLOG("Option [tenor, date] pair is [" << optionTenors[i] << ", " << io::iso_date(optionDates[i]) << "]");
        }
        return optionDates;
    }

    // More involved case where we need to adjust the optionlet pillars.
    ext::shared_ptr<OvernightIndex> onIndex;
    if (conv.isOis)
        onIndex = ext::static_pointer_cast<OvernightIndex>(index);

    for (Size i = 0; i < optionTenors.size(); ++i) {
        if (conv.isOis) {
            // Create a cap, on overnight indexed coupons, with the relevant option tenor.
            Leg capFloor = MakeOISCapFloor(CapFloor::Cap, optionTenors[i], onIndex, rateCompPeriod, 0.0)
                .withTelescopicValueDates(true)
                .withSettlementDays(conv.onSettlementDays);

            if (capFloor.empty()) {
                optionDates[i] = asof_ + 1;
            } else {
                // Get the last coupon of the cap and use its fixing date as the optionlet pillar.
                auto cpn = ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(capFloor.back());
                QL_REQUIRE(cpn, "ScenarioSimMarket: internal error, could not cast to "
                    "CappedFlooredOvernightIndexedCoupon when building optionlet vol for '" << name <<
                    "' with overnight index '" << onIndex->name() << "'");
                auto und = cpn->underlying();
                auto d = baseOvs->useEffectiveVolatility() ? und->fixingDateNoCutoff() : und->fixingDates().front();
                optionDates[i] = std::max(asof_ + 1, d);
            }
        } else {
            // Create a cap, on ibor coupons, with the relevant option tenor.
            // Use the fixing date of the last coupon as the optionlet pillar.
            ext::shared_ptr<CapFloor> capFloor = MakeCapFloor(CapFloor::Cap, optionTenors[i], index, 0.0, 0 * Days);
            if (capFloor->floatingLeg().empty()) {
                optionDates[i] = asof_ + 1;
            } else {
                optionDates[i] = std::max(asof_ + 1, capFloor->lastFloatingRateCoupon()->fixingDate());
            }
        }

        // Check that the option dates are increasing.
        QL_REQUIRE(i == 0 || optionDates[i] > optionDates[i - 1], "ScenarioSimMarket: got non-increasing option dates "
            << optionDates[i - 1] << ", " << optionDates[i] << " for tenors " << optionTenors[i - 1] << ", " <<
            optionTenors[i] << " for index " << index->name());

        DLOG("Option [tenor, date] pair is [" << optionTenors[i] << ", " << io::iso_date(optionDates[i]) << "]");
    }

    return optionDates;
}

vector<Rate> ScenarioSimMarket::getAtmStrikes(const vector<Period>& optionTenors, const vector<Date>& optionDates,
    const ext::shared_ptr<IborIndex>& index, const CapFloorConventions& conv, const Period& rateCompPeriod,
    const std::string& name, const string& configuration, const ext::shared_ptr<Market>& initMarket) const
{
    vector<Rate> result(optionTenors.size());

    QL_REQUIRE(index, "ScenarioSimMarket: expected ibor index for cap floor config " << name <<
        " or a curve config for a ccy");

    // Get the IR index from the initial market.
    auto oreIndexName = IndexNameTranslator::instance().oreName(index->name());
    const auto& initMktIndex = *initMarket->iborIndex(oreIndexName, configuration);

    // If using the term cap ATM rate is configured, caculate the ATM rates and return.
    if (parameters_->capFloorVolUseCapAtm()) {
        QL_REQUIRE(!conv.isOis, "ScenarioSimMarket: capFloorVolUseCapATM not supported for OIS indices (" <<
            initMktIndex->name() << ")");
        const auto& ccy = initMktIndex->currency().code();
        const auto& discTs = **initMarket->discountCurve(ccy, configuration);
        for (Size i = 0; i < optionTenors.size(); ++i) {
            ext::shared_ptr<CapFloor> cap = MakeCapFloor(CapFloor::Cap, optionTenors[i], initMktIndex, 0.0, 0 * Days);
            result[i] = cap->atmRate(discTs);
        }
        return result;
    }

    // If not an OIS index, the ATM rate is simple i.e. the Ibor index fixing on the optionlet date.
    if (!conv.isOis) {
        for (Size i = 0; i < optionTenors.size(); ++i)
            result[i] = initMktIndex->fixing(optionDates[i]);
        return result;
    }

    // Deal with the case now of ATM strikes for optionlet on OIS coupon.
    ext::shared_ptr<OvernightIndex> onIndex = ext::static_pointer_cast<OvernightIndex>(initMktIndex);
    for (Size i = 0; i < optionTenors.size(); ++i) {

        Leg capFloor = MakeOISCapFloor(CapFloor::Cap, optionTenors[i], onIndex, rateCompPeriod, 0.0)
            .withTelescopicValueDates(true)
            .withSettlementDays(conv.onSettlementDays);

        if (capFloor.empty()) {
            result[i] = initMktIndex->fixing(optionDates[i]);
        } else {
            // Get the last coupon of the cap and use its fixing date as the optionlet pillar.
            auto cpn = ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(capFloor.back());
            QL_REQUIRE(cpn, "ScenarioSimMarket: internal error, could not cast to "
                "CappedFlooredOvernightIndexedCoupon when building optionlet vol for '" << name <<
                "' with overnight index '" << onIndex->name() << "'");
            result[i] = cpn->underlying()->rate();
        }
    }
    return result;
}

vector<Real> ScenarioSimMarket::getProxyAdjustments(const vector<Period>& optionTenors, const vector<Date>& optionDates,
    const ext::shared_ptr<ProxyOptionletVolatility>& proxy) const
{
    vector<Real> result(optionTenors.size());
    for (Size i = 0; i < optionTenors.size(); ++i) {
        Real base = proxy->getAtmLevel(optionDates[i], proxy->baseIndex(), proxy->baseRateComputationPeriod());
        DLOG("Base ATM level from proxy for option tenor " << optionTenors[i] << " is " << base);
        Real target = proxy->getAtmLevel(optionDates[i], proxy->targetIndex(), proxy->targetRateComputationPeriod());
        DLOG("Target ATM level from proxy for option tenor " << optionTenors[i] << " is " << target);
        result[i] = base - target;
        DLOG("Adjusted strikes for option tenor " << optionTenors[i] << " by proxy adjustment of " << result[i]);
    }
    return result;
}

Handle<OptionletVolatilityStructure> ScenarioSimMarket::createNonSimulatedOptionletVol(
    const ext::shared_ptr<OptionletVolatilityStructure>& baseOvs, const string& name)
{
    DLOG("ScenarioSimMarket: building non-simulated optionlet volatility for " << name);
    ReactionToTimeDecay decayMode = parseDecayMode(parameters_->capFloorVolDecayMode());
    return Handle<OptionletVolatilityStructure>(ext::make_shared<DynamicOptionletVolatilityStructure>(
        baseOvs, 0, NullCalendar(), decayMode));
}

Handle<OptionletVolatilityStructure> ScenarioSimMarket::createOptionletVol(RiskFactorKey::KeyType rfKeyType,
    const string& name, bool& simDataWritten, const BuildContext& bc, const ext::shared_ptr<IborIndex>& index,
    const Handle<OptionletVolatilityStructure>& baseOvs, const Period& rateCompPeriod, Stickyness stickyness)
{
    DLOG("ScenarioSimMarket: building simulated optionlet volatility for " << name);

    // Some conventions to help with the creation of the cap floor volatility structure.
    CapFloorConventions conventions = getCapFloorConventions(name, bc.curveConfigs, index);

    // Configured tenors and strikes.
    vector<Period> optionTenors = parameters_->capFloorVolExpiries(name);
    vector<Real> configuredStrikes = parameters_->capFloorVolStrikes(name);
    auto nOptTenors = optionTenors.size();

    // Configued strikes may be empty which indicates that an ATM curve has been configured.
    bool isAtm = false;
    auto strikes = configuredStrikes;
    if (strikes.empty()) {
        QL_REQUIRE(parameters_->capFloorVolIsAtm(name), "ScenarioSimMarket: strikes for " << name <<
            " is empty in simulation parameters so expected its ATM flag to be true.");
        strikes = {0.0};
        isAtm = true;
    }
    auto nStrikes = strikes.size();

    // Get the option dates for the configured tenors.
    vector<Date> optionDates = getOptionDates(optionTenors, index, conventions, *baseOvs, rateCompPeriod, name);

    // Get the ATM strike for each tenor if necessary.
    vector<Rate> atmStrikes;
    if (isAtm) {
        atmStrikes = getAtmStrikes(optionTenors, optionDates, index, conventions, rateCompPeriod, name,
            bc.configuration, bc.initMarket);
    }

    // Elements to be populated in the main loop below.
    vector<vector<Handle<Quote>>> quotes(nOptTenors, vector<Handle<Quote>>(nStrikes, Handle<Quote>()));
    map<RiskFactorKey, ext::shared_ptr<SimpleQuote>> simDataTmp;
    map<RiskFactorKey, Real> absoluteSimDataTmp;

    // Main loop populating the SSM strikes and quotes.
    for (Size i = 0, counter = 0; i < optionTenors.size(); ++i) {
        for (Size j = 0; j < nStrikes; ++j, ++counter) {
            Real strike = isAtm ? atmStrikes[i] : strikes[j];
            Real vol = baseOvs->volatility(optionDates[i], strike, true);
            DLOG("Vol at [date, strike] pair [" << optionDates[i] << ", " << std::fixed
                << std::setprecision(4) << strike << "] is " << std::setprecision(12) << vol);
            auto quote = ext::make_shared<SimpleQuote>(useSpreadedTermStructures_ ? 0.0 : vol);

            simDataTmp.emplace(RiskFactorKey{ rfKeyType, name, counter }, quote);
            if (useSpreadedTermStructures_) {
                absoluteSimDataTmp.emplace(RiskFactorKey{ rfKeyType, name, counter }, vol);
            }

            quotes[i][j] = Handle<Quote>(quote);
        }
    }

    // Generate coordinates.
    vector<vector<Real>> coordinates(2);
    for (const auto& optTenor : optionTenors)
        coordinates[0].push_back(baseOvs->timeFromReference(baseOvs->optionDateFromTenor(optTenor)));
    if (isAtm) {
        // This is what was here before but it does not make sense why we would just use the last ATM strike.
        coordinates[1].push_back(atmStrikes.back());
    } else {
        coordinates[1] = strikes;
    }

    // Store the quotes and coordinates.
    writeSimData(simDataTmp, absoluteSimDataTmp, rfKeyType, name, coordinates);
    simDataWritten = true;

    // If we have sticky moneyness, we will need to pass in the initial market index and current SSM index below.
    ext::shared_ptr<IborIndex> initMktIndex;
    ext::shared_ptr<IborIndex> ssmIndex;
    if (stickyness == StickyMoneyness) {
        auto oreIndexName = IndexNameTranslator::instance().oreName(index->name());
        initMktIndex = *bc.initMarket->iborIndex(oreIndexName, bc.configuration);
        ssmIndex = *iborIndex(oreIndexName, bc.configuration);
    }

    // Create the SSM optionlet volatility structure.
    Handle<OptionletVolatilityStructure> hOvs;
    if (useSpreadedTermStructures_) {
        auto decayMode = parseDecayMode(parameters_->capFloorVolDecayMode());
        hOvs = Handle<OptionletVolatilityStructure>(ext::make_shared<SpreadedOptionletVolatility2>(
            baseOvs, optionDates, strikes, quotes, decayMode, stickyness, ssmIndex, initMktIndex));
    } else {
        // FIXME: Works as of today only e.g. for sensitivity / scenario analysis.
        // TODO: Build floating reference date StrippedOptionlet class for MC path generators.
        auto optionlet = ext::make_shared<QuantLib::StrippedOptionlet>(conventions.settleDays, baseOvs->calendar(),
            baseOvs->businessDayConvention(), index, optionDates, strikes, quotes, baseOvs->dayCounter(),
            baseOvs->volatilityType(), baseOvs->displacement(), baseOvs->useEffectiveVolatility());

        hOvs = Handle<OptionletVolatilityStructure>(
            ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(optionlet));
    }

    return hOvs;
}

Handle<OptionletVolatilityStructure> ScenarioSimMarket::createStickySabrOptionletVol(RiskFactorKey::KeyType rfKeyType,
    const string& name, bool& simDataWritten, const BuildContext& bc, const ext::shared_ptr<IborIndex>& index,
    const Handle<OptionletVolatilityStructure>& baseOvs, const Period& rateCompPeriod,
    const ext::shared_ptr<ProxyOptionletVolatility>& proxy) {

    DLOG("ScenarioSimMarket: building simulated, sticky SABR, optionlet volatility for " << name);

    // We don't continue if the underlying surface is not a SABR surface.
    auto sabrSoab = ext::dynamic_pointer_cast<SabrStrippedOptionletAdapterBase>(*baseOvs);
    QL_REQUIRE(sabrSoab, "ScenarioSimMarket: failed to cast baseOvs to SabrStrippedOptionletAdapterBase for " << name);

    // Make the notation a bit cleaner below.
    using QuoteRow = vector<Handle<Quote>>;
    using QuoteCol = QuoteRow;
    using QuoteMatrix = vector<QuoteRow>;
    using RealRow = vector<Real>;
    using RealMatrix = vector<RealRow>;

    // Configured tenors.
    vector<Period> optionTenors = parameters_->capFloorVolExpiries(name);
    auto nOptTenors = optionTenors.size();

    // We ignore the strikes for sticky SABR. Log a warning if they are configured.
    vector<Real> configuredStrikes = parameters_->capFloorVolStrikes(name);
    if (configuredStrikes.size() > 1 || (configuredStrikes.size() == 1 && !close(configuredStrikes[0], 0.0))) {
        WLOG("ScenarioSimMarket: ignoring configured strikes for sticky SABR optionlet volatility for " << name <<
            ". This will likely lead to missing / incorrect scenarios in the simulation.");
    }

    // Some conventions to help with the creation of the cap floor volatility structure.
    CapFloorConventions conventions = getCapFloorConventions(name, bc.curveConfigs, index);

    // Get the option dates for the configured tenors.
    vector<Date> optionDates = getOptionDates(optionTenors, index, conventions, *baseOvs, rateCompPeriod, name);

    // Get the ATM strike for each tenor.
    vector<Rate> atmStrikes = getAtmStrikes(optionTenors, optionDates, index, conventions, rateCompPeriod, name,
        bc.configuration, bc.initMarket);

    // If the initial market surface was a proxy volatility surface, calculate a proxy adjustment for each option tenor
    // and apply it to the ATM strikes.
    if (proxy) {
        vector<Real> proxyAdjs = getProxyAdjustments(optionTenors, optionDates, proxy);
        for (Size i = 0; i < atmStrikes.size(); ++i)
            atmStrikes[i] += proxyAdjs[i];
    }

    // Elements to be populated in the main loop below.
    map<RiskFactorKey, ext::shared_ptr<SimpleQuote>> simDataTmp;
    map<RiskFactorKey, Real> absoluteSimDataTmp;
    // These quotes will be populated and used below if useSpreadedTermStructures_ is true.
    QuoteCol quotes;
    vector<Time> optionTimes;
    // These quotes will be populated and used below if useSpreadedTermStructures_ is false.
    auto sabrSoabStrikes = sabrSoab->optionletStrikes(0);
    QuoteMatrix atmVolRelQuotes;

    // Main loop populating the SSM quotes.
    for (Size i = 0, counter = 0; i < nOptTenors; ++i, ++counter) {
        Real atmVol = baseOvs->volatility(optionDates[i], atmStrikes[i], true);
        DLOG("ATM vol at [date, strike] pair [" << optionDates[i] << ", " << std::fixed
            << std::setprecision(4) << atmStrikes[i] << "] is " << std::setprecision(12) << atmVol);

        ext::shared_ptr<SimpleQuote> quote;
        if (useSpreadedTermStructures_) {
            quote = ext::make_shared<SimpleQuote>(0.0);
            absoluteSimDataTmp.emplace(RiskFactorKey{ rfKeyType, name, counter }, atmVol);
            quotes.emplace_back(quote);
            optionTimes.push_back(baseOvs->timeFromReference(optionDates[i]));
        } else {
            quote = ext::make_shared<SimpleQuote>(atmVol);
            Handle<Quote> hQuote(quote);
            // Use the strikes from the first optionlet tenor underlying the SABR surface.
            auto& atmVolRelRow = atmVolRelQuotes.emplace_back();
            atmVolRelRow.reserve(sabrSoabStrikes.size());
            for (Size j = 0; j < sabrSoabStrikes.size(); ++j) {
                Rate strike = sabrSoabStrikes[j];
                Real vol = baseOvs->volatility(optionDates[i], strike, true);
                Real volSpread = vol - atmVol;
                DLOG("Vol at [date, strike] pair [" << optionDates[i] << ", " << std::fixed << std::setprecision(4)
                    << strike << "] is " << std::setprecision(12) << vol << " (vol spread = " << volSpread << ")");
                auto atmVolRelQuote = makeDerivedQuotePtr(hQuote, [volSpread](Real x) { return x + volSpread; });
                atmVolRelRow.emplace_back(atmVolRelQuote);
            }
        }

        simDataTmp.emplace(RiskFactorKey{ rfKeyType, name, counter }, quote);
    }

    // Generate coordinates.
    RealMatrix coordinates(2);
    for (const auto& optTenor : optionTenors)
        coordinates[0].push_back(baseOvs->timeFromReference(baseOvs->optionDateFromTenor(optTenor)));
    // This is what was here before but it does not make sense why we would just use the last ATM strike.
    coordinates[1].push_back(atmStrikes.back());

    // Store the quotes and coordinates.
    writeSimData(simDataTmp, absoluteSimDataTmp, rfKeyType, name, coordinates);
    simDataWritten = true;

    // We pass the SSM Ibor index into the SSM SABR surface below for reading / querying volatilities. It will react to 
    // changes in the index's forward curve giving the SABR adjusted delta. If this is not wanted, we could just pass 
    // in the initial market's Ibor index instead in its place.
    auto oreIndexName = IndexNameTranslator::instance().oreName(index->name());
    const auto& initMktIndex = *bc.initMarket->iborIndex(oreIndexName, bc.configuration);
    const auto& ssmIndex = *iborIndex(oreIndexName, bc.configuration);

    // If useSpreadedTermStructures_ is false, we create a new StrippedOptionlet to feed to the SABR surface below.
    // If useSpreadedTermStructures_ is true, we reuse the initial market's SABR StrippedOptionletBase.
    ext::shared_ptr<QuantLib::StrippedOptionlet> optionlet;
    if (!useSpreadedTermStructures_) {
        optionlet = ext::make_shared<QuantLib::StrippedOptionlet>(conventions.settleDays, baseOvs->calendar(),
            baseOvs->businessDayConvention(), ssmIndex, optionDates, sabrSoabStrikes, atmVolRelQuotes,
            baseOvs->dayCounter(), baseOvs->volatilityType(), baseOvs->displacement(),
            baseOvs->useEffectiveVolatility(), atmStrikes);
    }

    // Try to create a SabrStrippedOptionletAdapter for the optionlet above.
    RelinkableHandle<OptionletVolatilityStructure> hOvs;
    bool isSuccess = tryCreateSabrAdapter<Linear, LinearFlat, Cubic, CubicFlat, BackwardFlat>(
        hOvs, optionlet, quotes, optionTimes, *baseOvs, name, initMktIndex, ssmIndex, rateCompPeriod);
    if (!isSuccess) {
        QL_FAIL("ScenarioSimMarket: expected SabrStrippedOptionletAdapter for stickySabr optionlet vol for name "
            << name << ". T0 cap floor vol surface should be of a SABR variant. "
            << "Supported time interpolators are : Linear, LinearFlat, Cubic, CubicFlat, BackwardFlat.");
    }

    // Wrap the optionlet volatility structure if the initial market structure was a proxy volatility structure.
    if (proxy) {
        DLOG("Wrapping simulated vol structure with ProxyOptionletVolatility for " << name);
        hOvs.linkTo(ext::make_shared<ProxyOptionletVolatility>(hOvs, proxy->baseIndex(),
            proxy->targetIndex(), proxy->baseRateComputationPeriod(), proxy->targetRateComputationPeriod(),
            proxy->scalingFactor()));
    }

    return hOvs;
}

} // namespace analytics
} // namespace ore

