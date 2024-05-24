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

#include "parsensitivityanalysismanual.hpp"
#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/zerotoparcube.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>

#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/cdo.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/report/csvreport.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>

#include <oret/toplevelfixture.hpp>

#include <ql/time/date.hpp>

#include <test/oreatoplevelfixture.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

namespace {

QuantLib::ext::shared_ptr<EngineFactory> registerBuilders(QuantLib::ext::shared_ptr<EngineData> engineData,
                                                  QuantLib::ext::shared_ptr<Market> market) {
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);
    return factory;
}

void parSensiBumpAnalysis(QuantLib::ext::shared_ptr<Portfolio>& portfolio, QuantLib::ext::shared_ptr<EngineData>& engineData,
                          QuantLib::ext::shared_ptr<Market>& initMarket, map<string, Real>& baseManualPv, string& baseCcy,
                          vector<Handle<Quote>>& parValVecBase, vector<string>& labelVec, Real shiftSize,
                          ShiftType shiftType, std::map<std::pair<std::string, std::string>, Real>& parDelta,
                          std::map<std::pair<std::string, std::string>, Real>& zeroDelta, map<string, Real>& basePv) {

    Size tradeCount = portfolio->size();
    Real basePvTol = 0.00001;      // tolerance for base PV after re-bootstrapping
    Real sensiThold = 0.0001;      // sensi values below this are ignored
    Real sensiRelTol = 1.0;        // relative tolerance on par sensitivities
    Real sensiAbsTol = 0.2;        // absolute tolerance on par sensitivities for small values (EUR)
    Real relAbsTolBoundary = 10.0; // abs val threshold, below this abs tolerance checked, above this rel tolerance

    vector<Real> baseValues;
    for (Size i = 0; i < parValVecBase.size(); ++i) {
        Handle<Quote> q = parValVecBase[i];
        Real baseVal = q->value();
        baseValues.push_back(baseVal);
        QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*q)->setValue(baseVal);
    }
    QuantLib::ext::shared_ptr<EngineFactory> manualFactory2 = registerBuilders(engineData, initMarket);
    portfolio->reset();
    portfolio->build(manualFactory2);
    BOOST_CHECK_MESSAGE(portfolio->size() == tradeCount,
                        "Some trades not built correctly," << portfolio->size() << " vs. " << tradeCount);
    for (auto& [tradeId, trade] : portfolio->trades()) {
        Real fx = 1.0;
        if (trade->npvCurrency() != "EUR") {
            string ccyPair = trade->npvCurrency() + baseCcy;
            fx = initMarket->fxRate(ccyPair)->value();
        }
        BOOST_CHECK_MESSAGE(std::fabs(baseManualPv[tradeId] - (fx * trade->instrument()->NPV())) <= basePvTol,
                            "Equality error for trade " << tradeId << "; got " << (fx * trade->instrument()->NPV())
                                                        << ", but expected " << baseManualPv[tradeId]);
    }
    // bump each point in sequence and generate new par curve
    for (Size i = 0; i < parValVecBase.size(); ++i) {

        string sensiLabel = labelVec[i];
        // if ((curveType == "CDSVolatility") || (curveType == "EquityVolatility")) sensiLabel = sensiLabel + "/ATM";
        for (Size j = 0; j < parValVecBase.size(); ++j) {
            if (i == j) {
                Real newVal = (shiftType == ShiftType::Absolute) ? (baseValues[j] + shiftSize)
                                                                 : (baseValues[j] * (1.0 + shiftSize));
                QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*parValVecBase[j])->setValue(newVal);
            } else {
                QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*parValVecBase[j])->setValue(baseValues[j]);
            }
        }
        QuantLib::ext::shared_ptr<EngineFactory> manualFactoryDisc = registerBuilders(engineData, initMarket);
        portfolio->reset();
        portfolio->build(manualFactoryDisc);
        BOOST_CHECK_MESSAGE(portfolio->size() == tradeCount,
                            "Some trades not built correctly," << portfolio->size() << " vs. " << tradeCount);
        for (auto [tradeId, trade] : portfolio->trades()) {
            Real fx = 1.0;
            if (trade->npvCurrency() != "EUR") {
                string ccyPair = trade->npvCurrency() + baseCcy;
                fx = initMarket->fxRate(ccyPair)->value();
            }
            Real shiftedNPV = fx * trade->instrument()->NPV();
            Real anticipatedParDelta = shiftedNPV - baseManualPv[tradeId];
            Real computedParDelta = 0.0;
            if (parDelta.find(std::make_pair(tradeId, sensiLabel)) != parDelta.end())
                computedParDelta = parDelta[std::make_pair(tradeId, sensiLabel)];

            if ((std::fabs(anticipatedParDelta) > sensiThold) || (std::fabs(computedParDelta) > sensiThold)) {
                BOOST_TEST_MESSAGE("#reportrow," << tradeId << "," << sensiLabel << "," << baseManualPv[tradeId]
                                                 << "," << basePv[tradeId] << "," << anticipatedParDelta << ","
                                                 << computedParDelta << ","
                                                 << zeroDelta[std::make_pair(tradeId, sensiLabel)]);

                if ((std::fabs(anticipatedParDelta) > relAbsTolBoundary) &&
                    (std::fabs(computedParDelta) > relAbsTolBoundary))
                    BOOST_CHECK_CLOSE(anticipatedParDelta, computedParDelta, sensiRelTol);
                else
                    BOOST_CHECK(std::fabs(anticipatedParDelta - computedParDelta) < sensiAbsTol);
            }
        }
    }
    // set back to the base curve
    for (Size i = 0; i < parValVecBase.size(); ++i) {
        QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*parValVecBase[i])->setValue(baseValues[i]);
    }
    QuantLib::ext::shared_ptr<EngineFactory> manualFactory3 = registerBuilders(engineData, initMarket);

    portfolio->reset();
    portfolio->build(manualFactory3);
    BOOST_CHECK_MESSAGE(portfolio->size() == tradeCount,
                        "Some trades not built correctly," << portfolio->size() << " vs. " << tradeCount);
    for (auto [tradeId, trade] : portfolio->trades()) {
        Real fx = 1.0;
        if (trade->npvCurrency() != "EUR") {
            string ccyPair = trade->npvCurrency() + baseCcy;
            fx = initMarket->fxRate(ccyPair)->value();
        }
        BOOST_CHECK_MESSAGE(std::fabs(baseManualPv[tradeId] - (fx * trade->instrument()->NPV())) <= basePvTol,
                            "Equality error for trade " << tradeId << "; got " << (fx * trade->instrument()->NPV())
                                                        << ", but expected " << baseManualPv[tradeId]);
    }
}
} // namespace

