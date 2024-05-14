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

void setConventions() {
    QuantLib::ext::shared_ptr<data::Conventions> conventions(new data::Conventions());

    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    QuantLib::ext::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    InstrumentConventions::instance().setConventions(conventions);
}

struct TestData {
    TestData() : referenceDate(30, July, 2015) {
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
        irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
            "EUR", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
            ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

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

        vector<QuantLib::ext::shared_ptr<CalibrationInstrument>> instruments = { 
            QuantLib::ext::make_shared<CpiCapFloor>(CapFloor::Cap, 5 * Years, QuantLib::ext::make_shared<AbsoluteStrike>(0.0))
        };
        vector<CalibrationBasket> cbUkrpi = { CalibrationBasket(instruments) };

        instruments = {
            QuantLib::ext::make_shared<CpiCapFloor>(CapFloor::Floor, 5 * Years, QuantLib::ext::make_shared<AbsoluteStrike>(0.0))
        };
        vector<CalibrationBasket> cbEuhicpxt = { CalibrationBasket(instruments) };

        ReversionParameter reversion(LgmData::ReversionType::Hagan, false,
            ParamType::Piecewise, { 1.0 }, { 0.5, 0.5 });

        VolatilityParameter volatility(LgmData::VolatilityType::Hagan, true, 0.1);

        infConfigs.push_back(QuantLib::ext::make_shared<InfDkData>(CalibrationType::Bootstrap,
            cbUkrpi, "GBP", "UKRPI", reversion, volatility));
        infConfigs.push_back(QuantLib::ext::make_shared<InfDkData>(CalibrationType::Bootstrap,
            cbEuhicpxt, "EUR", "EUHICPXT", reversion, volatility));

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
        cmb.addCorrelation("INF:UKRPI", "IR:GBP", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.1)));
        cmb.addCorrelation("INF:EUHICPXT", "IR:EUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.1)));

        Real tolerance = 0.0001;
        QuantLib::ext::shared_ptr<CrossAssetModelData> config(
            QuantLib::ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, eqConfigs, infConfigs, crLgmConfigs,
                                                    crCirConfigs, comConfigs, 0, cmb.correlations(), tolerance));

        CrossAssetModelBuilder modelBuilder(market, config);
        ccLgm = *modelBuilder.model();

        lgm = QuantLib::ext::make_shared<QuantExt::LGM>(ccLgm->irlgm1f(0));
    }

    SavedSettings backup;
    Date referenceDate;
    QuantLib::ext::shared_ptr<CrossAssetModelData> config;
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> ccLgm;
    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm;
    QuantLib::ext::shared_ptr<ore::data::Market> market;
};

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

void test_lgm(bool sobol, bool antithetic, bool brownianBridge) {

    BOOST_TEST_MESSAGE("call test_lgm with sobol=" << sobol << " antithetic=" << antithetic << " brownianBridge=" << brownianBridge); 
    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years};
    DateGrid grid(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::LGM> model = d.lgm;

    // State process
    QuantLib::ext::shared_ptr<StochasticProcess1D> stateProcess =
        QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(model->stateProcess());

    // Simulation market parameters, we just need the yield curve structure here
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);
    // Multi path generator: Pseudo Random
    BigNatural seed = 42;
    // bool antithetic = true;
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen;
    if (sobol) {
        if (brownianBridge)
            pathGen = QuantLib::ext::make_shared<MultiPathGeneratorSobolBrownianBridge>(stateProcess, grid.timeGrid(),
                                                                                SobolBrownianGenerator::Diagonal, seed);
        else
            pathGen = QuantLib::ext::make_shared<MultiPathGeneratorSobol>(stateProcess, grid.timeGrid(), seed);
    } else
        pathGen =
            QuantLib::ext::make_shared<MultiPathGeneratorMersenneTwister>(stateProcess, grid.timeGrid(), seed, antithetic);

    // Scenario factory
    // We assume different implementations of the scenario objects which are more or less
    // optimized w.r.t. memory usage. Hence we use the scenario factory here to avoid
    // switching in the scenario generator class below.
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
        QuantLib::ext::make_shared<SimpleScenarioFactory>(true);

    // Scenario Generator
    QuantLib::ext::shared_ptr<LgmScenarioGenerator> scenGen =
        QuantLib::ext::make_shared<LgmScenarioGenerator>(model, pathGen, scenarioFactory, simMarketConfig, today, grid);

    // Basic martingale tests
    Size samples = 10000;
    Real eur = 0.0, eur2 = 0.0;
    for (Size i = 0; i < samples; i++) {
        for (Date d : grid.dates()) {
            QuantLib::ext::shared_ptr<Scenario> scenario = scenGen->next(d);
            if (d == grid.dates().back()) { // in 10 years from today
                RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, "EUR", 8);
                Real eur10yDiscount = scenario->get(key);
                Real numeraire = scenario->getNumeraire();
                eur += eur10yDiscount / numeraire;
                eur2 += 1.0 / numeraire;
            }
        }
    }

    eur /= samples;
    eur2 /= samples;

    Real relTolerance = 0.01;
    Real eurExpected = d.market->discountCurve("EUR")->discount(20.);
    BOOST_CHECK_MESSAGE(fabs(eur - eurExpected) / eurExpected < relTolerance,
                        "EUR 20Y Discount mismatch: " << eur << " vs " << eurExpected);
    Real eurExpected2 = d.market->discountCurve("EUR")->discount(10.);
    BOOST_CHECK_MESSAGE(fabs(eur2 - eurExpected2) / eurExpected2 < relTolerance,
                        "EUR 10Y Discount mismatch: " << eur2 << " vs " << eurExpected2);

    BOOST_TEST_MESSAGE("LGM " << (sobol ? "Sobol " : "MersenneTwister ") << (antithetic ? "Antithetic" : "")
                              << (brownianBridge ? "BrownianBridge" : ""));
    BOOST_TEST_MESSAGE("EUR 20Y Discount:        " << eur << " vs " << eurExpected);
    BOOST_TEST_MESSAGE("EUR 10Y Discount:        " << eur2 << " vs " << eurExpected2);
}

