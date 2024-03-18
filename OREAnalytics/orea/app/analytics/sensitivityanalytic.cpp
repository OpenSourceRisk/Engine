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

#include <orea/app/analytics/sensitivityanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/to_string.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void SensitivityAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                          const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("SensitivityAnalytic::runAnalytic called");

        Settings::instance().evaluationDate() = inputs_->asof();
        ObservationMode::instance().setMode(inputs_->observationModel());

        QL_REQUIRE(inputs_->portfolio(), "SensitivityAnalytic::run: No portfolio loaded.");

        CONSOLEW("SensitivityAnalytic: Build Market");
        analytic()->buildMarket(loader);
        CONSOLE("OK");

        CONSOLEW("SensitivityAnalytic: Build Portfolio");
        analytic()->buildPortfolio();
        CONSOLE("OK");

        // Check coverage
        for (const auto& rt : runTypes) {
            if (std::find(analytic()->analyticTypes().begin(), analytic()->analyticTypes().end(), rt) ==
                analytic()->analyticTypes().end()) {
                DLOG("requested analytic " << rt << " not covered by the PricingAnalytic");
            }
        }

        // This hook allows modifying the portfolio in derived classes before running the analytics below,
        // e.g. to apply SIMM exemptions.
        analytic()->modifyPortfolio();

        LOG("Sensi Analysis - Completed");
        CONSOLE("OK");
}

} // namespace analytics
} // namespace ore
