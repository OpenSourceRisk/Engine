/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/app/initbuilders.hpp>

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/imscheduleanalytic.hpp>
#include <orea/app/analytics/parconversionanalytic.hpp>
#include <orea/app/analytics/parstressconversionanalytic.hpp>
#include <orea/app/analytics/pnlanalytic.hpp>
#include <orea/app/analytics/pnlexplainanalytic.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/analytics/scenarioanalytic.hpp>
#include <orea/app/analytics/scenariostatisticsanalytic.hpp>
#include <orea/app/analytics/simmanalytic.hpp>
#include <orea/app/analytics/stresstestanalytic.hpp>
#include <orea/app/analytics/varanalytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/analytics/xvasensitivityanalytic.hpp>
#include <orea/app/analytics/xvastressanalytic.hpp>
#include <orea/app/analytics/zerotoparshiftanalytic.hpp>

#include <ored/utilities/databuilders.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ore::analytics {

using namespace ore::data;

void initBuilders(const bool registerOREAnalytics) {

    static boost::shared_mutex mutex;
    boost::unique_lock<boost::shared_mutex> lock(mutex);

    dataBuilders();

    if (registerOREAnalytics) {
        ORE_REGISTER_ANALYTIC_BUILDER("MARKETDATA", {}, MarketDataAnalytic, false)
        ORE_REGISTER_ANALYTIC_BUILDER("HISTSIM_VAR", {}, HistoricalSimulationVarAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("IM_SCHEDULE", {}, IMScheduleAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("PARAMETRIC_VAR", {}, ParametricVarAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("PARCONVERSION", {}, ParConversionAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("PNL", {}, PnlAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("PNL_EXPLAIN", {}, PnlExplainAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("PRICING", pricingAnalyticSubAnalytics, PricingAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("SCENARIO", {}, ScenarioAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("SCENARIO_STATISTICS", {}, ScenarioStatisticsAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("SIMM", {}, SimmAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("XVA", xvaAnalyticSubAnalytics, XvaAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("STRESS", {}, StressTestAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("PARSTRESSCONVERSION", {}, ParStressConversionAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("ZEROTOPARSHIFT", {}, ZeroToParShiftAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("XVA_STRESS", {}, XvaStressAnalytic, false);
        ORE_REGISTER_ANALYTIC_BUILDER("XVA_SENSITIVITY", {}, XvaSensitivityAnalytic, false);
    }
}

} // namespace ore::analytics