BOOST_AUTO_TEST_SUITE(ScenarioGeneratorTest)

BOOST_AUTO_TEST_CASE(testLgmMersenneTwister) {
    BOOST_TEST_MESSAGE("Testing LgmScenarioGenerator with MersenneTwister...");
    setConventions();
    test_lgm(false, false, false);
}

BOOST_AUTO_TEST_CASE(testLgmMersenneTwisterAntithetic) {
    BOOST_TEST_MESSAGE("Testing LgmScenarioGenerator with MersenneTwister/Antithetic...");
    setConventions();
    test_lgm(false, true, false);
}

BOOST_AUTO_TEST_CASE(testLgmLowDiscrepancy) {
    BOOST_TEST_MESSAGE("Testing LgmScenarioGenerator with LowDiscrepancy...");
    setConventions();
    test_lgm(true, false, false);
}

BOOST_AUTO_TEST_CASE(testLgmLowDiscrepancyBrownianBridge) {
    BOOST_TEST_MESSAGE("Testing LgmScenarioGenerator with LowDiscrepancy/BrownianBridge...");
    setConventions();
    test_lgm(true, false, true);
}

void test_crossasset(bool sobol, bool antithetic, bool brownianBridge) {
    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years};
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;

    // State process
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess = model->stateProcess();

    // Simulation market parameters, we just need the yield curve structure here
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);
    simMarketConfig->setZeroInflationTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                                 5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                                 30 * Years, 40 * Years, 50 * Years});

    // Multi path generator
    BigNatural seed = 42;
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(stateProcess)) {
        tmp->resetCache(grid->timeGrid().size() - 1);
    }
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen;
    if (sobol) {
        if (brownianBridge)
            pathGen = QuantLib::ext::make_shared<MultiPathGeneratorSobolBrownianBridge>(stateProcess, grid->timeGrid(),
                                                                                SobolBrownianGenerator::Diagonal, seed);
        else
            pathGen = QuantLib::ext::make_shared<MultiPathGeneratorSobol>(stateProcess, grid->timeGrid(), seed);
    } else
        pathGen =
            QuantLib::ext::make_shared<MultiPathGeneratorMersenneTwister>(stateProcess, grid->timeGrid(), seed, antithetic);

    // Scenario factory
    // We assume different implementations of the scenario objects which are more or less
    // optimized w.r.t. memory usage. Hence we use the scenario factory here to avoid
    // switching in the scenario generator class below.
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);

    // Scenario Generator
    QuantLib::ext::shared_ptr<CrossAssetModelScenarioGenerator> scenGen = QuantLib::ext::make_shared<CrossAssetModelScenarioGenerator>(
        model, pathGen, scenarioFactory, simMarketConfig, today, grid, d.market);

    // Basic martingale tests
    Size samples = 10000;
    Real eur = 0.0, usd = 0.0, gbp = 0.0, eur2 = 0.0, usd2 = 0.0, gbp2 = 0.0, eur3 = 0.0;

    cpu_timer timer;
    for (Size i = 0; i < samples; i++) {
        for (Date d : grid->dates()) {
            QuantLib::ext::shared_ptr<Scenario> scenario = scenGen->next(d);

            if (d == grid->dates().back()) { // in 10 years from today
                RiskFactorKey eurKey(RiskFactorKey::KeyType::DiscountCurve, "EUR", 8);
                RiskFactorKey usdKey(RiskFactorKey::KeyType::DiscountCurve, "USD", 8);
                RiskFactorKey gbpKey(RiskFactorKey::KeyType::DiscountCurve, "GBP", 8);
                RiskFactorKey usdeurKey(RiskFactorKey::KeyType::FXSpot, "USDEUR");
                RiskFactorKey gbpeurKey(RiskFactorKey::KeyType::FXSpot, "GBPEUR");
                RiskFactorKey euhicpKey(RiskFactorKey::KeyType::CPIIndex, "EUHICPXT");

                Real usdeurFX = scenario->get(usdeurKey);
                Real gbpeurFX = scenario->get(gbpeurKey);
                Real numeraire = scenario->getNumeraire();
                Real eur10yDiscount = scenario->get(eurKey);
                Real gbp10yDiscount = scenario->get(gbpKey);
                Real usd10yDiscount = scenario->get(usdKey);
                Real euhicp = scenario->get(euhicpKey);
                eur += eur10yDiscount / numeraire;
                gbp += gbp10yDiscount * gbpeurFX / numeraire;
                usd += usd10yDiscount * usdeurFX / numeraire;
                eur2 += 1.0 / numeraire;
                gbp2 += gbpeurFX / numeraire;
                usd2 += usdeurFX / numeraire;
                eur3 += euhicp / numeraire;
            }
        }
    }
    timer.stop();

    eur /= samples;
    gbp /= samples;
    usd /= samples;
    eur2 /= samples;
    gbp2 /= samples;
    usd2 /= samples;
    eur3 /= samples;

    Real relTolerance = 0.01;
    Real eurExpected = d.market->discountCurve("EUR")->discount(20.);
    BOOST_CHECK_MESSAGE(fabs(eur - eurExpected) / eurExpected < relTolerance,
                        "EUR 20Y Discount mismatch: " << eur << " vs " << eurExpected);
    Real gbpExpected = d.market->fxRate("GBPEUR")->value() * d.market->discountCurve("GBP")->discount(20.);
    BOOST_CHECK_MESSAGE(fabs(gbp - gbpExpected) / gbpExpected < relTolerance,
                        "GBP 20Y Discount mismatch: " << gbp << " vs " << gbpExpected);
    Real usdExpected = d.market->fxRate("USDEUR")->value() * d.market->discountCurve("USD")->discount(20.);
    BOOST_CHECK_MESSAGE(fabs(usd - usdExpected) / usdExpected < relTolerance,
                        "USD 20Y Discount mismatch: " << usd << " vs " << usdExpected);

    Real eurExpected2 = d.market->discountCurve("EUR")->discount(10.);
    BOOST_CHECK_MESSAGE(fabs(eur2 - eurExpected2) / eurExpected2 < relTolerance,
                        "EUR 10Y Discount mismatch: " << eur2 << " vs " << eurExpected2);
    Real gbpExpected2 = d.market->fxRate("GBPEUR")->value() * d.market->discountCurve("GBP")->discount(10.);
    BOOST_CHECK_MESSAGE(fabs(gbp2 - gbpExpected2) / gbpExpected2 < relTolerance,
                        "GBP 10Y Discount mismatch: " << gbp2 << " vs " << gbpExpected2);
    Real usdExpected2 = d.market->fxRate("USDEUR")->value() * d.market->discountCurve("USD")->discount(10.);
    BOOST_CHECK_MESSAGE(fabs(usd2 - usdExpected2) / usdExpected2 < relTolerance,
                        "USD 10Y Discount mismatch: " << usd2 << " vs " << usdExpected2);

    Real eurExpected3 =
        d.market->zeroInflationIndex("EUHICPXT")
            ->fixing(d.market->zeroInflationIndex("EUHICPXT")->zeroInflationTermStructure()->baseDate()) *
        pow(1 + d.market->zeroInflationIndex("EUHICPXT")->zeroInflationTermStructure()->zeroRate(10.), 10) *
        d.market->discountCurve("EUR")->discount(10.);
    BOOST_CHECK_MESSAGE(fabs(eur3 - eurExpected3) / eurExpected3 < relTolerance,
                        "EUHICPXT CPI Rate mismatch: " << eur3 << " vs " << eurExpected3);

    BOOST_TEST_MESSAGE("CrossAssetModel " << (sobol ? "Sobol " : "MersenneTwister ") << (antithetic ? "Antithetic" : "")
                                          << (brownianBridge ? "BrownianBridge" : ""));
    BOOST_TEST_MESSAGE("EUR 20Y Discount:        " << eur << " vs " << eurExpected);
    BOOST_TEST_MESSAGE("GBP 20Y Discount in EUR: " << gbp << " vs " << gbpExpected);
    BOOST_TEST_MESSAGE("USD 20Y Discount in EUR: " << usd << " vs " << usdExpected);
    BOOST_TEST_MESSAGE("EUR 10Y Discount:        " << eur2 << " vs " << eurExpected2);
    BOOST_TEST_MESSAGE("GBP 10Y Discount in EUR: " << gbp2 << " vs " << gbpExpected2);
    BOOST_TEST_MESSAGE("USD 10Y Discount in EUR: " << usd2 << " vs " << usdExpected2);
    BOOST_TEST_MESSAGE("EUHICPXT CPI:  " << eur3 << " vs " << eurExpected3);
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w"));
}

