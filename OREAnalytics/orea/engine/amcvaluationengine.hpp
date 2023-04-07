/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file engine/amcvaluationengine.hpp
    \brief valuation engine for amc
    \ingroup engine
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/scenario/scenariogeneratordata.hpp>

#include <ored/marketdata/loader.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/progressbar.hpp>

#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace analytics {

using std::string;

//! AMC Valuation Engine
class AMCValuationEngine : public ore::data::ProgressReporter {
public:
    //! Constructor for single-threaded runs
    AMCValuationEngine(const boost::shared_ptr<QuantExt::CrossAssetModel>& model,
                       const boost::shared_ptr<ore::analytics::ScenarioGeneratorData>& scenarioGeneratorData,
                       const boost::shared_ptr<ore::data::Market>& market, const std::vector<string>& aggDataIndices,
                       const std::vector<string>& aggDataCurrencies);

    //! Constructor for multi threaded runs
    AMCValuationEngine(
        const QuantLib::Size nThreads, const QuantLib::Date& today, const QuantLib::Size nSamples,
        const boost::shared_ptr<ore::data::Loader>& loader,
        const boost::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
        const std::vector<string>& aggDataIndices, const std::vector<string>& aggDataCurrencies,
        const boost::shared_ptr<CrossAssetModelData>& crossAssetModelData,
        const boost::shared_ptr<ore::data::EngineData>& engineData,
        const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
        const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
        const std::string& configurationLgmCalibration, const std::string& configurationFxCalibration,
        const std::string& configurationEqCalibration, const std::string& configurationInfCalibration,
        const std::string& configurationCrCalibration, const std::string& configurationFinalModel,
        const boost::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
        const ore::data::IborFallbackConfig& iborFallbackConfig = ore::data::IborFallbackConfig::defaultConfig(),
        const bool handlePseudoCurrenciesTodaysMarket = true,
        const std::function<boost::shared_ptr<ore::analytics::NPVCube>(
            const QuantLib::Date&, const std::set<std::string>&, const std::vector<QuantLib::Date>&,
            const QuantLib::Size)>& cubeFactory = {});

    //! build cube in single threaded run
    void buildCube(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                   boost::shared_ptr<ore::analytics::NPVCube>& outputCube);

    //! build cube in multi threaded run
    void buildCube(const boost::shared_ptr<ore::data::Portfolio>& portfolio);

    // result output cubes for multi threaded runs (mini-cubes, one per thread)
    std::vector<boost::shared_ptr<ore::analytics::NPVCube>> outputCubes() const { return miniCubes_; }

    //! Set aggregation data
    boost::shared_ptr<ore::analytics::AggregationScenarioData>& aggregationScenarioData() { return asd_; }

    //! Get aggregation data
    const boost::shared_ptr<ore::analytics::AggregationScenarioData>& aggregationScenarioData() const { return asd_; }

private:
    // set / get via additional methods
    boost::shared_ptr<ore::analytics::AggregationScenarioData> asd_;

    // running in single or multi threaded mode?
    bool useMultithreading_ = false;

    // shared inputs
    const std::vector<string> aggDataIndices_, aggDataCurrencies_;
    boost::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;

    // inputs for single-threaded run
    const boost::shared_ptr<QuantExt::CrossAssetModel> model_;
    const boost::shared_ptr<ore::data::Market> market_;

    // inputs for muulti-threaded run
    QuantLib::Size nThreads_;
    QuantLib::Date today_;
    QuantLib::Size nSamples_;
    boost::shared_ptr<ore::data::Loader> loader_;
    boost::shared_ptr<CrossAssetModelData> crossAssetModelData_;
    boost::shared_ptr<ore::data::EngineData> engineData_;
    boost::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    boost::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    std::string configurationLgmCalibration_;
    std::string configurationFxCalibration_;
    std::string configurationEqCalibration_;
    std::string configurationInfCalibration_;
    std::string configurationCrCalibration_;
    std::string configurationFinalModel_;
    boost::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    ore::data::IborFallbackConfig iborFallbackConfig_;
    bool handlePseudoCurrenciesTodaysMarket_;
    std::function<boost::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                             const std::vector<QuantLib::Date>&, const QuantLib::Size)>
        cubeFactory_;

    // result cubes for multi-threaded run
    std::vector<boost::shared_ptr<ore::analytics::NPVCube>> miniCubes_;
};

} // namespace analytics
} // namespace ore
