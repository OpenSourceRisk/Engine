/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/varanalytic.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parametricvar.hpp>
#include <ored/portfolio/trade.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/***********************************************************************************
 * VAR Analytic: DELTA-VAR, DELTA-GAMMA-NORMAL-VAR, MONTE-CARLO-VAR
 ***********************************************************************************/

void VarAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void VarAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                              const std::set<std::string>& runTypes) {
    MEM_LOG;
    LOG("Running parametric VaR");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    LOG("VAR: Build Market");
    CONSOLEW("Risk: Build Market for VaR");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Risk: Build Portfolio for VaR");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    LOG("Build trade to portfolio id mapping");
    map<string, set<pair<string, Size>>> tradePortfolio;
    for (auto const& [tradeId, trade] : analytic()->portfolio()->trades()) {
        for (auto const& pId : trade->portfolioIds())
            tradePortfolio[pId].insert(make_pair(tradeId, Null<Size>()));
    }

    ParametricVarCalculator::ParametricVarParams varParams(inputs_->varMethod(), inputs_->mcVarSamples(),
                                                           inputs_->mcVarSeed());

    LOG("Build VaR calculator");
    auto calc = boost::make_shared<ParametricVarReport>(tradePortfolio, inputs_->portfolioFilter(), 
        inputs_->sensitivityStream(), inputs_->covarianceData(), inputs_->varQuantiles(), 
        varParams, inputs_->varBreakDown(), inputs_->salvageCovariance());

    boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
    analytic()->reports()["VAR"]["var"] = report;

    LOG("Call VaR calculation");
    CONSOLEW("Risk: VaR Calculation");
    calc->calculate(*report);
    CONSOLE("OK");

    LOG("Parametric VaR completed");
    MEM_LOG;
}

} // namespace analytics
} // namespace ore
