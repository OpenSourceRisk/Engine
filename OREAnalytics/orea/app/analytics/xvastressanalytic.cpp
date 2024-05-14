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

auto buildNettingSetReport(){
    auto report = QuantLib::ext::make_shared<InMemoryReport>();
    report->addColumn("Scenario", string());
    report->addColumn("NettingSet", string());

    report->addColumn("BaseCVA", double(), 6);
    report->addColumn("CVA", double(), 6);
    report->addColumn("StressCVA", double(), 6);

    report->addColumn("BaseDVA", double(), 6);
    report->addColumn("DVA", double(), 6);
    report->addColumn("StressDVA", double(), 6);
    return report;
}

void addResultsToNettingSetReport(const std::string& label, QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report,
                                  const std::map<std::string, XvaResult>& stressResults,
                                  const std::map<std::string, XvaResult>& baseResults) {
                                        
    for (const auto& [nid, r] : stressResults) {
        
        if (auto baseIt = baseResults.find(nid); baseIt != baseResults.end()) {
            report->next();
            report->add(label);
            report->add(nid);
            report->add(r.cva);
            report->add(baseIt->second.cva);
            report->add(r.cva - baseIt->second.cva);
            report->add(r.dva);
            report->add(baseIt->second.dva);
            report->add(r.dva - baseIt->second.dva);
        }
    }
}

void XvaStressAnalyticImpl::addReports(const std::string& label,
                                       const QuantLib::ext::shared_ptr<XvaAnalytic>& xvaAnalytic) {
    analytic()->reports()["XVA_STRESS"]["xva_" + label] = xvaAnalytic->reports()["XVA"]["xva"];

    if (inputs_->rawCubeOutput()) {
        DLOG("Write raw cube under scenario " << label);
        analytic()->reports()["XVA_STRESS"]["rawcube_" + label] = xvaAnalytic->reports()["XVA"]["rawcube"];
    }

    if (inputs_->writeScenarios()) {
        DLOG("Write scenario report under scenario " << label);
        analytic()->reports()["XVA_STRESS"]["scenario" + label] = xvaAnalytic->reports()["XVA"]["scenario"];
    }
}

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
    addReports("org", xvaAnalytic);
    
    auto baseNettingSetResults = xvaAnalytic->nettingSetResults();

    auto nettingSetReport = buildNettingSetReport();
    for (size_t i = 0; i < scenarioGenerator->samples(); ++i) {
        auto scenario = scenarioGenerator->next(simMarket->asofDate());
        const std::string& label = scenario->label();
       
        auto newAnalytic = ext::make_shared<XvaAnalytic>(inputs_, scenario);
        CONSOLEW("XVA_STRESS: Compute scenario " << label);
        newAnalytic->runAnalytic(loader, {"EXPOSURE", "XVA"});
        addReports(label, newAnalytic);
        addResultsToNettingSetReport(label, nettingSetReport, newAnalytic->nettingSetResults(), baseNettingSetResults);
    }
    nettingSetReport->end();
    analytic()->reports()["XVA_STRESS"]["xva_stress"] = nettingSetReport;
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
