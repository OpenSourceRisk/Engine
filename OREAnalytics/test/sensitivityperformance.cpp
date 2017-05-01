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

#include "sensitivityperformance.hpp"
#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <ored/utilities/osutils.hpp>
#include <ored/utilities/log.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <ored/model/lgmdata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <ql/time/date.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <orea/engine/all.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <orea/engine/observationmode.hpp>
#include <boost/timer.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

namespace {

// Returns an int in the interval [min, max]. Inclusive.
inline unsigned long randInt(MersenneTwisterUniformRng& rng, Size min, Size max) {
    return min + (rng.nextInt32() % (max + 1 - min));
}

inline const string& randString(MersenneTwisterUniformRng& rng, const vector<string>& strs) {
    return strs[randInt(rng, 0, strs.size() - 1)];
}

inline bool randBoolean(MersenneTwisterUniformRng& rng) { return randInt(rng, 0, 1) == 1; }

boost::shared_ptr<data::Conventions> conv() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    // boost::shared_ptr<data::Convention> swapConv(
    //     new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "EUR-EURIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-3M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "GBP-LIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "JPY-LIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "CHF-LIBOR-6M"));

    conventions->add(boost::make_shared<data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    return conventions;
}

boost::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->yieldCurveTenors() = {1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                                         5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};
    simMarketData->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M",
                                "GBP-LIBOR-6M",   "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
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
    simMarketData->capFloorVolExpiries() = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                                            7 * Years,  10 * Years, 15 * Years, 20 * Years};
    simMarketData->capFloorVolStrikes() = {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06};

    return simMarketData;
}

boost::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5Big() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->yieldCurveTenors() = {
        1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
        9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
        17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
        25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
        3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
        53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
        67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
        7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
        20 * Years,  25 * Years,  30 * Years,  50 * Years};
    simMarketData->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M",
                                "GBP-LIBOR-6M",   "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = {
        3 * Months,  4 * Months,  5 * Months,  6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,
        13 * Months, 14 * Months, 15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months,
        21 * Months, 22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
        29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months, 42 * Months,
        43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months, 55 * Months, 56 * Months,
        5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months, 68 * Months, 6 * Years,   76 * Months,
        77 * Months, 78 * Months, 79 * Months, 80 * Months, 7 * Years,   88 * Months, 89 * Months, 90 * Months,
        91 * Months, 92 * Months, 10 * Years,  15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years};
    simMarketData->swapVolExpiries() = {
        1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
        9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
        17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
        25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
        3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
        53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
        67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
        7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
        20 * Years,  25 * Years,  30 * Years,  50 * Years};
    simMarketData->swapVolCcys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true; // false;

    simMarketData->fxVolExpiries() = {
        1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
        9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
        17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
        25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
        3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
        53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
        67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
        7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
        20 * Years,  25 * Years,  30 * Years,  50 * Years};
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; // false;
    simMarketData->fxVolCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"};

    simMarketData->fxCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};

    simMarketData->simulateCapFloorVols() = true;
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->capFloorVolCcys() = {"EUR", "USD"};
    simMarketData->capFloorVolExpiries() = {
        3 * Months,  4 * Months,  5 * Months,  6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,
        13 * Months, 14 * Months, 15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months,
        21 * Months, 22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
        29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months, 42 * Months,
        43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months, 55 * Months, 56 * Months,
        5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months, 68 * Months, 6 * Years,   76 * Months,
        77 * Months, 78 * Months, 79 * Months, 80 * Months, 7 * Years,   88 * Months, 89 * Months, 90 * Months,
        91 * Months, 92 * Months, 10 * Years,  15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years};
    simMarketData->capFloorVolStrikes() = {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06};

    return simMarketData;
}

boost::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5Big() {
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {
        1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
        9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
        17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
        25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
        3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
        53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
        67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
        7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
        20 * Years,  25 * Years,  30 * Years,  50 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    SensitivityScenarioData::FxShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::FxVolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,
                              6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months,
                              15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months,
                              22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
                              29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months,
                              42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months,
                              55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months,
                              68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
                              7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,
                              15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years};

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {
        3 * Months,  4 * Months,  5 * Months,  6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,
        13 * Months, 14 * Months, 15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months,
        21 * Months, 22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
        29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months, 42 * Months,
        43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months, 55 * Months, 56 * Months,
        5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months, 68 * Months, 6 * Years,   76 * Months,
        77 * Months, 78 * Months, 79 * Months, 80 * Months, 7 * Years,   88 * Months, 89 * Months, 90 * Months,
        91 * Months, 92 * Months, 10 * Years,  15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,
                              6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months,
                              15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months,
                              22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
                              29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months,
                              42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months,
                              55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months,
                              68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
                              7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,
                              15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years};
    swvsData.shiftTerms = {
        3 * Months,  4 * Months,  5 * Months,  6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,
        13 * Months, 14 * Months, 15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months,
        21 * Months, 22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
        29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months, 42 * Months,
        43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months, 55 * Months, 56 * Months,
        5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months, 68 * Months, 6 * Years,   76 * Months,
        77 * Months, 78 * Months, 79 * Months, 80 * Months, 7 * Years,   88 * Months, 89 * Months, 90 * Months,
        91 * Months, 92 * Months, 10 * Years,  15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years};

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

    return sensiData;
}

