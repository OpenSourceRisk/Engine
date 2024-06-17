/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/parscenarioanalytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/analytics/xvaexplainanalytic.hpp>
#include <orea/app/analytics/xvastressanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/parstressconverter.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/utilities.hpp>
namespace ore {
namespace analytics {

void curveShiftData(std::map<std::string, StressTestScenarioData::CurveShiftData>& data, const RiskFactorKey& key, double shift,
                    const std::vector<Period>& tenors) {
    if(data.count(key.name)==0){
        StressTestScenarioData::CurveShiftData shiftData;
        shiftData.shiftType = ShiftType::Absolute;
        shiftData.shiftTenors = tenors;
        shiftData.shifts = std::vector<double>(tenors.size(), 0.0);
        data[key.name] = shiftData;
    }
    data[key.name].shifts[key.index] = shift;
}

void volShiftData(std::map<std::string, StressTestScenarioData::VolShiftData>& volData, const RiskFactorKey& key, double shift, const std::vector<QuantLib::Period>& tenors) {
    if (volData.count(key.name) == 0) {
        StressTestScenarioData::VolShiftData shiftData;
        shiftData.shiftType = ShiftType::Absolute;
        shiftData.shiftExpiries = tenors;
        shiftData.shifts = std::vector<double>(shiftData.shiftExpiries.size(), 0.0);
        volData[key.name] = shiftData;
    }
    volData[key.name].shifts[key.index] = shift;
}

void swaptionVolShiftData(std::map<std::string, StressTestScenarioData::SwaptionVolShiftData>& volData,
                          const RiskFactorKey& key, double shift, const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParameters) {
    
    if(volData.count(key.name) == 0){
        StressTestScenarioData::SwaptionVolShiftData shiftData;

        shiftData.shiftType = ShiftType::Absolute;
        shiftData.shiftExpiries = simParameters->swapVolExpiries(key.name);
        shiftData.shiftTerms = simParameters->swapVolTerms(key.name);
        for (const auto& expiry : shiftData.shiftExpiries) {
            for (const auto& term : shiftData.shiftTerms) {
                shiftData.shifts[std::make_pair(expiry, term)] = 0;
            }
        }
        volData[key.name] = shiftData;
    }
    auto& shiftData = volData[key.name];

    size_t expiryIndex = key.index / shiftData.shiftTerms.size();
    size_t termIndex = key.index % shiftData.shiftTerms.size();
    auto expiry = shiftData.shiftExpiries[expiryIndex];
    auto term = shiftData.shiftTerms[termIndex];
    shiftData.shifts[std::make_pair(expiry, term)] = shift;
}

void capFloorVolShiftData(std::map<std::string, StressTestScenarioData::CapFloorVolShiftData>& volData,
                          const RiskFactorKey& key, double shift,
                          const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParameters) {
    if (volData.count(key.name) == 0) {

        StressTestScenarioData::CapFloorVolShiftData shiftData;
        shiftData.shiftType = ShiftType::Absolute;
        shiftData.shiftExpiries = simParameters->capFloorVolExpiries(key.name);
        shiftData.shiftStrikes = simParameters->capFloorVolStrikes(key.name);
        for (const auto& expiry : shiftData.shiftExpiries) {
            shiftData.shifts[expiry] = std::vector<double>(shiftData.shiftStrikes.size(), 0.0);
        }
        volData[key.name] = shiftData;
    }
    size_t expiryIndex = key.index / volData[key.name].shiftStrikes.size();
    size_t strikeIndex = key.index % volData[key.name].shiftStrikes.size();
    auto expiry = volData[key.name].shiftExpiries[expiryIndex];
    // auto strike = shiftData.shiftStrikes[strikeIndex];
    // std::cout << key << " expiryIndex " << expiryIndex << " " << expiry << " termindex " << strikeIndex << " " <<
    // strike
    //           << std::endl;
    volData[key.name].shifts[expiry][strikeIndex] = shift;
}

StressTestScenarioData::SpotShiftData
spotShiftData(const RiskFactorKey& key, double shift) {
    StressTestScenarioData::SpotShiftData shiftData;
    shiftData.shiftType = ShiftType::Absolute;
    shiftData.shiftSize = shift;
    return shiftData;
}

XvaExplainResults::XvaExplainResults(const QuantLib::ext::shared_ptr<InMemoryReport>& xvaReport) {
    size_t tradeIdColumn = xvaReport->columnPosition("TradeId");
    size_t nettingSetIdColumn = xvaReport->columnPosition("TradeId");
    size_t scenarioIdColumn = xvaReport->columnPosition("Scenario");
    size_t cvaColumn = xvaReport->columnPosition("CVA");
    for (size_t i = 0; i < xvaReport->rows(); ++i) {

        const std::string& scenario = boost::get<std::string>(xvaReport->data(scenarioIdColumn)[i]);

        const std::string& tradeId = boost::get<std::string>(xvaReport->data(tradeIdColumn)[i]);

        const std::string& nettingset = boost::get<std::string>(xvaReport->data(nettingSetIdColumn)[i]);

        const double cva = boost::get<double>(xvaReport->data(cvaColumn)[i]);

        const auto key = XvaReportKey{tradeId, nettingset};
        if (scenario != "BASE" && scenario != "t1") {
            try {
                auto rfKey = parseRiskFactorKey(scenario);
                fullRevalScenarioCva_[rfKey][key] = cva;
            } catch (const std::exception& e) {
                StructuredAnalyticsErrorMessage("XvaExplain", "Unexpected RiskFactor",
                                                scenario + "is not a valid riskfactor, skip it in xva explain report");
            }
        } else if (scenario == "BASE") {
            baseCvaData_[key] = cva;
        } else {
            fullRevalCva_[key] = cva;
        }
    }
}

bool operator<(const XvaExplainResults::XvaReportKey& a, const XvaExplainResults::XvaReportKey& b) {
    return std::tie(a.tradeId, a.nettingSet) < std::tie(b.tradeId, b.nettingSet);
}

XvaExplainAnalyticImpl::XvaExplainAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

void XvaExplainAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->xvaExplainSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->xvaExplainSensitivityScenarioData();
}

void XvaExplainAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running XVA Explain analytic.");
    QL_REQUIRE(inputs_->portfolio(), "XvaExplainAnalytic::run: No portfolio loaded.");

