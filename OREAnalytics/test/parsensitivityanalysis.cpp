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

#include "parsensitivityanalysis.hpp"
#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/engine/zerotoparcube.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>

#include <ored/model/lgmdata.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <test/oreatoplevelfixture.hpp>

#include <boost/timer/timer.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData2() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD"};
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 6 * Years, 7 * Years, 8 * Years, 9 * Years, 10 * Years,
                                            12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years});
    simMarketData->setIndices({"EUR-EURIBOR-6M", "GBP-LIBOR-6M"});
    simMarketData->interpolation() = "LogLinear";

    simMarketData->setDefaultNames({"BondIssuer1"});

    simMarketData->setDefaultTenors(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setSimulateSurvivalProbabilities(true);
    simMarketData->setDefaultCurveCalendars("", "TARGET");

    simMarketData->setSwapVolTerms(
        "", {1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true);

    simMarketData->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true);
    simMarketData->setFxVolCcyPairs({"EURGBP"});
    simMarketData->setFxVolIsSurface(true);
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.5, 1, 1.5, 2, 2.5, 2});

    simMarketData->setFxCcyPairs({"EURGBP"});

    simMarketData->setSimulateCapFloorVols(false);
    simMarketData->setEquityNames({"SP5", "Lufthansa"});
    simMarketData->setEquityDividendTenors("SP5", {6 * Months, 1 * Years, 2 * Years});
    simMarketData->setEquityDividendTenors("Lufthansa", {6 * Months, 1 * Years, 2 * Years});

    simMarketData->setSimulateEquityVols(true);
    simMarketData->setEquityVolDecayMode("ForwardVariance");
    simMarketData->setEquityVolNames({"SP5", "Lufthansa"});
    simMarketData->setEquityVolExpiries("", {2 * Weeks,  1 * Months, 3 * Months, 6 * Months, 1 * Years,
                                          2 * Years,  3 * Years,  5 * Years,  7 * Years,  10 * Years,
                                          13 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setEquityVolIsSurface("", true);
    simMarketData->setEquityVolMoneyness("", {0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2,
                                           1.3, 1.4, 1.5, 1.6, 1.7, 1.8,  1.9, 2.0,  2.5, 3.0});

    simMarketData->setYoyInflationIndices({"UKRP1"});
    simMarketData->setYoyInflationTenors(
        "UKRP1", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});

    simMarketData->setSimulateYoYInflationCapFloorVols(true);
    simMarketData->setYoYInflationCapFloorVolNames({"UKRP1"});
    simMarketData->setYoYInflationCapFloorVolExpiries(
        "UKRP1", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setYoYInflationCapFloorVolStrikes("", {-0.02, -0.01, 0.00, 0.01, 0.02, 0.03});
    simMarketData->yoyInflationCapFloorVolDecayMode() = "ForwardVariance";

    return simMarketData;
}

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";

    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveNames({"BondCurve1"});
    simMarketData->yieldCurveCurrencies()["BondCurve1"] = "EUR";
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices(
        {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    simMarketData->interpolation() = "LogLinear";

    simMarketData->setDefaultNames({"BondIssuer1", "dc"});
    simMarketData->setDefaultTenors(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setSimulateSurvivalProbabilities(true);
    simMarketData->setSecurities({"Bond1"});
    simMarketData->setDefaultCurveCalendars("", "TARGET");

    simMarketData->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true); // false;

    simMarketData->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true); // false;
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});
    simMarketData->setFxVolIsSurface(true);
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.5, 1, 1.5, 2, 2.5, 3});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    simMarketData->setSimulateCdsVols(true);
    simMarketData->cdsVolExpiries() = {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    simMarketData->cdsVolDecayMode() = "ForwardVariance";
    simMarketData->setCdsVolNames({"dc"});

    simMarketData->setEquityNames({"SP5", "Lufthansa"});
    simMarketData->setEquityDividendTenors("SP5", {6 * Months, 1 * Years, 2 * Years});
    simMarketData->setEquityDividendTenors("Lufthansa", {6 * Months, 1 * Years, 2 * Years});

    simMarketData->setSimulateEquityVols(true);
    simMarketData->setEquityVolDecayMode("ForwardVariance");
    simMarketData->setEquityVolNames({"SP5", "Lufthansa"});
    simMarketData->setEquityVolExpiries("", {2 * Weeks,  1 * Months, 3 * Months, 6 * Months, 1 * Years,
                                          2 * Years,  3 * Years,  5 * Years,  7 * Years,  10 * Years,
                                          13 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setEquityVolIsSurface("", true);
    simMarketData->setEquityVolMoneyness("", {0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2,
                                           1.3, 1.4, 1.5, 1.6, 1.7, 1.8,  1.9, 2.0,  2.5, 3.0});

    simMarketData->setYoyInflationIndices({"UKRP1"});
    simMarketData->setYoyInflationTenors(
        "UKRP1", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});

    simMarketData->setSimulateYoYInflationCapFloorVols(true);
    simMarketData->setYoYInflationCapFloorVolNames({"UKRP1"});
    simMarketData->setYoYInflationCapFloorVolExpiries(
        "UKRP1", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setYoYInflationCapFloorVolStrikes("", {-0.02, -0.01, 0.00, 0.01, 0.02, 0.03});
    simMarketData->yoyInflationCapFloorVolDecayMode() = "ForwardVariance";

    return simMarketData;
}

SensitivityScenarioData::CurveShiftParData createCurveShiftData() {
    SensitivityScenarioData::CurveShiftParData cvsData;
    cvsData.shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.0001;
    cvsData.parInstruments = {"DEP", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};

    return cvsData;
}
QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData2() {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = QuantLib::ext::make_shared<SensitivityScenarioData>(false);

    SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {2 * Years, 5 * Years};

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.05};

    SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

    SensitivityScenarioData::CurveShiftParData eurDiscountData = createCurveShiftData();
    eurDiscountData.parInstrumentSingleCurve = true;
    eurDiscountData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    eurDiscountData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["EUR"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(eurDiscountData);

    SensitivityScenarioData::CurveShiftParData gbpDiscountData = createCurveShiftData();
    gbpDiscountData.parInstrumentSingleCurve = true;
    gbpDiscountData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
    gbpDiscountData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["GBP"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(gbpDiscountData);

    SensitivityScenarioData::CurveShiftParData eurIndexData = createCurveShiftData();
    eurIndexData.parInstrumentSingleCurve = false;
    eurIndexData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    eurIndexData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(eurIndexData);

    SensitivityScenarioData::CurveShiftParData gbpIndexData = createCurveShiftData();
    gbpIndexData.parInstrumentSingleCurve = false;
    gbpIndexData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
    gbpIndexData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(gbpIndexData);

    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    sensiData->creditCcys()["BondIssuer1"] = "EUR";
    SensitivityScenarioData::CurveShiftParData bondData = createCurveShiftData();
    bondData.parInstruments = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
    bondData.parInstrumentSingleCurve = false;
    bondData.parInstrumentConventions["CDS"] = "CDS-STANDARD-CONVENTIONS";
    sensiData->creditCurveShiftData()["BondIssuer1"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(bondData);

    ore::analytics::SensitivityScenarioData::SpotShiftData eqsData;
    eqsData.shiftType = ShiftType::Relative;
    eqsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData eqvsData;
    eqvsData.shiftType = ShiftType::Relative;
    eqvsData.shiftSize = 0.01;
    eqvsData.shiftExpiries = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,
                              5 * Years, 7 * Years,  10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};

    ore::analytics::SensitivityScenarioData::CurveShiftData eqdivData;
    eqdivData.shiftType = ShiftType::Absolute;
    eqdivData.shiftSize = 0.00001;
    eqdivData.shiftTenors = {6 * Months, 1 * Years, 2 * Years};

    sensiData->equityShiftData()["SP5"] = eqsData;
    sensiData->equityShiftData()["Lufthansa"] = eqsData;

    sensiData->equityVolShiftData()["SP5"] = eqvsData;
    sensiData->equityVolShiftData()["Lufthansa"] = eqvsData;

    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftParData> yinfData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>();
    yinfData->shiftType = ShiftType::Absolute;
    yinfData->shiftSize = 0.0001;
    yinfData->shiftTenors = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years};
    yinfData->parInstruments = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
    yinfData->parInstrumentConventions["ZIS"] = "UKRP1";
    yinfData->parInstrumentConventions["YYS"] = "UKRP1";
    sensiData->yoyInflationCurveShiftData()["UKRP1"] = yinfData;

    auto yinfCfData = QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftParData>();
    yinfCfData->shiftType = ShiftType::Absolute;
    yinfCfData->shiftSize = 0.00001;
    yinfCfData->shiftExpiries = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                                 7 * Years, 10 * Years, 15 * Years, 20 * Years};
    yinfCfData->shiftStrikes = {-0.02, -0.01, 0.00, 0.01, 0.02, 0.03};
    yinfCfData->parInstruments = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
    yinfCfData->parInstrumentSingleCurve = false;
    yinfCfData->parInstrumentConventions["ZIS"] = "UKRP1";
    yinfCfData->parInstrumentConventions["YYS"] = "UKRP1";
    sensiData->yoyInflationCapFloorVolShiftData()["UKRP1"] = yinfCfData;

    return sensiData;
}

QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5(bool parConversion) {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData =
        QuantLib::ext::make_shared<SensitivityScenarioData>(parConversion);

    SensitivityScenarioData::SpotShiftData fxsData;

    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {5 * Years};

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {2 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {5 * Years, 10 * Years};

    SensitivityScenarioData::CdsVolShiftData cdsvsData;
    cdsvsData.shiftType = ShiftType::Relative;
    cdsvsData.shiftSize = 0.01;
    cdsvsData.shiftExpiries = {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};

    // sensiData->creditCurveShiftData()["dc"] = cvsData;

    SensitivityScenarioData::CurveShiftParData eurDiscountData = createCurveShiftData();
    eurDiscountData.parInstrumentSingleCurve = true;
    eurDiscountData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    eurDiscountData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["EUR"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(eurDiscountData);

    SensitivityScenarioData::CurveShiftParData usdDiscountData = createCurveShiftData();
    usdDiscountData.parInstrumentSingleCurve = true;
    usdDiscountData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
    usdDiscountData.parInstrumentConventions["IRS"] = "USD-3M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["USD"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(usdDiscountData);

    SensitivityScenarioData::CurveShiftParData gbpDiscountData = createCurveShiftData();
    gbpDiscountData.parInstrumentSingleCurve = true;
    gbpDiscountData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
    gbpDiscountData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["GBP"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(gbpDiscountData);

    SensitivityScenarioData::CurveShiftParData jpyDiscountData = createCurveShiftData();
    jpyDiscountData.parInstrumentSingleCurve = true;
    jpyDiscountData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
    jpyDiscountData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["JPY"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(jpyDiscountData);

    SensitivityScenarioData::CurveShiftParData chfDiscountData = createCurveShiftData();
    chfDiscountData.parInstrumentSingleCurve = true;
    chfDiscountData.parInstrumentConventions["DEP"] = "CHF-DEP-CONVENTIONS";
    chfDiscountData.parInstrumentConventions["IRS"] = "CHF-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["CHF"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(chfDiscountData);

    SensitivityScenarioData::CurveShiftParData bondData = createCurveShiftData();
    bondData.parInstrumentSingleCurve = true;
    bondData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    bondData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->yieldCurveShiftData()["BondCurve1"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(bondData);

    SensitivityScenarioData::CurveShiftParData eurIndexData = createCurveShiftData();
    eurIndexData.parInstrumentSingleCurve = false;
    eurIndexData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    eurIndexData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(eurIndexData);

    SensitivityScenarioData::CurveShiftParData usdIndexData = createCurveShiftData();
    usdIndexData.parInstrumentSingleCurve = false;
    usdIndexData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
    usdIndexData.parInstrumentConventions["IRS"] = "USD-3M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(usdIndexData);

    SensitivityScenarioData::CurveShiftParData gbpIndexData = createCurveShiftData();
    gbpIndexData.parInstrumentSingleCurve = false;
    gbpIndexData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
    gbpIndexData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(gbpIndexData);

    SensitivityScenarioData::CurveShiftParData jpyIndexData = createCurveShiftData();
    jpyIndexData.parInstrumentSingleCurve = false;
    jpyIndexData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
    jpyIndexData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(jpyIndexData);

    SensitivityScenarioData::CurveShiftParData chfIndexData = createCurveShiftData();
    chfIndexData.parInstrumentSingleCurve = false;
    chfIndexData.parInstrumentConventions["DEP"] = "CHF-DEP-CONVENTIONS";
    chfIndexData.parInstrumentConventions["IRS"] = "CHF-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(chfIndexData);

    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolShiftData()["EUR"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["EUR"]->indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["USD"]->indexName = "USD-LIBOR-3M";

    SensitivityScenarioData::CurveShiftParData dcData = createCurveShiftData();
    dcData.parInstruments = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
    dcData.parInstrumentSingleCurve = false;
    dcData.parInstrumentConventions["CDS"] = "CDS-STANDARD-CONVENTIONS";
    sensiData->creditCurveShiftData()["dc"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(dcData);
    sensiData->creditCcys()["dc"] = "EUR";

    SensitivityScenarioData::CurveShiftParData bondIssData = createCurveShiftData();
    bondIssData.parInstruments = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
    bondIssData.parInstrumentSingleCurve = false;
    bondIssData.parInstrumentConventions["CDS"] = "CDS-STANDARD-CONVENTIONS";
    sensiData->creditCurveShiftData()["BondIssuer1"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftParData>(bondIssData);
    sensiData->creditCcys()["BondIssuer1"] = "EUR";

    sensiData->cdsVolShiftData()["dc"] = cdsvsData;

    ore::analytics::SensitivityScenarioData::SpotShiftData eqsData;
    eqsData.shiftType = ShiftType::Relative;
    eqsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData eqvsData;
    eqvsData.shiftType = ShiftType::Relative;
    eqvsData.shiftSize = 0.01;
    eqvsData.shiftExpiries = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years, 3 * Years,
                              5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};

    ore::analytics::SensitivityScenarioData::CurveShiftData eqdivData;
    eqdivData.shiftType = ShiftType::Absolute;
    eqdivData.shiftSize = 0.00001;
    eqdivData.shiftTenors = {6 * Months, 1 * Years, 2 * Years};

    sensiData->equityShiftData()["SP5"] = eqsData;
    sensiData->equityShiftData()["Lufthansa"] = eqsData;

    sensiData->equityVolShiftData()["SP5"] = eqvsData;
    sensiData->equityVolShiftData()["Lufthansa"] = eqvsData;

    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftParData> yinfData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>();
    yinfData->shiftType = ShiftType::Absolute;
    yinfData->shiftSize = 0.0001;
    yinfData->shiftTenors = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years};
    yinfData->parInstruments = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
    yinfData->parInstrumentConventions["ZIS"] = "UKRP1";
    yinfData->parInstrumentConventions["YYS"] = "UKRP1";
    sensiData->yoyInflationCurveShiftData()["UKRP1"] = yinfData;

    auto yinfCfData = QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftParData>();
    yinfCfData->shiftType = ShiftType::Absolute;
    yinfCfData->shiftSize = 0.00001;
    yinfCfData->shiftExpiries = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                                 7 * Years, 10 * Years, 15 * Years, 20 * Years};
    yinfCfData->shiftStrikes = {-0.02, -0.01, 0.00, 0.01, 0.02, 0.03};
    yinfCfData->parInstruments = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
    yinfCfData->parInstrumentSingleCurve = false;
    yinfCfData->parInstrumentConventions["ZIS"] = "UKRP1";
    yinfCfData->parInstrumentConventions["YYS"] = "UKRP1";
    sensiData->yoyInflationCapFloorVolShiftData()["UKRP1"] = yinfCfData;

    return sensiData;
}

namespace testsuite {
    
void ParSensitivityAnalysisTest::testPortfolioZeroSensitivity() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity");

    SavedSettings backup;
    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5(false);
    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
        QuantLib::ext::make_shared<ore::analytics::DeltaScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build porfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("FxForward") = "DiscountedCashflows";
    data->engine("FxForward") = "DiscountingFxForwardEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    data->model("CapFloor") = "IborCapModel";
    data->engine("CapFloor") = "IborCapEngine";
    data->model("CapFlooredIborLeg") = "BlackOrBachelier";
    data->engine("CapFlooredIborLeg") = "BlackIborCouponPricer";
    data->model("YYCapFloor") = "YYCapModel";
    data->engine("YYCapFloor") = "YYCapEngine";
    data->model("IndexCreditDefaultSwapOption") = "Black";
    data->engine("IndexCreditDefaultSwapOption") = "BlackIndexCdsOptionEngine";
    map<string, string> engineParamMap1;
    engineParamMap1["Curve"] = "Underlying";
    data->engineParameters("IndexCreditDefaultSwapOption") = engineParamMap1;

    data->model("IndexCreditDefaultSwap") = "DiscountedCashflows";
    data->engine("IndexCreditDefaultSwap") = "MidPointIndexCdsEngine";
    map<string, string> engineParamMap2;
    engineParamMap2["Curve"] = "Underlying";
    data->engineParameters("IndexCreditDefaultSwap") = engineParamMap2;
    data->model("Bond") = "DiscountedCashflows";
    data->engine("Bond") = "DiscountingRiskyBondEngine";
    data->engineParameters("Bond")["TimestepPeriod"] = "6M";
    data->model("EquityOption") = "BlackScholesMerton";
    data->engine("EquityOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    // QuantLib::ext::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(buildSwap("1_Swap_EUR", "EUR", true, 10000000.0, 0, 10, 0.03, 0.00, "1Y",
                                                "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildSwap("2_Swap_USD", "USD", true, 10000000.0, 0, 15, 0.02, 0.00, "6M",
                                                "30/360", "3M", "A360", "USD-LIBOR-3M"));
    portfolio->add(buildSwap("3_Swap_GBP", "GBP", true, 10000000.0, 0, 20, 0.04, 0.00, "6M",
                                                "30/360", "3M", "A360", "GBP-LIBOR-6M"));
    portfolio->add(buildSwap("4_Swap_JPY", "JPY", true, 1000000000.0, 0, 5, 0.01, 0.00, "6M",
                                                "30/360", "3M", "A360", "JPY-LIBOR-6M"));
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 1000000.0, 10, 10,
                                                            0.02, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M",
                                                            "Physical"));
    portfolio->add(buildEuropeanSwaption("6_Swaption_EUR", "Long", "EUR", true, 1000000.0, 2, 5,
                                                            0.02, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M",
                                                            "Physical"));
    portfolio->add(buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10000000.0, "USD",
                                                    11000000.0));
    portfolio->add(buildFxOption("8_FxOption_EUR_GBP", "Long", "Call", 7, "EUR", 10000000.0, "GBP",
                                                    11000000.0));
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360",
                                               "EUR-EURIBOR-6M"));
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360",
                                                 "USD-LIBOR-3M"));

    // vector<string> names(1, "dc");
    // vector<string> ccys(1, "EUR");
    // vector<Real> notionals(1, 1000000.0);
    // QuantLib::ext::shared_ptr<CdsConvention> cdsConv =
    //     QuantLib::ext::dynamic_pointer_cast<CdsConvention>(InstrumentConventions::instance().conventions()->get("CDS-STANDARD-CONVENTIONS"));
    // portfolio->add(buildIndexCdsOption(
    //     "11_CDSOption_EUR", "dc", names, "Long", "EUR", ccys, true, notionals, 1000000.0, 1, 4, 0.4, 0.1,
    //     ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter()),
    //     "Physical"));
    portfolio->add(
        buildEquityOption("12_EquityOption_SP5", "Long", "Call", 2, "SP5", "USD", 2147.56, 1000));
    portfolio->add(buildEquityOption("13_EquityOption_Lufthansa", "Long", "Call", 2, "Lufthansa",
                                                        "EUR", 12.75, 775));

    portfolio->add(buildYYInflationCapFloor("14_YoYInflationCap_UKRPI", "GBP", 100000.0, true, true,
                                                               0.02, 0, 10, "1Y", "ACT/ACT", "UKRP1", "2M", 2));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());
    // build the scenario valuation engine
    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>(
        "1,0W"); // TODO - extend the DateGrid interface so that it can actually take a vector of dates as input
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(simMarketData->baseCcy()));
    ValuationEngine engine(today, dg, simMarket);
    // run scenarios and fill the cube
    boost::timer::cpu_timer t;
    QuantLib::ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        today, portfolio->ids(), vector<Date>(1, today), scenarioGenerator->samples());
    engine.buildCube(portfolio, cube, calculators);
    double elapsed = t.elapsed().wall * 1e-9;

    struct Results {
        string id;
        string label;
        Real npv;
        Real sensi;
    };

    std::vector<Results> cachedResults = {
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/0/6M", -928826, -2.51631},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/1/1Y", -928826, 14.6846},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/2/2Y", -928826, 19.0081},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/3/3Y", -928826, 46.1186},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/4/5Y", -928826, 85.1033},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/5/7Y", -928826, 149.43},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/6/10Y", -928826, 205.064},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/0/6M", -928826, 2.51644},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/1/1Y", -928826, -14.6863},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/2/2Y", -928826, -19.0137},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/3/3Y", -928826, -46.1338},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/4/5Y", -928826, -85.1406},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/5/7Y", -928826, -149.515},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/6/10Y", -928826, -205.239},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/0/6M", -928826, -495.013},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/1/1Y", -928826, 14.7304},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", -928826, 38.7816},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", -928826, 94.186},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", -928826, 173.125},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", -928826, 304.648},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", -928826, 8479.55},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/0/6M", -928826, 495.037},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/1/1Y", -928826, -14.5864},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", -928826, -38.4045},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", -928826, -93.532},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", -928826, -171.969},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", -928826, -302.864},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", -928826, -8478.14},
        {"2_Swap_USD", "Up:DiscountCurve/USD/0/6M", 980404, -1.04797},
        {"2_Swap_USD", "Up:DiscountCurve/USD/1/1Y", 980404, -6.06931},
        {"2_Swap_USD", "Up:DiscountCurve/USD/2/2Y", 980404, -15.8605},
        {"2_Swap_USD", "Up:DiscountCurve/USD/3/3Y", 980404, -38.0708},
        {"2_Swap_USD", "Up:DiscountCurve/USD/4/5Y", 980404, -68.7288},
        {"2_Swap_USD", "Up:DiscountCurve/USD/5/7Y", 980404, -118.405},
        {"2_Swap_USD", "Up:DiscountCurve/USD/6/10Y", 980404, -244.946},
        {"2_Swap_USD", "Up:DiscountCurve/USD/7/15Y", 980404, -202.226},
        {"2_Swap_USD", "Up:DiscountCurve/USD/8/20Y", 980404, 0.0148314},
        {"2_Swap_USD", "Down:DiscountCurve/USD/0/6M", 980404, 1.04797},
        {"2_Swap_USD", "Down:DiscountCurve/USD/1/1Y", 980404, 6.06959},
        {"2_Swap_USD", "Down:DiscountCurve/USD/2/2Y", 980404, 15.8623},
        {"2_Swap_USD", "Down:DiscountCurve/USD/3/3Y", 980404, 38.0784},
        {"2_Swap_USD", "Down:DiscountCurve/USD/4/5Y", 980404, 68.7502},
        {"2_Swap_USD", "Down:DiscountCurve/USD/5/7Y", 980404, 118.458},
        {"2_Swap_USD", "Down:DiscountCurve/USD/6/10Y", 980404, 245.108},
        {"2_Swap_USD", "Down:DiscountCurve/USD/7/15Y", 980404, 202.42},
        {"2_Swap_USD", "Down:DiscountCurve/USD/8/20Y", 980404, -0.0148314},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/0/6M", 980404, -201.015},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/1/1Y", 980404, 18.134},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/2/2Y", 980404, 47.3066},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/3/3Y", 980404, 113.4},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/4/5Y", 980404, 205.068},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/5/7Y", 980404, 352.859},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/6/10Y", 980404, 730.076},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/7/15Y", 980404, 8626.78},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/8/20Y", 980404, 5.86437},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/0/6M", 980404, 201.03},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/1/1Y", 980404, -18.0746},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/2/2Y", 980404, -47.1526},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/3/3Y", 980404, -113.136},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/4/5Y", 980404, -204.611},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/5/7Y", 980404, -352.166},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/6/10Y", 980404, -729.248},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/7/15Y", 980404, -8626.13},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/8/20Y", 980404, -5.86436},
        {"2_Swap_USD", "Up:FXSpot/EURUSD/0/spot", 980404, -9706.97},
        {"2_Swap_USD", "Down:FXSpot/EURUSD/0/spot", 980404, 9903.07},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/0/6M", 69795.3, 2.12392},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/1/1Y", 69795.3, -0.646097},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/2/2Y", 69795.3, -1.75066},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/3/3Y", 69795.3, -4.24827},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/4/5Y", 69795.3, -7.2252},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/5/7Y", 69795.3, -12.5287},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/6/10Y", 69795.3, -24.7828},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/7/15Y", 69795.3, -39.2456},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/8/20Y", 69795.3, 31.2081},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/0/6M", 69795.3, -2.12413},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/1/1Y", 69795.3, 0.645698},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/2/2Y", 69795.3, 1.74981},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/3/3Y", 69795.3, 4.2473},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/4/5Y", 69795.3, 7.22426},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/5/7Y", 69795.3, 12.5298},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/6/10Y", 69795.3, 24.7939},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/7/15Y", 69795.3, 39.2773},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/8/20Y", 69795.3, -31.2925},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/0/6M", 69795.3, -308.49},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/1/1Y", 69795.3, 68.819},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/2/2Y", 69795.3, 81.3735},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/3/3Y", 69795.3, 239.034},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/4/5Y", 69795.3, 372.209},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/5/7Y", 69795.3, 654.949},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/6/10Y", 69795.3, 1343.01},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/7/15Y", 69795.3, 2139.68},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/8/20Y", 69795.3, 12633.8},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/0/6M", 69795.3, 308.513},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/1/1Y", 69795.3, -68.7287},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/2/2Y", 69795.3, -81.1438},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/3/3Y", 69795.3, -238.649},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/4/5Y", 69795.3, -371.553},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/5/7Y", 69795.3, -653.972},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/6/10Y", 69795.3, -1341.88},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/7/15Y", 69795.3, -2138.11},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/8/20Y", 69795.3, -12632.5},
        {"3_Swap_GBP", "Up:FXSpot/EURGBP/0/spot", 69795.3, -691.043},
        {"3_Swap_GBP", "Down:FXSpot/EURGBP/0/spot", 69795.3, 705.003},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/0/6M", 871.03, -0.00750246},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/1/1Y", 871.03, -0.00147994},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/2/2Y", 871.03, -0.020079},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/3/3Y", 871.03, -0.0667249},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/4/5Y", 871.03, 4.75708},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/0/6M", 871.03, 0.00747801},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/1/1Y", 871.03, 0.00140807},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/2/2Y", 871.03, 0.0199001},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/3/3Y", 871.03, 0.0664106},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/4/5Y", 871.03, -4.75978},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/0/6M", 871.03, -193.514},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/1/1Y", 871.03, 2.95767},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/2/2Y", 871.03, 7.81453},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/3/3Y", 871.03, 19.3576},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/4/5Y", 871.03, 3832.83},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/0/6M", 871.03, 193.528},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/1/1Y", 871.03, -2.90067},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/2/2Y", 871.03, -7.6631},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/3/3Y", 871.03, -19.0907},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/4/5Y", 871.03, -3832.59},
        {"4_Swap_JPY", "Up:FXSpot/EURJPY/0/spot", 871.03, -8.62406},
        {"4_Swap_JPY", "Down:FXSpot/EURJPY/0/spot", 871.03, 8.79829},
        {"5_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 37524.3, -10.0118},
        {"5_Swaption_EUR", "Up:DiscountCurve/EUR/7/15Y", 37524.3, -28.0892},
        {"5_Swaption_EUR", "Up:DiscountCurve/EUR/8/20Y", 37524.3, -17.527},
        {"5_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 37524.3, 10.0186},
        {"5_Swaption_EUR", "Down:DiscountCurve/EUR/7/15Y", 37524.3, 28.117},
        {"5_Swaption_EUR", "Down:DiscountCurve/EUR/8/20Y", 37524.3, 17.5502},
        {"5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 37524.3, -395.215},
        {"5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/7/15Y", 37524.3, 56.7319},
        {"5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/8/20Y", 37524.3, 722.287},
        {"5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 37524.3, 397.907},
        {"5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/7/15Y", 37524.3, -56.508},
        {"5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/8/20Y", 37524.3, -713.45},
        {"5_Swaption_EUR", "Up:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 37524.3, 367.609},
        {"5_Swaption_EUR", "Down:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 37524.3, -367.608},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/2/2Y", 10738, -0.485552},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/3/3Y", 10738, -1.09018},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/4/5Y", 10738, -1.98726},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/5/7Y", 10738, -0.591243},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 10738, 0.00670807},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/2/2Y", 10738, 0.485614},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/3/3Y", 10738, 1.09029},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/4/5Y", 10738, 1.9877},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/5/7Y", 10738, 0.591282},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 10738, -0.00670808},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 10738, -97.3791},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 10738, 4.0232},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 10738, 8.90271},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 10738, 322.893},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 10738, 1.23647},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 10738, 97.9474},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 10738, -3.98874},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 10738, -8.83916},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 10738, -316.846},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 10738, -1.23638},
        {"6_Swaption_EUR", "Up:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 10738, 102.503},
        {"6_Swaption_EUR", "Up:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 10738, 0.187152},
        {"6_Swaption_EUR", "Down:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 10738, -102.502},
        {"6_Swaption_EUR", "Down:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 10738, -0.187152},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/EUR/3/3Y", 1.36968e+06, -2107.81},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/EUR/4/5Y", 1.36968e+06, -3.85768},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/USD/3/3Y", 1.36968e+06, 1698.91},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/USD/4/5Y", 1.36968e+06, 3.10717},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/EUR/3/3Y", 1.36968e+06, 2109.74},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/EUR/4/5Y", 1.36968e+06, 3.85768},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/USD/3/3Y", 1.36968e+06, -1698.12},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/USD/4/5Y", 1.36968e+06, -3.10717},
        {"7_FxOption_EUR_USD", "Up:FXSpot/EURUSD/0/spot", 1.36968e+06, 56850.7},
        {"7_FxOption_EUR_USD", "Down:FXSpot/EURUSD/0/spot", 1.36968e+06, -56537.6},
        {"7_FxOption_EUR_USD", "Up:FXVolatility/EURUSD/0/5Y/ATM", 1.36968e+06, 672236},
        {"7_FxOption_EUR_USD", "Down:FXVolatility/EURUSD/0/5Y/ATM", 1.36968e+06, -329688},
        {"8_FxOption_EUR_GBP", "Up:DiscountCurve/EUR/5/7Y", 798336, -2435.22},
        {"8_FxOption_EUR_GBP", "Up:DiscountCurve/GBP/5/7Y", 798336, 1880.89},
        {"8_FxOption_EUR_GBP", "Down:DiscountCurve/EUR/5/7Y", 798336, 2441.08},
        {"8_FxOption_EUR_GBP", "Down:DiscountCurve/GBP/5/7Y", 798336, -1878.05},
        {"8_FxOption_EUR_GBP", "Up:FXSpot/EURGBP/0/spot", 798336, 27009.9},
        {"8_FxOption_EUR_GBP", "Down:FXSpot/EURGBP/0/spot", 798336, -26700.2},
        {"8_FxOption_EUR_GBP", "Up:FXVolatility/EURGBP/0/5Y/ATM", 798336, 1.36635e+06},
        {"8_FxOption_EUR_GBP", "Down:FXVolatility/EURGBP/0/5Y/ATM", 798336, -798336},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/2/2Y", 289.105, -7.28588e-07},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/3/3Y", 289.105, -0.000381869},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/4/5Y", 289.105, -0.00790528},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/5/7Y", 289.105, -0.0764893},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/6/10Y", 289.105, -0.162697},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/2/2Y", 289.105, 7.28664e-07},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/3/3Y", 289.105, 0.000381934},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/4/5Y", 289.105, 0.00790776},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/5/7Y", 289.105, 0.0765231},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/6/10Y", 289.105, 0.162824},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/1/1Y", 289.105, -1.81582e-05},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 289.105, -0.00670729},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 289.105, -0.330895},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 289.105, -2.03937},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 289.105, -6.42991},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 289.105, 15.5182},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/1/1Y", 289.105, 1.97218e-05},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 289.105, 0.00746096},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 289.105, 0.353405},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 289.105, 2.24481},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 289.105, 7.1522},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 289.105, -14.6675},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/4/1Y/0.05", 289.105, 8.49293e-05},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/9/2Y/0.05", 289.105, 0.0150901},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/14/3Y/0.05", 289.105, 0.620393},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/19/5Y/0.05", 289.105, 17.2057},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/24/10Y/0.05", 289.105, 24.4267},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/4/1Y/0.05", 289.105, -6.97789e-05},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/9/2Y/0.05", 289.105, -0.0125099},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/14/3Y/0.05", 289.105, -0.554344},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/19/5Y/0.05", 289.105, -16.1212},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/24/10Y/0.05", 289.105, -23.0264},
        {"10_Floor_USD", "Up:DiscountCurve/USD/0/6M", 3406.46, -7.03494e-09},
        {"10_Floor_USD", "Up:DiscountCurve/USD/1/1Y", 3406.46, -8.41429e-05},
        {"10_Floor_USD", "Up:DiscountCurve/USD/2/2Y", 3406.46, -0.00329744},
        {"10_Floor_USD", "Up:DiscountCurve/USD/3/3Y", 3406.46, -0.053884},
        {"10_Floor_USD", "Up:DiscountCurve/USD/4/5Y", 3406.46, -0.269714},
        {"10_Floor_USD", "Up:DiscountCurve/USD/5/7Y", 3406.46, -0.989583},
        {"10_Floor_USD", "Up:DiscountCurve/USD/6/10Y", 3406.46, -1.26544},
        {"10_Floor_USD", "Down:DiscountCurve/USD/0/6M", 3406.46, 7.0354e-09},
        {"10_Floor_USD", "Down:DiscountCurve/USD/1/1Y", 3406.46, 8.41464e-05},
        {"10_Floor_USD", "Down:DiscountCurve/USD/2/2Y", 3406.46, 0.00329786},
        {"10_Floor_USD", "Down:DiscountCurve/USD/3/3Y", 3406.46, 0.0538949},
        {"10_Floor_USD", "Down:DiscountCurve/USD/4/5Y", 3406.46, 0.269802},
        {"10_Floor_USD", "Down:DiscountCurve/USD/5/7Y", 3406.46, 0.990038},
        {"10_Floor_USD", "Down:DiscountCurve/USD/6/10Y", 3406.46, 1.26635},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/0/6M", 3406.46, 0.00150733},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/1/1Y", 3406.46, 0.240284},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/2/2Y", 3406.46, 2.17175},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/3/3Y", 3406.46, 7.77249},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/4/5Y", 3406.46, 12.9642},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/5/7Y", 3406.46, 16.8269},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/6/10Y", 3406.46, -81.4363},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/0/6M", 3406.46, -0.00139804},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/1/1Y", 3406.46, -0.230558},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/2/2Y", 3406.46, -2.00123},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/3/3Y", 3406.46, -7.14862},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/4/5Y", 3406.46, -11.2003},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/5/7Y", 3406.46, -13.7183},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/6/10Y", 3406.46, 84.0113},
        {"10_Floor_USD", "Up:FXSpot/EURUSD/0/spot", 3406.46, -33.7273},
        {"10_Floor_USD", "Down:FXSpot/EURUSD/0/spot", 3406.46, 34.4087},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/0/1Y/0.01", 3406.46, 0.402913},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/5/2Y/0.01", 3406.46, 3.32861},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/10/3Y/0.01", 3406.46, 16.8798},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/15/5Y/0.01", 3406.46, 96.415},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/20/10Y/0.01", 3406.46, 92.2212},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/0/1Y/0.01", 3406.46, -0.37428},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/5/2Y/0.01", 3406.46, -3.14445},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/10/3Y/0.01", 3406.46, -16.3074},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/15/5Y/0.01", 3406.46, -94.5309},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/20/10Y/0.01", 3406.46, -90.9303},
        // {"11_CDSOption_EUR", "Up:DiscountCurve/EUR/0/6M", 56003.6, -0.0395449},
        // {"11_CDSOption_EUR", "Up:DiscountCurve/EUR/1/1Y", 56003.6, -5.57561},
        // {"11_CDSOption_EUR", "Up:DiscountCurve/EUR/2/2Y", 56003.6, 0.0377165},
        // {"11_CDSOption_EUR", "Up:DiscountCurve/EUR/3/3Y", 56003.6, 0.0799626},
        // {"11_CDSOption_EUR", "Up:DiscountCurve/EUR/4/5Y", 56003.6, 0.0998674},
        // {"11_CDSOption_EUR", "Up:DiscountCurve/EUR/5/7Y", 56003.6, 0.00709491},
        // {"11_CDSOption_EUR", "Up:SurvivalProbability/dc/0/6M", 56003.6, -0.00227111},
        // {"11_CDSOption_EUR", "Up:SurvivalProbability/dc/1/1Y", 56003.6, 53.186},
        // {"11_CDSOption_EUR", "Up:SurvivalProbability/dc/2/2Y", 56003.6, 0.206306},
        // {"11_CDSOption_EUR", "Up:SurvivalProbability/dc/3/3Y", 56003.6, 0.396504},
        // {"11_CDSOption_EUR", "Up:SurvivalProbability/dc/4/5Y", 56003.6, 2.63},
        // {"11_CDSOption_EUR", "Up:SurvivalProbability/dc/5/7Y", 56003.6, 0.259055},
        // {"11_CDSOption_EUR", "Down:DiscountCurve/EUR/0/6M", 56003.6, 0.0395562},
        // {"11_CDSOption_EUR", "Down:DiscountCurve/EUR/1/1Y", 56003.6, 5.57616},
        // {"11_CDSOption_EUR", "Down:DiscountCurve/EUR/2/2Y", 56003.6, -0.0377084},
        // {"11_CDSOption_EUR", "Down:DiscountCurve/EUR/3/3Y", 56003.6, -0.0799231},
        // {"11_CDSOption_EUR", "Down:DiscountCurve/EUR/4/5Y", 56003.6, -0.0926819},
        // {"11_CDSOption_EUR", "Down:DiscountCurve/EUR/5/7Y", 56003.6, -0.00709513},
        // {"11_CDSOption_EUR", "Down:SurvivalProbability/dc/0/6M", 56003.6, 0.00227126},
        // {"11_CDSOption_EUR", "Down:SurvivalProbability/dc/1/1Y", 56003.6, -53.1913},
        // {"11_CDSOption_EUR", "Down:SurvivalProbability/dc/2/2Y", 56003.6, -0.206064},
        // {"11_CDSOption_EUR", "Down:SurvivalProbability/dc/3/3Y", 56003.6, -0.395576},
        // {"11_CDSOption_EUR", "Down:SurvivalProbability/dc/4/5Y", 56003.6, -2.58855},
        // {"11_CDSOption_EUR", "Down:SurvivalProbability/dc/5/7Y", 56003.6, -0.258819},
        // {"11_CDSOption_EUR", "Up:CDSVolatility/dc/1/1Y/ATM", 56003.6, 10.5781},
        // {"11_CDSOption_EUR", "Down:CDSVolatility/dc/1/1Y/ATM", 56003.6, -10.0622},
        { "12_EquityOption_SP5", "Up:DiscountCurve/USD/2/2Y", 278936, 158.718 },
        { "12_EquityOption_SP5", "Up:DiscountCurve/USD/3/3Y", 278936, 1.31198 },
        { "12_EquityOption_SP5", "Down:DiscountCurve/USD/2/2Y", 278936, -158.676 },
        { "12_EquityOption_SP5", "Down:DiscountCurve/USD/3/3Y", 278936, -1.31197 },
        { "12_EquityOption_SP5", "Up:FXSpot/EURUSD/0/spot", 278936, -2761.74 },
        { "12_EquityOption_SP5", "Down:FXSpot/EURUSD/0/spot", 278936, 2817.53 },
        { "12_EquityOption_SP5", "Up:EquitySpot/SP5/0/spot", 278936, 10869.4 },
        { "12_EquityOption_SP5", "Down:EquitySpot/SP5/0/spot", 278936, -10681 },
        { "12_EquityOption_SP5", "Up:EquityVolatility/SP5/5/2Y/ATM", 278936, 2388.21 },
        { "12_EquityOption_SP5", "Down:EquityVolatility/SP5/5/2Y/ATM", 278936, -2388.67 },
        { "13_EquityOption_Lufthansa", "Up:DiscountCurve/EUR/2/2Y", 1830.8, 0.854602 },
        { "13_EquityOption_Lufthansa", "Up:DiscountCurve/EUR/3/3Y", 1830.8, 0.0070644 },
        { "13_EquityOption_Lufthansa", "Down:DiscountCurve/EUR/2/2Y", 1830.8, -0.854422 },
        { "13_EquityOption_Lufthansa", "Down:DiscountCurve/EUR/3/3Y", 1830.8, -0.00706439 },
        { "13_EquityOption_Lufthansa", "Up:EquitySpot/Lufthansa/0/spot", 1830.8, 61.7512 },
        { "13_EquityOption_Lufthansa", "Down:EquitySpot/Lufthansa/0/spot", 1830.8, -60.866 },
        { "13_EquityOption_Lufthansa", "Up:EquityVolatility/Lufthansa/5/2Y/ATM", 1830.8, 15.975 },
        { "13_EquityOption_Lufthansa", "Down:EquityVolatility/Lufthansa/5/2Y/ATM", 1830.8, -15.9808 },
        {"14_YoYInflationCap_UKRPI", "Up:DiscountCurve/GBP/1/1Y", 3495.36, -0.0190824},
        {"14_YoYInflationCap_UKRPI", "Up:DiscountCurve/GBP/2/2Y", 3495.36, -0.0518755},
        {"14_YoYInflationCap_UKRPI", "Up:DiscountCurve/GBP/3/3Y", 3495.36, -0.159743},
        {"14_YoYInflationCap_UKRPI", "Up:DiscountCurve/GBP/4/5Y", 3495.36, -0.346412},
        {"14_YoYInflationCap_UKRPI", "Up:DiscountCurve/GBP/5/7Y", 3495.36, -0.701974},
        {"14_YoYInflationCap_UKRPI", "Up:DiscountCurve/GBP/6/10Y", 3495.36, -0.837257},
        {"14_YoYInflationCap_UKRPI", "Down:DiscountCurve/GBP/1/1Y", 3495.36, 0.0190843},
        {"14_YoYInflationCap_UKRPI", "Down:DiscountCurve/GBP/2/2Y", 3495.36, 0.0518857},
        {"14_YoYInflationCap_UKRPI", "Down:DiscountCurve/GBP/3/3Y", 3495.36, 0.159784},
        {"14_YoYInflationCap_UKRPI", "Down:DiscountCurve/GBP/4/5Y", 3495.36, 0.346541},
        {"14_YoYInflationCap_UKRPI", "Down:DiscountCurve/GBP/5/7Y", 3495.36, 0.702328},
        {"14_YoYInflationCap_UKRPI", "Down:DiscountCurve/GBP/6/10Y", 3495.36, 0.83791},
        {"14_YoYInflationCap_UKRPI", "Up:FXSpot/EURGBP/0/spot", 3495.36, -34.6075},
        {"14_YoYInflationCap_UKRPI", "Down:FXSpot/EURGBP/0/spot", 3495.36, 35.3067},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCurve/UKRP1/0/1Y", 3495.36, 6.11718},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCurve/UKRP1/1/2Y", 3495.36, 5.77751},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCurve/UKRP1/2/3Y", 3495.36, 8.22785},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCurve/UKRP1/3/5Y", 3495.36, 10.2605},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCurve/UKRP1/4/7Y", 3495.36, 11.7006},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCurve/UKRP1/5/10Y", 3495.36, 8.6242},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCurve/UKRP1/0/1Y", 3495.36, -5.99639},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCurve/UKRP1/1/2Y", 3495.36, -5.6966},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCurve/UKRP1/2/3Y", 3495.36, -8.15092},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCurve/UKRP1/3/5Y", 3495.36, -10.1917},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCurve/UKRP1/4/7Y", 3495.36, -11.6375},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCurve/UKRP1/5/10Y", 3495.36, -8.58138},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCapFloorVolatility/UKRP1/4/1Y/0.02", 3495.36, 0.706362},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCapFloorVolatility/UKRP1/10/2Y/0.02", 3495.36, 0.575052},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCapFloorVolatility/UKRP1/16/3Y/0.02", 3495.36, 1.21162},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCapFloorVolatility/UKRP1/22/5Y/0.02", 3495.36, 1.83575},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCapFloorVolatility/UKRP1/28/7Y/0.02", 3495.36, 2.52242},
        {"14_YoYInflationCap_UKRPI", "Up:YoYInflationCapFloorVolatility/UKRP1/34/10Y/0.02", 3495.36, 1.8872},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCapFloorVolatility/UKRP1/4/1Y/0.02", 3495.36, -0.706362},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCapFloorVolatility/UKRP1/10/2Y/0.02", 3495.36, -0.575052},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCapFloorVolatility/UKRP1/16/3Y/0.02", 3495.36, -1.21162},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCapFloorVolatility/UKRP1/22/5Y/0.02", 3495.36, -1.83575},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCapFloorVolatility/UKRP1/28/7Y/0.02", 3495.36, -2.52242},
        {"14_YoYInflationCap_UKRPI", "Down:YoYInflationCapFloorVolatility/UKRP1/34/10Y/0.02", 3495.36, -1.8872}};

    std::map<pair<string, string>, Real> npvMap, sensiMap;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        pair<string, string> p(cachedResults[i].id, cachedResults[i].label);
        npvMap[p] = cachedResults[i].npv;
        sensiMap[p] = cachedResults[i].sensi;
    }
    vector<pair<string, string>> ids;
    Real tiny = 1.0e-10;
    Real tolerance = 0.01;
    Size count = 0;
    vector<ShiftScenarioGenerator::ScenarioDescription> desc = scenarioGenerator->scenarioDescriptions();
    size_t currentTradeIdx = 0;
    for (const auto& [tradeId, _] : portfolio->trades()) {
        Real npv0 = cube->getT0(currentTradeIdx, 0);
        for (Size j = 1; j < scenarioGenerator->samples(); ++j) { // skip j = 0, this is the base scenario
            Real npv = cube->get(currentTradeIdx, 0, j, 0);
            Real sensi = npv - npv0;
            string label = to_string(desc[j]);
            if (fabs(sensi) > tiny) {
                count++;

                pair<string, string> p(tradeId, label);
                ids.push_back(p);
                QL_REQUIRE(npvMap.find(p) != npvMap.end(),
                           "pair (" << p.first << ", " << p.second << ") not found in npv map");
                QL_REQUIRE(sensiMap.find(p) != sensiMap.end(),
                           "pair (" << p.first << ", " << p.second << ") not found in sensi map");
                BOOST_CHECK_MESSAGE(fabs(npv0 - npvMap[p]) < tolerance || fabs((npv0 - npvMap[p]) / npv0) < tolerance,
                                    "npv regression failed for pair (" << p.first << ", " << p.second << "): " << npv0
                                                                       << " vs " << npvMap[p]);
                BOOST_CHECK_MESSAGE(fabs(sensi - sensiMap[p]) < tolerance ||
                                        fabs((sensi - sensiMap[p]) / sensi) < tolerance,
                                    "sensitivity regression failed for pair ("
                                        << p.first << ", " << p.second << "): " << sensi << " vs " << sensiMap[p]);
                // uncomment to output code for cached results
                // BOOST_TEST_MESSAGE("{\"" << p.first << "\", \"" << p.second << "\", " << npv0 << ", " << sensi <<
                // "},");
            }
        }
        currentTradeIdx++;
    }
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero sensitivities ("
                                                           << count << ") do not match regression data ("
                                                           << cachedResults.size() << ")");

    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