boost::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5() {
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    SensitivityScenarioData::FxShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::FxVolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {5 * Years};

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {6 * Months, 1 * Years, 3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {1 * Years, 3 * Years, 5 * Years, 10 * Years, 20 * Years};

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

    return sensiData;
}

void addCrossGammas(vector<pair<string, string>>& cgFilter) {
    BOOST_CHECK_EQUAL(cgFilter.size(), 0);
    cgFilter.push_back(pair<string, string>("DiscountCurve/EUR", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/GBP", "DiscountCurve/GBP"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CHF", "DiscountCurve/CHF"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/JPY", "DiscountCurve/JPY"));
    cgFilter.push_back(pair<string, string>("IndexCurve/EUR", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("IndexCurve/GBP", "DiscountCurve/GBP"));
    cgFilter.push_back(pair<string, string>("IndexCurve/CHF", "DiscountCurve/CHF"));
    cgFilter.push_back(pair<string, string>("IndexCurve/JPY", "DiscountCurve/JPY"));
    cgFilter.push_back(pair<string, string>("IndexCurve/EUR", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("IndexCurve/GBP", "IndexCurve/GBP"));
    cgFilter.push_back(pair<string, string>("IndexCurve/CHF", "IndexCurve/CHF"));
    cgFilter.push_back(pair<string, string>("IndexCurve/JPY", "IndexCurve/JPY"));
    cgFilter.push_back(pair<string, string>("SwaptionVolatility/EUR", "SwaptionVolatility/EUR"));
    cgFilter.push_back(pair<string, string>("SwaptionVolatility/USD", "SwaptionVolatility/USD"));
    cgFilter.push_back(pair<string, string>("SwaptionVolatility/GBP", "SwaptionVolatility/GBP"));
    // cgFilter.push_back(pair<string, string>("SwaptionVolatility/CHF", "SwaptionVolatility/CHF"));
    // cgFilter.push_back(pair<string, string>("SwaptionVolatility/JPY", "SwaptionVolatility/JPY"));
}

boost::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, boost::shared_ptr<EngineFactory> factory = {}) {

    boost::shared_ptr<Portfolio> portfolio(new Portfolio());

    vector<string> ccys = {"EUR", "USD", "GBP", "JPY", "CHF"};

    map<string, vector<string>> indices = {{"EUR", {"EUR-EURIBOR-6M"}},
                                           {"USD", {"USD-LIBOR-3M"}},
                                           {"GBP", {"GBP-LIBOR-6M"}},
                                           {"CHF", {"CHF-LIBOR-6M"}},
                                           {"JPY", {"JPY-LIBOR-6M"}}};

    vector<string> fixedTenors = {"6M", "1Y"};

    Size minStart = 0;
    Size maxStart = 5;
    Size minTerm = 2;
    Size maxTerm = 30;

    Size minFixedBps = 10;
    Size maxFixedBps = 400;

    Size seed = 5; // keep this constant to ensure portfolio doesn't change
    MersenneTwisterUniformRng rng(seed);

    Calendar cal = TARGET();
    string calStr = "TARGET";
    string conv = "MF";
    string rule = "Forward";
    string fixDC = "30/360";
    string floatDC = "ACT/365";

    Real notional = 1000000;
    Real spread = 0.0;

    for (Size i = 0; i < portfolioSize; i++) {

        // ccy + index
        string ccy = portfolioSize == 1 ? "EUR" : randString(rng, ccys);
        string index = portfolioSize == 1 ? "EUR-EURIBOR-6M" : randString(rng, indices[ccy]);
        string floatFreq = portfolioSize == 1 ? "6M" : index.substr(index.find('-', 4) + 1);

        // fixed details
        string fixedTenor = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);
        Real fixedRate = portfolioSize == 1 ? 0.02 : randInt(rng, minFixedBps, maxFixedBps) / 100.0;
        string fixFreq = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);

        bool isPayer = randBoolean(rng);

        // id
        std::ostringstream oss;
        oss.clear();
        oss.str("");
        oss << "Trade_" << i + 1;
        string id = oss.str();

        if (i % 2 == 0) {
            int start = randInt(rng, minTerm, maxTerm);
            Size term = portfolioSize == 1 ? 20 : randInt(rng, minTerm, maxTerm);
            string longShort = randBoolean(rng) ? "Long" : "Short";
            portfolio->add(testsuite::buildEuropeanSwaption(id, longShort, ccy, isPayer, notional, start, term,
                                                            fixedRate, spread, fixFreq, fixDC, floatFreq, floatDC,
                                                            index));
        } else {
            int start = randInt(rng, minStart, maxStart);
            Size end = randInt(rng, minTerm, maxTerm);
            portfolio->add(testsuite::buildSwap(id, ccy, isPayer, notional, start, end, fixedRate, spread, fixFreq,
                                                fixDC, floatFreq, floatDC, index));
        }
    }
    if (factory)
        portfolio->build(factory);

    BOOST_CHECK_MESSAGE(portfolio->size() == portfolioSize,
                        "Failed to build portfolio (got " << portfolio->size() << " expected " << portfolioSize << ")");

    return portfolio;
}

void test_performance(bool bigPortfolio, bool bigScenario, bool lotsOfSensis, bool crossGammas,
                      ObservationMode::Mode om) {
    Size portfolioSize = bigPortfolio ? 100 : 1;
    string om_str = (om == ObservationMode::Mode::None) ? "None" : (om == ObservationMode::Mode::Disable)
                                                                       ? "Disable"
                                                                       : (om == ObservationMode::Mode::Defer)
                                                                             ? "Defer"
                                                                             : (om == ObservationMode::Mode::Unregister)
                                                                                   ? "Unregister"
                                                                                   : "???";
    string bigPfolioStr = bigPortfolio ? "big" : "small";
    string bigScenarioStr = bigScenario ? "big" : "small";
    string lotsOfSensisStr = lotsOfSensis ? "lots" : "few";
    string crossGammasStr = crossGammas ? "included" : "excluded";

    BOOST_TEST_MESSAGE("Testing Sensitivity Performance "
                       << "(portfolio=" << bigPfolioStr << ")"
                       << "(scenarioSize=" << bigScenarioStr << ")"
                       << "(numSensis=" << lotsOfSensisStr << ")"
                       << "(crossGammas=" << crossGammasStr << ")"
                       << "(observation=" << om_str << ")...");

    SavedSettings backup;
    ObservationMode::Mode backupOm = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(om);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<testsuite::TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();
    boost::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5();
    if (bigScenario) {
        simMarketData = setupSimMarketData5Big();
    }
    if (lotsOfSensis) {
        sensiData = setupSensitivityScenarioData5Big();
    }
    if (crossGammas) {
        addCrossGammas(sensiData->crossGammaFilter());
    }

    // build scenario sim market
    Conventions conventions = *conv();

    // build porfolio
    boost::shared_ptr<EngineData> data = boost::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";

    boost::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize);

    boost::timer t2;
    t2.restart();
    boost::shared_ptr<SensitivityAnalysis> sa = boost::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, conventions);
    sa->generateSensitivities();
    Real elapsed = t2.elapsed();
    Size numScenarios = sa->scenarioGenerator()->samples();
    Size scenarioSize = sa->scenarioGenerator()->scenarios().front()->keys().size();
    BOOST_TEST_MESSAGE("number of scenarios=" << numScenarios);
    BOOST_TEST_MESSAGE("Size of scenario = " << scenarioSize << " keys");
    BOOST_TEST_MESSAGE("time = " << elapsed << " seconds");
    Real avTime = (elapsed / ((Real)(numScenarios * portfolioSize)));
    BOOST_TEST_MESSAGE("Average pricing time =  " << avTime << " seconds");
    BOOST_TEST_MESSAGE("Memory usage - " << os::getMemoryUsage());

    ObservationMode::instance().setMode(backupOm);
}
}

