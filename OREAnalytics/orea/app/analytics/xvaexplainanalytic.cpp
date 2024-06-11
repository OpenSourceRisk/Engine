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
}

XvaExplainAnalytic::XvaExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaExplainAnalyticImpl>(inputs), {"XVA_STRESS"}, inputs, true, false, false, false) {
    impl()->addDependentAnalytic("XVA", QuantLib::ext::make_shared<XvaAnalytic>(inputs));
}

std::vector<QuantLib::Date> XvaExplainAnalyticImpl::additionalMarketDates() const { return {inputs_->asof() + 1 * Days}; }

} // namespace analytics
} // namespace ore