BOOST_AUTO_TEST_CASE(testCrossAssetMersenneTwister) {
    BOOST_TEST_MESSAGE("Testing CrossAssetScenarioGenerator with MersenneTwister...");
    setConventions();
    test_crossasset(false, false, false);
}

BOOST_AUTO_TEST_CASE(testCrossAssetMersenneTwisterAntithetic) {
    BOOST_TEST_MESSAGE("Testing CrossAssetScenarioGenerator with MersenneTwister/Antithetic...");
    setConventions();
    test_crossasset(false, true, false);
}

BOOST_AUTO_TEST_CASE(testCrossAssetLowDiscrepancy) {
    BOOST_TEST_MESSAGE("Testing CrossAssetScenarioGenerator with LowDiscrepancy...");
    setConventions();
    test_crossasset(true, false, false);
}

BOOST_AUTO_TEST_CASE(testCrossAssetLowDiscrepancyBrownianBridge) {
    BOOST_TEST_MESSAGE("Testing CrossAssetScenarioGenerator with LowDiscrepancy/BrownianBridge...");
    setConventions();
    test_crossasset(true, false, true);
}

BOOST_AUTO_TEST_CASE(testCrossAssetSimMarket) {
    BOOST_TEST_MESSAGE("Testing CrossAssetScenarioGenerator via SimMarket (Martingale tests)...");
    setConventions();

    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years};
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;

    // State process
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess = model->stateProcess();

    // Simulation market parameters, we just need the yield curve structure here
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);

    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M"});
    simMarketConfig->interpolation() = "LogLinear";
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});
    simMarketConfig->setCpiIndices({"UKRPI", "EUHICPXT"});

    BOOST_TEST_MESSAGE("set up scenario generator builder");
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = Sobol;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>();
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);

    BOOST_TEST_MESSAGE("set up scenario sim market");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    // Basic Martingale tests
    Size samples = 10000;
    Real eur = 0.0, usd = 0.0, gbp = 0.0, eur2 = 0.0, usd2 = 0.0, gbp2 = 0.0;
    Real eur3 = 0.0, usd3 = 0.0, gbp3 = 0.0;
    int horizon = 10;

    Date d1 = grid->dates().back();
    Date d2 = d1 + horizon * Years;
    Real relTolerance = 0.015;
    Real eurExpected = d.market->discountCurve("EUR")->discount(d2);
    Real eurExpected2 = d.market->discountCurve("EUR")->discount(d1);
    Real gbpExpected = d.market->fxRate("GBPEUR")->value() * d.market->discountCurve("GBP")->discount(d2);
    Real gbpExpected2 = d.market->fxRate("GBPEUR")->value() * d.market->discountCurve("GBP")->discount(d1);
    Real usdExpected = d.market->fxRate("USDEUR")->value() * d.market->discountCurve("USD")->discount(d2);
    Real usdExpected2 = d.market->fxRate("USDEUR")->value() * d.market->discountCurve("USD")->discount(d1);

    cpu_timer timer;
    Real updateTime = 0.0;
    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        for (Date d : grid->dates()) {
            timer.resume();
            simMarket->update(d);
            timer.stop();
            updateTime += timer.elapsed().wall * 1e-9;
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

    eur /= samples;
    gbp /= samples;
    usd /= samples;
    eur2 /= samples;
    gbp2 /= samples;
    usd2 /= samples;
    eur3 /= samples;
    gbp3 /= samples;
    usd3 /= samples;

    BOOST_CHECK_MESSAGE(fabs(eur - eurExpected) / eurExpected < relTolerance,
                        "EUR 20Y Discount mismatch: " << eur << " vs " << eurExpected);
    BOOST_CHECK_MESSAGE(fabs(gbp - gbpExpected) / gbpExpected < relTolerance,
                        "GBP 20Y Discount mismatch: " << gbp << " vs " << gbpExpected);
    BOOST_CHECK_MESSAGE(fabs(usd - usdExpected) / usdExpected < relTolerance,
                        "USD 20Y Discount mismatch: " << usd << " vs " << usdExpected);
    BOOST_CHECK_MESSAGE(fabs(eur3 - eurExpected) / eurExpected < relTolerance,
                        "EUR 20Y Index Discount mismatch: " << eur3 << " vs " << eurExpected);
    BOOST_CHECK_MESSAGE(fabs(gbp3 - gbpExpected) / gbpExpected < relTolerance,
                        "GBP 20Y Index Discount mismatch: " << gbp3 << " vs " << gbpExpected);
    BOOST_CHECK_MESSAGE(fabs(usd3 - usdExpected) / usdExpected < relTolerance,
                        "USD 20Y Index Discount mismatch: " << usd3 << " vs " << usdExpected);
    BOOST_CHECK_MESSAGE(fabs(eur2 - eurExpected2) / eurExpected2 < relTolerance,
                        "EUR 10Y Discount mismatch: " << eur2 << " vs " << eurExpected2);
    BOOST_CHECK_MESSAGE(fabs(gbp2 - gbpExpected2) / gbpExpected2 < relTolerance,
                        "GBP 10Y Discount mismatch: " << gbp2 << " vs " << gbpExpected2);
    BOOST_CHECK_MESSAGE(fabs(usd2 - usdExpected2) / usdExpected2 < relTolerance,
                        "USD 10Y Discount mismatch: " << usd2 << " vs " << usdExpected2);

    BOOST_TEST_MESSAGE("CrossAssetModel via ScenarioSimMarket");
    BOOST_TEST_MESSAGE("EUR " << QuantLib::io::iso_date(d2) << " Discount:        " << eur << " vs " << eurExpected);
    BOOST_TEST_MESSAGE("GBP " << QuantLib::io::iso_date(d2) << " Discount in EUR: " << gbp << " vs " << gbpExpected);
    BOOST_TEST_MESSAGE("USD " << QuantLib::io::iso_date(d2) << " Discount in EUR: " << usd << " vs " << usdExpected);
    BOOST_TEST_MESSAGE("EUR " << QuantLib::io::iso_date(d1) << " Discount:        " << eur2 << " vs " << eurExpected2);
    BOOST_TEST_MESSAGE("GBP " << QuantLib::io::iso_date(d1) << " Discount in EUR: " << gbp2 << " vs " << gbpExpected2);
    BOOST_TEST_MESSAGE("USD " << QuantLib::io::iso_date(d1) << " Discount in EUR: " << usd2 << " vs " << usdExpected2);
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w") << ", update time " << updateTime);
}

