/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/app/xvarunner.hpp
    \brief A class to run the xva analysis
    \ingroup app
*/
#pragma once

#include <orea/app/inputparameters.hpp>
#include <orea/aggregation/postprocess.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/scenario/scenariogeneratordata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <orea/engine/valuationengine.hpp>

#include <boost/optional.hpp>

namespace ore {
namespace analytics {

class XvaRunner {
public:
    virtual ~XvaRunner() {}

    XvaRunner(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
              QuantLib::Date asof, const std::string& baseCurrency,
              const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
              const QuantLib::ext::shared_ptr<ore::data::NettingSetManager>& netting,
              const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances,
              const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
              const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
              const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
              const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
              const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
              const QuantLib::ext::shared_ptr<ore::data::CrossAssetModelData>& crossAssetModelData,
              const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
              const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
              QuantLib::Real dimQuantile = 0.99, QuantLib::Size dimHorizonCalendarDays = 14,
              map<string, bool> analytics = {}, string calculationType = "Symmetric", string dvaName = "",
              string fvaBorrowingCurve = "", string fvaLendingCurve = "", bool fullInitialCollateralisation = true,
              bool storeFlows = false);

    // run xva on full portfolio and build post processor
    void runXva(const QuantLib::ext::shared_ptr<ore::data::Market>& market, bool continueOnErr = true,
                const std::map<std::string, QuantLib::Real>& currentIM = std::map<std::string, QuantLib::Real>());

    // get post processor, this requires a previous runXva() or generatePostProcessor() call
    const QuantLib::ext::shared_ptr<PostProcess>& postProcess() { return postProcess_; }

    // step 1: build cam model
    void buildCamModel(const QuantLib::ext::shared_ptr<ore::data::Market>& market, bool continueOnErr = true);

    // optional step 2: buffer simulation paths, this is required, if a currency filter is used
    void bufferSimulationPaths();

    // build sim market, optionally on filtered currencies
    virtual void buildSimMarket(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                        const boost::optional<std::set<std::string>>& currencyFilter = boost::none,
                        const bool continueOnErr = true);

    // step 5: build npv, netting cube (optionally filtered on trades) and generate scenario data
    void buildCube(const boost::optional<std::set<std::string>>& tradeIds, bool continueOnErr = true);

    // get generated trade cube from step 5
    QuantLib::ext::shared_ptr<NPVCube> npvCube() const { return cube_; }

    // get generated netting set cube from step 5
    QuantLib::ext::shared_ptr<NPVCube> nettingCube() const { return nettingCube_; }

    // get aggregation scenario data from step 5
    QuantLib::Handle<AggregationScenarioData> aggregationScenarioData() { return scenarioData_; }

    // partial step 3: build post processor on given cubes / agg scen data (requires runXva() or buildCamModel() call)
    void generatePostProcessor(
        const QuantLib::ext::shared_ptr<Market>& market, const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
        const QuantLib::ext::shared_ptr<NPVCube>& nettingCube, const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
        const bool continueOnErr = true,
        const std::map<std::string, QuantLib::Real>& currentIM = std::map<std::string, QuantLib::Real>());

    // get a vector of netting set ids for the given portfolio sorted in alphabetical order, if no portfolio
    // is given here, the netting sets for the global portfolio set in the ctor are returned
    std::set<std::string> getNettingSetIds(const QuantLib::ext::shared_ptr<Portfolio>& portfolio = nullptr) const;

protected:
    virtual QuantLib::ext::shared_ptr<NPVCube>
    getNettingSetCube(std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
                      const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {
        return nullptr;
    };

    virtual QuantLib::ext::shared_ptr<NPVCube> getNpvCube(const Date& asof, const std::set<std::string>& ids,
                                                  const std::vector<Date>& dates, const Size samples,
                                                  const Size depth) const;

    virtual QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator>
    getDimCalculator(const QuantLib::ext::shared_ptr<NPVCube>& cube,
                     const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpreter,
                     const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
                     const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& model = nullptr,
                     const QuantLib::ext::shared_ptr<NPVCube>& nettingCube = nullptr,
                     const std::map<std::string, QuantLib::Real>& currentIM = std::map<std::string, QuantLib::Real>());

    virtual QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>
    projectSsmData(const std::set<std::string>& currencyFilter) const;

    virtual QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator> getProjectedScenarioGenerator(
        const boost::optional<std::set<std::string>>& currencyFilter, const QuantLib::ext::shared_ptr<Market>& market,
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& projectedSsmData,
        const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory, const bool continueOnErr) const;
    
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    QuantLib::Date asof_;
    std::string baseCurrency_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<ore::data::NettingSetManager> netting_;
    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> collateralBalances_;
    QuantLib::ext::shared_ptr<ore::data::EngineData> engineData_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;
    QuantLib::ext::shared_ptr<ore::data::CrossAssetModelData> crossAssetModelData_;
    QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
    QuantLib::Real dimQuantile_;
    QuantLib::Size dimHorizonCalendarDays_;
    map<string, bool> analytics_;
    string inputCalculationType_;
    string dvaName_;
    string fvaBorrowingCurve_;
    string fvaLendingCurve_;
    bool fullInitialCollateralisation_;
    bool storeFlows_;

    // generated data
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<EngineFactory> simFactory_;
    QuantLib::RelinkableHandle<AggregationScenarioData> scenarioData_;
    QuantLib::ext::shared_ptr<NPVCube> cube_, nettingCube_;
    QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpreter_;
    std::string calculationType_;
    QuantLib::ext::shared_ptr<PostProcess> postProcess_;

    QuantLib::ext::shared_ptr<std::vector<std::vector<QuantLib::Path>>> bufferedPaths_;
};

} // namespace analytics
} // namespace ore
