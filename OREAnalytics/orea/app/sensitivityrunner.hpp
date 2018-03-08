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
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/portfolio.hpp>

namespace ore {
namespace analytics {

class SensitivityRunner {
public:
    virtual ~SensitivityRunner() {};

    virtual void runSensitivityAnalysis(boost::shared_ptr<ore::data::Market> market, Conventions& conventions, 
                                        boost::shared_ptr<Parameters> params, std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraBuilders = {});


    //! Initialize input parameters to the sensitivities analysis
    virtual void sensiInputInitialize(boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
        boost::shared_ptr<SensitivityScenarioData>& sensiData,
        boost::shared_ptr<EngineData>& engineData, boost::shared_ptr<Portfolio>& sensiPortfolio,
        string& marketConfiguration, boost::shared_ptr<Parameters> params);

    //! Write out some standard sensitivities reports
    virtual void sensiOutputReports(const boost::shared_ptr<SensitivityAnalysis>& sensiAnalysis, boost::shared_ptr<Parameters> params);
};

} // namespace analytics
} // namespace ore

