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

/*! \file orea/engine/xvaenginecg.hpp
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

#include <qle/ad/external_randomvariable_ops.hpp>

#include <ql/types.hpp>

namespace ore {
namespace analytics {

using namespace QuantLib;
using namespace ore::data;

class XvaEngineCG : public ore::data::ProgressReporter {
public:
    XvaEngineCG(const Size nThreads, const Date& asof, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
                const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                const QuantLib::ext::shared_ptr<ore::analytics::CrossAssetModelData>& crossAssetModelData,
                const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGeneratorData>& scenarioGeneratorData,
                const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                const string& marketConfiguration = Market::defaultConfiguration,
                const string& marketConfigurationInCcy = Market::inCcyConfiguration,
                const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData = nullptr,
                const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                const bool bumpCvaSensis = false, const bool useExternalComputeDevice = false,
                const bool externalDeviceCompatibilityMode = false,
                const bool useDoublePrecisionForExternalCalculation = false,
                const std::string& externalComputeDevice = std::string(), const bool continueOnCalibrationError = true,
                const bool continueOnError = true, const std::string& context = "xva engine cg");

    QuantLib::ext::shared_ptr<InMemoryReport> exposureReport() { return epeReport_; }
    QuantLib::ext::shared_ptr<InMemoryReport> sensiReport() { return sensiReport_; }

private:
    void populateRandomVariates(std::vector<RandomVariable>& values,
                                std::vector<ExternalRandomVariable>& valuesExternal) const;
    void populateConstants(std::vector<RandomVariable>& values,
                           std::vector<ExternalRandomVariable>& valuesExternal) const;
    void populateModelParameters(const std::vector<std::pair<std::size_t, double>>& modelParameters,
                                 std::vector<RandomVariable>& values,
                                 std::vector<ExternalRandomVariable>& valuesExternal) const;

    // input parameters
    Date asof_;
    QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> engineData_;
    QuantLib::ext::shared_ptr<ore::analytics::CrossAssetModelData> crossAssetModelData_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioGeneratorData> scenarioGeneratorData_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    std::string marketConfiguration_;
    std::string marketConfigurationInCcy_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensitivityData_;
    QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
    bool bumpCvaSensis_;
    bool useExternalComputeDevice_;
    bool externalDeviceCompatibilityMode_;
    bool useDoublePrecisionForExternalCalculation_;
    std::string externalComputeDevice_;
    bool continueOnCalibrationError_;
    bool continueOnError_;
    std::string context_;

    // artefacts produced during run
    QuantLib::ext::shared_ptr<ore::data::Market> initMarket_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> sensiScenarioGenerator_;
    QuantLib::ext::shared_ptr<CrossAssetModelBuilder> camBuilder_;
    QuantLib::ext::shared_ptr<GaussianCamCG> model_;
    std::vector<std::pair<std::size_t, double>> baseModelParams_;
    std::vector<RandomVariableOpNodeRequirements> opNodeRequirements_;
    std::vector<RandomVariableOp> ops_;
    std::vector<RandomVariableGrad> grads_;
    std::vector<ExternalRandomVariableOp> opsExternal_;
    std::vector<ExternalRandomVariableGrad> gradsExternal_;
    std::size_t externalCalculationId_;

    // output reports
    QuantLib::ext::shared_ptr<InMemoryReport> epeReport_, sensiReport_;
};

} // namespace analytics
} // namespace ore
