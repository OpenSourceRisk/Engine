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

#include <orea/app/analytics/xvastressanalytic.hpp>

#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>

namespace ore {
namespace analytics {

XvaStressAnalyticImpl::XvaStressAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

void XvaStressAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                        const std::set<std::string>& runTypes) {

    // basic setup

    LOG("Running XVA Stress analytic.");

    Settings::instance().evaluationDate() = inputs_->asof();

    QL_REQUIRE(inputs_->portfolio(), "XvaStressAnalytic::run: No portfolio loaded.");

    Settings::instance().evaluationDate() = inputs_->asof();
    std::string marketConfig = inputs_->marketConfig("pricing"); // FIXME

    auto xvaAnalytic = dependentAnalytic<XvaAnalytic>("XVA");

    // build t0, sim market, stress scenario generator

    CONSOLEW("XVA_STRESS: Build T0 and Sim Markets and Stress Scenario Generator");

    analytic()->buildMarket(loader);

    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), inputs_->xvaStressSimMarketParams(), marketConfig,
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), inputs_->xvaStressScenarioData()->useSpreadedTermStructures(), false, false,
        *inputs_->iborFallbackConfig(), true);

    auto baseScenario = simMarket->baseScenario();
    auto scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    auto scenarioGenerator = QuantLib::ext::make_shared<StressScenarioGenerator>(
        inputs_->xvaStressScenarioData(), baseScenario, inputs_->xvaStressSimMarketParams(), simMarket, scenarioFactory,
        simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;

    CONSOLE("OK");

    // generate the stress scenarios and run dependent xva analytic under each of them

    CONSOLEW("XVA_STRESS: Running stress scenarios");

    // base scenario run
    
    xvaAnalytic->runAnalytic(loader, {"EXPOSURE", "XVA"});

    // debug
    xvaAnalytic->reports()["XVA"]["xva"]->toFile("xva_org.csv");

    // stress scenario runs
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        
        
        auto scenario = scenarioGenerator->next(simMarket->asofDate());
        XvaAnalytic newAnalytic(inputs_, scenario);
        CONSOLEW("XVA_STRESS: Compute scenario " << scenario->label());
        newAnalytic.runAnalytic(loader, {"EXPOSURE", "XVA"});
        newAnalytic.reports()["XVA"]["xva"]->toFile("xva_" + scenario->label() + ".csv");
    }



    // set the result report

    // analytic()->reports()["XVA_STRESS"]["xva_stress"] = report;

    LOG("Running XVA Stress analytic finished.");
}

void XvaStressAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->xvaStressSimMarketParams();
}

XvaStressAnalytic::XvaStressAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaStressAnalyticImpl>(inputs), {"XVA_STRESS"}, inputs, true, false, false, false) {
    impl()->addDependentAnalytic("XVA", QuantLib::ext::make_shared<XvaAnalytic>(inputs));
}

} // namespace analytics
} // namespace ore
