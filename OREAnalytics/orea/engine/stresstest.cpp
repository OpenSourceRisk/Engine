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

#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/cashflowreportgenerator.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>

#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace ore {
namespace analytics {

namespace {
Real diffWithNull(const Real x, const Real y) {
    if (x == Null<Real>() || y == Null<Real>())
        return Null<Real>();
    return x - y;
}
} // namespace

void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                   const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                   const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                   const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                   const boost::shared_ptr<ore::data::Report>& report,
                   const boost::shared_ptr<ore::data::Report>& cfReport, const double threshold, const Size precision,
                   const bool includePastCashflows, const CurveConfigurations& curveConfigs,
                   const TodaysMarketParameters& todaysMarketParams,
                   QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                   const IborFallbackConfig& iborFallbackConfig, bool continueOnError) {

    // run stress simulation

    LOG("Run Stress Test");

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        market, simMarketData, marketConfiguration, curveConfigs, todaysMarketParams, continueOnError,
        stressData->useSpreadedTermStructures(), false, false, iborFallbackConfig, true);

    Date asof = market->asofDate();
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    scenarioFactory =
        scenarioFactory ? scenarioFactory : QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    QuantLib::ext::shared_ptr<StressScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<StressScenarioGenerator>(stressData, baseScenario, simMarketData, simMarket,
                                                            scenarioFactory, simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;

    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration;
    auto ed = QuantLib::ext::make_shared<EngineData>(*engineData);
    ed->globalParameters()["RunType"] = "Stress";
    QuantLib::ext::shared_ptr<EngineFactory> factory =
        QuantLib::ext::make_shared<EngineFactory>(ed, simMarket, configurations, referenceData, iborFallbackConfig);

    portfolio->reset();
    portfolio->build(factory, "stress analysis");

    QuantLib::ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        asof, portfolio->ids(), vector<Date>(1, asof), scenarioGenerator->samples());

    std::vector<std::vector<std::vector<TradeCashflowReportData>>> cfCube;
    if (cfReport)
        cfCube = std::vector<std::vector<std::vector<TradeCashflowReportData>>>(
            portfolio->ids().size(),
            std::vector<std::vector<TradeCashflowReportData>>(scenarioGenerator->samples() + 1));

    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>("1,0W", NullCalendar());
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(simMarketData->baseCcy()));
    if (cfReport) {
        calculators.push_back(QuantLib::ext::make_shared<CashflowReportCalculator>(simMarketData->baseCcy(),
                                                                                   includePastCashflows, cfCube));
    }
    ValuationEngine engine(asof, dg, simMarket, factory->modelBuilders());

    engine.registerProgressIndicator(
        QuantLib::ext::make_shared<ProgressLog>("stress scenarios", 100, oreSeverity::notice));
    engine.buildCube(portfolio, cube, calculators, ValuationEngine::ErrorPolicy::RemoveSample);

    // write stressed npv report

    report->addColumn("TradeId", string());
    report->addColumn("ScenarioLabel", string());
    report->addColumn("Base NPV", double(), precision);
    report->addColumn("Scenario NPV", double(), precision);
    report->addColumn("Sensitivity", double(), precision);

    for (auto const& [tradeId, trade] : portfolio->trades()) {
        auto index = cube->idsAndIndexes().find(tradeId);
        QL_REQUIRE(index != cube->idsAndIndexes().end(), "runStressTest(): tradeId not found in cube, internal error.");
        Real npv0 = cube->getT0(index->second, 0);
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            const string& label = scenarioGenerator->scenarios()[j]->label();
            TLOG("Adding stress test result for trade '" << tradeId << "' and scenario #" << j << " '" << label << "'");
            Real npv = cube->get(index->second, 0, j, 0);
            Real sensi = npv - npv0;
            if (fabs(sensi) > threshold || QuantLib::close_enough(sensi, threshold)) {
                report->next();
                report->add(tradeId);
                report->add(label);
                report->add(npv0);
                report->add(npv);
                report->add(sensi);
            }
        }
    }

    report->end();

    // write stressed cashflow report

    if (cfReport) {

        cfReport->addColumn("TradeId", string());
        cfReport->addColumn("ScenarioLabel", string());
        cfReport->addColumn("Type", string());
        cfReport->addColumn("CashflowNo", Size());
        cfReport->addColumn("LegNo", Size());
        cfReport->addColumn("PayDate", Date());
        cfReport->addColumn("FlowType", string());
        cfReport->addColumn("Amount_Base", double(), precision);
        cfReport->addColumn("Amount_Scen", double(), precision);
        cfReport->addColumn("Amount_Diff", double(), precision);
        cfReport->addColumn("Currency", string());
        cfReport->addColumn("Coupon_Base", double(), 10);
        cfReport->addColumn("Coupon_Scen", double(), 10);
        cfReport->addColumn("Coupon_Diff", double(), 10);
        cfReport->addColumn("Accrual", double(), 10);
        cfReport->addColumn("AccrualStartDate", Date(), 4);
        cfReport->addColumn("AccrualEndDate", Date(), 4);
        cfReport->addColumn("AccruedAmount_Base", double(), 4);
        cfReport->addColumn("AccruedAmount_Scen", double(), 4);
        cfReport->addColumn("AccruedAmount_Diff", double(), 4);
        cfReport->addColumn("fixingDate", Date());
        cfReport->addColumn("fixingValue_Base", double(), 10);
        cfReport->addColumn("fixingValue_Scen", double(), 10);
        cfReport->addColumn("fixingValue_Diff", double(), 10);
        cfReport->addColumn("Notional_Base", double(), 4);
        cfReport->addColumn("Notional_Scen", double(), 4);
        cfReport->addColumn("Notional_Diff", double(), 4);
        cfReport->addColumn("DiscountFactor_Base", double(), 10);
        cfReport->addColumn("DiscountFactor_Scen", double(), 10);
        cfReport->addColumn("PresentValue_Base", double(), 10);
        cfReport->addColumn("PresentValue_Scen", double(), 10);
        cfReport->addColumn("PresentValue_Diff", double(), 10);
        cfReport->addColumn("FXRate(Local-Base)_Base", double(), 10);
        cfReport->addColumn("FXRate(Local-Base)_Scen", double(), 10);
        cfReport->addColumn("PresentValue(Base)_Base", double(), 10);
        cfReport->addColumn("PresentValue(Base)_Scen", double(), 10);
        cfReport->addColumn("PresentValue(Base)_Diff", double(), 10);
        cfReport->addColumn("BaseCurrency", string());
        cfReport->addColumn("FloorStrike", double(), 6);
        cfReport->addColumn("CapStrike", double(), 6);
        cfReport->addColumn("FloorVolatility_Base", double(), 6);
        cfReport->addColumn("FloorVolatility_Scen", double(), 6);
        cfReport->addColumn("CapVolatility_Base", double(), 6);
        cfReport->addColumn("CapVolatility_Scen", double(), 6);
        cfReport->addColumn("EffectiveFloorVolatility_Base", double(), 6);
        cfReport->addColumn("EffectiveFloorVolatility_Scen", double(), 6);
        cfReport->addColumn("EffectiveCapVolatility_Base", double(), 6);
        cfReport->addColumn("EffectiveCapVolatility_Scen", double(), 6);

        for (auto const& [tradeId, trade] : portfolio->trades()) {
            auto index = cube->idsAndIndexes().find(tradeId);
            QL_REQUIRE(index != cube->idsAndIndexes().end(),
                       "runStressTest(): tradeId not found in cube, internal error.");

            std::map<std::pair<Size, Size>, TradeCashflowReportData> baseCf;
            for (auto const& t : cfCube[index->second][0])
                baseCf[std::make_pair(t.legNo, t.cashflowNo)] = t;

            for (Size j = 0; j < scenarioGenerator->samples(); ++j) {

                const string& label = scenarioGenerator->scenarios()[j]->label();
                TLOG("Adding stress test cashflow result for trade '" << tradeId << "' and scenario #" << j << " '"
                                                                      << label << "'");

                std::map<std::pair<Size, Size>, TradeCashflowReportData> scenCf;
                for (auto const& t : cfCube[index->second][j + 1])
                    scenCf[std::make_pair(t.legNo, t.cashflowNo)] = t;

                for (auto const& [idx, t0] : baseCf) {

                    Real amount0 = t0.amount, amount1 = Null<Real>();
                    Real coupon0 = t0.coupon, coupon1 = Null<Real>();
                    Real accruedAmount0 = t0.accruedAmount, accruedAmount1 = Null<Real>();
                    Real fixingValue0 = t0.fixingValue, fixingValue1 = Null<Real>();
                    Real notional0 = t0.notional, notional1 = Null<Real>();
                    Real discountFactor0 = t0.discountFactor, discountFactor1 = Null<Real>();
                    Real presentValue0 = t0.presentValue, presentValue1 = Null<Real>();
                    Real fxRateLocalBase0 = t0.fxRateLocalBase, fxRateLocalBase1 = Null<Real>();
                    Real presentValueBase0 = t0.presentValueBase, presentValueBase1 = Null<Real>();
                    Real floorVolatility0 = t0.floorVolatility, floorVolatility1 = Null<Real>();
                    Real capVolatility0 = t0.capVolatility, capVolatility1 = Null<Real>();
                    Real effectiveFloorVolatility0 = t0.effectiveFloorVolatility,
                         effectiveFloorVolatility1 = Null<Real>();
                    Real effectiveCapVolatility0 = t0.effectiveCapVolatility, effectiveCapVolatility1 = Null<Real>();

                    if (auto scen = scenCf.find(idx); scen != scenCf.end()) {
                        amount1 = scen->second.amount;
                        coupon1 = scen->second.coupon;
                        accruedAmount1 = scen->second.accruedAmount;
                        fixingValue1 = scen->second.fixingValue;
                        notional1 = scen->second.notional;
                        discountFactor1 = scen->second.discountFactor;
                        presentValue1 = scen->second.presentValue;
                        fxRateLocalBase1 = scen->second.fxRateLocalBase;
                        presentValueBase1 = scen->second.presentValueBase;
                        floorVolatility1 = scen->second.floorVolatility;
                        capVolatility1 = scen->second.capVolatility;
                        effectiveFloorVolatility1 = scen->second.effectiveFloorVolatility;
                        effectiveCapVolatility1 = scen->second.effectiveCapVolatility;
                    }

                    cfReport->next();
                    cfReport->add(tradeId);
                    cfReport->add(label);
                    cfReport->add(trade->tradeType());
                    cfReport->add(idx.second);
                    cfReport->add(idx.first);
                    cfReport->add(t0.payDate);
                    cfReport->add(t0.flowType);
                    cfReport->add(amount0);
                    cfReport->add(amount1);
                    cfReport->add(diffWithNull(amount1, amount0));
                    cfReport->add(t0.currency);
                    cfReport->add(coupon0);
                    cfReport->add(coupon1);
                    cfReport->add(diffWithNull(coupon1, coupon0));
                    cfReport->add(t0.accrual);
                    cfReport->add(t0.accrualStartDate);
                    cfReport->add(t0.accrualEndDate);
                    cfReport->add(accruedAmount0);
                    cfReport->add(accruedAmount1);
                    cfReport->add(diffWithNull(accruedAmount1, accruedAmount0));
                    cfReport->add(t0.fixingDate);
                    cfReport->add(fixingValue0);
                    cfReport->add(fixingValue1);
                    cfReport->add(diffWithNull(fixingValue1, fixingValue0));
                    cfReport->add(notional0);
                    cfReport->add(notional1);
                    cfReport->add(diffWithNull(notional1, notional0));
                    cfReport->add(discountFactor0);
                    cfReport->add(discountFactor1);
                    cfReport->add(presentValue0);
                    cfReport->add(presentValue1);
                    cfReport->add(diffWithNull(presentValue1, presentValue0));
                    cfReport->add(fxRateLocalBase0);
                    cfReport->add(fxRateLocalBase1);
                    cfReport->add(presentValueBase0);
                    cfReport->add(presentValueBase1);
                    cfReport->add(diffWithNull(presentValueBase1, presentValueBase0));
                    cfReport->add(t0.baseCurrency);
                    cfReport->add(t0.floorStrike);
                    cfReport->add(t0.capStrike);
                    cfReport->add(floorVolatility0);
                    cfReport->add(floorVolatility1);
                    cfReport->add(capVolatility0);
                    cfReport->add(capVolatility1);
                    cfReport->add(effectiveFloorVolatility0);
                    cfReport->add(effectiveFloorVolatility1);
                    cfReport->add(effectiveCapVolatility0);
                    cfReport->add(effectiveCapVolatility1);
                }
            }
        }
        cfReport->end();
    }

    LOG("Stress testing done");
}

} // namespace analytics
} // namespace ore
