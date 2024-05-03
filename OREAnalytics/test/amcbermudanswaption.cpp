/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include "oreatoplevelfixture.hpp"

#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/lgmscenariogenerator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>

#include <orea/engine/amcvaluationengine.hpp>

#include <qle/instruments/fxforward.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

#include <qle/pricingengines/mclgmswaptionengine.hpp>

#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/all.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/indexes/swap/usdliborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <boost/timer/timer.hpp>

#include "testmarket.hpp"

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore::analytics;
using namespace ore::data;

struct TestData : ore::test::OreaTopLevelFixture {
    TestData() : referenceDate(30, July, 2015) {
        ObservationMode::instance().setMode(ObservationMode::Mode::None);
        Settings::instance().evaluationDate() = referenceDate;
        
        // Build test market
        market = QuantLib::ext::make_shared<testsuite::TestMarket>(referenceDate);

        // Build IR configurations
        CalibrationType calibrationType = CalibrationType::None;
        LgmData::ReversionType revType = LgmData::ReversionType::HullWhite;
        LgmData::VolatilityType volType = LgmData::VolatilityType::HullWhite;
        vector<string> swaptionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y", "15Y", "20Y", "30Y"};
        vector<string> swaptionTerms = {"5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y"};
        vector<string> swaptionStrikes(swaptionExpiries.size(), "ATM");
        vector<Time> hTimes = {};
        vector<Time> aTimes = {};

        std::vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs;

        vector<Real> hValues = {0.02}; // reversion
        vector<Real> aValues = {0.01}; // HW vol
        irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
            "EUR", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
            ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

        hValues = {0.012};  // reversion
        aValues = {0.0075}; // HW vol
        irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
            "USD", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
            ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

        // Compile FX configurations
        vector<string> optionExpiries = {};
        vector<string> optionStrikes = {};
        vector<Time> sigmaTimes = {};

        std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs;
        vector<Real> sigmaValues = {0.15};
        fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("USD", "EUR", calibrationType, false, ParamType::Constant,
                                                         sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

        CorrelationMatrixBuilder cmb;
        cmb.addCorrelation("IR:EUR", "IR:USD", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.5)));
        cmb.addCorrelation("IR:EUR", "FX:USDEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6)));
        cmb.addCorrelation("IR:USD", "FX:USDEUR", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.7)));

        // CAM configuration
        QuantLib::ext::shared_ptr<CrossAssetModelData> config(
            QuantLib::ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, cmb.correlations()));

        // build CAM and marginal LGM models
        CrossAssetModelBuilder modelBuilder(market, config);
        ccLgm = *modelBuilder.model();
        lgm_eur = QuantLib::ext::make_shared<QuantExt::LGM>(ccLgm->irlgm1f(0));
        lgm_usd = QuantLib::ext::make_shared<QuantExt::LGM>(ccLgm->irlgm1f(1));
    }

    Date referenceDate;
    QuantLib::ext::shared_ptr<ore::data::Conventions> conventions_;
    QuantLib::ext::shared_ptr<CrossAssetModelData> config;
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> ccLgm;
    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm_eur, lgm_usd;
    QuantLib::ext::shared_ptr<ore::data::Market> market;
};

struct TestCase {
    const char* label;                            // label of test case
    Real tolerance;                               // tolerance requirement
    bool isPhysical;                              // physical settlement (otherwise it is cash settled)
    Size gridEvalEachNth;                         // evaluate grid engine each nth sim point
    bool fineGrid;                                // monthly grid instead of default semiannually?
    bool inBaseCcy;                               // if true simulate deal in EUR = base ccy, otherwise in USD
    Size numExercises;                            // number of exercises, yearly, starting at 10y
    Size swapLen;                                 // swap len in years, starting at 10y
    bool isAmortising;                            // is swap amortising?
    Size simYears;                                // number of years to cover in simulation
    Real horizonShift;                            // horizon shift to be applied to CAM, AMC LGM and Grid LGM models
    Size samples;                                 // EPE simulation samples
    Size trainingPaths;                           // training paths
    Real sx;                                      // numerical lgm engine number of std devs
    Size nx;                                      // nuermical lgm engine number of points per std dev
    std::vector<std::vector<Real>> cachedResults; // results from grid engine run
};

// Needed for BOOST_DATA_TEST_CASE below as it writes out TestCase
std::ostream& operator<<(std::ostream& os, const TestCase& testCase) { return os << testCase.label; }

