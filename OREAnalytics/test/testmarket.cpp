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

#include <boost/make_shared.hpp>
#include <test/testmarket.hpp>
#include <iostream>
namespace testsuite {

boost::shared_ptr<ore::data::Conventions> TestConfigurationObjects::conv() {
    boost::shared_ptr<ore::data::Conventions> conventions(new ore::data::Conventions());

    conventions->add(boost::make_shared<ore::data::SwapIndexConvention>("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(boost::make_shared<ore::data::SwapIndexConvention>("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));

    // boost::shared_ptr<data::Convention> swapConv(
    //     new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(boost::make_shared<ore::data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                     "30/360", "EUR-EURIBOR-6M"));
    conventions->add(boost::make_shared<ore::data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                     "30/360", "USD-LIBOR-3M"));
    conventions->add(boost::make_shared<ore::data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                     "30/360", "USD-LIBOR-6M"));
    conventions->add(boost::make_shared<ore::data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                     "30/360", "GBP-LIBOR-6M"));
    conventions->add(boost::make_shared<ore::data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                     "30/360", "JPY-LIBOR-6M"));
    conventions->add(boost::make_shared<ore::data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                     "30/360", "CHF-LIBOR-6M"));

    conventions->add(boost::make_shared<ore::data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    return conventions;
}

boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> TestConfigurationObjects::setupSimMarketData2() {
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP"};
    simMarketData->yieldCurveNames() = {"BondCurve1"};
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 6 * Years, 7 * Years, 8 * Years, 9 * Years, 10 * Years,
                                            12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years});
    simMarketData->indices() = {"EUR-EURIBOR-6M", "GBP-LIBOR-6M"};
    simMarketData->defaultNames() = {"BondIssuer1"};
    simMarketData->setDefaultTenors("", {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years});
    simMarketData->securities() = {"Bond1"};
    simMarketData->simulateSurvivalProbabilities() = true;

    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = {1 * Years, 2 * Years, 3 * Years,  4 * Years,
                                     5 * Years, 7 * Years, 10 * Years, 20 * Years};
    simMarketData->swapVolExpiries() = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                        5 * Years,  7 * Years, 10 * Years, 20 * Years};
    simMarketData->swapVolCcys() = {"EUR", "GBP"};
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true;

    simMarketData->fxVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true;
    simMarketData->fxVolCcyPairs() = {"EURGBP"};

    simMarketData->fxCcyPairs() = {"EURGBP"};

    simMarketData->simulateCapFloorVols() = false;

    return simMarketData;
}

boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> TestConfigurationObjects::setupSimMarketData5() {
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M",
                                "GBP-LIBOR-6M",   "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    simMarketData->swapIndices() = {{"EUR-CMS-2Y", "EUR-EURIBOR-6M"}, {"EUR-CMS-30Y", "EUR-EURIBOR-6M"}};
    simMarketData->yieldCurveNames() = {"BondCurve1"};
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years};
    simMarketData->swapVolExpiries() = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                        5 * Years,  7 * Years, 10 * Years, 20 * Years};
    simMarketData->swapVolCcys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true; // false;

    simMarketData->fxVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; // false;
    simMarketData->fxVolCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"};

    simMarketData->fxCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};

    simMarketData->simulateCapFloorVols() = true;
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->capFloorVolCcys() = {"EUR", "USD"};
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->capFloorVolStrikes() = {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06};

    simMarketData->defaultNames() = {"BondIssuer1"};
    simMarketData->setDefaultTenors("", {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years});
    simMarketData->simulateSurvivalProbabilities() = true;
    simMarketData->securities() = {"Bond1"};

    simMarketData->equityNames() = { "SP5", "Lufthansa" };
    simMarketData->setEquityDividendTenors("SP5", { 6 * Months, 1 * Years, 2 * Years });
    simMarketData->setEquityForecastTenors("SP5", { 6 * Months, 1 * Years, 2 * Years });
    simMarketData->setEquityForecastTenors("Lufthansa", { 6 * Months, 1 * Years, 2 * Years });

    simMarketData->simulateEquityVols() = true;
    simMarketData->equityVolDecayMode() = "ForwardVariance";
    simMarketData->equityVolNames() = { "SP5", "Lufthansa" };
    simMarketData->equityVolExpiries() = { 6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                           5 * Years,  7 * Years, 10 * Years, 20 * Years };
    simMarketData->equityVolIsSurface() = false;

    return simMarketData;
}

boost::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData2() {
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        boost::make_shared<ore::analytics::SensitivityScenarioData>();

    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                           7 * Years, 10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.05};

    ore::analytics::SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

    sensiData->discountCurrencies() = {"EUR", "GBP"};
    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->indexNames() = {"EUR-EURIBOR-6M", "GBP-LIBOR-6M"};
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->yieldCurveNames() = {"BondCurve1"};
    sensiData->yieldCurveShiftData()["BondCurve1"] = cvsData;

    sensiData->fxCcyPairs() = {"EURGBP"};
    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolCcyPairs() = {"EURGBP"};
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolCurrencies() = {"EUR", "GBP"};
    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    sensiData->creditNames() = {"BondIssuer1"};
    sensiData->creditCurveShiftData()["BondIssuer1"] = cvsData;

    // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
    // sensiData->capFloorVolCurrencies() = { "EUR", "GBP" };
    // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
    // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

    return sensiData;
}

boost::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData5() {
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        boost::make_shared<ore::analytics::SensitivityScenarioData>();

    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    ore::analytics::SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {2 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {5 * Years, 10 * Years};

    ore::analytics::SensitivityScenarioData::SpotShiftData eqsData;
    eqsData.shiftType = "Relative";
    eqsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData eqvsData;
    eqvsData.shiftType = "Relative";
    eqvsData.shiftSize = 0.01;
    eqvsData.shiftExpiries = { 5 * Years };

    sensiData->discountCurrencies() = {"EUR", "USD", "GBP", "CHF", "JPY"};
    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["USD"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->discountCurveShiftData()["JPY"] = cvsData;

    sensiData->discountCurveShiftData()["CHF"] = cvsData;

    sensiData->indexNames() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;

    sensiData->yieldCurveNames() = {"BondCurve1"};
    sensiData->yieldCurveShiftData()["BondCurve1"] = cvsData;


    sensiData->creditNames() = {"BondIssuer1"};
    sensiData->creditCurveShiftData()["BondIssuer1"] = cvsData;

    sensiData->fxCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"};
    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

    sensiData->swaptionVolCurrencies() = {"EUR", "USD", "GBP", "CHF", "JPY"};
    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolCurrencies() = {"EUR", "USD"};
    sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] = cfvsData;
    sensiData->capFloorVolShiftData()["USD"].indexName = "USD-LIBOR-3M";

    sensiData->equityNames() = { "SP5", "Lufthansa" };
    sensiData->equityShiftData()["SP5"] = eqsData;
    sensiData->equityShiftData()["Lufthansa"] = eqsData;

    sensiData->equityVolNames() = { "SP5", "Lufthansa" };
    sensiData->equityVolShiftData()["SP5"] = eqvsData;
    sensiData->equityVolShiftData()["Lufthansa"] = eqvsData;

    return sensiData;
}

} // namespace testsuite