namespace testsuite {

void ParSensitivityAnalysisManualTest::testParSwapBenchmark() {

    BOOST_TEST_MESSAGE("Testing swap par sensitivities against manual bump of par curve instruments");
    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::Mode om = ObservationMode::Mode::None;
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
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarketParCurves>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData(true, false);

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData =
        TestConfigurationObjects::setupSensitivityScenarioData(true, false, true);
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
    engineData->model("CreditDefaultSwap") = "DiscountedCashflows";
    engineData->engine("CreditDefaultSwap") = "MidPointCdsEngine";

    engineData->model("IndexCreditDefaultSwapOption") = "Black";
    engineData->engine("IndexCreditDefaultSwapOption") = "BlackIndexCdsOptionEngine";
    map<string, string> engineParamMap1;
    engineParamMap1["Curve"] = "Underlying";
    engineData->engineParameters("IndexCreditDefaultSwapOption") = engineParamMap1;

    engineData->model("IndexCreditDefaultSwap") = "DiscountedCashflows";
    engineData->engine("IndexCreditDefaultSwap") = "MidPointIndexCdsEngine";
    map<string, string> engineParamMap2;
    engineParamMap2["Curve"] = "Underlying";
    engineData->engineParameters("IndexCreditDefaultSwap") = engineParamMap2;

    engineData->model("CMS") = "LinearTSR";
    engineData->engine("CMS") = "LinearTSRPricer";
    map<string, string> engineparams;
    engineparams["MeanReversion"] = "0.0";
    engineparams["Policy"] = "RateBound";
    engineparams["LowerRateBoundNormal"] = "-2.0000";
    engineparams["UpperRateBoundNormal"] = "2.0000";
    engineData->engineParameters("CMS") = engineparams;
    engineData->model("SyntheticCDO") = "GaussCopula";
    engineData->engine("SyntheticCDO") = "Bucketing";
    map<string, string> modelParamMap3;
    map<string, string> engineParamMap3;
    modelParamMap3["correlation"] = "0.0";
    modelParamMap3["min"] = "-5.0";
    modelParamMap3["max"] = "5.0";
    modelParamMap3["steps"] = "64";
    engineParamMap3["buckets"] = "200";
    engineParamMap3["homogeneousPoolWhenJustified"] = "N";
    engineData->modelParameters("SyntheticCDO") = modelParamMap3;
    engineData->engineParameters("SyntheticCDO") = engineParamMap3;

    engineData->model("EquityOption") = "BlackScholesMerton";
    engineData->engine("EquityOption") = "AnalyticEuropeanEngine";

    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> builders;
    // builders.push_back(QuantLib::ext::make_shared<oreplus::data::MidPointIndexCdsEngineBuilder>());
    // builders.push_back(QuantLib::ext::make_shared<oreplus::data::BlackIndexCdsOptionEngineBuilder>());
    builders.push_back(QuantLib::ext::make_shared<ore::data::GaussCopulaBucketingCdoEngineBuilder>());

    QuantLib::ext::shared_ptr<EngineFactory> factory = registerBuilders(engineData, initMarket);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<IRSwapConvention> eurConv =
        QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conventions->get("EUR-6M-SWAP-CONVENTIONS"));
    string eurIdx = "EUR-EURIBOR-6M";
    Period eurFloatTenor = initMarket->iborIndex(eurIdx)->tenor();
    QuantLib::ext::shared_ptr<IRSwapConvention> usdConv =
        QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conventions->get("USD-6M-SWAP-CONVENTIONS"));
    string usdIdx = "USD-LIBOR-6M";
    Period usdFloatTenor = initMarket->iborIndex(usdIdx)->tenor();
    QuantLib::ext::shared_ptr<IRSwapConvention> jpyConv =
        QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conventions->get("JPY-6M-SWAP-CONVENTIONS"));
    string jpyIdx = "JPY-LIBOR-6M";
    Period jpyFloatTenor = initMarket->iborIndex(jpyIdx)->tenor();
    QuantLib::ext::shared_ptr<CrossCcyBasisSwapConvention> chfBasisConv =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(conventions->get("CHF-XCCY-BASIS-CONVENTIONS"));
    QuantLib::ext::shared_ptr<CdsConvention> cdsConv =
        QuantLib::ext::dynamic_pointer_cast<CdsConvention>(conventions->get("CDS-STANDARD-CONVENTIONS"));

    string chfIdx = chfBasisConv->spreadIndexName();
    string otherIdx = chfBasisConv->flatIndexName();
    Period chfFloatTenor = initMarket->iborIndex(chfIdx)->tenor();
    Period otherFloatTenor = initMarket->iborIndex(otherIdx)->tenor();

    portfolio->add(buildSwap(
        "1_Swap_EUR", "EUR", true, 10000000.0, 0, 10, 0.02, 0.00,
        ore::data::to_string(Period(eurConv->fixedFrequency())), ore::data::to_string(eurConv->fixedDayCounter()),
        ore::data::to_string(eurFloatTenor), "A360", eurIdx, eurConv->fixedCalendar(),
        initMarket->iborIndex(eurIdx)->fixingDays(), true));
    portfolio->add(buildSwap(
        "2_Swap_USD", "USD", true, 10000000.0, 0, 15, 0.03, 0.00,
        ore::data::to_string(Period(usdConv->fixedFrequency())), ore::data::to_string(usdConv->fixedDayCounter()),
        ore::data::to_string(usdFloatTenor), "A360", usdIdx, usdConv->fixedCalendar(),
        initMarket->iborIndex(usdIdx)->fixingDays(), true));
    portfolio->add(buildCap(
        "9_Cap_EUR", "EUR", "Long", 0.02, 1000000.0, 0, 10, ore::data::to_string(eurFloatTenor), "A360", eurIdx,
        eurConv->fixedCalendar(), initMarket->iborIndex(eurIdx)->fixingDays(), true));
    portfolio->add(buildFloor(
        "10_Floor_USD", "USD", "Long", 0.03, 1000000.0, 0, 10, ore::data::to_string(usdFloatTenor), "A360", usdIdx,
        usdConv->fixedCalendar(), initMarket->iborIndex(usdIdx)->fixingDays(), true));
    portfolio->add(buildSwap(
        "3_Swap_EUR", "EUR", false, 10000000.0, 1, 12, 0.025, 0.00,
        ore::data::to_string(Period(eurConv->fixedFrequency())), ore::data::to_string(eurConv->fixedDayCounter()),
        ore::data::to_string(eurFloatTenor), "A360", eurIdx, eurConv->fixedCalendar(),
        initMarket->iborIndex(eurIdx)->fixingDays(), true));
    portfolio->add(buildCrossCcyBasisSwap(
        "4_XCCY_SWAP", "CHF", 10000000, "EUR", 10000000, 0, 15, 0.0000, 0.0000, ore::data::to_string(chfFloatTenor),
        "A360", chfIdx, chfBasisConv->settlementCalendar(), ore::data::to_string(otherFloatTenor), "A360", otherIdx,
        chfBasisConv->settlementCalendar(), chfBasisConv->settlementDays(), true));
    portfolio->add(buildCrossCcyBasisSwap(
        "5_XCCY_SWAP_WithPrincipal", "CHF", 10000000, "EUR", 10000000, 0, 15, 0.0000, 0.0000,
        ore::data::to_string(chfFloatTenor), "A360", chfIdx, chfBasisConv->settlementCalendar(),
        ore::data::to_string(otherFloatTenor), "A360", otherIdx, chfBasisConv->settlementCalendar(),
        chfBasisConv->settlementDays(), true, true, true, true, false));
    portfolio->add(buildSwap(
        "6_Swap_JPY", "JPY", true, 1000000000.0, 0, 10, 0.005, 0.00,
        ore::data::to_string(Period(jpyConv->fixedFrequency())), ore::data::to_string(jpyConv->fixedDayCounter()),
        ore::data::to_string(jpyFloatTenor), "A360", jpyIdx, jpyConv->fixedCalendar(),
        initMarket->iborIndex(jpyIdx)->fixingDays(), true));
    portfolio->add(buildCrossCcyBasisSwap(
        "7_XCCY_SWAP_OffMarket", "EUR", 10000000, "CHF", 10500000, 0, 15, 0.0000, 0.0010,
        ore::data::to_string(chfFloatTenor), "A360", chfIdx, chfBasisConv->settlementCalendar(),
        ore::data::to_string(otherFloatTenor), "A360", otherIdx, chfBasisConv->settlementCalendar(),
        chfBasisConv->settlementDays(), true));
    portfolio->add(buildCrossCcyBasisSwap(
        "8_XCCY_SWAP_RESET", "CHF", 10000000, "EUR", 10000000, 0, 15, 0.0000, 0.0000,
        ore::data::to_string(chfFloatTenor), "A360", chfIdx, chfBasisConv->settlementCalendar(),
        ore::data::to_string(otherFloatTenor), "A360", otherIdx, chfBasisConv->settlementCalendar(),
        chfBasisConv->settlementDays(), true, true, true, false, false, true));
    portfolio->add(buildCreditDefaultSwap("9_CDS_USD", "USD", "dc", "dc", true, 10000000, 0, 15, 0.4,
                                          0.009, ore::data::to_string(Period(cdsConv->frequency())),
                                          ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "9_CDS_EUR", "EUR", "dc2", "dc2", true, 10000000, 0, 15, 0.4, 0.009,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "10_CDS_USD", "USD", "dc", "dc", true, 10000000, 0, 10, 0.4, 0.001,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "10_CDS_EUR", "EUR", "dc2", "dc2", true, 10000000, 0, 10, 0.4, 0.001,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "11_CDS_EUR", "EUR", "dc2", "dc2", true, 10000000, 0, 5, 0.4, 0.001,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap("11_CDS_USD", "USD", "dc", "dc", true, 10000000, 0, 5, 0.4,
                                          0.001, ore::data::to_string(Period(cdsConv->frequency())),
                                          ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap("12_CDS_USD", "USD", "dc", "dc", true, 10000000, 0, 2, 0.4,
                                          0.004, ore::data::to_string(Period(cdsConv->frequency())),
                                          ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "12_CDS_EUR", "EUR", "dc2", "dc2", true, 10000000, 0, 2, 0.4, 0.001,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "13_CDS_USD", "USD", "dc", "dc", true, 10000000, 0, 15, 0.4, 0.001,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));
    portfolio->add(buildCreditDefaultSwap(
        "13_CDS_EUR", "EUR", "dc2", "dc2", true, 10000000, 0, 15, 0.4, 0.001,
        ore::data::to_string(Period(cdsConv->frequency())), ore::data::to_string(cdsConv->dayCounter())));

    vector<string> names = {"dc", "dc2", "dc3"};
    vector<string> indexCcys = {"USD", "EUR", "GBP"};
    vector<Real> notionals = {3000000, 3000000, 3000000};
    // portfolio->add(buildIndexCdsOption("14_IndexCDSOption_USD", "dc", names, "Long", "USD",
    // indexCcys, true, notionals, notional, 1, 4,
    //                                                         0.4, 0.001,
    //                                                         ore::data::to_string(Period(cdsConv->frequency())),
    //                                                        ore::data::to_string(cdsConv->dayCounter()), "Physical"));

    vector<string> names2(1, "dc2");
    vector<string> indexCcys2(1, "EUR");
    vector<Real> notionals2(1, 10000000.0);
    // portfolio->add(buildIndexCdsOption("15_IndexCDSOption_EUR", "dc2", names2, "Long", "EUR",
    // indexCcys2, true, notionals2, 10000000.0, 1, 4,
    //                                                       0.4, 0.001,
    //                                                       ore::data::to_string(Period(cdsConv->frequency())),
    //                                                       ore::data::to_string(cdsConv->dayCounter()), "Physical"));
    portfolio->add(buildSyntheticCDO("16_SyntheticCDO_EUR", "dc2", names2, "Long", "EUR", indexCcys2,
                                     true, notionals2, 1000000.0, 0, 5, 0.03, 0.01, "1Y", "30/360"));

    portfolio->add(buildCmsCapFloor("17_CMS_EUR", "EUR", "EUR-CMS-30Y", true, 2000000, 0, 5, 0.0, 1,
                                    0.0, ore::data::to_string(Period(eurConv->fixedFrequency())),
                                    ore::data::to_string(eurConv->fixedDayCounter())));
    portfolio->add(
        buildEquityOption("18_EquityOption_SP5", "Long", "Call", 2, "SP5", "USD", 2147.56, 775));

    portfolio->add(buildCPIInflationSwap("19_CPIInflationSwap_UKRPI", "GBP", true, 100000.0, 0, 10,
                                         0.0, "6M", "ACT/ACT", "GBP-LIBOR-6M", "1Y", "ACT/ACT",
                                         "UKRPI", 201.0, "2M", false, 0.005));
    portfolio->add(buildYYInflationSwap("20_YoYInflationSwap_UKRPI", "GBP", true, 100000.0, 0, 10,
                                        0.0, "1Y", "ACT/ACT", "GBP-LIBOR-6M", "1Y", "ACT/ACT",
                                        "UKRPI", "2M", 2));

    Size tradeCount = portfolio->size();
    portfolio->build(factory);
    BOOST_CHECK_MESSAGE(portfolio->size() == tradeCount,
                        "Some trades not built correctly," << portfolio->size() << " vs. " << tradeCount);
    // build the sensitivity analysis object
    QuantLib::ext::shared_ptr<SensitivityAnalysis> zeroAnalysis =
        QuantLib::ext::make_shared<SensitivityAnalysis>(portfolio, initMarket, Market::defaultConfiguration, engineData,
                                                simMarketData, sensiData,
                                                false, nullptr, nullptr, false, nullptr);
    ParSensitivityAnalysis parAnalysis(today, simMarketData, *sensiData, Market::defaultConfiguration);
    parAnalysis.alignPillars();
    zeroAnalysis->overrideTenors(true);
    zeroAnalysis->generateSensitivities();
    parAnalysis.computeParInstrumentSensitivities(zeroAnalysis->simMarket());
    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
        QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis.parSensitivities(), parAnalysis.shiftSizes());
    QuantLib::ext::shared_ptr<SensitivityCube> sensiCube = zeroAnalysis->sensiCube();
    ZeroToParCube parCube(sensiCube, parConverter);

    std::map<std::pair<std::string, std::string>, Real> parDelta;
    std::map<std::pair<std::string, std::string>, Real> zeroDelta;
    map<string, Real> baseManualPv;
    map<string, Real> basePv;
    for (const auto& tradeId : portfolio->ids()) {
        basePv[tradeId] = sensiCube->npv(tradeId);
        for (const auto& f : sensiCube->factors()) {
            string des = sensiCube->factorDescription(f);
            zeroDelta[make_pair(tradeId, des)] = sensiCube->delta(tradeId, f);
        }
        // Fill the par deltas map
        auto temp = parCube.parDeltas(tradeId);
        for (const auto& kv : temp) {
            string des = sensiCube->factorDescription(kv.first);
            parDelta[make_pair(tradeId, des)] = kv.second;
        }
    }
    QuantLib::ext::shared_ptr<EngineFactory> manualFactory = registerBuilders(engineData, initMarket);

    portfolio->reset();
    portfolio->build(manualFactory);

    for (auto [tradeId, trade] : portfolio->trades()) {
        Real fx = 1.0;
        if (trade->npvCurrency() != "EUR") {
            string ccyPair = trade->npvCurrency() + baseCcy;
            fx = initMarket->fxRate(ccyPair)->value();
        }
        baseManualPv[tradeId] = fx * trade->instrument()->NPV();
        Real tradeNotional = fx * trade->notional();
        Real simMarketTol =
            1.e-5 * tradeNotional; // tolerance for difference to sim market is 0.1bp upfront (should this be tightened?)

        BOOST_TEST_MESSAGE("Base PV check for trade " << tradeId << "; got " << baseManualPv[tradeId]
                                                      << ", expected " << basePv[tradeId]);
        BOOST_CHECK_MESSAGE(std::fabs(baseManualPv[tradeId] - basePv[tradeId]) < simMarketTol,
                            "Base PV check error for trade " << tradeId << "; got " << baseManualPv[tradeId]
                                                             << ", but expected " << basePv[tradeId]);
    }

    QuantLib::ext::shared_ptr<TestMarketParCurves> initParMarket = QuantLib::ext::dynamic_pointer_cast<TestMarketParCurves>(initMarket);
    BOOST_ASSERT(initParMarket);
    BOOST_TEST_MESSAGE("testing discount curve par sensis");
    // the discount curve par sensis
    for (auto d_it : initParMarket->discountRateHelpersInstMap()) {
        string ccy = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->discountCurveShiftData()[ccy]->shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->discountCurveShiftData()[ccy]->shiftType;
        vector<Period> parTenorVec = initParMarket->discountRateHelperTenorsMap().find(ccy)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->discountRateHelperValuesMap().find(ccy)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "DiscountCurve/" + ccy + "/" + to_string(i) + "/" + to_string(parTenorVec[i]);
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, parDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing index curve par sensis");
    // the index curve par sensis
    for (auto d_it : initParMarket->indexCurveRateHelperInstMap()) {
        string idxName = d_it.first;
        BOOST_TEST_MESSAGE(idxName);
        auto itr = zeroAnalysis->sensitivityData()->indexCurveShiftData().find(idxName);
        if (itr == zeroAnalysis->sensitivityData()->indexCurveShiftData().end())
            zeroAnalysis->sensitivityData()->indexCurveShiftData()[idxName] =
                QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>();
        Real shiftSize = zeroAnalysis->sensitivityData()->indexCurveShiftData()[idxName]->shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->indexCurveShiftData()[idxName]->shiftType;
        vector<Period> parTenorVec = initParMarket->indexCurveRateHelperTenorsMap().find(idxName)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->indexCurveRateHelperValuesMap().find(idxName)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "IndexCurve/" + idxName + "/" + to_string(i) + "/" + to_string(parTenorVec[i]);
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, parDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing default curve par sensis");
    // the default curve par sensis
    for (auto d_it : initParMarket->defaultRateHelpersInstMap()) {
        string name = d_it.first;
        auto itr = zeroAnalysis->sensitivityData()->creditCurveShiftData().find(name);
        if (itr == zeroAnalysis->sensitivityData()->creditCurveShiftData().end())
            zeroAnalysis->sensitivityData()->creditCurveShiftData()[name] =
                QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>();
        Real shiftSize = zeroAnalysis->sensitivityData()->creditCurveShiftData()[name]->shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->creditCurveShiftData()[name]->shiftType;
        vector<Period> parTenorVec = initParMarket->defaultRateHelperTenorsMap().find(name)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->defaultRateHelperValuesMap().find(name)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "SurvivalProbability/" + name + "/" + to_string(i) + "/" + to_string(parTenorVec[i]);
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, parDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing cds par sensis");
    // CDS Vol sensis (compare with zero sensi)
    for (auto d_it : initParMarket->cdsVolRateHelperValuesMap()) {
        string name = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->cdsVolShiftData()[name].shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->cdsVolShiftData()[name].shiftType;
        vector<Period> parTenorVec = initParMarket->cdsVolRateHelperTenorsMap().find(name)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->cdsVolRateHelperValuesMap().find(name)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "CDSVolatility/" + name + "/" + to_string(i) + "/" + to_string(parTenorVec[i]) + "/ATM";
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, zeroDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing eqVol curve par sensis");
    // Equity Vol sensis (compare with zero sensi)
    for (auto d_it : initParMarket->equityVolRateHelperValuesMap()) {
        string name = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->equityVolShiftData()[name].shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->equityVolShiftData()[name].shiftType;
        vector<Period> parTenorVec = initParMarket->equityVolRateHelperTenorsMap().find(name)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->equityVolRateHelperValuesMap().find(name)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "EquityVolatility/" + name + "/" + to_string(i) + "/" + to_string(parTenorVec[i]) + "/ATM";
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, zeroDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing swaption vol par sensis");
    // Swaption Vol sensis (compare with zero sensi)
    for (auto d_it : initParMarket->swaptionVolRateHelperValuesMap()) {
        string name = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->swaptionVolShiftData()[name].shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->swaptionVolShiftData()[name].shiftType;
        vector<Period> parTenorVec = initParMarket->swaptionVolRateHelperTenorsMap().find(name)->second;
        vector<Period> swapTenorVec = initParMarket->swaptionVolRateHelperSwapTenorsMap().find(name)->second;
        vector<Real> strikeSpreadVec = zeroAnalysis->sensitivityData()->swaptionVolShiftData()[name].shiftStrikes;
        vector<Handle<Quote>> parValVecBase = initParMarket->swaptionVolRateHelperValuesMap().find(name)->second;
        vector<string> sensiLabels(parValVecBase.size());
        Size j = swapTenorVec.size();
        Size k = zeroAnalysis->sensitivityData()->swaptionVolShiftData()[name].shiftStrikes.size();

        for (Size i = 0; i < parValVecBase.size(); i++) {
            Size strike = i % k;
            Size parTenor = i / (j * k);
            Size swapTenor = (i - parTenor * j * k - strike) / k;
            std::ostringstream o;
            if (close_enough(strikeSpreadVec[strike], 0))
                o << "SwaptionVolatility/" << name << "/" << i << "/" << parTenorVec[parTenor] << "/"
                  << swapTenorVec[swapTenor] << "/ATM";
            else
                o << "SwaptionVolatility/" << name << "/" << i << "/" << parTenorVec[parTenor] << "/"
                  << swapTenorVec[swapTenor] << "/" << std::setprecision(4) << strikeSpreadVec[strike];
            sensiLabels[i] = o.str();
        }

        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, zeroDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing base correlation par sensis");
    // Base correlations sensis (compare with zero sensi)
    for (auto d_it : initParMarket->baseCorrRateHelperValuesMap()) {
        string name = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->baseCorrelationShiftData()[name].shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->baseCorrelationShiftData()[name].shiftType;
        vector<Period> parTenorVec = initParMarket->baseCorrRateHelperTenorsMap().find(name)->second;
        vector<string> lossLevelVec = initParMarket->baseCorrLossLevelsMap().find(name)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->baseCorrRateHelperValuesMap().find(name)->second;
        vector<string> sensiLabels(parValVecBase.size());
        Size j = lossLevelVec.size();
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "BaseCorrelation/" + name + "/" + to_string(i) + "/" + lossLevelVec[i % j] + "/" +
                             to_string(parTenorVec[i / j]);
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, zeroDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing zero inflation par sensis");
    // Zero Inflation sensis
    for (auto d_it : initParMarket->zeroInflationRateHelperInstMap()) {
        string idxName = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->zeroInflationCurveShiftData()[idxName]->shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->zeroInflationCurveShiftData()[idxName]->shiftType;
        vector<Period> parTenorVec = initParMarket->zeroInflationRateHelperTenorsMap().find(idxName)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->zeroInflationRateHelperValuesMap().find(idxName)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "ZeroInflationCurve/" + idxName + "/" + to_string(i) + "/" + to_string(parTenorVec[i]);
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, parDelta, zeroDelta, basePv);
    }
    BOOST_TEST_MESSAGE("testing yoy inflation par sensis");
    // YoY Inflation sensis
    for (auto d_it : initParMarket->yoyInflationRateHelperInstMap()) {
        string idxName = d_it.first;
        Real shiftSize = zeroAnalysis->sensitivityData()->yoyInflationCurveShiftData()[idxName]->shiftSize;
        ShiftType shiftType = zeroAnalysis->sensitivityData()->yoyInflationCurveShiftData()[idxName]->shiftType;
        vector<Period> parTenorVec = initParMarket->yoyInflationRateHelperTenorsMap().find(idxName)->second;
        vector<Handle<Quote>> parValVecBase = initParMarket->yoyInflationRateHelperValuesMap().find(idxName)->second;
        vector<string> sensiLabels(parValVecBase.size());
        for (Size i = 0; i < parValVecBase.size(); i++) {
            sensiLabels[i] = "YoYInflationCurve/" + idxName + "/" + to_string(i) + "/" + to_string(parTenorVec[i]);
        }
        parSensiBumpAnalysis(portfolio, engineData, initMarket, baseManualPv, baseCcy, parValVecBase, sensiLabels,
                             shiftSize, shiftType, parDelta, zeroDelta, basePv);
    }

    // end
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(ParSensitivityAnalysisManual)

BOOST_AUTO_TEST_CASE(ParSwapBenchmarkTest){ 
    BOOST_TEST_MESSAGE("Testing ParSwapBenchmark");
    ParSensitivityAnalysisManualTest::testParSwapBenchmark();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testsuite