TestCase testCaseData[] = {
    {"Physical Settled Swaption EUR 10y10y",
     25E-4,
     true,
     1,
     false,
     true,
     10,
     10,
     false,
     21,
     0.0,
     10000,
     10000,
     4.0,
     10,
     {{0.509357, 0.091875}, {1.00662, 0.0918901}, {1.50411, 0.0918698}, {2.00274, 0.0918969}, {2.50411, 0.0919471},
      {3, 0.0919541},       {3.50411, 0.0919187}, {4, 0.091858},        {4.50389, 0.0917798}, {5.00116, 0.0918077},
      {5.50959, 0.0917394}, {6, 0.0917051},       {6.50685, 0.0915082}, {7.00548, 0.0914719}, {7.50411, 0.0916239},
      {8.00274, 0.0916299}, {8.50389, 0.0915738}, {9.00116, 0.0917204}, {9.50411, 0.0920092}, {10, 0.0918714},
      {10.5041, 0.0917764}, {11, 0.0844412},      {11.5096, 0.0831162}, {12, 0.0758481},      {12.5066, 0.0743106},
      {13.0039, 0.0670203}, {13.5041, 0.0661777}, {14, 0.0583992},      {14.5041, 0.0571024}, {15, 0.0494685},
      {15.5041, 0.0481926}, {16, 0.0403637},      {16.5039, 0.0389221}, {17.0012, 0.0309469}, {17.5068, 0.0294415},
      {18.0055, 0.0214747}, {18.5041, 0.0199978}, {19.0027, 0.0118743}, {19.5041, 0.0098358}, {20, 0.00239165},
      {20.5039, 0},         {21.0012, 0}}},
    {"Cash Settled Swaption EUR 10y10y",
     20E-4,
     false,
     1,
     false,
     true,
     10,
     10,
     false,
     21,
     0.0,
     10000,
     10000,
     4.0,
     10,
     {{0.509357, 0.091875},   {1.00662, 0.0918901},
      {1.50411, 0.0918698},   {2.00274, 0.0918969},
      {2.50411, 0.0919471},   {3, 0.0919541},
      {3.50411, 0.0919187},   {4, 0.091858},
      {4.50389, 0.0917798},   {5.00116, 0.0918077},
      {5.50959, 0.0917394},   {6, 0.0917051},
      {6.50685, 0.0915082},   {7.00548, 0.0914719},
      {7.50411, 0.0916239},   {8.00274, 0.0916299},
      {8.50389, 0.0915738},   {9.00116, 0.0917204},
      {9.50411, 0.0920092},   {10, 0.0918714},
      {10.5041, 0.0216995},   {11, 0.0216537},
      {11.5096, 0.0165814},   {12, 0.0165204},
      {12.5066, 0.0114238},   {13.0039, 0.0112071},
      {13.5041, 0.00810083},  {14, 0.00810626},
      {14.5041, 0.00498351},  {15, 0.00494857},
      {15.5041, 0.00302471},  {16, 0.00300223},
      {16.5039, 0.00161475},  {17.0012, 0.00150577},
      {17.5068, 0.000872231}, {18.0055, 0.000691536},
      {18.5041, 0.000427726}, {19.0027, 0.000259685},
      {19.5041, 0},           {20, 0},
      {20.5039, 0},           {21.0012, 0}}},
    {"Physical Settled Swaption USD 10y10y",
     40E-4,
     true,
     1,
     false,
     false,
     10,
     10,
     false,
     21,
     0.0,
     10000,
     10000,
     4.0,
     10,
     {{0.509357, 0.05351},   {1.00662, 0.0534781},  {1.50411, 0.0533827},  {2.00274, 0.0532503},  {2.50411, 0.0533026},
      {3, 0.053185},         {3.50411, 0.0530595},  {4, 0.0529746},        {4.50389, 0.0530751},  {5.00116, 0.0532191},
      {5.50959, 0.0529909},  {6, 0.0530138},        {6.50685, 0.0529023},  {7.00548, 0.0529725},  {7.50411, 0.0530392},
      {8.00274, 0.0525997},  {8.50389, 0.052524},   {9.00116, 0.052617},   {9.50411, 0.0528042},  {10, 0.0527962},
      {10.5041, 0.0501823},  {11, 0.043772},        {11.5096, 0.0440506},  {12, 0.0384473},       {12.5066, 0.0392294},
      {13.0039, 0.0338302},  {13.5041, 0.0339911},  {14, 0.0287289},       {14.5041, 0.0292609},  {15, 0.0235833},
      {15.5041, 0.0240571},  {16, 0.0184883},       {16.5039, 0.0189846},  {17.0012, 0.0133277},  {17.5068, 0.0134281},
      {18.0055, 0.00808448}, {18.5041, 0.00806168}, {19.0027, 0.00289948}, {19.5041, 0.00248467}, {20, 1.91824e-06},
      {20.5039, 0},          {21.0012, 0}}},
    // FIXME, this test case fails; the AMC profile looks more reasonable than the reference results though...? to be
    // checked.
    /*{"Cash Settled Swaption USD 10y10y",
     20E-4,
     false,
     1,
     false,
     false,
     10,
     10,
     false,
     21,
     0.0,
     10000,
     10000,
     4.0,
     10,
     {{0.509357, 0.0535937},  {1.00662, 0.0535972},
      {1.50411, 0.053576},    {2.00274, 0.0535674},
      {2.50411, 0.0535863},   {3, 0.0535605},
      {3.50411, 0.0535707},   {4, 0.0535609},
      {4.50389, 0.0535324},   {5.00116, 0.0535029},
      {5.50959, 0.0534719},   {6, 0.0534826},
      {6.50685, 0.0534808},   {7.00548, 0.0534828},
      {7.50411, 0.0534604},   {8.00274, 0.0534544},
      {8.50389, 0.0534457},   {9.00116, 0.0534749},
      {9.50411, 0.0534384},   {10, 0.0533359},
      {10.5041, 0.0115775},   {11, 0.0112601},
      {11.5096, 0.0107997},   {12, 0.0102093},
      {12.5066, 0.00892546},  {13.0039, 0.00837041},
      {13.5041, 0.00697047},  {14, 0.0063756},
      {14.5041, 0.00537863},  {15, 0.00480582},
      {15.5041, 0.00387191},  {16, 0.00326433},
      {16.5039, 0.00265382},  {17.0012, 0.00205139},
      {17.5068, 0.00157963},  {18.0055, 0.000919889},
      {18.5041, 0.000675715}, {19.0027, 5.29768e-05},
      {19.5041, 0},           {20, 0},
      {20.5039, 0},           {21.0012, 0}}},*/
    {"Physical Settled Swaption EUR 10y50y (Long Term Simulation)",
     200E-4,
     true,
     4,
     false,
     true,
     50,
     50,
     false,
     61,
     50.0,
     10000,
     10000,
     4.0,
     10,
     {{0.509357, 0}, {1.00662, 0}, {1.50411, 0}, {2.00274, 0.36692},
      {2.50411, 0},  {3, 0},       {3.50411, 0}, {4, 0.367243},
      {4.50389, 0},  {5.00116, 0}, {5.50959, 0}, {6, 0.366503},
      {6.50685, 0},  {7.00548, 0}, {7.50411, 0}, {8.00274, 0.365889},
      {8.50389, 0},  {9.00116, 0}, {9.50411, 0}, {10, 0.363011},
      {10.5041, 0},  {11, 0},      {11.5096, 0}, {12, 0.352021},
      {12.5066, 0},  {13.0039, 0}, {13.5041, 0}, {14, 0.337706},
      {14.5041, 0},  {15, 0},      {15.5041, 0}, {16, 0.320619},
      {16.5039, 0},  {17.0012, 0}, {17.5068, 0}, {18.0055, 0.304939},
      {18.5041, 0},  {19.0027, 0}, {19.5041, 0}, {20, 0.287801},
      {20.5039, 0},  {21.0012, 0}, {21.5041, 0}, {22, 0.271108},
      {22.5096, 0},  {23, 0},      {23.5068, 0}, {24.0055, 0.254674},
      {24.5039, 0},  {25.0012, 0}, {25.5041, 0}, {26, 0.237545},
      {26.5041, 0},  {27, 0},      {27.5041, 0}, {28, 0.221542},
      {28.5094, 0},  {29.0066, 0}, {29.5041, 0}, {30.0027, 0.204903},
      {30.5041, 0},  {31, 0},      {31.5041, 0}, {32, 0.189248},
      {32.5039, 0},  {33.0012, 0}, {33.5096, 0}, {34, 0.173405},
      {34.5068, 0},  {35.0055, 0}, {35.5041, 0}, {36.0027, 0.158597},
      {36.5039, 0},  {37.0012, 0}, {37.5041, 0}, {38, 0.144585},
      {38.5041, 0},  {39, 0},      {39.5096, 0}, {40, 0.129709},
      {40.5066, 0},  {41.0039, 0}, {41.5041, 0}, {42, 0.115654},
      {42.5041, 0},  {43, 0},      {43.5041, 0}, {44, 0.101223},
      {44.5039, 0},  {45.0012, 0}, {45.5068, 0}, {46.0055, 0.0882498},
      {46.5041, 0},  {47.0027, 0}, {47.5041, 0}, {48, 0.0751091},
      {48.5039, 0},  {49.0012, 0}, {49.5041, 0}, {50, 0.0619377},
      {50.5096, 0},  {51, 0},      {51.5068, 0}, {52.0055, 0.0496666},
      {52.5039, 0},  {53.0012, 0}, {53.5041, 0}, {54, 0.0373868},
      {54.5041, 0},  {55, 0},      {55.5041, 0}, {56, 0.0249966},
      {56.5094, 0},  {57.0066, 0}, {57.5041, 0}, {58.0027, 0.0131594},
      {58.5041, 0},  {59, 0},      {59.5041, 0}, {60, 0.00169942},
      {60.5039, 0},  {61.0012, 0}}},
    {"Physical Settled Amortising Swaption EUR 10y10y",
     20E-4,
     true,
     1,
     false,
     true,
     10,
     10,
     true,
     21,
     0.0,
     10000,
     10000,
     4.0,
     10,
     {{0.509357, 0.0465975}, {1.00662, 0.046597},    {1.50411, 0.0465705},
      {2.00274, 0.0465701},  {2.50411, 0.0465922},   {3, 0.0465888},
      {3.50411, 0.0465547},  {4, 0.0465135},         {4.50389, 0.0464943},
      {5.00116, 0.0465701},  {5.50959, 0.0464974},   {6, 0.0464543},
      {6.50685, 0.0463007},  {7.00548, 0.0462769},   {7.50411, 0.0464078},
      {8.00274, 0.0464715},  {8.50389, 0.0464315},   {9.00116, 0.0464997},
      {9.50411, 0.0467324},  {10, 0.0466733},        {10.5041, 0.0465448},
      {11, 0.0389981},       {11.5096, 0.0380827},   {12, 0.0312491},
      {12.5066, 0.0304138},  {13.0039, 0.0243758},   {13.5041, 0.0237934},
      {14, 0.0183401},       {14.5041, 0.0177131},   {15, 0.0131241},
      {15.5041, 0.0125303},  {16, 0.00865102},       {16.5039, 0.00818706},
      {17.0012, 0.00505281}, {17.5068, 0.00468286},  {18.0055, 0.00240449},
      {18.5041, 0.00216216}, {19.0027, 0.000691815}, {19.5041, 0.00058673},
      {20, 1.52066e-05},     {20.5039, 0},           {21.0012, 0}}}};

