/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/app/sensitivityrunner.hpp
    \brief A class to run the sensitivity analysis
    \ingroup app
*/
#pragma once

#include <boost/make_shared.hpp>
#include <orea/app/parameters.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/portfolio.hpp>

namespace ore {
namespace analytics {

class SensitivityRunner {
public:
    SensitivityRunner(boost::shared_ptr<Parameters> params,
        std::map<string, boost::shared_ptr<AbstractTradeBuilder>> extraTradeBuilders = {},
        std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders = {},
        std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders = {})
        : params_(params), extraTradeBuilders_(extraTradeBuilders), extraEngineBuilders_(extraEngineBuilders), 
        extraLegBuilders_(extraLegBuilders) {}

    virtual ~SensitivityRunner(){};

    virtual void runSensitivityAnalysis(boost::shared_ptr<ore::data::Market> market, Conventions& conventions);

    //! Initialize input parameters to the sensitivities analysis
    virtual void sensiInputInitialize(boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                      boost::shared_ptr<SensitivityScenarioData>& sensiData,
                                      boost::shared_ptr<EngineData>& engineData,
                                      boost::shared_ptr<Portfolio>& sensiPortfolio, string& marketConfiguration);

    //! Write out some standard sensitivities reports
    virtual void sensiOutputReports(const boost::shared_ptr<SensitivityAnalysis>& sensiAnalysis);

protected:
    boost::shared_ptr<Parameters> params_;
    std::map<string, boost::shared_ptr<AbstractTradeBuilder>> extraTradeBuilders_;
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders_;
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders_;
};

} // namespace analytics
} // namespace ore