void testParConversion(ObservationMode::Mode om) {

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(om);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");
    ccys.push_back("CHF");
    ccys.push_back("USD");
    ccys.push_back("JPY");

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5(true);

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
        QuantLib::ext::make_shared<ore::analytics::DeltaScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build porfolio
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    engineData->model("CrossCurrencySwap") = "DiscountedCashflows";
    engineData->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("FxForward") = "DiscountedCashflows";
    engineData->engine("FxForward") = "DiscountingFxForwardEngine";
    engineData->model("FxOption") = "GarmanKohlhagen";
    engineData->engine("FxOption") = "AnalyticEuropeanEngine";
    engineData->model("CapFloor") = "IborCapModel";
    engineData->engine("CapFloor") = "IborCapEngine";
    engineData->model("CapFlooredIborLeg") = "BlackOrBachelier";
    engineData->engine("CapFlooredIborLeg") = "BlackIborCouponPricer";
    engineData->model("YYCapFloor") = "YYCapModel";
    engineData->engine("YYCapFloor") = "YYCapEngine";
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    engineData->engineParameters("Bond")["TimestepPeriod"] = "6M";
    engineData->model("CreditDefaultSwap") = "DiscountedCashflows";
    engineData->engine("CreditDefaultSwap") = "MidPointCdsEngine";
    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, simMarket);

    // QuantLib::ext::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(buildSwap("1_Swap_EUR", "EUR", true, 10000000.0, 0, 10, 0.03, 0.00, "1Y",
                                                "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildSwap("2_Swap_USD", "USD", true, 10000000.0, 0, 15, 0.02, 0.00, "6M",
                                                "30/360", "3M", "A360", "USD-LIBOR-3M"));
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360",
                                               "EUR-EURIBOR-6M"));
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360",
                                                 "USD-LIBOR-3M"));
    portfolio->add(buildZeroBond("11_ZeroBond_EUR", "EUR", 1000000.0, 10));
    portfolio->add(buildZeroBond("12_ZeroBond_USD", "USD", 1000000.0, 10));
    portfolio->add(
        buildEquityOption("13_EquityOption_SP5", "Long", "Call", 2, "SP5", "USD", 2147.56, 1000));
    portfolio->add(buildYYInflationCapFloor("14_YoYInflationCap_UKRPI", "GBP", 100000.0, true, true,
                                                               0.02, 0, 10, "1Y", "ACT/ACT", "UKRP1", "2M", 2));
    portfolio->build(factory);
    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the sensitivity analysis object
    // first build the par analysis object, so that we can align the pillars for the zero sensi analysis
    ParSensitivityAnalysis parAnalysis(today, simMarketData, *sensiData, Market::defaultConfiguration);
    parAnalysis.alignPillars();
    QuantLib::ext::shared_ptr<SensitivityAnalysis> zeroAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, engineData, simMarketData, sensiData, false);
    BOOST_TEST_MESSAGE("SensitivityAnalysis object built");
    zeroAnalysis->overrideTenors(true);
    zeroAnalysis->generateSensitivities();
    BOOST_TEST_MESSAGE("Raw sensitivity analsis done");
    BOOST_TEST_MESSAGE("Par sensitivity analsis object built");

    // RL: CRITICAL CHANGE
    //     // TODO: should the next two lines not come before call to zeroAnalysis->generateSensitivities();?
    //     // PC, 30-07-2018, yes they should, cached results have to be updated then (postponed...)
    //     parAnalysis.alignPillars();
    //     zeroAnalysis->overrideTenors(true);
    parAnalysis.computeParInstrumentSensitivities(zeroAnalysis->simMarket());
    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
        QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis.parSensitivities(), parAnalysis.shiftSizes());
    QuantLib::ext::shared_ptr<SensitivityCube> sensiCube = zeroAnalysis->sensiCube();
    ZeroToParCube parCube(sensiCube, parConverter);

    map<pair<string, string>, Real> parDelta;
    for (const auto& tradeId : portfolio->ids()) {
        // Fill the par deltas map
        auto temp = parCube.parDeltas(tradeId);
        for (const auto& kv : temp) {
            // We only care about the actual "converted" par deltas here
            if (ParSensitivityAnalysis::isParType(kv.first.keytype)) {
                string des = sensiCube->factorDescription(kv.first);
                parDelta[make_pair(tradeId, des)] = kv.second;
            }
        }
    }

    struct Results {
        string id;
        string label;
        Real sensi;
    };

    std::vector<Results> cachedResults = {
        {"10_Floor_USD", "DiscountCurve/USD/0/6M", -0.00112886},
        {"10_Floor_USD", "DiscountCurve/USD/1/1Y", 0.00675206},
        {"10_Floor_USD", "DiscountCurve/USD/2/2Y", 0.00900048},
        {"10_Floor_USD", "DiscountCurve/USD/3/3Y", -0.0302434},
        {"10_Floor_USD", "DiscountCurve/USD/4/5Y", -0.262464},
        {"10_Floor_USD", "DiscountCurve/USD/5/7Y", -1.07006},
        {"10_Floor_USD", "DiscountCurve/USD/6/10Y", -1.04325},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/0/6M", 0.00386584},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", 0.2381},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", 2.2426},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/3/3Y", 7.56822},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/4/5Y", 15.9842},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/5/7Y", 22.2464},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/6/10Y", -89.3588},
        {"10_Floor_USD", "OptionletVolatility/USD/0/1Y/0.01", -0.622505},
        {"10_Floor_USD", "OptionletVolatility/USD/10/3Y/0.01", -2.20215},
        {"10_Floor_USD", "OptionletVolatility/USD/15/5Y/0.01", 1.77487},
        {"10_Floor_USD", "OptionletVolatility/USD/20/10Y/0.01", 207.854},
        {"10_Floor_USD", "OptionletVolatility/USD/5/2Y/0.01", 1.78417},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/0/6M", 1.53634},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/1/1Y", 3.53444},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/2/2Y", 8.6117},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/3/3Y", 18.5064},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/4/5Y", 39.4197},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/5/7Y", 36.4505},
        {"11_ZeroBond_EUR", "SurvivalProbability/BondIssuer1/6/10Y", -600.06},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/0/6M", -0.657215},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/1/1Y", 2.95782},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/2/2Y", 6.06677},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/3/3Y", 14.1153},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/4/5Y", 32.8224},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/5/7Y", 58.069},
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve1/6/10Y", -690.301},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/0/6M", 1.28029},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/1/1Y", 2.94537},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/2/2Y", 7.17642},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/3/3Y", 15.422},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/4/5Y", 32.8498},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/5/7Y", 30.3754},
        {"12_ZeroBond_USD", "SurvivalProbability/BondIssuer1/6/10Y", -500.05},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/0/6M", -0.547679},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/1/1Y", 2.46485},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/2/2Y", 5.05564},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/3/3Y", 11.7627},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/4/5Y", 27.352},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/5/7Y", 48.3909},
        {"12_ZeroBond_USD", "YieldCurve/BondCurve1/6/10Y", -575.251},
        {"13_EquityOption_SP5", "DiscountCurve/USD/0/6M", 0.270388},
        {"13_EquityOption_SP5", "DiscountCurve/USD/1/1Y", -1.35418},
        {"13_EquityOption_SP5", "DiscountCurve/USD/2/2Y", 158.893},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/0/6M", 0.00347664},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/1/1Y", -0.00921372},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/2/2Y", -0.0271867},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/3/3Y", -0.0973079},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/4/5Y", -0.298947},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/5/7Y", -0.69657},
        {"14_YoYInflationCap_UKRPI", "DiscountCurve/GBP/6/10Y", -0.950666},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCapFloorVolatility/UKRP1/10/2Y/0.02", 0.131713},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCapFloorVolatility/UKRP1/16/3Y/0.02", -0.155071},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCapFloorVolatility/UKRP1/22/5Y/0.02", 0.336249},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCapFloorVolatility/UKRP1/28/7Y/0.02", -0.585254},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCapFloorVolatility/UKRP1/34/10Y/0.02", 9.11852},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCapFloorVolatility/UKRP1/4/1Y/0.02", -0.0981938},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCurve/UKRP1/0/1Y", -0.501498},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCurve/UKRP1/1/2Y", 0.104595},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCurve/UKRP1/2/3Y", -0.258415},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCurve/UKRP1/3/5Y", 1.13565},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCurve/UKRP1/4/7Y", -2.64434},
        {"14_YoYInflationCap_UKRPI", "YoYInflationCurve/UKRP1/5/10Y", 52.8805},
        {"1_Swap_EUR", "DiscountCurve/EUR/0/6M", 3.55166},
        {"1_Swap_EUR", "DiscountCurve/EUR/1/1Y", 8.07755},
        {"1_Swap_EUR", "DiscountCurve/EUR/2/2Y", 15.787},
        {"1_Swap_EUR", "DiscountCurve/EUR/3/3Y", 36.2307},
        {"1_Swap_EUR", "DiscountCurve/EUR/4/5Y", 81.6737},
        {"1_Swap_EUR", "DiscountCurve/EUR/5/7Y", 146.97},
        {"1_Swap_EUR", "DiscountCurve/EUR/6/10Y", 170.249},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/0/6M", -492.385},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", 0.267094},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.0571774},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.00710812},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.201881},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 34.3404},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 8928.34},
        {"2_Swap_USD", "DiscountCurve/USD/0/6M", -1.47948},
        {"2_Swap_USD", "DiscountCurve/USD/1/1Y", -3.99176},
        {"2_Swap_USD", "DiscountCurve/USD/2/2Y", -10.9621},
        {"2_Swap_USD", "DiscountCurve/USD/3/3Y", -25.1411},
        {"2_Swap_USD", "DiscountCurve/USD/4/5Y", -57.393},
        {"2_Swap_USD", "DiscountCurve/USD/5/7Y", -103.903},
        {"2_Swap_USD", "DiscountCurve/USD/6/10Y", -250.483},
        {"2_Swap_USD", "DiscountCurve/USD/7/15Y", -269.282},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/0/6M", -198.455},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", 0.163363},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.0310057},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.00237856},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.126057},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/5/7Y", 0.117712},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/6/10Y", 10.6825},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/7/15Y", 9972.55},
        {"9_Cap_EUR", "DiscountCurve/EUR/0/6M", 0.000267715},
        {"9_Cap_EUR", "DiscountCurve/EUR/1/1Y", 1.93692e-06},
        {"9_Cap_EUR", "DiscountCurve/EUR/2/2Y", 0.00120582},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", 0.0038175},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", 0.00870478},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", -0.0375854},
        {"9_Cap_EUR", "DiscountCurve/EUR/6/10Y", -0.25186},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/0/6M", 0.000685155},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", -0.00175651},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.0118899},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.301921},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -2.28152},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -7.16938},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 16.3599},
        {"9_Cap_EUR", "OptionletVolatility/EUR/14/3Y/0.05", -0.0903623},
        {"9_Cap_EUR", "OptionletVolatility/EUR/19/5Y/0.05", 0.0577696},
        {"9_Cap_EUR", "OptionletVolatility/EUR/24/10Y/0.05", 41.9784},
        {"9_Cap_EUR", "OptionletVolatility/EUR/4/1Y/0.05", -0.0489527},
        {"9_Cap_EUR", "OptionletVolatility/EUR/9/2Y/0.05", 0.0995465}};

    std::map<pair<string, string>, Real> sensiMap;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        pair<string, string> p(cachedResults[i].id, cachedResults[i].label);
        sensiMap[p] = cachedResults[i].sensi;
    }

    Real tolerance = 0.01;
    Size count = 0;
    for (auto data : parDelta) {
        pair<string, string> p = data.first;
        Real delta = data.second;
        if (fabs(delta) > 0.0) {
            count++;
            BOOST_CHECK_MESSAGE(sensiMap.find(p) != sensiMap.end(),
                                  "pair (" << p.first << ", " << p.second << ") not found in sensi map");
            if (sensiMap.find(p) != sensiMap.end()) {
                BOOST_CHECK_MESSAGE(fabs(delta - sensiMap[p]) < tolerance ||
                                        fabs((delta - sensiMap[p]) / delta) < tolerance,
                                    "sensitivity regression failed for pair ("
                                        << p.first << ", " << p.second << "): " << delta << " vs " << sensiMap[p]);
            }
        }
    }
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero par sensitivities ("
                                                           << count << ") do not match regression data ("
                                                           << cachedResults.size() << ")");
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

