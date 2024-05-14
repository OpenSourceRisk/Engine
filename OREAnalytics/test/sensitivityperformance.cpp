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

#include "testmarket.hpp"
#include "testportfolio.hpp"
#include <boost/test/unit_test.hpp>
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>

#include <boost/timer/timer.hpp>
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
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace {

// Returns an int in the interval [min, max]. Inclusive.
inline unsigned long randInt(MersenneTwisterUniformRng& rng, Size min, Size max) {
    return min + (rng.nextInt32() % (max + 1 - min));
}

inline const string& randString(MersenneTwisterUniformRng& rng, const vector<string>& strs) {
    return strs[randInt(rng, 0, strs.size() - 1)];
}

inline bool randBoolean(MersenneTwisterUniformRng& rng) { return randInt(rng, 0, 1) == 1; }

QuantLib::ext::shared_ptr<data::Conventions> conv() {
    QuantLib::ext::shared_ptr<data::Conventions> conventions(new data::Conventions());

    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    // QuantLib::ext::shared_ptr<data::Convention> swapConv(
    //     new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "GBP-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "JPY-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "CHF-LIBOR-6M"));

    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    InstrumentConventions::instance().setConventions(conventions);

    return conventions;
}

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
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

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
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"});

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
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.0001;

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

    sensiData->discountCurveShiftData()["EUR"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["USD"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["JPY"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["CHF"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

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

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.0001;

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

    sensiData->discountCurveShiftData()["EUR"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["USD"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["JPY"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["CHF"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

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

QuantLib::ext::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, QuantLib::ext::shared_ptr<EngineFactory> factory = {}) {

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
    cpu_timer t_base;
    t_base.start();
    Size portfolioSize = bigPortfolio ? 100 : 1;
    string om_str = (om == ObservationMode::Mode::None)
                        ? "None"
                        : (om == ObservationMode::Mode::Disable)
                              ? "Disable"
                              : (om == ObservationMode::Mode::Defer)
                                    ? "Defer"
                                    : (om == ObservationMode::Mode::Unregister) ? "Unregister" : "???";
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
    conv();

    // build portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize);

    cpu_timer t2;
    t2.start();
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, false);
    sa->generateSensitivities();
    t2.stop();
    Real elapsed = t2.elapsed().wall * 1e-9;
    Size numScenarios = sa->scenarioGenerator()->samples();
    Size scenarioSize = sa->scenarioGenerator()->scenarios().front()->keys().size();
    BOOST_TEST_MESSAGE("number of scenarios=" << numScenarios);
    BOOST_TEST_MESSAGE("Size of scenario = " << scenarioSize << " keys");
    BOOST_TEST_MESSAGE("time = " << elapsed << " seconds");
    Real avTime = (elapsed / ((Real)(numScenarios * portfolioSize)));
    BOOST_TEST_MESSAGE("Average pricing time =  " << avTime << " seconds");
    BOOST_TEST_MESSAGE("Memory usage - " << os::getMemoryUsage());

    ObservationMode::instance().setMode(backupOm);
    t_base.stop();

    BOOST_TEST_MESSAGE("total time = " << t_base.format(default_places, "%w") << " seconds");
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SensitivityPerformanceTest, *boost::unit_test::disabled())

BOOST_AUTO_TEST_CASE(testSensiPerformanceNoneObs) {
    test_performance(false, false, false, false, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceDisableObs) {
    test_performance(false, false, false, false, ObservationMode::Mode::Disable);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceDeferObs) {
    test_performance(false, false, false, false, ObservationMode::Mode::Defer);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceUnregisterObs) {
    test_performance(false, false, false, false, ObservationMode::Mode::Unregister);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceCrossGammaNoneObs) {
    test_performance(false, false, false, true, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceBigScenarioNoneObs) {
    test_performance(false, true, false, false, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceBigPortfolioNoneObs) {
    test_performance(true, false, false, false, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceBigPortfolioBigScenarioNoneObs) {
    test_performance(true, true, false, false, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceBigPortfolioCrossGammaNoneObs) {
    test_performance(true, false, false, true, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceBigScenarioCrossGammaNoneObs) {
    test_performance(false, true, false, true, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testSensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs) {
    test_performance(true, true, false, true, ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
