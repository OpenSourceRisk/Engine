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

/*! \file engine/xvacgengine.hpp
    \brief xva engine using cg infrastructure
    \ingroup engine
*/

#pragma once

#include <orea/scenario/scenariogeneratordata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/scripting/models/gaussiancamcg.hpp>
#include <ored/utilities/progressbar.hpp>

#include <ored/marketdata/todaysmarket.hpp>

#include <ql/types.hpp>

namespace ore {
namespace analytics {

using namespace QuantLib;
using namespace ore::data;

class XvaEngineCG : public ore::data::ProgressReporter {
public:
    XvaEngineCG(const Size nThreads, const Date& asof, const boost::shared_ptr<ore::data::Loader>& loader,
                const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                const boost::shared_ptr<ore::data::EngineData>& engineData,
                const boost::shared_ptr<ore::analytics::CrossAssetModelData>& crossAssetModelData,
                const boost::shared_ptr<ore::analytics::ScenarioGeneratorData>& scenarioGeneratorData,
                const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                const string& marketConfiguration = Market::defaultConfiguration,
                const string& marketConfigurationInCcy = Market::inCcyConfiguration,
                const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData = nullptr,
                const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                const bool continueOnCalibrationError = true, const bool continueOnError = true,
                const std::string& context = "xva engine cg");

private:
    void populateRandomVariates(std::vector<RandomVariable>& values) const;
    void populateConstants(std::vector<RandomVariable>& values) const;
    void populateModelParameters(std::vector<RandomVariable>& values,
                                 const std::vector<std::pair<std::size_t, double>>& modelParameters) const;
    void runFullForwardEvaluation(std::vector<RandomVariable>& values, std::vector<bool>& keepNodes) const;

    // input parameters
    Size nThreads_;
    Date asof_;
    boost::shared_ptr<ore::data::Loader> loader_;
    boost::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData_;
    boost::shared_ptr<ore::data::EngineData> engineData_;
    boost::shared_ptr<ore::analytics::CrossAssetModelData> crossAssetModelData_;
    boost::shared_ptr<ore::analytics::ScenarioGeneratorData> scenarioGeneratorData_;
    boost::shared_ptr<ore::data::Portfolio> portfolio_;
    std::string marketConfiguration_;
    std::string marketConfigurationInCcy_;
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensitivityData_;
    boost::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
    bool continueOnCalibrationError_;
    bool continueOnError_;
    std::string context_;

    std::vector<bool> nodesA_;
    std::vector<bool> nodesB_;
    std::vector<bool> nodesC_;
    std::vector<bool> nodesD_;

    // artefacts produced during run
    boost::shared_ptr<ore::data::Market> initMarket_;
    boost::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket_;
    boost::shared_ptr<SensitivityScenarioGenerator> sensiScenarioGenerator_;
    boost::shared_ptr<CrossAssetModelBuilder> camBuilder_;
    boost::shared_ptr<GaussianCamCG> model_;
    std::vector<std::pair<std::size_t, double>> baseModelParams_;
    std::vector<RandomVariableOpNodeRequirements> opNodeRequirements_;
    std::vector<RandomVariableOp> ops_;
    std::vector<RandomVariableGrad> grads_;
};

} // namespace analytics
} // namespace ore