void ParSensitivityAnalysisTest::testParConversionNoneObs() {
    BOOST_TEST_MESSAGE("Testing Sensitivity Par Conversion (None observation mode)");
    testParConversion(ObservationMode::Mode::None);
}

void ParSensitivityAnalysisTest::testParConversionDisableObs() {
    BOOST_TEST_MESSAGE("Testing Sensitivity Par Conversion (Disable observation mode)");
    testParConversion(ObservationMode::Mode::Disable);
}

void ParSensitivityAnalysisTest::testParConversionDeferObs() {
    BOOST_TEST_MESSAGE("Testing Sensitivity Par Conversion (Defer observation mode)");
    testParConversion(ObservationMode::Mode::Defer);
}

void ParSensitivityAnalysisTest::testParConversionUnregisterObs() {
    BOOST_TEST_MESSAGE("Testing Sensitivity Par Conversion (Unregister observation mode)");
    testParConversion(ObservationMode::Mode::Unregister);
}

void ParSensitivityAnalysisTest::test1dZeroShifts() {
    BOOST_TEST_MESSAGE("Testing 1d shifts");

    SavedSettings backup;
    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData2();

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData2();

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
        QuantLib::ext::make_shared<ore::analytics::DeltaScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);

    // cache initial zero rates
    vector<Period> tenors = simMarketData->yieldCurveTenors("");
    vector<Real> initialZeros(tenors.size());
    vector<Real> times(tenors.size());
    string ccy = simMarketData->ccys()[0];
    Handle<YieldTermStructure> ts = initMarket->discountCurve(ccy);
    DayCounter dc = ts->dayCounter();
    for (Size j = 0; j < tenors.size(); ++j) {
        Date d = today + simMarketData->yieldCurveTenors("")[j];
        initialZeros[j] = ts->zeroRate(d, dc, Continuous);
        times[j] = dc.yearFraction(today, d);
    }

    // apply zero shifts for tenors on the shift curve
    // collect shifted data at tenors of the underlying curve
    // aggregate "observed" shifts
    // compare to expected total shifts
    vector<Period> shiftTenors = sensiData->discountCurveShiftData()["EUR"]->shiftTenors;
    vector<Time> shiftTimes(shiftTenors.size());
    for (Size i = 0; i < shiftTenors.size(); ++i)
        shiftTimes[i] = dc.yearFraction(today, today + shiftTenors[i]);

    vector<Real> shiftedZeros(tenors.size());
    vector<Real> diffAbsolute(tenors.size(), 0.0);
    vector<Real> diffRelative(tenors.size(), 0.0);
    Real shiftSize = 0.01;
    ShiftType shiftTypeAbsolute = ShiftType::Absolute;
    ShiftType shiftTypeRelative = ShiftType::Relative;
    for (Size i = 0; i < shiftTenors.size(); ++i) {
        scenarioGenerator->applyShift(i, shiftSize, true, shiftTypeAbsolute, shiftTimes, initialZeros, times,
                                      shiftedZeros, true);
        for (Size j = 0; j < tenors.size(); ++j)
            diffAbsolute[j] += shiftedZeros[j] - initialZeros[j];
        scenarioGenerator->applyShift(i, shiftSize, true, shiftTypeRelative, shiftTimes, initialZeros, times,
                                      shiftedZeros, true);
        for (Size j = 0; j < tenors.size(); ++j)
            diffRelative[j] += shiftedZeros[j] / initialZeros[j] - 1.0;
    }

    Real tolerance = 1.0e-10;
    for (Size j = 0; j < tenors.size(); ++j) {
        // BOOST_TEST_MESSAGE("1d shift: checking sensitivity to curve tenor point " << j << ": " << diff[j]);
        BOOST_CHECK_MESSAGE(fabs(diffAbsolute[j] - shiftSize) < tolerance,
                            "inconsistency in absolute 1d shifts at curve tenor point " << j);
        BOOST_CHECK_MESSAGE(fabs(diffRelative[j] - shiftSize) < tolerance,
                            "inconsistency in relative 1d shifts at curve tenor point " << j);
    }
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

