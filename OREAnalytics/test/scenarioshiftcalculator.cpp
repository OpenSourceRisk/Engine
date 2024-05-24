/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <oret/toplevelfixture.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>

using namespace boost::unit_test_framework;

using ore::analytics::RiskFactorKey;
using ore::analytics::ScenarioSimMarketParameters;
using ore::analytics::SensitivityScenarioData;
using ore::analytics::SimpleScenario;
using ore::analytics::ScenarioShiftCalculator;
using ore::analytics::ShiftType;
using namespace QuantLib;

using RFType = RiskFactorKey::KeyType;
using CurveShiftData = SensitivityScenarioData::CurveShiftData;
using SpotShiftData = SensitivityScenarioData::SpotShiftData;

// Tolerance for comparisons below
Real tol = 1e-10;
Date asof(14, Jun, 2018);

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ScenarioShiftCalculatorTest)

BOOST_AUTO_TEST_CASE(testAbsoluteDiscountShift) {

    BOOST_TEST_MESSAGE("Testing absolute shift in a discount curve");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    // Discount curve sensitivity set up to have 1bp absolute shift
    ssd->discountCurveShiftData()["EUR"] = QuantLib::ext::make_shared<CurveShiftData>();
    ssd->discountCurveShiftData()["EUR"]->shiftSize = 0.0001;
    ssd->discountCurveShiftData()["EUR"]->shiftType = ShiftType::Absolute;

    ssp->setYieldCurveTenors("EUR", {3 * Months, 6 * Months});

    // Pick out the 6M discount from the scenarios
    RiskFactorKey rf(RFType::DiscountCurve, "EUR", 1);

    SimpleScenario scen_1(asof);
    DiscountFactor v_1 = 0.995;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    DiscountFactor v_2 = 0.990;
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    Real exp = 100.480591307889;
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 2.5;
    ssd->discountCurveShiftData()["EUR"]->shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testRelativeDiscountShift) {

    BOOST_TEST_MESSAGE("Testing relative shift in a discount curve");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    // Discount curve sensitivity set up to have 1% relative shift
    ssd->discountCurveShiftData()["EUR"] = QuantLib::ext::make_shared<CurveShiftData>();
    ssd->discountCurveShiftData()["EUR"]->shiftSize = 0.01;
    ssd->discountCurveShiftData()["EUR"]->shiftType = ShiftType::Relative;

    ssp->setYieldCurveTenors("EUR", {3 * Months, 6 * Months});

    // Pick out the 6M discount from the scenarios
    RiskFactorKey rf(RFType::DiscountCurve, "EUR", 1);

    SimpleScenario scen_1(asof);
    DiscountFactor v_1 = 0.995;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    DiscountFactor v_2 = 0.990;
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    Real exp = 100.503780463123;
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 1.5;
    ssd->discountCurveShiftData()["EUR"]->shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testAbsoluteSurvivalShift) {

    BOOST_TEST_MESSAGE("Testing absolute shift in a survival curve");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    // Credit curve sensitivity set up to have 10bp absolute shift
    ssd->creditCurveShiftData()["APPLE"] = QuantLib::ext::make_shared<CurveShiftData>();
    ssd->creditCurveShiftData()["APPLE"]->shiftSize = 0.0010;
    ssd->creditCurveShiftData()["APPLE"]->shiftType = ShiftType::Absolute;

    ssp->setDefaultTenors("APPLE", {3 * Months, 6 * Months, 1 * Years});

    // Pick out the 1Y default curve point from the scenarios
    RiskFactorKey rf(RFType::SurvivalProbability, "APPLE", 2);

    SimpleScenario scen_1(asof);
    Probability v_1 = 0.90;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    Probability v_2 = 0.95;
    scen_2.add(rf, v_2);

    // Set up the calculator
    // In a realistic case we wil have a ScenarioSimMarket to be passed here as third argument.
    // In the absence of this the shift calculator will assume A365 term structure daycount for date/time conversion.
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    // This is based on A360 default term structure daycount set before in the scenario sim market parameters for default curves
    // Real exp = -53.3265744035596;
    // This is based on A365 daycount now assumed by default in the scenarioshiftcalculator.*pp if sim market is not passed in
    Real exp = -54.067221270275702;

    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 2.0;
    ssd->creditCurveShiftData()["APPLE"]->shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testRelativeSurvivalShift) {

    BOOST_TEST_MESSAGE("Testing relative shift in a survival curve");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    // Credit curve sensitivity set up to have 10% relative shift
    ssd->creditCurveShiftData()["APPLE"] = QuantLib::ext::make_shared<CurveShiftData>();
    ssd->creditCurveShiftData()["APPLE"]->shiftSize = 0.10;
    ssd->creditCurveShiftData()["APPLE"]->shiftType = ShiftType::Relative;

    ssp->setDefaultTenors("APPLE", {3 * Months, 6 * Months, 1 * Years});

    // Pick out the 1Y default curve point from the scenarios
    RiskFactorKey rf(RFType::SurvivalProbability, "APPLE", 2);

    SimpleScenario scen_1(asof);
    Probability v_1 = 0.90;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    Probability v_2 = 0.95;
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    Real exp = -5.1316397734676;
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 2.0;
    ssd->creditCurveShiftData()["APPLE"]->shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testAbsoluteFxShift) {

    BOOST_TEST_MESSAGE("Testing absolute shift in a FX spot rate");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    Rate shift = 0.0005;
    Real exp = 3.0;

    // FX spot sensitivity set up to have 5bp absolute shift
    ssd->fxShiftData()["EURUSD"].shiftSize = shift;
    ssd->fxShiftData()["EURUSD"].shiftType = ShiftType::Absolute;

    // EURUSD spot scenario
    RiskFactorKey rf(RFType::FXSpot, "EURUSD", 0);

    SimpleScenario scen_1(asof);
    Rate v_1 = 1.1637;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    Rate v_2 = v_1 + exp * shift;
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 1 / 5.0;
    ssd->fxShiftData()["EURUSD"].shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testRelativeFxShift) {

    BOOST_TEST_MESSAGE("Testing relative shift in a FX spot rate");

    Rate shift = 0.02;
    Real exp = 4.5;

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    // FX spot sensitivity set up to have 2% relative shift
    ssd->fxShiftData()["EURUSD"].shiftSize = shift;
    ssd->fxShiftData()["EURUSD"].shiftType = ShiftType::Relative;

    // EURUSD spot scenario
    RiskFactorKey rf(RFType::FXSpot, "EURUSD", 0);

    SimpleScenario scen_1(asof);
    Rate v_1 = 1.1637;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    Rate v_2 = v_1 * (1.0 + exp * shift);
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 1 / 2.0;
    ssd->fxShiftData()["EURUSD"].shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testAbsoluteSwaptionVolShift) {

    BOOST_TEST_MESSAGE("Testing absolute shift in a swaption volatility");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    Rate shift = 0.0001;
    Real exp = 8.45;

    // Swaption volatility sensitivity set up to have 1bp absolute shift
    ssd->swaptionVolShiftData()["EUR"].shiftSize = shift;
    ssd->swaptionVolShiftData()["EUR"].shiftType = ShiftType::Absolute;

    // Swaption volatility scenario (index will correspond to some point on
    // a cube or matrix)
    RiskFactorKey rf(RFType::SwaptionVolatility, "EUR", 8);

    SimpleScenario scen_1(asof);
    Rate v_1 = 0.0064;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    Rate v_2 = v_1 + exp * shift;
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 1 / 2.0;
    ssd->swaptionVolShiftData()["EUR"].shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_CASE(testRelativeSwaptionVolShift) {

    BOOST_TEST_MESSAGE("Testing relative shift in a swaption volatility");

    // Set up
    auto ssd = QuantLib::ext::make_shared<SensitivityScenarioData>();
    auto ssp = QuantLib::ext::make_shared<ScenarioSimMarketParameters>();

    Rate shift = 0.01;
    Real exp = 5.5;

    // Swaption volatility sensitivity set up to have 1% relative shift
    ssd->swaptionVolShiftData()["EUR"].shiftSize = shift;
    ssd->swaptionVolShiftData()["EUR"].shiftType = ShiftType::Relative;

    // Swaption volatility scenario (index will correspond to some point on
    // a cube or matrix)
    RiskFactorKey rf(RFType::SwaptionVolatility, "EUR", 8);

    SimpleScenario scen_1(asof);
    Rate v_1 = 0.0064;
    scen_1.add(rf, v_1);

    SimpleScenario scen_2(asof);
    Rate v_2 = v_1 * (1 + exp * shift);
    scen_2.add(rf, v_2);

    // Set up the calculator
    ScenarioShiftCalculator ssc(ssd, ssp);

    // Test the result
    Real cal = ssc.shift(rf, scen_1, scen_2);
    BOOST_CHECK_CLOSE(cal, exp, tol);

    // Scale the shift size and the calculated shift multiple should scale
    Real factor = 1 / 2.0;
    ssd->swaptionVolShiftData()["EUR"].shiftSize *= factor;
    cal = ssc.shift(rf, scen_1, scen_2);
    exp /= factor;
    BOOST_CHECK_CLOSE(cal, exp, tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
