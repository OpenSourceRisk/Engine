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
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/cube/cube_io.hpp>

#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/utilities.hpp>
namespace ore {
namespace analytics {

StressTestScenarioData::CurveShiftData
curveShiftData(const RiskFactorKey& key, double mporValue, const std::map<RiskFactorKey, double>& t0Values,
               const boost::shared_ptr<ScenarioSimMarketParameters>& simParameters) {
    auto t0Value = t0Values.find(key);
    QL_REQUIRE(t0Value != t0Values.end(), "XVAExplain: Mismatch between t0 and mpor riskfactors, can not find "
                                              << key << " in todays riskfactors");
    StressTestScenarioData::CurveShiftData shiftData;
    shiftData.shiftType = ShiftType::Absolute;
    shiftData.shiftTenors = simParameters->yieldCurveTenors(key.name);
    shiftData.shifts = std::vector<double>(shiftData.shiftTenors.size(), 0.0);
    shiftData.shifts[key.index] = mporValue - t0Value->second;
    return shiftData;
}

StressTestScenarioData::VolShiftData volShiftData(const RiskFactorKey& key, double mporValue,
                                                  const std::map<RiskFactorKey, double>& t0Values,
                                                  const boost::shared_ptr<ScenarioSimMarketParameters>& simParameters) {
    auto t0Value = t0Values.find(key);
    QL_REQUIRE(t0Value != t0Values.end(), "XVAExplain: Mismatch between t0 and mpor riskfactors, can not find "
                                              << key << " in todays riskfactors");
    StressTestScenarioData::VolShiftData shiftData;
    shiftData.shiftType = ShiftType::Absolute;
    shiftData.shiftExpiries = simParameters->yieldCurveTenors(key.name);
    shiftData.shifts = std::vector<double>(shiftData.shiftExpiries.size(), 0.0);
    shiftData.shifts[key.index] = mporValue - t0Value->second;
    return shiftData;
}

StressTestScenarioData::SpotShiftData
spotShiftData(const RiskFactorKey& key, double mporValue, const std::map<RiskFactorKey, double>& t0Values,
              const boost::shared_ptr<ScenarioSimMarketParameters>& simParameters) {
    auto t0Value = t0Values.find(key);
    QL_REQUIRE(t0Value != t0Values.end(), "XVAExplain: Mismatch between t0 and mpor riskfactors, can not find "
                                              << key << " in todays riskfactors");
    StressTestScenarioData::SpotShiftData shiftData;
    shiftData.shiftType = ShiftType::Absolute;
    shiftData.shiftSize = mporValue - t0Value->second;
    return shiftData;
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

    auto xvaAnalytic = dependentAnalytic<XvaAnalytic>("XVA");

    CONSOLEW("XVA_EXPLAIN: Build T0 and Sim Market");

    analytic()->buildMarket(loader);

    ParScenarioAnalytic todayParAnalytic(inputs_);
    todayParAnalytic.configurations().asofDate = inputs_->asof();
    todayParAnalytic.configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    todayParAnalytic.configurations().simMarketParams = analytic()->configurations().simMarketParams;
    todayParAnalytic.configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;
    todayParAnalytic.runAnalytic(loader);
    auto todaysRates = dynamic_cast<ParScenarioAnalyticImpl*>(todayParAnalytic.impl().get())->parRates();

    CONSOLEW("XVA_EXPLAIN: Build MPOR Market and get par rates");
    ParScenarioAnalytic mporParAnalytic(inputs_);
    Settings::instance().evaluationDate() = inputs_->asof() + 1 * Days;
    mporParAnalytic.configurations().asofDate = inputs_->asof() + 1 * Days;
    mporParAnalytic.configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    mporParAnalytic.configurations().simMarketParams = analytic()->configurations().simMarketParams;
    mporParAnalytic.configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;
    mporParAnalytic.runAnalytic(loader);
    auto mporRates = dynamic_cast<ParScenarioAnalyticImpl*>(mporParAnalytic.impl().get())->parRates();
    std::cout << "Key,Today,Mpor,Shift" << std::endl;
    for (const auto& [key, value] : mporRates) {
        std::cout << key << "," << todaysRates[key] << "," << value << "," << value - todaysRates[key] << std::endl;
    }
    Settings::instance().evaluationDate() = inputs_->asof();
    const auto& simParameters = analytic()->configurations().simMarketParams;
    // Build Stresstest Data
    StressTestScenarioData scenarioData;
    scenarioData.useSpreadedTermStructures() = true;
    for (const auto& [key, value] : mporRates) {
        StressTestScenarioData::StressTestData scenario;
        scenario.label = to_string(key);
        scenario.irCurveParShifts = true;
        scenario.irCapFloorParShifts = true;
        scenario.creditCurveParShifts = true;
        bool inScope = false;
        switch (key.keytype) {
        case RiskFactorKey::KeyType::DiscountCurve: {
            scenario.discountCurveShifts[key.name] = curveShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::YieldCurve: {
            scenario.yieldCurveShifts[key.name] = curveShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::IndexCurve: {
            scenario.indexCurveShifts[key.name] = curveShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::SurvivalProbability: {
            scenario.survivalProbabilityShifts[key.name] = curveShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::RecoveryRate: {
            scenario.recoveryRateShifts[key.name] = spotShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::EquitySpot: {
            scenario.equityShifts[key.name] = spotShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::FXSpot: {
            scenario.fxShifts[key.name] = spotShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::EquityVolatility: {
            scenario.equityVolShifts[key.name] = volShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::FXVolatility: {
            scenario.equityVolShifts[key.name] = volShiftData(key, value, todaysRates, simParameters);
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::SwaptionVolatility: {
            QL_REQUIRE(todaysRates.count(key) == 1, "XVA Explain, can not find " << key << "in todaysMarket");
            StressTestScenarioData::SwaptionVolShiftData shiftData;
            shiftData.shiftType = ShiftType::Absolute;
            shiftData.shiftExpiries = simParameters->swapVolExpiries(key.name);
            shiftData.shiftTerms = simParameters->swapVolTerms(key.name);
            for (const auto& expiry : shiftData.shiftExpiries) {
                for (const auto& term : shiftData.shiftTerms) {
                    shiftData.shifts[std::make_pair(expiry, term)] = 0;
                }
            }
            size_t expiryIndex = key.index / shiftData.shiftTerms.size();
            size_t termIndex = key.index % shiftData.shiftTerms.size();
            auto expiry = shiftData.shiftExpiries[expiryIndex];
            auto term = shiftData.shiftTerms[termIndex];
            std::cout << key << " expiryIndex " << expiryIndex << " " << expiry << " termindex " << termIndex << " "
                      << term << std::endl;
            shiftData.shifts[std::make_pair(expiry, term)] = value - todaysRates[key];
            scenario.swaptionVolShifts[key.name] = shiftData;
            inScope = true;
            break;
        }
        case RiskFactorKey::KeyType::OptionletVolatility: {
            std::cout << "Optionlet" << std::endl;
            StressTestScenarioData::CapFloorVolShiftData shiftData;
            shiftData.shiftType = ShiftType::Absolute;
            shiftData.shiftExpiries = simParameters->capFloorVolExpiries(key.name);
            shiftData.shiftStrikes = simParameters->capFloorVolStrikes(key.name);
            for (const auto& expiry : shiftData.shiftExpiries) {
                shiftData.shifts[expiry] = std::vector<double>(shiftData.shiftStrikes.size(), 0.0);
            }

            size_t expiryIndex = key.index / shiftData.shiftStrikes.size();
            size_t strikeIndex = key.index % shiftData.shiftStrikes.size();
            auto expiry = shiftData.shiftExpiries[expiryIndex];
            auto strike = shiftData.shiftStrikes[strikeIndex];
            std::cout << key << " expiryIndex " << expiryIndex << " " << expiry << " termindex " << strikeIndex << " "
                      << strike << std::endl;

            shiftData.shifts[expiry][strikeIndex] = value - todaysRates[key];
            scenario.capVolShifts[key.name] = shiftData;
            inScope = true;
            break;
        }
        default:
            inScope = false;
            break;
        }
        if (inScope) {
            scenarioData.data().push_back(scenario);
        }
    }
    CONSOLEW("XVA_EXPLAIN: Export generated Stresstestfile");
    scenarioData.toFile("xvaExplainStress.xml");
}

XvaExplainAnalytic::XvaExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaExplainAnalyticImpl>(inputs), {"XVA_STRESS"}, inputs, true, false, false, false) {
    impl()->addDependentAnalytic("XVA", QuantLib::ext::make_shared<XvaAnalytic>(inputs));
}

std::vector<QuantLib::Date> XvaExplainAnalyticImpl::additionalMarketDates() const {
    return {inputs_->asof() + 1 * Days};
}

} // namespace analytics
} // namespace ore
