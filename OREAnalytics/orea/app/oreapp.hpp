/*
   Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file App/ore.hpp
  \brief Open Risk Engine setup and analytics choice
  \ingroup
 */

#pragma once

#include <map>
#include <vector>

#include <ored/report/csvreport.hpp>
#include <ored/utilities/xmlutils.hpp>

using namespace std;
using namespace ore::data;
namespace ore {
namespace analytics {

class OreApp {
public:
    OreApp(boost::shared_ptr<Parameters> params) : params_(params) {
        tab = 40;
        string asofString = params_->get("setup", "asofDate");
        asof_ = parseDate(asofString);
        Settings::instance().evaluationDate() = asof_;
    }
    void run();
    void setupLogFile();
    void getConventions();
    TodaysMarketParameters getMarketParameters();
    boost::shared_ptr<Market> buildMarket(TodaysMarketParameters& marketParams);
    boost::shared_ptr<EngineFactory> buildFactory(boost::shared_ptr<Market> market);
    boost::shared_ptr<Portfolio> buildPortfolio(boost::shared_ptr<EngineFactory> factory);
    boost::shared_ptr<ScenarioGenerator>
    buildScenarioGenerator(boost::shared_ptr<Market> market,
                           boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
                           boost::shared_ptr<ScenarioGeneratorData> sgd);
    boost::shared_ptr<ScenarioSimMarketParameters> getSimMarketData();
    boost::shared_ptr<ScenarioGeneratorData> getScenarioGeneratorData();
    boost::shared_ptr<EngineFactory> buildSimFactory(boost::shared_ptr<ScenarioSimMarket> simMarket);

    boost::shared_ptr<Conventions> conventions_;

private:
    boost::shared_ptr<Parameters> params_;
    Size tab;
    Date asof_;
};
}
}