BOOST_AUTO_TEST_CASE(testCrossAssetSimMarket2) {
    BOOST_TEST_MESSAGE("Testing CrossAssetScenarioGenerator via SimMarket (direct test against model)...");
    setConventions();
    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years};
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;

    // State process
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess = model->stateProcess();

    // Simulation market parameters, we just need the yield curve structure here
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);

    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M"});
    simMarketConfig->interpolation() = "LogLinear";
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});
    simMarketConfig->setCpiIndices({"UKRPI", "EUHICPXT"});

    BOOST_TEST_MESSAGE("set up scenario generator builder");
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = Sobol;
    sgd->directionIntegers() = SobolRsg::JoeKuoD7;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);
    // QuantLib::ext::shared_ptr<DateGrid> grid = sb.dateGrid();

    BOOST_TEST_MESSAGE("set up scenario sim market");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    // set up model based simulation (mimicking exactly the scenario generator builder above)
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(stateProcess)) {
        tmp->resetCache(grid->timeGrid().size() - 1);
    }
    MultiPathGeneratorSobol pathGen(stateProcess, grid->timeGrid(), 42);

    Size samples = 10000;
    double horizon = 10.0; // sample point for curves
    Real tol0 = 1.0E-10;   // for numeraire, fx spot
    Real tol1 = 1.0E-4;    // for curves (different interpolation, this is 0.1bp in zero yield)

    // manual copy of the initial index curves with fixed reference date (in market, they have floating ref date)
    Handle<YieldTermStructure> eurIndexCurve(
        QuantLib::ext::make_shared<FlatForward>(d.referenceDate, 0.02, ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> usdIndexCurve(
        QuantLib::ext::make_shared<FlatForward>(d.referenceDate, 0.03, ActualActual(ActualActual::ISDA)));
    Handle<YieldTermStructure> gbpIndexCurve(
        QuantLib::ext::make_shared<FlatForward>(d.referenceDate, 0.04, ActualActual(ActualActual::ISDA)));

    cpu_timer timer;

    Real updateTime = 0.0;
    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        Sample<MultiPath> path = pathGen.next();
        Size idx = 0;
        for (Date date : grid->dates()) {
            timer.resume();
            simMarket->update(date);
            timer.stop();
            updateTime += timer.elapsed().wall * 1e-9;
            // compare a sample of the simulated data with a parallel direct run of the model
            // sim market
            Real numeraire = simMarket->numeraire();
            Real usdeurFX = simMarket->fxRate("USDEUR")->value();
            Real gbpeurFX = simMarket->fxRate("GBPEUR")->value();
            Real eurDiscount = simMarket->discountCurve("EUR")->discount(1.0 * horizon);
            Real gbpDiscount = simMarket->discountCurve("GBP")->discount(1.0 * horizon);
            ;
            Real usdDiscount = simMarket->discountCurve("USD")->discount(1.0 * horizon);
            ;
            Real eurIndex = simMarket->iborIndex("EUR-EURIBOR-6M")->forwardingTermStructure()->discount(1.0 * horizon);
            Real gbpIndex = simMarket->iborIndex("GBP-LIBOR-6M")->forwardingTermStructure()->discount(1.0 * horizon);
            ;
            Real usdIndex = simMarket->iborIndex("USD-LIBOR-3M")->forwardingTermStructure()->discount(1.0 * horizon);
            ;
            // model based values
            idx++;
            Real t = grid->timeGrid()[idx];
            Real state_eur = path.value[0][idx];
            Real numeraire_m = model->numeraire(0, t, state_eur);
            Real usdeurFX_m = std::exp(path.value[3][idx]);
            Real gbpeurFX_m = std::exp(path.value[4][idx]);
            Real eurDiscount_m = model->discountBond(0, t, t + 1.0 * horizon, path.value[0][idx]);
            Real usdDiscount_m = model->discountBond(1, t, t + 1.0 * horizon, path.value[1][idx]);
            Real gbpDiscount_m = model->discountBond(2, t, t + 1.0 * horizon, path.value[2][idx]);
            Real eurIndex_m = model->discountBond(0, t, t + 1.0 * horizon, path.value[0][idx], eurIndexCurve);
            Real usdIndex_m = model->discountBond(1, t, t + 1.0 * horizon, path.value[1][idx], usdIndexCurve);
            Real gbpIndex_m = model->discountBond(2, t, t + 1.0 * horizon, path.value[2][idx], gbpIndexCurve);
            BOOST_CHECK_MESSAGE(fabs(numeraire - numeraire_m) < tol0,
                                "numeraire mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                            << numeraire << ", model = " << numeraire_m);
            BOOST_CHECK_MESSAGE(fabs(usdeurFX - usdeurFX_m) < tol0,
                                "usdeurFX mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                           << usdeurFX << ", model = " << usdeurFX_m);
            BOOST_CHECK_MESSAGE(fabs(gbpeurFX - gbpeurFX_m) < tol0,
                                "gbpeurFX mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                           << gbpeurFX << ", model = " << gbpeurFX_m);
            BOOST_CHECK_MESSAGE(fabs(eurDiscount - eurDiscount_m) < tol1,
                                "eurDiscount mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                              << eurDiscount << ", model = " << eurDiscount_m);
            BOOST_CHECK_MESSAGE(fabs(usdDiscount - usdDiscount_m) < tol1,
                                "usdDiscount mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                              << usdDiscount << ", model = " << usdDiscount_m);
            BOOST_CHECK_MESSAGE(fabs(gbpDiscount - gbpDiscount_m) < tol1,
                                "gbpDiscount mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                              << gbpDiscount << ", model = " << gbpDiscount_m);
            BOOST_CHECK_MESSAGE(fabs(eurIndex - eurIndex_m) < tol1,
                                "eurIndex mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                           << eurIndex << ", model = " << eurIndex_m);
            BOOST_CHECK_MESSAGE(fabs(usdIndex - usdIndex_m) < tol1,
                                "usdIndex mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                           << usdIndex << ", model = " << usdIndex_m);
            BOOST_CHECK_MESSAGE(fabs(gbpIndex - gbpIndex_m) < tol1,
                                "gbpIndex mismatch, path " << i << ", grid point " << idx << ", simmarket = "
                                                           << gbpIndex << ", model = " << gbpIndex_m);
        }
    }
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w") << ", update time " << updateTime);
}

BOOST_AUTO_TEST_CASE(testVanillaSwapExposure) {
    BOOST_TEST_MESSAGE("Testing EUR and USD vanilla swap exposure profiles generated with CrossAssetScenarioGenerator");
    setConventions();

    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid;
    for (Size i = 0; i < 20; ++i)
        tenorGrid.push_back((i + 1) * Years);
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;
    model->irlgm1f(0)->shift() = 20.0;

    Size samples = 5000;

    // Simulation market parameters, we just need the yield curve structure here
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);

    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M"});
    simMarketConfig->interpolation() = "LogLinear";
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});
    simMarketConfig->setCpiIndices({"UKRPI", "EUHICPXT"});

    BOOST_TEST_MESSAGE("set up scenario generator builder");
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = SobolBrownianBridge;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);
    // QuantLib::ext::shared_ptr<DateGrid> grid = sb.dateGrid();

    BOOST_TEST_MESSAGE("set up scenario sim market");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    // swaps for expsoure generation

    QuantLib::ext::shared_ptr<VanillaSwap> swap_eur =
        MakeVanillaSwap(20 * Years, *simMarket->iborIndex("EUR-EURIBOR-6M"), 0.02);
    QuantLib::ext::shared_ptr<VanillaSwap> swap_usd = MakeVanillaSwap(20 * Years, *simMarket->iborIndex("USD-LIBOR-3M"), 0.03);

    // swaptions (manual inspection reveals that the expiry
    // dates for usd are identical to eur)
    QuantLib::ext::shared_ptr<PricingEngine> eurLgmSwaptionEngine =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model, 0, simMarket->discountCurve("EUR"));
    QuantLib::ext::shared_ptr<PricingEngine> usdLgmSwaptionEngine =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model, 1, simMarket->discountCurve("USD"));
    std::vector<Real> swaptions_eur, swaptions_usd;
    for (Size i = 1; i <= 19; ++i) {
        QuantLib::ext::shared_ptr<SwapIndex> swapIdx_eur = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(
            (20 - i) * Years, simMarket->iborIndex("EUR-EURIBOR-6M")->forwardingTermStructure(),
            simMarket->discountCurve("EUR"));
        QuantLib::ext::shared_ptr<SwapIndex> swapIdx_usd = QuantLib::ext::make_shared<UsdLiborSwapIsdaFixAm>(
            (20 - i) * Years, simMarket->iborIndex("USD-LIBOR-3M")->forwardingTermStructure(),
            simMarket->discountCurve("USD"));
        QuantLib::ext::shared_ptr<Swaption> swaption_eur =
            MakeSwaption(swapIdx_eur, i * Years, 0.02).withPricingEngine(eurLgmSwaptionEngine);
        QuantLib::ext::shared_ptr<Swaption> swaption_usd =
            MakeSwaption(swapIdx_usd, i * Years, 0.03).withPricingEngine(usdLgmSwaptionEngine);
        swaptions_eur.push_back(swaption_eur->NPV());
        swaptions_usd.push_back(swaption_usd->NPV() * simMarket->fxRate("USDEUR")->value());
    }
    swaptions_eur.push_back(0.0);
    swaptions_usd.push_back(0.0);

    // collect discounted epe
    std::vector<Real> swap_eur_epe(grid->dates().size()), swap_usd_epe(grid->dates().size());

    cpu_timer timer;

    Real updateTime = 0.0;
    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        Size idx = 0;
        for (Date date : grid->dates()) {
            timer.resume();
            simMarket->update(date);
            // do not include the first payments (to be comparable with a standard swaption)
            // i.e. set a settlement lag that kills this payment
            Date settlementDate = date + 10;
            QuantLib::ext::shared_ptr<PricingEngine> swapEngine_eur =
                QuantLib::ext::make_shared<QuantExt::DiscountingSwapEngineMultiCurve>(simMarket->discountCurve("EUR"), true,
                                                                              boost::none, settlementDate, date);
            swap_eur->setPricingEngine(swapEngine_eur);
            QuantLib::ext::shared_ptr<PricingEngine> swapEngine_usd =
                QuantLib::ext::make_shared<QuantExt::DiscountingSwapEngineMultiCurve>(simMarket->discountCurve("USD"), true,
                                                                              boost::none, settlementDate, date);
            swap_usd->setPricingEngine(swapEngine_usd);
            // we do not use the valuation engine, so in case updates are disabled we need to
            // take care of the instrument update ourselves
            swap_eur->update();
            swap_usd->update();
            timer.stop();
            updateTime += timer.elapsed().wall * 1e-9;
            Real numeraire = simMarket->numeraire();
            Real usdeurFX = simMarket->fxRate("USDEUR")->value();
            // swap
            swap_eur_epe[idx] += std::max(swap_eur->NPV(), 0.0) / numeraire;
            swap_usd_epe[idx] += std::max(swap_usd->NPV(), 0.0) * usdeurFX / numeraire;
            idx++;
        }
    }
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w") << ", update time " << updateTime);

    // compute summary statistics for swap
    Real tol_eur = 4.0E-4, tol_usd = 13.0E-4;
    for (Size i = 0; i < swap_eur_epe.size(); ++i) {
        Real t = grid->timeGrid()[i + 1];
        swap_eur_epe[i] /= samples;
        swap_usd_epe[i] /= samples;
        // BOOST_TEST_MESSAGE(t << " " << swap_eur_epe[i] << " "
        //                      << swaptions_eur[i] << " " << swap_usd_epe[i]
        //                      << " " << swaptions_usd[i]);
        BOOST_CHECK_MESSAGE(fabs(swap_eur_epe[i] - swaptions_eur[i]) < tol_eur,
                            "discounted EUR swap epe at t=" << t << " (" << swap_eur_epe[i]
                                                            << ") inconsistent to analytical swaption premium ("
                                                            << swaptions_eur[i] << "), tolerance is " << tol_eur);
        BOOST_CHECK_MESSAGE(fabs(swap_usd_epe[i] - swaptions_usd[i]) < tol_usd,
                            "discounted USD swap epe at t=" << t << " (" << swap_usd_epe[i]
                                                            << ") inconsistent to analytical swaption premium ("
                                                            << swaptions_usd[i] << "), tolerance is " << tol_usd);
    }
}

BOOST_AUTO_TEST_CASE(testFxForwardExposure) {
    BOOST_TEST_MESSAGE("Testing EUR-USD FX Forward and FX Vanilla Option exposure");
    setConventions();

    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;

    // Simulation market parameters
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
 
    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M"});
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxVolExpiries("",
        vector<Period>{6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setFxVolDecayMode(string("ForwardVariance"));
    simMarketConfig->setFxVolCcyPairs({"USDEUR"});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);
    simMarketConfig->setCpiIndices({"UKRPI", "EUHICPXT"});

    BOOST_TEST_MESSAGE("set up scenario generator builder");
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = SobolBrownianBridge;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);
    // QuantLib::ext::shared_ptr<DateGrid> grid = sb.dateGrid();

    BOOST_TEST_MESSAGE("set up scenario sim market");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    Size samples = 5000;

    // fx forward for expsoure generation (otm) and engine
    QuantLib::ext::shared_ptr<FxForward> fxfwd =
        QuantLib::ext::make_shared<FxForward>(1.0, EURCurrency(), 1.3, USDCurrency(), grid->dates().back() + 1, false);
    QuantLib::ext::shared_ptr<PricingEngine> fxFwdEngine =
        QuantLib::ext::make_shared<DiscountingFxForwardEngine>(EURCurrency(), simMarket->discountCurve("EUR"), USDCurrency(),
                                                       simMarket->discountCurve("USD"), simMarket->fxRate("USDEUR"));
    fxfwd->setPricingEngine(fxFwdEngine);

    // fx option as reference
    QuantLib::ext::shared_ptr<VanillaOption> fxOption =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, 1.0 / 1.3),
                                          QuantLib::ext::make_shared<EuropeanExercise>(grid->dates().back()));
    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> modelEngine =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(model, 0);
    fxOption->setPricingEngine(modelEngine);
    Real refNpv = fxOption->NPV() * 1.3;

    // fx option for simulation
    QuantLib::ext::shared_ptr<VanillaOption> fxOptionSim =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, 1.0 / 1.3),
                                          QuantLib::ext::make_shared<EuropeanExercise>(grid->dates().back() + 1));
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> simGbm =
        QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(simMarket->fxRate("USDEUR"), simMarket->discountCurve("USD"),
                                                           simMarket->discountCurve("EUR"), simMarket->fxVol("USDEUR"));
    QuantLib::ext::shared_ptr<AnalyticEuropeanEngine> fxOptionEngine = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(simGbm);
    fxOptionSim->setPricingEngine(fxOptionEngine);

    // collect discounted epe
    Real fxfwd_epe = 0.0, fxoption_epe = 0.0;
    cpu_timer timer;

    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        for (Date date : grid->dates()) {
            simMarket->update(date);
            // we do not use the valuation engine, so in case updates are disabled we need to
            // take care of the instrument update ourselves
            fxfwd->update();
            fxOptionSim->update();
            Real numeraire = simMarket->numeraire();
            if (date == grid->dates().back()) {
                fxfwd_epe += std::max(fxfwd->NPV(), 0.0) / numeraire; // NPV is in EUR already by engine construction
                fxoption_epe += fxOptionSim->NPV() * 1.3 / numeraire;
            }
        }
    }
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w"));

    // compute summary statistics for swap
    Real tol = 1.5E-4;
    fxfwd_epe /= samples;
    fxoption_epe /= samples;
    BOOST_TEST_MESSAGE("FxForward discounted epe = " << fxfwd_epe << " FxOption discounted epe = " << fxoption_epe
                                                     << " FxOption npv = " << refNpv << " difference fwd/ref is "
                                                     << (fxfwd_epe - refNpv) << " difference fxoption/ref is "
                                                     << (fxoption_epe - refNpv));
    BOOST_CHECK_MESSAGE(fabs(fxfwd_epe - refNpv) < tol,
                        "discounted FxForward epe (" << fxfwd_epe << ") inconsistent to analytical FxOption premium ("
                                                     << refNpv << "), tolerance is " << tol);
    BOOST_CHECK_MESSAGE(fabs(fxoption_epe - refNpv) < tol,
                        "discounted FxOption epe (" << fxoption_epe << ") inconsistent to analytical FxOption premium ("
                                                    << refNpv << "), tolerance is " << tol);
}

