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

#include "sensitivityperformanceplus.hpp"
#include <test/oreatoplevelfixture.hpp>
#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <boost/timer/timer.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/engine/riskfilter.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/engine/sensitivityfilestream.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <orea/engine/sensitivityrecord.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/report/csvreport.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

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

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices(
        {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    simMarketData->interpolation() = "LogLinear";

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
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.5, 1, 1.5, 2, 2.5, 2});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    return simMarketData;
}

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData7() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY", "SEK", "CAD"});
    simMarketData->setYieldCurveTenors("", {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years, 2 * Years,
                                            3 * Years, 5 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices({"EUR-EONIA", "EUR-EURIBOR-6M", "EUR-EURIBOR-3M", "USD-FedFunds", "USD-LIBOR-3M",
                               "USD-LIBOR-6M", "USD-LIBOR-1M", "GBP-SONIA", "GBP-LIBOR-6M", "GBP-LIBOR-3M", "CHF-TOIS",
                               "CHF-LIBOR-3M", "CHF-LIBOR-6M", "JPY-TONAR", "JPY-LIBOR-3M", "JPY-LIBOR-6M",
                               "CAD-CDOR-3M", "CAD-CORRA", "SEK-STIBOR-3M"});
    simMarketData->interpolation() = "LogLinear";


    simMarketData->swapIndices()["USD-CMS-1Y"] = "USD-LIBOR-3M";
    simMarketData->swapIndices()["USD-CMS-30Y"] = "USD-LIBOR-6M";
    simMarketData->swapIndices()["JPY-CMS-1Y"] = "JPY-LIBOR-6M";
    simMarketData->swapIndices()["JPY-CMS-30Y"] = "JPY-LIBOR-6M";
    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY", "EURSEK", "EURCAD"});

    return simMarketData;
}

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5Big() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveTenors(
        "", {1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
             9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
             17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
             25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
             3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
             53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
             67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
             7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
             20 * Years,  25 * Years,  30 * Years,  50 * Years});
    simMarketData->setIndices(
        {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    simMarketData->interpolation() = "LogLinear";

    simMarketData->setSwapVolTerms(
        "", {3 * Months,  4 * Months,  5 * Months,  6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,
             13 * Months, 14 * Months, 15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months,
             21 * Months, 22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
             29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months, 42 * Months,
             43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months, 55 * Months, 56 * Months,
             5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months, 68 * Months, 6 * Years,   76 * Months,
             77 * Months, 78 * Months, 79 * Months, 80 * Months, 7 * Years,   88 * Months, 89 * Months, 90 * Months,
             91 * Months, 92 * Months, 10 * Years,  15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years});
    simMarketData->setSwapVolExpiries(
        "", {1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
             9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
             17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
             25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
             3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
             53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
             67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
             7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
             20 * Years,  25 * Years,  30 * Years,  50 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true); // false;
    vector<Period> tmpFxVolExpiries = {
        1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
        9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
        17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
        25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
        3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
        53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
        67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
        7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
        20 * Years,  25 * Years,  30 * Years,  50 * Years};
    simMarketData->setFxVolExpiries("", tmpFxVolExpiries);
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true); // false;
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});
    simMarketData->setFxVolIsSurface(false);
    simMarketData->setFxVolMoneyness(vector<Real>{0});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {3 * Months,  4 * Months,  5 * Months,  6 * Months,  9 * Months,  10 * Months, 11 * Months, 1 * Years,
             13 * Months, 14 * Months, 15 * Months, 16 * Months, 17 * Months, 18 * Months, 19 * Months, 20 * Months,
             21 * Months, 22 * Months, 23 * Months, 2 * Years,   25 * Months, 26 * Months, 27 * Months, 28 * Months,
             29 * Months, 30 * Months, 31 * Months, 32 * Months, 3 * Years,   40 * Months, 41 * Months, 42 * Months,
             43 * Months, 44 * Months, 4 * Years,   52 * Months, 53 * Months, 54 * Months, 55 * Months, 56 * Months,
             5 * Years,   64 * Months, 65 * Months, 66 * Months, 67 * Months, 68 * Months, 6 * Years,   76 * Months,
             77 * Months, 78 * Months, 79 * Months, 80 * Months, 7 * Years,   88 * Months, 89 * Months, 90 * Months,
             91 * Months, 92 * Months, 10 * Years,  15 * Years,  20 * Years,  25 * Years,  30 * Years,  50 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    return simMarketData;
}

QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5Big() {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = QuantLib::ext::make_shared<SensitivityScenarioData>();

    QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData> cvsData =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>();
    cvsData->shiftTenors = {
        1 * Weeks,   2 * Weeks,   1 * Months,  2 * Months,  3 * Months,  4 * Months,  5 * Months,  6 * Months,
        9 * Months,  10 * Months, 11 * Months, 1 * Years,   13 * Months, 14 * Months, 15 * Months, 16 * Months,
        17 * Months, 18 * Months, 19 * Months, 20 * Months, 21 * Months, 22 * Months, 23 * Months, 2 * Years,
        25 * Months, 26 * Months, 27 * Months, 28 * Months, 29 * Months, 30 * Months, 31 * Months, 32 * Months,
        3 * Years,   40 * Months, 41 * Months, 42 * Months, 43 * Months, 44 * Months, 4 * Years,   52 * Months,
        53 * Months, 54 * Months, 55 * Months, 56 * Months, 5 * Years,   64 * Months, 65 * Months, 66 * Months,
        67 * Months, 68 * Months, 6 * Years,   76 * Months, 77 * Months, 78 * Months, 79 * Months, 80 * Months,
        7 * Years,   88 * Months, 89 * Months, 90 * Months, 91 * Months, 92 * Months, 10 * Years,  15 * Years,
        20 * Years,  25 * Years,  30 * Years,  50 * Years}; // multiple tenors: triangular shifts
    cvsData->shiftType = ShiftType::Absolute;
    cvsData->shiftSize = 0.0001;

    SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
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
    cfvsData.shiftType = ShiftType::Absolute;
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

    SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
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

    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["USD"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->discountCurveShiftData()["JPY"] = cvsData;

    sensiData->discountCurveShiftData()["CHF"] = cvsData;

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;

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

    return sensiData;
}

QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5() {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = QuantLib::ext::make_shared<SensitivityScenarioData>();

    QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData> cvsData =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>();
    cvsData->shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                            7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData->shiftType = ShiftType::Absolute;
    cvsData->shiftSize = 0.0001;

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
    swvsData.shiftExpiries = {6 * Months, 1 * Years, 3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {1 * Years, 3 * Years, 5 * Years, 10 * Years, 20 * Years};
    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["USD"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->discountCurveShiftData()["JPY"] = cvsData;

    sensiData->discountCurveShiftData()["CHF"] = cvsData;

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;

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

    return sensiData;
}

QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData7() {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = QuantLib::ext::make_shared<SensitivityScenarioData>();

    QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData> cvsData =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>();
    cvsData->shiftTenors = {
        2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years,
        3 * Years, 5 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years}; // multiple tenors: triangular shifts
    cvsData->shiftType = ShiftType::Absolute;
    cvsData->shiftSize = 0.0001;

    SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["USD"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->discountCurveShiftData()["JPY"] = cvsData;

    sensiData->discountCurveShiftData()["CHF"] = cvsData;

    sensiData->discountCurveShiftData()["CAD"] = cvsData;

    sensiData->discountCurveShiftData()["SEK"] = cvsData;

    sensiData->indexCurveShiftData()["EUR-EONIA"] = cvsData;

    sensiData->indexCurveShiftData()["EUR-EURIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-SONIA"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-FedFunds"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-1M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-TONAR"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-TOIS"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["CAD-CDOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["CAD-CORRA"] = cvsData;

    sensiData->indexCurveShiftData()["SEK-STIBOR-3M"] = cvsData;

    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;
    sensiData->fxShiftData()["EURCAD"] = fxsData;
    sensiData->fxShiftData()["EURSEK"] = fxsData;

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
}

void addCrossGammas7(vector<pair<string, string>>& cgFilter) {
    BOOST_CHECK_EQUAL(cgFilter.size(), 0);
    cgFilter.push_back(pair<string, string>("DiscountCurve/EUR", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/JPY", "DiscountCurve/JPY"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/GBP", "DiscountCurve/GBP"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CHF", "DiscountCurve/CHF"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/SEK", "DiscountCurve/SEK"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CAD", "DiscountCurve/CAD"));

    cgFilter.push_back(pair<string, string>("IndexCurve/EUR", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/JPY", "IndexCurve/JPY"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("IndexCurve/GBP", "IndexCurve/GBP"));
    cgFilter.push_back(pair<string, string>("IndexCurve/CHF", "IndexCurve/CHF"));
    cgFilter.push_back(pair<string, string>("IndexCurve/SEK", "IndexCurve/SEK"));
    cgFilter.push_back(pair<string, string>("IndexCurve/CAD", "IndexCurve/CAD"));

    cgFilter.push_back(pair<string, string>("DiscountCurve/EUR", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/JPY", "IndexCurve/JPY"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/GBP", "IndexCurve/GBP"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CHF", "IndexCurve/CHF"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/SEK", "IndexCurve/SEK"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CAD", "IndexCurve/CAD"));

    cgFilter.push_back(pair<string, string>("DiscountCurve/EUR", "SwaptionVolatility/EUR"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/JPY", "SwaptionVolatility/JPY"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "SwaptionVolatility/USD"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/GBP", "SwaptionVolatility/GBP"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CHF", "SwaptionVolatility/CHF"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/SEK", "SwaptionVolatility/SEK"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/CAD", "SwaptionVolatility/CAD"));

    cgFilter.push_back(pair<string, string>("IndexCurve/EUR", "SwaptionVolatility/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/JPY", "SwaptionVolatility/JPY"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "SwaptionVolatility/USD"));
    cgFilter.push_back(pair<string, string>("IndexCurve/GBP", "SwaptionVolatility/GBP"));
    cgFilter.push_back(pair<string, string>("IndexCurve/CHF", "SwaptionVolatility/CHF"));
    cgFilter.push_back(pair<string, string>("IndexCurve/SEK", "SwaptionVolatility/SEK"));
    cgFilter.push_back(pair<string, string>("IndexCurve/CAD", "SwaptionVolatility/CAD"));

    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("FXSpot/JPYUSD", "DiscountCurve/JPY"));
    cgFilter.push_back(pair<string, string>("FXSpot/JPYUSD", "IndexCurve/JPY"));
    cgFilter.push_back(pair<string, string>("FXSpot/GBPUSD", "DiscountCurve/GBP"));
    cgFilter.push_back(pair<string, string>("FXSpot/GBPUSD", "IndexCurve/GBP"));
    cgFilter.push_back(pair<string, string>("FXSpot/CHFUSD", "DiscountCurve/CHF"));
    cgFilter.push_back(pair<string, string>("FXSpot/CHFUSD", "IndexCurve/CHF"));
    cgFilter.push_back(pair<string, string>("FXSpot/SEKUSD", "DiscountCurve/SEK"));
    cgFilter.push_back(pair<string, string>("FXSpot/SEKUSD", "IndexCurve/SEK"));
    cgFilter.push_back(pair<string, string>("FXSpot/CADUSD", "DiscountCurve/CAD"));
    cgFilter.push_back(pair<string, string>("FXSpot/CADUSD", "IndexCurve/CAD"));

    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "SwaptionVolatility/EUR"));
    cgFilter.push_back(pair<string, string>("FXSpot/JPYUSD", "SwaptionVolatility/JPY"));
    cgFilter.push_back(pair<string, string>("FXSpot/GBPUSD", "SwaptionVolatility/GBP"));
    cgFilter.push_back(pair<string, string>("FXSpot/CHFUSD", "SwaptionVolatility/CHF"));
    cgFilter.push_back(pair<string, string>("FXSpot/SEKUSD", "SwaptionVolatility/SEK"));
    cgFilter.push_back(pair<string, string>("FXSpot/CADUSD", "SwaptionVolatility/CAD"));

    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/JPYUSD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/JPYUSD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/GBPUSD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/GBPUSD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/CHFUSD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/CHFUSD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/SEKUSD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/SEKUSD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/CADUSD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("FXSpot/CADUSD", "IndexCurve/USD"));

    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/JPY"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/GBP"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/CHF"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/SEK"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/CAD"));

    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/JPY"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/GBP"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/CHF"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/SEK"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "DiscountCurve/CAD"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/EUR"));

    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/JPY"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/GBP"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/CHF"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/SEK"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "IndexCurve/CAD"));

    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/JPY"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/GBP"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/CHF"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/SEK"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/CAD"));
}

QuantLib::ext::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, bool swapsOnly = false,
                                            QuantLib::ext::shared_ptr<EngineFactory> factory = {}) {

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());

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
        Real fixedRate = portfolioSize == 1 ? 0.02 : randInt(rng, minFixedBps, maxFixedBps) / 100.0;
        string fixFreq = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);

        bool isPayer = randBoolean(rng);

        // id
        std::ostringstream oss;
        oss.clear();
        oss.str("");
        oss << "Trade_" << i + 1;
        string id = oss.str();

        if (i % 2 == 0 && !swapsOnly) {
            int start = randInt(rng, minTerm, maxTerm);
            Size term = portfolioSize == 1 ? 20 : randInt(rng, minTerm, maxTerm);
            string longShort = randBoolean(rng) ? "Long" : "Short";
            portfolio->add(testsuite::buildEuropeanSwaption(id, longShort, ccy, isPayer, notional,
                                                            start, term, fixedRate, spread, fixFreq,
                                                            fixDC, floatFreq, floatDC, index));
        } else {
            int start = randInt(rng, minStart, maxStart);
            Size end = randInt(rng, minTerm, maxTerm);
            portfolio->add(testsuite::buildSwap(id, ccy, isPayer, notional, start, end, fixedRate,
                                                spread, fixFreq, fixDC, floatFreq, floatDC, index));
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
    string om_str = (om == ObservationMode::Mode::None)         ? "None"
                    : (om == ObservationMode::Mode::Disable)    ? "Disable"
                    : (om == ObservationMode::Mode::Defer)      ? "Defer"
                    : (om == ObservationMode::Mode::Unregister) ? "Unregister"
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
    ObservationMode::instance().setMode(om);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<testsuite::TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5();
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

    // build porfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize);

    boost::timer::cpu_timer timer;
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, false);
    sa->generateSensitivities();
    timer.stop();
    Real elapsed = timer.elapsed().wall * 1e-9;
    Size numScenarios = sa->scenarioGenerator()->samples();
    Size scenarioSize = sa->scenarioGenerator()->scenarios().front()->keys().size();
    BOOST_TEST_MESSAGE("number of scenarios=" << numScenarios);
    BOOST_TEST_MESSAGE("Size of scenario = " << scenarioSize << " keys");
    BOOST_TEST_MESSAGE("time = " << elapsed << " seconds");
    Real avTime = (elapsed / ((Real)(numScenarios * portfolioSize)));
    BOOST_TEST_MESSAGE("Average pricing time =  " << avTime << " seconds");
    BOOST_TEST_MESSAGE("Memory usage - " << os::getMemoryUsage());
}

void BT_Benchmark(bool crossGammas, ObservationMode::Mode om) {
    Size portfolioSize = 100;
    string om_str = (om == ObservationMode::Mode::None)         ? "None"
                    : (om == ObservationMode::Mode::Disable)    ? "Disable"
                    : (om == ObservationMode::Mode::Defer)      ? "Defer"
                    : (om == ObservationMode::Mode::Unregister) ? "Unregister"
                                                                : "???";

    SavedSettings backup;
    ObservationMode::instance().setMode(om);

    Date today = Date(13, April, 2016);
    Settings::instance().evaluationDate() = today;

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<testsuite::TestMarket>(today, true);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData7();
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData7();
    if (crossGammas) {
        addCrossGammas7(sensiData->crossGammaFilter());
    }

    // build porfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize, true);

    boost::timer::cpu_timer timer;
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, false);
    sa->generateSensitivities();

    // TODO: do we really want to write a report in the unit tests?
    // Create a stream from the sensitivity cube
    CSVFileReport cgReport("crossgammReport");
    auto baseCurrency = sa->simMarketData()->baseCcy();
    auto ss = QuantLib::ext::make_shared<SensitivityCubeStream>(sa->sensiCube(), baseCurrency);
    ReportWriter().writeSensitivityReport(cgReport, ss, 0.000001);
    timer.stop();

    Real elapsed = timer.elapsed().wall * 1e-9;
    Size numScenarios = sa->scenarioGenerator()->samples();
    Size scenarioSize = sa->scenarioGenerator()->scenarios().front()->keys().size();
    BOOST_TEST_MESSAGE("number of scenarios=" << numScenarios);
    BOOST_TEST_MESSAGE("Size of scenario = " << scenarioSize << " keys");
    BOOST_TEST_MESSAGE("time = " << elapsed << " seconds");
    Real avTime = (elapsed / ((Real)(numScenarios * portfolioSize)));
    BOOST_TEST_MESSAGE("Average pricing time =  " << avTime << " seconds");
    BOOST_TEST_MESSAGE("Memory usage - " << os::getMemoryUsage());
}
} // namespace

namespace testsuite {

void SensitivityPerformancePlusTest::testSensiPerformanceNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, false, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceDisableObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    test_performance(false, false, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceDeferObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    test_performance(false, false, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceUnregisterObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    test_performance(false, false, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceCrossGammaNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, false, false, true, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBigScenarioNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, true, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, false, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioBigScenarioNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, true, false, false, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioCrossGammaNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, false, false, true, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBigScenarioCrossGammaNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(false, true, false, true, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    test_performance(true, true, false, true, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

void SensitivityPerformancePlusTest::testSensiPerformanceBTSetupNoneObs() {
    boost::timer::cpu_timer timer;
    ObservationMode::Mode om = ObservationMode::Mode::None;
    BT_Benchmark(true, om);
    timer.stop();
    BOOST_TEST_MESSAGE("total time = " << timer.format(boost::timer::default_places, "%w") << " seconds");
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)
BOOST_AUTO_TEST_SUITE(SensitivityPerformanceTest)

BOOST_AUTO_TEST_CASE(SensiPerformanceNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance NoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceNoneObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceDisableObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance DisableObs");
    SensitivityPerformancePlusTest::testSensiPerformanceDisableObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceDeferObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance DeferObs");
    SensitivityPerformancePlusTest::testSensiPerformanceDeferObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance UnregisterObs");
    SensitivityPerformancePlusTest::testSensiPerformanceUnregisterObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceCrossGammaNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance CrossGammaNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceCrossGammaNoneObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceBigScenarioNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BigScenarioNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBigScenarioNoneObs();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)
BOOST_AUTO_TEST_SUITE(SensitivityPerformanceBigPortfolioTest)

BOOST_AUTO_TEST_CASE(SensiPerformanceBigPortfolioObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BigPortfolioNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioNoneObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceBigPortfolioBigScenarioNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BigPortfolioBigScenarioNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioBigScenarioNoneObs();
}

BOOST_AUTO_TEST_CASE(SensiPerformanceBigPortfolioCrossGammaNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BigPortfolioCrossGammaNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioCrossGammaNoneObs();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)
BOOST_AUTO_TEST_SUITE(SensitivityPerformanceBigScenarioTest)

BOOST_AUTO_TEST_CASE(SensiPerformanceBigScenarioCrossGammaNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BigScenarioCrossGammaNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBigScenarioCrossGammaNoneObs();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)
BOOST_AUTO_TEST_SUITE(SensitivityPerformanceBigPortfolioBigScenarioTest)

BOOST_AUTO_TEST_CASE(SensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BigPortfolioBigScenarioCrossGammaNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)
BOOST_AUTO_TEST_SUITE(SensitivityPerformanceBTSetupTest)

BOOST_AUTO_TEST_CASE(SensiPerformanceBTSetupNoneObs) {
    BOOST_TEST_MESSAGE("Testing Sensitivity Performance BTSetupNoneObs");
    SensitivityPerformancePlusTest::testSensiPerformanceBTSetupNoneObs();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

} // namespace testsuite