    Settings::instance().evaluationDate() = inputs_->asof();
    std::string marketConfig = inputs_->marketConfig("pricing");

    CONSOLEW("XVA_EXPLAIN: Build T0 and Sim Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("XVA_EXPLAIN: Compute t0 par rates");
    ParScenarioAnalytic todayParAnalytic(inputs_);
    todayParAnalytic.configurations().asofDate = inputs_->asof();
    todayParAnalytic.configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    todayParAnalytic.configurations().simMarketParams = analytic()->configurations().simMarketParams;
    todayParAnalytic.configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;
    todayParAnalytic.runAnalytic(loader);
    auto todaysRates = dynamic_cast<ParScenarioAnalyticImpl*>(todayParAnalytic.impl().get())->parRates();
    CONSOLE("OK");

    CONSOLEW("XVA_EXPLAIN: Compute t1 par rates");
    ParScenarioAnalytic mporParAnalytic(inputs_);
    Settings::instance().evaluationDate() = inputs_->asof() + 1 * Days;
    mporParAnalytic.configurations().asofDate = inputs_->asof() + 1 * Days;
    mporParAnalytic.configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    mporParAnalytic.configurations().simMarketParams = analytic()->configurations().simMarketParams;
    mporParAnalytic.configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;
    mporParAnalytic.runAnalytic(loader);
    CONSOLE("OK");
    CONSOLEW("XVA_EXPLAIN: Generate Stresstests");
    auto mporRates = dynamic_cast<ParScenarioAnalyticImpl*>(mporParAnalytic.impl().get())->parRates();
    Settings::instance().evaluationDate() = inputs_->asof();
    const auto& simParameters = analytic()->configurations().simMarketParams;
    const auto& sensitivityData = analytic()->configurations().sensiScenarioData;
    // Build Stresstest Data
    auto scenarioData = QuantLib::ext::make_shared<StressTestScenarioData>();
    scenarioData->useSpreadedTermStructures() = true;
    StressTestScenarioData::StressTestData fullRevalScenario;
    fullRevalScenario.label = "t1";
    fullRevalScenario.irCurveParShifts = true;
    fullRevalScenario.irCapFloorParShifts = true;
    fullRevalScenario.creditCurveParShifts = true;
    for (const auto& [key, mporValue] : mporRates) {
        auto t0Value = todaysRates.find(key);
        QL_REQUIRE(t0Value != todaysRates.end(), "XVAExplain: Mismatch between t0 and mpor riskfactors, can not find "
                                                     << key << " in todays riskfactors");
        double shift = mporValue - t0Value->second;
        
        if (std::abs(shift) > 1e-4) {
            std::cout << key << " " << t0Value->second << " " << mporValue << " " << shift << std::endl;
            StressTestScenarioData::StressTestData scenario;
            scenario.label = to_string(key);
            scenario.irCurveParShifts = true;
            scenario.irCapFloorParShifts = true;
            scenario.creditCurveParShifts = true;
            bool inScope = false;
            switch (key.keytype) {
            case RiskFactorKey::KeyType::DiscountCurve: {
                curveShiftData(scenario.discountCurveShifts, key, shift,
                                            sensitivityData->discountCurveShiftData()[key.name]->shiftTenors);
                curveShiftData(fullRevalScenario.discountCurveShifts, key, shift,
                               sensitivityData->discountCurveShiftData()[key.name]->shiftTenors);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::YieldCurve: {
                curveShiftData(fullRevalScenario.yieldCurveShifts, key, shift,
                               sensitivityData->yieldCurveShiftData()[key.name]->shiftTenors);
                curveShiftData(scenario.yieldCurveShifts, key, shift,
                               sensitivityData->yieldCurveShiftData()[key.name]->shiftTenors);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::IndexCurve: {
                curveShiftData(fullRevalScenario.indexCurveShifts, key, shift,
                               sensitivityData->indexCurveShiftData()[key.name]->shiftTenors);
                curveShiftData(scenario.indexCurveShifts, key, shift,
                               sensitivityData->indexCurveShiftData()[key.name]->shiftTenors);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::SurvivalProbability: {
                curveShiftData(fullRevalScenario.survivalProbabilityShifts, key, shift,
                               sensitivityData->creditCurveShiftData()[key.name]->shiftTenors);
                curveShiftData(scenario.survivalProbabilityShifts, key, shift,
                               sensitivityData->creditCurveShiftData()[key.name]->shiftTenors);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::RecoveryRate: {
                fullRevalScenario.recoveryRateShifts[key.name] = spotShiftData(key, shift);
                scenario.recoveryRateShifts[key.name] = spotShiftData(key, shift);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::EquitySpot: {
                fullRevalScenario.equityShifts[key.name] = spotShiftData(key, shift);
                scenario.equityShifts[key.name] = spotShiftData(key, shift);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::FXSpot: {
                fullRevalScenario.fxShifts[key.name] = spotShiftData(key, shift);
                scenario.fxShifts[key.name] = spotShiftData(key, shift);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::EquityVolatility: {                
                volShiftData(fullRevalScenario.equityVolShifts,key, shift, simParameters->equityVolExpiries(key.name));
                volShiftData(scenario.equityVolShifts, key, shift, simParameters->equityVolExpiries(key.name));
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::FXVolatility: {
                volShiftData(fullRevalScenario.fxVolShifts, key, shift, simParameters->fxVolExpiries(key.name));
                volShiftData(scenario.fxVolShifts, key, shift, simParameters->fxVolExpiries(key.name));
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::SwaptionVolatility: {
                swaptionVolShiftData(fullRevalScenario.swaptionVolShifts, key, shift, simParameters);
                swaptionVolShiftData(scenario.swaptionVolShifts, key, shift, simParameters);
                inScope = true;
                break;
            }
            case RiskFactorKey::KeyType::OptionletVolatility: {
                capFloorVolShiftData(fullRevalScenario.capVolShifts, key, shift, simParameters);
                capFloorVolShiftData(scenario.capVolShifts, key, shift, simParameters);
                inScope = true;
                break;
            }
            default:
                inScope = false;
                break;
            }
            if (inScope) {
                scenarioData->data().push_back(scenario);
            }
        }
    }
    scenarioData->data().push_back(fullRevalScenario);
    CONSOLE("OK");
    analytic()->stressTests()[label()]["xvaExplain_parStressTest"] = scenarioData;
    CONSOLEW("XVA_EXPLAIN: Convert Stresstest to zero domain");
    ParStressTestConverter converter(
        analytic()->configurations().asofDate, analytic()->configurations().todaysMarketParams,
        analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
        analytic()->configurations().curveConfig, analytic()->market(), inputs_->iborFallbackConfig());
    auto zeroScenarioData = converter.convertStressScenarioData(scenarioData);

    analytic()->stressTests()[label()]["xvaExplain_zeroStressTest"] = zeroScenarioData;

    auto stressAnalytic = QuantLib::ext::make_shared<XvaStressAnalytic>(inputs_, zeroScenarioData);

    stressAnalytic->configurations().asofDate = inputs_->asof();
    stressAnalytic->configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    stressAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;
    stressAnalytic->runAnalytic(loader);

    auto xvaReport = stressAnalytic->reports()["XVA_STRESS"]["xva"];

    analytic()->reports()[label()]["xvaExplain_details"] = xvaReport;

    XvaExplainResults xvaData(xvaReport);

    // Report Write
    auto xvaExplainReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString()).writeXvaExplainReport(*xvaExplainReport, xvaData);
    analytic()->reports()[label()]["xvaExplain"] = xvaExplainReport;

    

    analytic()->reports()[label()]["xvaExplain_summary"] = reportAgg;
}

XvaExplainAnalytic::XvaExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaExplainAnalyticImpl>(inputs), {"XVA_EXPLAIN"}, inputs, true, false, false, false) {
}

std::vector<QuantLib::Date> XvaExplainAnalyticImpl::additionalMarketDates() const {
    return {inputs_->asof() + 1 * Days};
}

} // namespace analytics
} // namespace ore