BOOST_AUTO_TEST_CASE(testFxForwardExposureZeroIrVol) {
    BOOST_TEST_MESSAGE("Testing EUR-USD FX Forward exposure (zero IR vol)");
    setConventions();

    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;

    // set ir vols to zero
    for (Size j = 0; j < 3; ++j) {
        for (Size i = 0; i < model->irlgm1f(j)->parameter(0)->size(); ++i) {
            model->irlgm1f(j)->parameter(0)->setParam(i, 0.0);
        }
    }
    model->update();

    // Simulation market parameters
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);

    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M"});
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});
    simMarketConfig->setCpiIndices({"UKRPI", "EUHICPXT"});

    BOOST_TEST_MESSAGE("set up scenario generator builder");
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = SobolBrownianBridge;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);
    // QuantLib::ext::shared_ptr<DateGrid> grid = sb.dateGrid();

    BOOST_TEST_MESSAGE("set up scenario sim market");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    Size samples = 10000;

    // fx forward for expsoure generation (otm) and engine
    Date maturity = grid->dates().back() + 1; // make sure the option is live on last grid date
    QuantLib::ext::shared_ptr<FxForward> fxfwd =
        QuantLib::ext::make_shared<FxForward>(1.0, EURCurrency(), 1.3, USDCurrency(), maturity, false);
    QuantLib::ext::shared_ptr<PricingEngine> fxFwdEngine =
        QuantLib::ext::make_shared<DiscountingFxForwardEngine>(EURCurrency(), simMarket->discountCurve("EUR"), USDCurrency(),
                                                       simMarket->discountCurve("USD"), simMarket->fxRate("USDEUR"));
    fxfwd->setPricingEngine(fxFwdEngine);

    // fx (forward) options as reference
    // note that we set the IR vols to zero, so that we can
    // use a simple adjustment of strike an notional
    std::vector<Real> refNpv;
    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> modelEngine =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(model, 0);
    for (Size i = 0; i < grid->dates().size(); ++i) {
        // amend strike and nominal for forward option pricing
        Date expiry = grid->dates()[i];
        Real domFwdDf =
            simMarket->discountCurve("EUR")->discount(maturity) / simMarket->discountCurve("EUR")->discount(expiry);
        Real forFwdDf =
            simMarket->discountCurve("USD")->discount(maturity) / simMarket->discountCurve("USD")->discount(expiry);
        Real strike = 1.0 / 1.3 * domFwdDf / forFwdDf;
        Real nominal = 1.3 * forFwdDf;
        QuantLib::ext::shared_ptr<VanillaOption> fxOption = QuantLib::ext::make_shared<VanillaOption>(
            QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, strike), QuantLib::ext::make_shared<EuropeanExercise>(expiry));
        fxOption->setPricingEngine(modelEngine);
        refNpv.push_back(fxOption->NPV() * nominal);
    }

    // collect discounted epe
    std::vector<Real> fxfwd_epe(grid->dates().size(), 0.0);
    cpu_timer timer;

    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        Size idx = 0;
        for (Date date : grid->dates()) {
            simMarket->update(date);
            // we do not use the valuation engine, so in case updates are disabled we need to
            // take care of the instrument update ourselves
            fxfwd->update();
            Real numeraire = simMarket->numeraire();
            fxfwd_epe[idx++] += std::max(fxfwd->NPV(), 0.0) / numeraire; // NPV is in EUR already by engine construction
        }
    }
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w"));

    // compute summary statistics for swap
    Real tol = 3.0E-4;
    for (Size i = 0; i < fxfwd_epe.size(); ++i) {
        fxfwd_epe[i] /= samples;
        BOOST_TEST_MESSAGE("FxForward at t=" << grid->times()[i] << " depe = " << fxfwd_epe[i] << " FxOption npv = "
                                             << refNpv[i] << " difference is " << (fxfwd_epe[i] - refNpv[i]));
        BOOST_CHECK_MESSAGE(fabs(fxfwd_epe[i] - refNpv[i]) < tol,
                            "discounted FxForward epe ("
                                << fxfwd_epe[i] << ") inconsistent to analytical FxOption premium (" << refNpv[i]
                                << "), tolerance is " << tol);
    }
}