namespace testsuite {

void SensitivityPerformanceTest::testSensiPerformanceNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(false, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(false, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(false, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceCrossGammaNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceCrossGammaDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(false, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceCrossGammaDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(false, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceCrossGammaUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(false, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(false, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(false, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(false, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(true, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(true, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(true, false, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(true, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(true, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(true, true, false, false, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(true, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(true, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(true, false, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(false, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(false, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(false, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaDisableObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(true, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaDeferObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(true, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

void SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaUnregisterObs() {
    boost::timer t_base;
    t_base.restart();
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(true, true, false, true, om);
    BOOST_TEST_MESSAGE("total time = " << t_base.elapsed() << " seconds");
}

test_suite* SensitivityPerformanceTest::suite() {

    test_suite* suite = BOOST_TEST_SUITE("SensitivityPerformanceTest");
    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceNoneObs));
    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceDisableObs));
    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceDeferObs));
    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceUnregisterObs));

    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceCrossGammaNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceCrossGammaDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceCrossGammaDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceCrossGammaUnregisterObs));

    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioUnregisterObs));

    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioUnregisterObs));

    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioUnregisterObs));

    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioCrossGammaUnregisterObs));

    suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigScenarioCrossGammaUnregisterObs));

    suite->add(
        BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaDisableObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaDeferObs));
    // suite->add(BOOST_TEST_CASE(&SensitivityPerformanceTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaUnregisterObs));
    return suite;
}
}