BOOST_FIXTURE_TEST_SUITE(OreAmcTestSuite, ore::test::TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(AmcBermudanSwaptionTest, TestData)

BOOST_DATA_TEST_CASE(testBermudanSwaptionExposure, boost::unit_test::data::make(testCaseData), testCase) {

    // if true, only output results (e.g. for plotting), do no checks
    const bool outputResults = false;

    // if true, cached results are used for the reference values computed with the grid engine
    // if false, the grid engine is used for the computation, which is slow
    // note that the cached results are for sx=4, nx=10 and gridEvalEachNth=1 (except for Long Term Simulation case,
    // where it is 4) if these parameters change, the cached results should be refreshed
    const bool useCachedResults = true;

    BOOST_TEST_MESSAGE("Testing Bermudan swaption exposure profile");

    // Simulation date grid
    Date today = referenceDate;
    std::vector<Period> tenorGrid;

    if (!testCase.fineGrid) {
        // coarse grid 6m spacing
        for (Size i = 0; i < 2 * testCase.simYears; ++i) {
            tenorGrid.push_back(((i + 1) * 6) * Months);
        }
    } else {
        // fine grid 1m spacing
        for (Size i = 0; i < 12 * testCase.simYears; ++i) {
            tenorGrid.push_back(((i + 1)) * Months);
        }
    }

    Calendar cal;
    if (testCase.inBaseCcy)
        cal = TARGET();
    else
        cal = JointCalendar(UnitedStates(UnitedStates::Settlement), UnitedKingdom());

    QuantLib::ext::shared_ptr<DateGrid> grid = QuantLib::ext::make_shared<DateGrid>(tenorGrid, cal, ActualActual(ActualActual::ISDA));

    // Model
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = ccLgm;

    // Simulation market parameters, we just need the yield curve structure here
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig(new ScenarioSimMarketParameters);
    simMarketConfig->setYieldCurveTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                              5 * Years, 7 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                              30 * Years, 40 * Years, 50 * Years});
    simMarketConfig->setSimulateFXVols(false);

    simMarketConfig->baseCcy() = "EUR";
    simMarketConfig->setDiscountCurveNames({"EUR", "USD"});
    simMarketConfig->ccys() = {"EUR", "USD"};
    std::vector<std::string> tmp;
    for (auto c : simMarketConfig->ccys()) {
        if (c != simMarketConfig->baseCcy())
            tmp.push_back(c + simMarketConfig->baseCcy());
    }
    simMarketConfig->setFxCcyPairs(tmp);

    simMarketConfig->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M"});
    simMarketConfig->interpolation() = "LogLinear";
    simMarketConfig->setSwapVolExpiries("EUR", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years});
    simMarketConfig->setSwapVolTerms("EUR", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years});

    QuantLib::ext::shared_ptr<ScenarioGeneratorData> sgd(new ScenarioGeneratorData);
    sgd->sequenceType() = SobolBrownianBridge;
    sgd->seed() = 42;
    sgd->setGrid(grid);

    ScenarioGeneratorBuilder sgb(sgd);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketConfig, today, market);

    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(market, simMarketConfig);
    simMarket->scenarioGenerator() = sg;

    // Bermudan swaption for exposure generation
    Date startDate = cal.advance(today, 2 * Days);
    Date fwdStartDate = cal.advance(startDate, 10 * Years);
    Date endDate = cal.advance(fwdStartDate, testCase.swapLen * Years);
    Schedule fixedSchedule(fwdStartDate, endDate, 1 * Years, cal, Following, Following, DateGeneration::Forward, false);
    Schedule floatingSchedule(fwdStartDate, endDate, (testCase.inBaseCcy ? 6 : 3) * Months, cal, Following, Following,
                              DateGeneration::Forward, false);
    Schedule fixedScheduleSwap(startDate, endDate, 1 * Years, cal, Following, Following, DateGeneration::Forward,
                               false);
    Schedule floatingScheduleSwap(startDate, endDate, (testCase.inBaseCcy ? 6 : 3) * Months, cal, Following, Following,
                                  DateGeneration::Forward, false);
    Schedule fixedScheduleSwapShort(startDate, fwdStartDate, 1 * Years, cal, Following, Following,
                                    DateGeneration::Forward, false);
    Schedule floatingScheduleSwapShort(startDate, fwdStartDate, (testCase.inBaseCcy ? 6 : 3) * Months, cal, Following,
                                       Following, DateGeneration::Forward, false);
    QuantLib::ext::shared_ptr<VanillaSwap> underlying, vanillaswap, vanillaswapshort;
    QuantLib::ext::shared_ptr<NonstandardSwap> underlyingNs, vanillaswapNs, vanillaswapshortNs;
    if (!testCase.isAmortising) {
        // Standard Vanilla Swap
        if (testCase.inBaseCcy) {
            underlying = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                                         floatingSchedule, *simMarket->iborIndex("EUR-EURIBOR-6M"), 0.0,
                                                         Actual360());
        } else {
            underlying = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule, 0.03, Thirty360(Thirty360::BondBasis),
                                                         floatingSchedule, *simMarket->iborIndex("USD-LIBOR-3M"), 0.0,
                                                         Actual360());
        }
    } else {
        // Nonstandard Swap
        std::vector<Real> fixNotionals(fixedSchedule.size() - 1), floatNotionals(floatingSchedule.size() - 1);
        std::vector<Real> fixNotionalsSwap(fixedScheduleSwap.size() - 1),
            floatNotionalsSwap(floatingScheduleSwap.size() - 1);
        std::vector<Real> fixNotionalsShort(fixedScheduleSwapShort.size() - 1),
            floatNotionalsShort(floatingScheduleSwapShort.size() - 1);
        for (Size i = 0; i < fixNotionals.size(); ++i)
            fixNotionals[i] = 1.0 - static_cast<Real>(i) / static_cast<Real>(fixNotionals.size());
        for (Size i = 0; i < floatNotionals.size(); ++i)
            floatNotionals[i] = 1.0 - static_cast<Real>(i) / static_cast<Real>(floatNotionals.size());
        for (Size i = 0; i < fixNotionalsSwap.size(); ++i)
            fixNotionalsSwap[i] = 1.0 - static_cast<Real>(i) / static_cast<Real>(fixNotionalsSwap.size());
        for (Size i = 0; i < floatNotionalsSwap.size(); ++i)
            floatNotionalsSwap[i] = 1.0 - static_cast<Real>(i) / static_cast<Real>(floatNotionalsSwap.size());
        for (Size i = 0; i < fixNotionalsShort.size(); ++i)
            fixNotionalsShort[i] = 1.0 - static_cast<Real>(i) / static_cast<Real>(fixNotionalsShort.size());
        for (Size i = 0; i < floatNotionalsShort.size(); ++i)
            floatNotionalsShort[i] = 1.0 - static_cast<Real>(i) / static_cast<Real>(floatNotionalsShort.size());

        if (testCase.inBaseCcy) {
            underlyingNs = QuantLib::ext::make_shared<NonstandardSwap>(
                VanillaSwap::Payer, fixNotionals, floatNotionals, fixedSchedule,
                std::vector<Real>(fixNotionals.size(), 0.02), Thirty360(Thirty360::BondBasis), floatingSchedule,
                *simMarket->iborIndex("EUR-EURIBOR-6M"), 1.0, 0.0, Actual360());
        } else {
            underlyingNs = QuantLib::ext::make_shared<NonstandardSwap>(
                VanillaSwap::Payer, fixNotionals, floatNotionals, fixedSchedule,
                std::vector<Real>(fixNotionals.size(), 0.03), Thirty360(Thirty360::BondBasis), floatingSchedule,
                *simMarket->iborIndex("USD-LIBOR-3M"), 1.0, 0.0, Actual360());
        }
    }

    // needed for physical exercise in the option wrapper
    QuantLib::ext::shared_ptr<PricingEngine> underlyingEngine = QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(
        testCase.inBaseCcy ? simMarket->discountCurve("EUR") : simMarket->discountCurve("USD"));
    if (underlying)
        underlying->setPricingEngine(underlyingEngine);
    if (underlyingNs)
        underlyingNs->setPricingEngine(underlyingEngine);
    if (vanillaswap) {
        vanillaswap->setPricingEngine(underlyingEngine);
        vanillaswapshort->setPricingEngine(underlyingEngine);
    }
    if (vanillaswapNs) {
        vanillaswapNs->setPricingEngine(underlyingEngine);
        vanillaswapshortNs->setPricingEngine(underlyingEngine);
    }

    // collect fixing dates
    std::vector<Date> fixingDates;
    if (vanillaswap) {
        for (Size i = 0; i < vanillaswap->leg(1).size(); ++i) {
            QuantLib::ext::shared_ptr<IborCoupon> c = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(vanillaswap->leg(1)[i]);
            fixingDates.push_back(c->fixingDate());
        }
    } else if (vanillaswapNs) {
        for (Size i = 0; i < vanillaswapNs->leg(1).size(); ++i) {
            QuantLib::ext::shared_ptr<IborCoupon> c = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(vanillaswapNs->leg(1)[i]);
            fixingDates.push_back(c->fixingDate());
        }
    } else if (underlying) {
        for (Size i = 0; i < underlying->leg(1).size(); ++i) {
            QuantLib::ext::shared_ptr<IborCoupon> c = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(underlying->leg(1)[i]);
            fixingDates.push_back(c->fixingDate());
        }
    } else if (underlyingNs) {
        for (Size i = 0; i < underlyingNs->leg(1).size(); ++i) {
            QuantLib::ext::shared_ptr<IborCoupon> c = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(underlyingNs->leg(1)[i]);
            fixingDates.push_back(c->fixingDate());
        }
    }

    // exercise dates
    std::vector<Date> exerciseDates;
    for (Size i = 0; i < testCase.numExercises; ++i) {
        if (!testCase.fineGrid)
            // coarse grid
            exerciseDates.push_back(grid->dates()[19 + 2 * i]);
        else
            // find grid
            exerciseDates.push_back(grid->dates()[119 + 12 * i]);
    }

    QuantLib::ext::shared_ptr<QuantLib::Instrument> tmpUnd;
    if (testCase.isAmortising)
        tmpUnd = underlyingNs;
    else
        tmpUnd = underlying;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>> undInst(exerciseDates.size(), tmpUnd);

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates);
    QuantLib::ext::shared_ptr<Instrument> swaption;
    Settlement::Type settlementType = testCase.isPhysical ? Settlement::Physical : Settlement::Cash;
    Settlement::Method settlementMethod =
        testCase.isPhysical ? Settlement::PhysicalOTC : Settlement::CollateralizedCashPrice;
    if (testCase.isAmortising)
        swaption = QuantLib::ext::make_shared<NonstandardSwaption>(underlyingNs, exercise, settlementType, settlementMethod);
    else
        swaption = QuantLib::ext::make_shared<Swaption>(underlying, exercise, settlementType, settlementMethod);

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> param;
    // vol and rev must be consistent to CAM's LGM models in TestData
    Array emptyTimes;
    Array alphaEur(1), kappaEur(1), alphaUsd(1), kappaUsd(1);
    alphaEur[0] = lgm_eur->parametrization()->hullWhiteSigma(1.0);
    kappaEur[0] = lgm_eur->parametrization()->kappa(1.0);
    alphaUsd[0] = lgm_usd->parametrization()->hullWhiteSigma(1.0);
    kappaUsd[0] = lgm_usd->parametrization()->kappa(1.0);
    if (testCase.inBaseCcy)
        param = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            EURCurrency(), simMarket->discountCurve("EUR"), emptyTimes, alphaEur, emptyTimes, kappaEur);
    else
        param = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            USDCurrency(), simMarket->discountCurve("USD"), emptyTimes, alphaUsd, emptyTimes, kappaUsd);
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> bermmodel = QuantLib::ext::make_shared<LinearGaussMarkovModel>(param);

    // apply horizon shift
    // for grid engine
    param->shift() = -param->H(testCase.horizonShift);
    // for CAM and EUR AMC engine
    QuantLib::ext::static_pointer_cast<IrLgm1fParametrization>(lgm_eur->parametrization())->shift() =
        -QuantLib::ext::static_pointer_cast<IrLgm1fParametrization>(lgm_eur->parametrization())->H(testCase.horizonShift);
    // for USD AMC engine (in CAM no effect)
    QuantLib::ext::static_pointer_cast<IrLgm1fParametrization>(lgm_usd->parametrization())->shift() =
        -QuantLib::ext::static_pointer_cast<IrLgm1fParametrization>(lgm_usd->parametrization())->H(testCase.horizonShift);

    // grid engine

    QuantLib::ext::shared_ptr<PricingEngine> engineGrid;
    if (testCase.isAmortising)
        engineGrid = QuantLib::ext::make_shared<NumericLgmNonstandardSwaptionEngine>(bermmodel, Real(testCase.sx), testCase.nx,
                                                                             Real(testCase.sx), testCase.nx);
    else
        engineGrid = QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(bermmodel, Real(testCase.sx), testCase.nx,
                                                                  Real(testCase.sx), testCase.nx);

    // mc engine
    std::vector<Size> externalModelIndices = testCase.inBaseCcy ? std::vector<Size>{0} : std::vector<Size>{1};
    QuantLib::ext::shared_ptr<PricingEngine> engineMc;
    if (testCase.isAmortising) {
        engineMc = QuantLib::ext::make_shared<McLgmNonstandardSwaptionEngine>(
            testCase.inBaseCcy ? lgm_eur : lgm_usd, MersenneTwisterAntithetic, SobolBrownianBridge,
            testCase.trainingPaths, 0, 4711, 4712, 6, LsmBasisSystem::Monomial, SobolBrownianGenerator::Steps,
            SobolRsg::JoeKuoD7, Handle<YieldTermStructure>(), grid->dates(), externalModelIndices);
        swaption->setPricingEngine(engineMc);
    } else {
        engineMc = QuantLib::ext::make_shared<McLgmSwaptionEngine>(
            testCase.inBaseCcy ? lgm_eur : lgm_usd, MersenneTwisterAntithetic, SobolBrownianBridge,
            testCase.trainingPaths, 0, 4711, 4712, 6, LsmBasisSystem::Monomial, SobolBrownianGenerator::Steps,
            SobolRsg::JoeKuoD7, Handle<YieldTermStructure>(), grid->dates(), externalModelIndices);
        swaption->setPricingEngine(engineMc);
    }

    // wrapper (long option)

    QuantLib::ext::shared_ptr<InstrumentWrapper> wrapperGrid = QuantLib::ext::make_shared<BermudanOptionWrapper>(
        swaption, vanillaswap ? false : true, exerciseDates, testCase.isPhysical, undInst);
    wrapperGrid->initialise(grid->dates());

    // collect discounted epe
    std::vector<Real> swaption_epe_grid(grid->dates().size(), 0.0);
    std::vector<Real> swaption_epe_amc(grid->dates().size(), 0.0);

    // amc valuation
    swaption->setPricingEngine(engineMc);
    class TestTrade : public Trade {
    public:
        TestTrade(const string& tradeType, const string& curr, const QuantLib::ext::shared_ptr<InstrumentWrapper>& inst)
            : Trade(tradeType) {
            instrument_ = inst;
            npvCurrency_ = curr;
        }
        void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override {}
    };
    AMCValuationEngine amcValEngine(model, sgd, QuantLib::ext::shared_ptr<Market>(), std::vector<string>(),
                                    std::vector<string>(), 0);
    auto trade = QuantLib::ext::make_shared<TestTrade>("BermudanSwaption", testCase.inBaseCcy ? "EUR" : "USD",
                                               QuantLib::ext::make_shared<VanillaInstrument>(swaption));
    trade->id() = "DummyTradeId";
    auto portfolio = QuantLib::ext::make_shared<Portfolio>();
    portfolio->add(trade);
    QuantLib::ext::shared_ptr<NPVCube> outputCube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        referenceDate, std::set<string>{"DummyTradeId"}, grid->dates(), testCase.samples);
    boost::timer::cpu_timer timer;
    amcValEngine.buildCube(portfolio, outputCube);
    timer.stop();
    Real amcTime = timer.elapsed().wall * 1e-9;

    // epe computation (this is divided by the number of samples below)
    for (Size j = 0; j < grid->dates().size(); ++j) {
        for (Size i = 0; i < testCase.samples; ++i) {
            swaption_epe_amc[j] += std::max(outputCube->get(0, j, i, 0), 0.0);
        }
    }

    Real fx = 1.0;
    if (!testCase.inBaseCcy)
        fx = simMarket->fxRate("USDEUR")->value();

    Real amcNPV = outputCube->getT0(0, 0) / fx; // convert back to USD, cube contains base ccy values

    timer.start();
    swaption->setPricingEngine(engineGrid);
    Real gridNPV = swaption->NPV();
    timer.stop();
    Real gridTime0 = timer.elapsed().wall * 1e-9;

    BOOST_CHECK_MESSAGE((std::abs(gridNPV - amcNPV) <= testCase.tolerance),
                        "Can not verify gridNPV (" << gridNPV << ") and amcNPV (" << amcNPV << ")"
                                                   << ", difference is " << gridNPV - amcNPV << ", tolerance is "
                                                   << testCase.tolerance);

    // grid engine simulation (only if not cached, this takes a long time)
    if (useCachedResults) {
        for (Size t = 0; t < grid->dates().size(); ++t) {
            swaption_epe_grid.at(t) = testCase.cachedResults.at(t).at(1) * static_cast<Real>(testCase.samples);
        }
    } else {
        Real updateTime = 0.0;
        Real gridTime = 0.0;
        BOOST_TEST_MESSAGE("running " << testCase.samples << " samples simulation over " << grid->dates().size()
                                      << " time steps");
        QuantLib::ext::shared_ptr<IborIndex> index =
            *(testCase.inBaseCcy ? simMarket->iborIndex("EUR-EURIBOR-6M") : simMarket->iborIndex("USD-LIBOR-3M"));
        for (Size i = 0; i < testCase.samples; ++i) {
            if (i % 100 == 0)
                BOOST_TEST_MESSAGE("Sample " << i);
            Size idx = 0, fixIdx = 0;
            Size gridCnt = testCase.gridEvalEachNth;
            for (Date date : grid->dates()) {
                timer.start();
                // if we use cached results, we do not need the sim market
                simMarket->update(date);
                // set fixings
                Real v = index->fixing(date);
                while (fixIdx < fixingDates.size() && fixingDates[fixIdx] <= date) {
                    index->addFixing(fixingDates[fixIdx], v);
                    ++fixIdx;
                }
                // we do not use the valuation engine, so in case updates are disabled we need to
                // take care of the instrument update ourselves
                // this is only relevant for the discrete sim market, not the model sim market
                swaption->update();
                if (underlying)
                    underlying->update();
                if (underlyingNs)
                    underlyingNs->update();
                if (vanillaswap) {
                    vanillaswap->update();
                    vanillaswapshort->update();
                }
                if (vanillaswapNs) {
                    vanillaswapNs->update();
                    vanillaswapshortNs->update();
                }
                timer.stop();
                updateTime += timer.elapsed().wall * 1e-9;
                Real numeraire = simMarket->numeraire();
                Real fx = 1.0;
                if (!testCase.inBaseCcy)
                    fx = simMarket->fxRate("USDEUR")->value();
                // swaption epe accumulation
                if (--gridCnt == 0) {
                    timer.start();
                    swaption_epe_grid[idx] += std::max(wrapperGrid->NPV() * fx, 0.0) / numeraire;
                    timer.stop();
                    gridTime += timer.elapsed().wall * 1e-9;
                    gridCnt = testCase.gridEvalEachNth;
                }
                idx++;
            }
            wrapperGrid->reset();
            index->clearFixings();
        }
        BOOST_TEST_MESSAGE("Simulation time, grid " << gridTime << ", updates " << updateTime);
    }

    // compute summary statistics for swaption and check results
    if (outputResults) {
        std::clog << "time swaption_epe_grid swaption_epe_amc" << std::endl;
    }
    Size gridCnt = testCase.gridEvalEachNth;
    Real maxSwaptionErr = 0.0;
    for (Size i = 0; i < swaption_epe_grid.size(); ++i) {
        Real t = grid->timeGrid()[i + 1];
        swaption_epe_grid[i] /= testCase.samples;
        swaption_epe_amc[i] /= testCase.samples;
        if (outputResults) {
            if (useCachedResults) {
                // output all results
                std::clog << t << " " << swaption_epe_grid[i] << " " << swaption_epe_amc[i] << " " << std::endl;
            } else {
                // output results in a format that makes it easy to insert them as cached results in the code above
                std::clog << "{" << t << ", " << swaption_epe_grid[i] << std::endl;
            }
        }
        if (--gridCnt == 0) {
            if (!outputResults) {
                BOOST_CHECK_MESSAGE((std::abs(swaption_epe_grid[i] - swaption_epe_amc[i]) <= testCase.tolerance),
                                    "Can not verify swaption epe at grid point t="
                                        << t << ", grid = " << swaption_epe_grid[i] << ", amc = " << swaption_epe_amc[i]
                                        << ", difference " << (swaption_epe_grid[i] - swaption_epe_amc[i])
                                        << ", tolerance " << testCase.tolerance);
            }
            maxSwaptionErr = std::max(maxSwaptionErr, std::abs(swaption_epe_grid[i] - swaption_epe_amc[i]));
            gridCnt = testCase.gridEvalEachNth;
        }
    }
    BOOST_TEST_MESSAGE("AMC simulation time = " << amcTime << "s, T0 NPV (AMC) = " << amcNPV
                                                << ", T0 NPV (Grid) = " << gridNPV << " (" << gridTime0 * 1000.0
                                                << " ms), Max Error Swaption = " << maxSwaptionErr);

} // testBermudanSwaptionExposure

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