BOOST_AUTO_TEST_CASE(testCpiSwapExposure) {
    BOOST_TEST_MESSAGE("Testing CPI Swap exposure");
    setConventions();

    TestData d;

    // Simulation date grid
    Date today = d.referenceDate;
    std::vector<Period> tenorGrid = {5 * Years};
    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid);

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = d.ccLgm;

    // set ir vols to zero
    for (Size j = 0; j < 3; ++j) {
        for (Size i = 0; i < model->irlgm1f(j)->parameter(0)->size(); ++i) {
            model->irlgm1f(j)->parameter(0)->setParam(i, 0.0);
        }
    }
    for (Size k = 0; k < 2; ++k) {
        for (Size i = 0; i < model->infdk(k)->parameter(0)->size(); ++i) {
            model->infdk(k)->parameter(0)->setParam(i, 0.0);
        }
    }

    model->update();

    // Simulation market parameters
    BOOST_TEST_MESSAGE("set up sim market parameters");
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);
    simMarketConfig->setSimulateEquityVols(false);
    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD", "GBP"});
    simMarketConfig->setIndices({"EUR-EURIBOR-6M"});
    simMarketConfig->setSwapVolExpiries("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});
    simMarketConfig->setFxCcyPairs({"USDEUR", "GBPEUR"});
    simMarketConfig->setZeroInflationIndices({"UKRPI", "EUHICPXT"});
    simMarketConfig->setZeroInflationTenors("", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years,
                                                 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years});
    simMarketConfig->setCpiIndices({"UKRPI", "EUHICPXT"});

    BOOST_TEST_MESSAGE("set up scenario generator builder");
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = SobolBrownianBridge;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, d.market);

    BOOST_TEST_MESSAGE("set up scenario sim market");
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(d.market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    Size samples = 5000;

    Date maturity = grid->dates().back() + 1; // make sure the option is live on last grid date

    Handle<ZeroInflationIndex> infIndex = simMarket->zeroInflationIndex("EUHICPXT");
    Real baseCPI = infIndex->fixing(infIndex->zeroInflationTermStructure()->baseDate());

    Schedule cpiSchedule(vector<Date>{maturity});
    Leg cpiLeg = QuantLib::CPILeg(cpiSchedule, infIndex.currentLink(), baseCPI, 2 * Months)
                     .withFixedRates(1)
                     .withNotionals(1)
                     .withObservationInterpolation(CPI::Flat)
                     .withPaymentDayCounter(ActualActual(ActualActual::ISDA))
                     .withPaymentAdjustment(Following)
                     .withSubtractInflationNominal(true);

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());

    QuantLib::ext::shared_ptr<QuantLib::Swap> cpiSwap =
        QuantLib::ext::make_shared<QuantLib::Swap>(vector<Leg>{cpiLeg}, vector<bool>{false});
    auto dscEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(simMarket->discountCurve("EUR"));
    cpiSwap->setPricingEngine(dscEngine);

    // cpi floor options as reference
    // note that we set the IR vols to zero, so that we can
    // use a simple adjustment of strike an notional
    QuantLib::ext::shared_ptr<AnalyticDkCpiCapFloorEngine> modelEngine =
        QuantLib::ext::make_shared<AnalyticDkCpiCapFloorEngine>(model, 1, baseCPI);

    QuantLib::ext::shared_ptr<CPICapFloor> cap = QuantLib::ext::make_shared<CPICapFloor>(
        Option::Type::Call, 1.0, today, baseCPI, maturity, infIndex->fixingCalendar(), ModifiedFollowing,
        infIndex->fixingCalendar(), ModifiedFollowing, 0.0, infIndex, 2 * Months, CPI::Flat);
    cap->setPricingEngine(modelEngine);
    Real capNpv = cap->NPV();

    // collect discounted epe
    Real cpiSwap_epe = 0.0;
    cpu_timer timer;
    BOOST_TEST_MESSAGE("running " << samples << " samples simulation over " << grid->dates().size() << " time steps");
    for (Size i = 0; i < samples; i++) {
        simMarket->update(grid->dates().back());
        // we do not use the valuation engine, so in case updates are disabled we need to
        // take care of the instrument update ourselves
        cpiSwap->update();
        Real numeraire = simMarket->numeraire();
        cpiSwap_epe += std::max(cpiSwap->NPV(), 0.0) / numeraire;

        simMarket->fixingManager()->reset();
    }
    BOOST_TEST_MESSAGE("Simulation time " << timer.format(default_places, "%w"));

    // compute summary statistics for swap
    Real tol = 3.0E-4;
    cpiSwap_epe /= samples;
    BOOST_TEST_MESSAGE("CPI Swap at t=" << grid->dates().back() << " epe = " << cpiSwap_epe
                                        << " CPI Cap epe = " << capNpv << " difference is " << (cpiSwap_epe - capNpv));
    BOOST_CHECK_MESSAGE(fabs(cpiSwap_epe - capNpv) < tol,
                        "discounted CPI Swap epe (" << cpiSwap_epe << ") inconsistent to analytical CPI Cap premium ("
                                                    << capNpv << "), tolerance is " << tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