void ParSensitivityAnalysisTest::test2dZeroShifts() {
    BOOST_TEST_MESSAGE("Testing 2d shifts");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData2();

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData2();

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
        QuantLib::ext::make_shared<ore::analytics::DeltaScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);

    // cache initial zero rates
    vector<Period> expiries = simMarketData->swapVolExpiries("");
    vector<Period> terms = simMarketData->swapVolTerms("");
    vector<vector<Real>> initialData(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<Real> expiryTimes(expiries.size());
    vector<Real> termTimes(terms.size());
    string ccy = simMarketData->ccys()[0];
    Handle<SwaptionVolatilityStructure> ts = initMarket->swaptionVol(ccy);
    DayCounter dc = ts->dayCounter();
    for (Size i = 0; i < expiries.size(); ++i)
        expiryTimes[i] = dc.yearFraction(today, today + expiries[i]);
    for (Size j = 0; j < terms.size(); ++j)
        termTimes[j] = dc.yearFraction(today, today + terms[j]);
    for (Size i = 0; i < expiries.size(); ++i) {
        for (Size j = 0; j < terms.size(); ++j)
            initialData[i][j] = ts->volatility(expiries[i], terms[j], Null<Real>()); // ATM
    }

    // apply shifts for tenors on the 2d shift grid
    // collect shifted data at tenors of the underlying 2d grid (different from the grid above)
    // aggregate "observed" shifts
    // compare to expected total shifts
    vector<Period> expiryShiftTenors = sensiData->swaptionVolShiftData()["EUR"].shiftExpiries;
    vector<Period> termShiftTenors = sensiData->swaptionVolShiftData()["EUR"].shiftTerms;
    vector<Real> shiftExpiryTimes(expiryShiftTenors.size());
    vector<Real> shiftTermTimes(termShiftTenors.size());
    for (Size i = 0; i < expiryShiftTenors.size(); ++i)
        shiftExpiryTimes[i] = dc.yearFraction(today, today + expiryShiftTenors[i]);
    for (Size j = 0; j < termShiftTenors.size(); ++j)
        shiftTermTimes[j] = dc.yearFraction(today, today + termShiftTenors[j]);

    vector<vector<Real>> shiftedData(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<vector<Real>> diffAbsolute(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<vector<Real>> diffRelative(expiries.size(), vector<Real>(terms.size(), 0.0));
    Real shiftSize = 0.01; // arbitrary
    ShiftType shiftTypeAbsolute = ShiftType::Absolute;
    ShiftType shiftTypeRelative = ShiftType::Relative;
    for (Size i = 0; i < expiryShiftTenors.size(); ++i) {
        for (Size j = 0; j < termShiftTenors.size(); ++j) {
            scenarioGenerator->applyShift(i, j, shiftSize, true, shiftTypeAbsolute, shiftExpiryTimes, shiftTermTimes,
                                          expiryTimes, termTimes, initialData, shiftedData, true);
            for (Size k = 0; k < expiries.size(); ++k) {
                for (Size l = 0; l < terms.size(); ++l)
                    diffAbsolute[k][l] += shiftedData[k][l] - initialData[k][l];
            }
            scenarioGenerator->applyShift(i, j, shiftSize, true, shiftTypeRelative, shiftExpiryTimes, shiftTermTimes,
                                          expiryTimes, termTimes, initialData, shiftedData, true);
            for (Size k = 0; k < expiries.size(); ++k) {
                for (Size l = 0; l < terms.size(); ++l)
                    diffRelative[k][l] += shiftedData[k][l] / initialData[k][l] - 1.0;
            }
        }
    }

    Real tolerance = 1.0e-10;
    for (Size k = 0; k < expiries.size(); ++k) {
        for (Size l = 0; l < terms.size(); ++l) {
            // BOOST_TEST_MESSAGE("2d shift: checking sensitivity to underlying grid point (" << k << ", " << l << "): "
            // << diff[k][l]);
            BOOST_CHECK_MESSAGE(fabs(diffAbsolute[k][l] - shiftSize) < tolerance,
                                "inconsistency in absolute 2d shifts at grid point (" << k << ", " << l
                                                                                      << "): " << diffAbsolute[k][l]);
            BOOST_CHECK_MESSAGE(fabs(diffRelative[k][l] - shiftSize) < tolerance,
                                "inconsistency in relative 2d shifts at grid point (" << k << ", " << l
                                                                                      << "): " << diffRelative[k][l]);
        }
    }
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(ParSensitivityAnalysis)

BOOST_AUTO_TEST_CASE(ZeroShifts1d) { 
    BOOST_TEST_MESSAGE("Testing 1-d Zero Shifts"); 
    ParSensitivityAnalysisTest::test1dZeroShifts();
}

BOOST_AUTO_TEST_CASE(ZeroShifts2d) {
    BOOST_TEST_MESSAGE("Testing 2-d Zero Shifts");
    ParSensitivityAnalysisTest::test2dZeroShifts();
}

BOOST_AUTO_TEST_CASE(ZeroSensitivity) {
    BOOST_TEST_MESSAGE("Testing Portfolio Zero Sensitivity");
    ParSensitivityAnalysisTest::testPortfolioZeroSensitivity();
}

BOOST_AUTO_TEST_CASE(ParConversionNoneObs) {
    BOOST_TEST_MESSAGE("Testing Par Conversion NoneObs");
    ParSensitivityAnalysisTest::testParConversionNoneObs();
}

BOOST_AUTO_TEST_CASE(ParConversionDisableObs) {
    BOOST_TEST_MESSAGE("Testing Par Conversion DisableObs");
    ParSensitivityAnalysisTest::testParConversionDisableObs();
}

BOOST_AUTO_TEST_CASE(ParConversionDeferObs) {
    BOOST_TEST_MESSAGE("Testing Par Conversion DeferObs");
    ParSensitivityAnalysisTest::testParConversionDeferObs();
}

BOOST_AUTO_TEST_CASE(ParConversionUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Par Conversion UnregisterObs");
    ParSensitivityAnalysisTest::testParConversionUnregisterObs();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testsuite
