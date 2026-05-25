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

/*! \file engine/multithreadedstresstest.hpp
    \brief multi-threaded stress test engine
    \ingroup engine
*/

#pragma once

#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/report/report.hpp>

namespace ore {
namespace analytics {

class MultiThreadedStressTest : public ore::data::ProgressReporter {
public:
    MultiThreadedStressTest(const QuantLib::Size nThreads,
                            const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                            const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                            const std::string& marketConfiguration,
                            const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                            const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                            const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                            const ore::data::CurveConfigurations& curveConfigs,
                            const ore::data::TodaysMarketParameters& todaysMarketParams,
                            const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory = nullptr,
                            const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                            const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig = nullptr,
                            const QuantLib::ext::shared_ptr<ore::data::Loader>& loader = nullptr,
                            bool continueOnError = false, const bool useAtParCouponsTrades = false);

    void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                       const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport = nullptr,
                       const double threshold = 0.0, const QuantLib::Size precision = 2,
                       const bool includePastCashflows = false,
                       const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr);

private:
    QuantLib::Size nThreads_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    std::string marketConfiguration_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> engineData_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    QuantLib::ext::shared_ptr<StressTestScenarioData> stressData_;
    ore::data::CurveConfigurations curveConfigs_;
    ore::data::TodaysMarketParameters todaysMarketParams_;
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory_;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    QuantLib::ext::shared_ptr<IborFallbackConfig> iborFallbackConfig_;
    QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
    bool continueOnError_;
    bool useAtParCouponsTrades_;
};

} // namespace analytics
} // namespace ore
