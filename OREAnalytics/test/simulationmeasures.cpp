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

#include <boost/test/unit_test.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/lgmscenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>

#include <qle/instruments/fxforward.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/all.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/indexes/swap/usdliborswap.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include "testmarket.hpp"

#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore::analytics;
using namespace ore::data;
using namespace ore;
using boost::timer::cpu_timer;
using boost::timer::default_places;
using testsuite::TestMarket;

namespace {

QuantLib::ext::shared_ptr<data::Conventions> convs() {
    QuantLib::ext::shared_ptr<data::Conventions> conventions(new data::Conventions());

    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    QuantLib::ext::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    InstrumentConventions::instance().setConventions(conventions);
    
    return conventions;
}

struct TestData {
    TestData(std::string measure, Real shiftHorizon = 0.0) : referenceDate(30, July, 2015) {
        Settings::instance().evaluationDate() = referenceDate;

        // Build test market
        market = QuantLib::ext::make_shared<TestMarket>(referenceDate);

        // Build IR configurations
        CalibrationType calibrationType = CalibrationType::Bootstrap;
        LgmData::ReversionType revType = LgmData::ReversionType::HullWhite;
        LgmData::VolatilityType volType = LgmData::VolatilityType::Hagan;
        vector<string> swaptionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y", "15Y", "20Y", "30Y"};
        vector<string> swaptionTerms = {"5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y"};
        vector<string> swaptionStrikes(swaptionExpiries.size(), "ATM");
        vector<Time> hTimes = {};
        vector<Time> aTimes = {};

        std::vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs;

        vector<Real> hValues = {0.02};
        vector<Real> aValues = {0.08};
        irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>("EUR", calibrationType, revType, volType, false,
                                                          ParamType::Constant, hTimes, hValues, true,
                                                          ParamType::Piecewise, aTimes, aValues, shiftHorizon, 1.0,
                                                          swaptionExpiries, swaptionTerms, swaptionStrikes));

        hValues = {0.03};
        aValues = {0.009};
        irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
            "USD", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
            ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

        hValues = {0.04};
        aValues = {0.01};
        irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
            "GBP", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
            ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

        // Compile FX configurations
        vector<string> optionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y"};
        vector<string> optionStrikes(optionExpiries.size(), "ATMF");
        vector<Time> sigmaTimes = {};

        std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs;

