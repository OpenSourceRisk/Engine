/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include "oredtestmarket.hpp"

#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/models/blackscholes.hpp>
#include <ored/scripting/models/gaussiancam.hpp>
#include <ored/scripting/staticanalyser.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/models/dummymodel.hpp>

#include <oret/toplevelfixture.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(GaussianCamTest)

BOOST_AUTO_TEST_CASE(testRepricingCalibrationInstruments) {

    BOOST_TEST_MESSAGE("test repricing of calibration instruments in Gaussian CAM...");

    constexpr Size paths = 25000;

    Date asof(7, July, 2019);
    Settings::instance().evaluationDate() = asof;
    auto testMarket = QuantLib::ext::make_shared<OredTestMarket>(asof);

    // build IR-FX-EQ CAM

    std::vector<Date> calibrationExpiries;
    std::vector<std::string> calibrationExpiriesStr;
    std::vector<Real> calibrationTimes;
    for (Size i = 1; i <= 9; ++i) {
        Date d = TARGET().advance(asof, i * Years);
        calibrationExpiries.push_back(d);
        calibrationExpiriesStr.push_back(ore::data::to_string(asof + i * Years));
        calibrationTimes.push_back(testMarket->discountCurve("EUR")->timeFromReference(d));
    }
    std::vector<Date> calibrationTerms(calibrationExpiriesStr.size(), Date(7, Jul, 2029));
    std::vector<std::string> calibrationTermsStr(calibrationExpiriesStr.size(), "2029-07-07");

    std::vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs;
    std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs;
    std::vector<QuantLib::ext::shared_ptr<EqBsData>> eqConfigs;

    auto configEUR = QuantLib::ext::make_shared<IrLgmData>();
    configEUR->qualifier() = "EUR";
    configEUR->reversionType() = LgmData::ReversionType::HullWhite;
    configEUR->volatilityType() = LgmData::VolatilityType::Hagan;
    configEUR->calibrateH() = false;
    configEUR->hParamType() = ParamType::Constant;
    configEUR->hTimes() = std::vector<Real>();
    configEUR->calibrationType() = CalibrationType::Bootstrap;
    configEUR->scaling() = 1.0;
    configEUR->shiftHorizon() = 0.0;
    configEUR->hValues() = {0.0050};
    configEUR->calibrateA() = true;
    configEUR->aParamType() = ParamType::Piecewise;
    configEUR->aTimes() = calibrationTimes;
    configEUR->aValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030);
    configEUR->optionExpiries() = calibrationExpiriesStr;
    configEUR->optionTerms() = calibrationTermsStr;
    configEUR->optionStrikes() = std::vector<std::string>(calibrationExpiriesStr.size(), "ATM");

    auto configUSD = QuantLib::ext::make_shared<IrLgmData>();
    configUSD->qualifier() = "USD";
    configUSD->reversionType() = LgmData::ReversionType::HullWhite;
    configUSD->volatilityType() = LgmData::VolatilityType::Hagan;
    configUSD->calibrateH() = false;
    configUSD->hParamType() = ParamType::Constant;
    configUSD->hTimes() = std::vector<Real>();
    configUSD->calibrationType() = CalibrationType::Bootstrap;
    configUSD->scaling() = 1.0;
    configUSD->shiftHorizon() = 0.0;
    configUSD->hValues() = {0.0030};
    configUSD->calibrateA() = true;
    configUSD->aParamType() = ParamType::Piecewise;
    configUSD->aTimes() = calibrationTimes;
    configUSD->aValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030);
    configUSD->optionExpiries() = calibrationExpiriesStr;
    configUSD->optionTerms() = calibrationTermsStr;
    configUSD->optionStrikes() = std::vector<std::string>(calibrationExpiriesStr.size(), "ATM");

    irConfigs.push_back(configEUR);
    irConfigs.push_back(configUSD);

    auto configFX = QuantLib::ext::make_shared<FxBsData>();
    configFX->foreignCcy() = "USD";
    configFX->domesticCcy() = "EUR";
    configFX->calibrationType() = CalibrationType::Bootstrap;
    configFX->calibrateSigma() = true;
    configFX->sigmaParamType() = ParamType::Piecewise;
    configFX->sigmaTimes() = calibrationTimes;
    configFX->sigmaValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030);
    configFX->optionExpiries() = calibrationExpiriesStr;
    configFX->optionStrikes() = std::vector<std::string>(calibrationExpiriesStr.size(), "ATMF");
    fxConfigs.push_back(configFX);

    auto configEQ = QuantLib::ext::make_shared<EqBsData>();
    configEQ->eqName() = "SP5";
    configEQ->currency() = "USD";
    configEQ->calibrationType() = CalibrationType::Bootstrap;
    configEQ->calibrateSigma() = true;
    configEQ->sigmaParamType() = ParamType::Piecewise;
    configEQ->sigmaTimes() = calibrationTimes;
    configEQ->sigmaValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030);
    configEQ->optionExpiries() = calibrationExpiriesStr;
    configEQ->optionStrikes() = std::vector<std::string>(calibrationExpiriesStr.size(), "ATMF");
    eqConfigs.push_back(configEQ);

    CorrelationMatrixBuilder cmb;
    cmb.addCorrelation("IR:EUR", "IR:USD", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6)));
    cmb.addCorrelation("IR:EUR", "FX:EURUSD", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.2)));
    cmb.addCorrelation("IR:EUR", "EQ:SP5", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.2)));
    cmb.addCorrelation("IR:USD", "FX:EURUSD", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.3)));
    cmb.addCorrelation("IR:USD", "EQ:SP5", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.5)));
    cmb.addCorrelation("FX:EURUSD", "EQ:SP5", Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4)));

    auto camBuilder = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        testMarket, QuantLib::ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, eqConfigs, cmb.correlations()));
    auto model = camBuilder->model();

    //  set up gaussian cam adapter with simulation dates = calibration expiries

    std::vector<std::string> modelCcys = {"EUR", "USD"};
    std::vector<Handle<YieldTermStructure>> modelCurves = {testMarket->discountCurve("EUR"),
                                                           testMarket->discountCurve("USD")};
    std::vector<Handle<Quote>> modelFxSpots = {testMarket->fxRate("USDEUR")};
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>> irIndices = {
        std::make_pair("EUR-EURIBOR-6M", *testMarket->iborIndex("EUR-EURIBOR-6M"))};
    std::vector<std::string> indices = {"FX-GENERIC-USD-EUR", "EQ-SP5"};
    std::vector<std::string> indexCurrencies = {"USD", "USD"};
    auto gaussianCam = QuantLib::ext::make_shared<GaussianCam>(
        model, paths, modelCcys, modelCurves, modelFxSpots, irIndices,
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>(), indices, indexCurrencies,
        std::set<Date>(calibrationExpiries.begin(), calibrationExpiries.end()), Model::McParams());

    // generate MC prices for the calibration instruments and compare them with analytical prices

    // FX instruments

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Option"] = RandomVariable(paths, 0.0);
    context->scalars["Underlying"] = IndexVec{paths, "FX-GENERIC-USD-EUR"};
    context->scalars["PayCcy"] = CurrencyVec{paths, "EUR"};
    context->scalars["PutCall"] = RandomVariable(paths, 1.0);
    std::string optionScript =
        "Option = PAY( max( PutCall * (Underlying(Expiry)-Strike), 0), Expiry, Expiry, PayCcy );";
    auto optionAst = ScriptParser(optionScript).ast();
    BOOST_REQUIRE(optionAst);
    ScriptEngine optionEngine(optionAst, context, gaussianCam);

    for (Size i = 0; i < calibrationExpiries.size(); ++i) {
        Real atmf = testMarket->fxRate("USDEUR")->value() /
                    testMarket->discountCurve("EUR")->discount(calibrationExpiries[i]) *
                    testMarket->discountCurve("USD")->discount(calibrationExpiries[i]);
        // compute the script price
        context->scalars["Expiry"] = EventVec{paths, calibrationExpiries[i]};
        context->scalars["Strike"] = RandomVariable(paths, atmf);
        optionEngine.run();
        Real scriptPrice = expectation(QuantLib::ext::get<RandomVariable>(context->scalars["Option"])).at(0);
        // compute the analytical price
        auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            testMarket->fxRate("USDEUR"), testMarket->discountCurve("USD"), testMarket->discountCurve("EUR"),
            testMarket->fxVol("USDEUR"));
        auto engine = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(process);
        VanillaOption option(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, atmf),
                             QuantLib::ext::make_shared<EuropeanExercise>(calibrationExpiries[i]));
        option.setPricingEngine(engine);
        Real analyticalPrice = option.NPV();
        // compare script and analytical price
        BOOST_TEST_MESSAGE("FX Option expiry " << calibrationExpiries[i] << " analyticalPrice= " << analyticalPrice
                                               << " scriptPrice=" << scriptPrice << " relative error "
                                               << (scriptPrice - analyticalPrice) / analyticalPrice);
        BOOST_CHECK_CLOSE(analyticalPrice, scriptPrice, 0.5);
    }

    // EQ instruments

    context->scalars["Underlying"] = IndexVec{paths, "EQ-SP5"};
    context->scalars["PayCcy"] = CurrencyVec{paths, "USD"};

    for (Size i = 0; i < calibrationExpiries.size(); ++i) {
        Real atmf = testMarket->equitySpot("SP5")->value() /
                    testMarket->equityForecastCurve("SP5")->discount(calibrationExpiries[i]) *
                    testMarket->equityDividendCurve("SP5")->discount(calibrationExpiries[i]);
        // compute the script price
        context->scalars["Expiry"] = EventVec{paths, calibrationExpiries[i]};
        context->scalars["Strike"] = RandomVariable(paths, atmf);
        optionEngine.run();
        Real scriptPrice = expectation(QuantLib::ext::get<RandomVariable>(context->scalars["Option"])).at(0);
        // compute the analytical price
        auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            testMarket->equitySpot("SP5"), testMarket->equityDividendCurve("SP5"),
            testMarket->equityForecastCurve("SP5"), testMarket->equityVol("SP5"));
        auto engine = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(process, testMarket->discountCurve("USD"));
        VanillaOption option(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, atmf),
                             QuantLib::ext::make_shared<EuropeanExercise>(calibrationExpiries[i]));
        option.setPricingEngine(engine);
        Real analyticalPrice = option.NPV() * testMarket->fxRate("USDEUR")->value();
        // compare script and analytical price
        BOOST_TEST_MESSAGE("Equity Option expiry " << calibrationExpiries[i] << " analyticalPrice= " << analyticalPrice
                                                   << " scriptPrice=" << scriptPrice << " relative error "
                                                   << (scriptPrice - analyticalPrice) / analyticalPrice);
        BOOST_CHECK_CLOSE(analyticalPrice, scriptPrice, 1.0); // eq has higher market vol compared to fx
    }

    // IR instruments

    context = QuantLib::ext::make_shared<Context>();
    context->scalars["Option"] = RandomVariable(paths, 0.0);
    context->scalars["FloatIndex"] = IndexVec{paths, "EUR-EURIBOR-6M"};
    context->scalars["PayCurrency"] = CurrencyVec{paths, "EUR"};
    context->scalars["FixedRatePayer"] = RandomVariable(paths, -1.0);
    context->scalars["Notional"] = RandomVariable(paths, 1.0);
    context->scalars["FloatSpread"] = RandomVariable(paths, 0.0);
    context->scalars["FixedDayCounter"] = DaycounterVec{paths, "30/360"};
    context->scalars["FloatDayCounter"] = DaycounterVec{paths, "A360"};
    std::string swaptionScript =
        "NUMBER UnderlyingNpv;\n"
        "NUMBER i, j;\n"
        "FOR j IN (2, SIZE(FixedLegSchedule), 1) DO\n"
        "    UnderlyingNpv = UnderlyingNpv + PAY( Notional * FixedRate * dcf( FixedDayCounter, \n"
        "                                                    FixedLegSchedule[j-1], FixedLegSchedule[j] ),\n"
        "                                         OptionExpiry, FixedLegSchedule[j], PayCurrency );\n"
        "END;\n"
        "FOR j IN (2, SIZE(FloatLegSchedule), 1) DO\n"
        "    UnderlyingNpv = UnderlyingNpv - PAY( Notional * (FloatIndex(OptionExpiry, FixingSchedule[j-1]) + \n"
        "               FloatSpread) * dcf( FloatDayCounter, FloatLegSchedule[j-1], FloatLegSchedule[j] ),\n"
        "                                         OptionExpiry, FloatLegSchedule[j], PayCurrency );\n"
        "END;\n"
        "Option = max( FixedRatePayer * UnderlyingNpv, 0);\n";
    auto swaptionAst = ScriptParser(swaptionScript).ast();
    BOOST_REQUIRE(swaptionAst);

    auto swvol = testMarket->swaptionVol("EUR");
    auto swapIndex = testMarket->swapIndex(testMarket->swapIndexBase("EUR"));
    auto iborIndex = swapIndex->iborIndex();
    auto fixedLegTenor = swapIndex->fixedLegTenor();
    auto fixedDayCounter = swapIndex->dayCounter();
    auto floatDayCounter = swapIndex->iborIndex()->dayCounter();

    for (Size i = 0; i < calibrationExpiries.size(); ++i) {
        Real optionTime = swvol->timeFromReference(calibrationExpiries[i]);
        Real swapLength = swvol->swapLength(calibrationExpiries[i], calibrationTerms[i]);
        // dummy strike (we don't have a smile in the market)
        Handle<Quote> vol(QuantLib::ext::make_shared<SimpleQuote>(swvol->volatility(optionTime, swapLength, 0.01)));
        // atm swaption helper
        auto helper = QuantLib::ext::make_shared<SwaptionHelper>(
            calibrationExpiries[i], Date(7, July, 2029), vol, iborIndex, fixedLegTenor, fixedDayCounter,
            floatDayCounter, testMarket->discountCurve("EUR"), BlackCalibrationHelper::RelativePriceError, Null<Real>(),
            1.0, swvol->volatilityType(), swvol->shift(optionTime, swapLength));
        Real atmStrike = helper->underlyingSwap()->fairRate();
        // script price
        auto workingContext = QuantLib::ext::make_shared<Context>(*context);
        std::vector<ValueType> fixedSchedule, floatSchedule, fixingSchedule;
        for (auto const& d : helper->underlyingSwap()->fixedSchedule().dates())
            fixedSchedule.push_back(EventVec{paths, d});
        for (auto const& d : helper->underlyingSwap()->floatingSchedule().dates()) {
            floatSchedule.push_back(EventVec{paths, d});
            fixingSchedule.push_back(EventVec{paths, iborIndex->fixingDate(d)});
        }
        workingContext->scalars["OptionExpiry"] = EventVec{paths, calibrationExpiries[i]};
        workingContext->arrays["FixedLegSchedule"] = fixedSchedule;
        workingContext->arrays["FloatLegSchedule"] = floatSchedule;
        workingContext->arrays["FixingSchedule"] = fixingSchedule;
        workingContext->scalars["FixedRate"] = RandomVariable(paths, atmStrike);
        ScriptEngine swaptionEngine(swaptionAst, workingContext, gaussianCam);
        swaptionEngine.run(swaptionScript);
        Real scriptPrice = expectation(QuantLib::ext::get<RandomVariable>(workingContext->scalars["Option"])).at(0);
        // analytical price
        Real analyticalPrice = helper->marketValue();
        // compare script and analytical price
        BOOST_TEST_MESSAGE("Swaption Option expiry "
                           << calibrationExpiries[i] << " analyticalPrice= " << analyticalPrice << " scriptPrice="
                           << scriptPrice << " relative error " << (scriptPrice - analyticalPrice) / analyticalPrice);
        BOOST_CHECK_CLOSE(analyticalPrice, scriptPrice, 1.0);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