        vector<Real> sigmaValues = {0.15};
        fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("USD", "EUR", calibrationType, true, ParamType::Piecewise,
                                                         sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

        sigmaValues = {0.15};
        fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("GBP", "EUR", calibrationType, true, ParamType::Piecewise,
                                                         sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

        std::vector<QuantLib::ext::shared_ptr<EqBsData>> eqConfigs;
        // Inflation configurations
        vector<QuantLib::ext::shared_ptr<InflationModelData>> infConfigs;
        // Credit configs
        std::vector<QuantLib::ext::shared_ptr<CrLgmData>> crLgmConfigs;
        std::vector<QuantLib::ext::shared_ptr<CrCirData>> crCirConfigs;

        std::vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>> comConfigs;

        CorrelationMatrixBuilder cmb;
        cmb.addCorrelation("IR:EUR", "IR:USD", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6)));
        cmb.addCorrelation("IR:EUR", "IR:GBP", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.3)));
        cmb.addCorrelation("IR:USD", "IR:GBP", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.1)));
        cmb.addCorrelation("FX:USDEUR", "FX:GBPEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.3)));
        cmb.addCorrelation("IR:EUR", "FX:USDEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.2)));
        cmb.addCorrelation("IR:EUR", "FX:GBPEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.3)));
        cmb.addCorrelation("IR:USD", "FX:USDEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(-0.2)));
        cmb.addCorrelation("IR:USD", "FX:GBPEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(-0.1)));
        cmb.addCorrelation("IR:GBP", "FX:USDEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)));
        cmb.addCorrelation("IR:GBP", "FX:GBPEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.1)));

        Real tolerance = 1e-4;
        QuantLib::ext::shared_ptr<CrossAssetModelData> config1(QuantLib::ext::make_shared<CrossAssetModelData>(
            irConfigs, fxConfigs, eqConfigs, infConfigs, crLgmConfigs, crCirConfigs, comConfigs, 0, cmb.correlations(),
            tolerance, measure, CrossAssetModel::Discretization::Exact));
        QuantLib::ext::shared_ptr<CrossAssetModelData> config2(QuantLib::ext::make_shared<CrossAssetModelData>(
            irConfigs, fxConfigs, eqConfigs, infConfigs, crLgmConfigs, crCirConfigs, comConfigs, 0, cmb.correlations(),
            tolerance, measure, CrossAssetModel::Discretization::Euler));

        CrossAssetModelBuilder modelBuilder1(market, config1);
        ccLgmExact = *modelBuilder1.model();

        CrossAssetModelBuilder modelBuilder2(market, config2);
        ccLgmEuler = *modelBuilder2.model();

        lgm = QuantLib::ext::make_shared<QuantExt::LGM>(ccLgmExact->irlgm1f(0));
    }

    SavedSettings backup;
    Date referenceDate;
    QuantLib::ext::shared_ptr<CrossAssetModelData> config;
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> ccLgmExact, ccLgmEuler;
    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm;
    QuantLib::ext::shared_ptr<ore::data::Market> market;
};

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SimulationMeasuresTest)

void test_measure(std::string measureName, Real shiftHorizon, std::string discName) {

    BOOST_TEST_MESSAGE("Testing market simulation, measure " << measureName << ", horizon " << shiftHorizon
                                                             << ", discretization " << discName);

    TestData d(measureName, shiftHorizon);

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid;
    if (discName == "exact")
        tenorGrid = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years};
    else {
        for (Size i = 1; i <= 60; ++i)
            tenorGrid.push_back(i * 2 * Months);
    }
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = discName == "exact" ? d.ccLgmExact : d.ccLgmEuler;

    // Simulation market parameters, we just need the yield curve structure here
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);

    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M"});
    simMarketConfig->interpolation() = "LogLinear";
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});

    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = Sobol;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);

    convs();
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    // Basic Martingale tests
    Size samples = 5000;
    Real eur = 0.0, usd = 0.0, gbp = 0.0, eur2 = 0.0, usd2 = 0.0, gbp2 = 0.0;
    Real eur3 = 0.0, usd3 = 0.0, gbp3 = 0.0;
    int horizon = 10;

    Date d1 = grid->dates().back();
    Date d2 = d1 + horizon * Years;
    Real relTolerance = 0.01;
    Real eurExpected = d.market->discountCurve("EUR")->discount(d2);
    Real eurExpected2 = d.market->discountCurve("EUR")->discount(d1);
    Real gbpExpected = d.market->fxRate("GBPEUR")->value() * d.market->discountCurve("GBP")->discount(d2);
    Real gbpExpected2 = d.market->fxRate("GBPEUR")->value() * d.market->discountCurve("GBP")->discount(d1);
    Real usdExpected = d.market->fxRate("USDEUR")->value() * d.market->discountCurve("USD")->discount(d2);
    Real usdExpected2 = d.market->fxRate("USDEUR")->value() * d.market->discountCurve("USD")->discount(d1);

    cpu_timer timer, timer2;
    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        for (Date d : grid->dates()) {
            timer.resume();
            simMarket->update(d);
            timer.stop();
            if (d == grid->dates().back()) {
                Real numeraire = simMarket->numeraire();
                Real usdeurFX = simMarket->fxRate("USDEUR")->value();
                Real gbpeurFX = simMarket->fxRate("GBPEUR")->value();
                Real eurDiscount = simMarket->discountCurve("EUR")->discount(1.0 * horizon);
                Real gbpDiscount = simMarket->discountCurve("GBP")->discount(1.0 * horizon);
                ;
                Real usdDiscount = simMarket->discountCurve("USD")->discount(1.0 * horizon);
                ;
                Real eurIndex =
                    simMarket->iborIndex("EUR-EURIBOR-6M")->forwardingTermStructure()->discount(1.0 * horizon);
                Real gbpIndex =
                    simMarket->iborIndex("GBP-LIBOR-6M")->forwardingTermStructure()->discount(1.0 * horizon);
                ;
                Real usdIndex =
                    simMarket->iborIndex("USD-LIBOR-3M")->forwardingTermStructure()->discount(1.0 * horizon);
                ;
                eur += eurDiscount / numeraire;
                gbp += gbpDiscount * gbpeurFX / numeraire;
                usd += usdDiscount * usdeurFX / numeraire;
                eur2 += 1.0 / numeraire;
                gbp2 += gbpeurFX / numeraire;
                usd2 += usdeurFX / numeraire;
                eur3 += eurIndex / numeraire;
                gbp3 += gbpIndex * gbpeurFX / numeraire;
                usd3 += usdIndex * usdeurFX / numeraire;
            }
        }
    }

    timer2.stop();

    eur /= samples;
    gbp /= samples;
    usd /= samples;
    eur2 /= samples;
    gbp2 /= samples;
    usd2 /= samples;
    eur3 /= samples;
    gbp3 /= samples;
    usd3 /= samples;

    Real eurDiff = fabs(eur - eurExpected) / eurExpected;
    BOOST_CHECK_MESSAGE(eurDiff < relTolerance, "EUR 20Y Discount mismatch: " << eur << " vs " << eurExpected);

    Real gbpDiff = fabs(gbp - gbpExpected) / gbpExpected;
    BOOST_CHECK_MESSAGE(gbpDiff < relTolerance,
                        "GBP 20Y Discount mismatch: " << gbp << " vs " << gbpExpected << " (" << gbpDiff << ")");

    Real usdDiff = fabs(usd - usdExpected) / usdExpected;
    BOOST_CHECK_MESSAGE(usdDiff < relTolerance, "USD 20Y Discount mismatch: " << usd << " vs " << usdExpected);

    Real eur3Diff = fabs(eur3 - eurExpected) / eurExpected;
    BOOST_CHECK_MESSAGE(eur3Diff < relTolerance, "EUR 20Y Index Discount mismatch: " << eur3 << " vs " << eurExpected);

    Real gbp3Diff = fabs(gbp3 - gbpExpected) / gbpExpected;
    BOOST_CHECK_MESSAGE(gbp3Diff < relTolerance, "GBP 20Y Index Discount mismatch: " << gbp3 << " vs " << gbpExpected);

    Real usd3Diff = fabs(usd3 - usdExpected) / usdExpected;
    BOOST_CHECK_MESSAGE(usd3Diff < relTolerance, "USD 20Y Index Discount mismatch: " << usd3 << " vs " << usdExpected);

    Real eur2Diff = fabs(eur2 - eurExpected2) / eurExpected2;
    BOOST_CHECK_MESSAGE(eur2Diff < relTolerance, "EUR 10Y Discount mismatch: " << eur2 << " vs " << eurExpected2);

    Real gbp2Diff = fabs(gbp2 - gbpExpected2) / gbpExpected2;
    BOOST_CHECK_MESSAGE(gbp2Diff < relTolerance, "GBP 10Y Discount mismatch: " << gbp2 << " vs " << gbpExpected2);

    Real usd2Diff = fabs(usd2 - usdExpected2) / usdExpected2;
    BOOST_CHECK_MESSAGE(usd2Diff < relTolerance, "USD 10Y Discount mismatch: " << usd2 << " vs " << usdExpected2);

    BOOST_TEST_MESSAGE("CrossAssetModel via ScenarioSimMarket");
    BOOST_TEST_MESSAGE("EUR " << QuantLib::io::iso_date(d2) << " Discount:        " << eur << " vs " << eurExpected
                              << " (" << eurDiff << ")");
    BOOST_TEST_MESSAGE("GBP " << QuantLib::io::iso_date(d2) << " Discount in EUR: " << gbp << " vs " << gbpExpected
                              << " (" << gbpDiff << ")");
    BOOST_TEST_MESSAGE("USD " << QuantLib::io::iso_date(d2) << " Discount in EUR: " << usd << " vs " << usdExpected
                              << " (" << usdDiff << ")");
    BOOST_TEST_MESSAGE("EUR " << QuantLib::io::iso_date(d1) << " Discount:        " << eur2 << " vs " << eurExpected2
                              << " (" << eur2Diff << ")");
    BOOST_TEST_MESSAGE("GBP " << QuantLib::io::iso_date(d1) << " Discount in EUR: " << gbp2 << " vs " << gbpExpected2
                              << " (" << gbp2Diff << ")");
    BOOST_TEST_MESSAGE("USD " << QuantLib::io::iso_date(d1) << " Discount in EUR: " << usd2 << " vs " << usdExpected2
                              << " (" << usd2Diff << ")");
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w") << ", total "
                                          << timer2.format(default_places, "%w"));
}

BOOST_AUTO_TEST_CASE(testLgmExact) { test_measure("LGM", 0.0, "exact"); }

// BOOST_AUTO_TEST_CASE(testLgmEuler) { test_measure("LGM", 0.0, "euler"); }

BOOST_AUTO_TEST_CASE(testFwdExact) { test_measure("LGM", 30.0, "exact"); }

// BOOST_AUTO_TEST_CASE(testFwdEuler) { test_measure("LGM", 30.0, "euler"); }

BOOST_AUTO_TEST_CASE(testBaExact) { test_measure("BA", 0.0, "exact"); }

BOOST_AUTO_TEST_CASE(testBaEuler) { test_measure("BA", 0.0, "euler"); }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
