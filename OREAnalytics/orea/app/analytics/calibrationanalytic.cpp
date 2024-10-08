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

#include <orea/app/analytics/calibrationanalytic.hpp>

#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

  /****************************************************************
 * CalibrationAnalytic: Cross Asset Model calibration and reporting
 ******************************************************************/

void CalibrationAnalyticImpl::setUpConfigurations() {
    LOG("CalibrationAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().crossAssetModelData = inputs_->crossAssetModelData();
}

QuantLib::ext::shared_ptr<EngineFactory> CalibrationAnalyticImpl::engineFactory() {
    LOG("CalibrationAnalytic::engineFactory() called");
    QuantLib::ext::shared_ptr<EngineData> edCopy =
        QuantLib::ext::make_shared<EngineData>(*inputs_->simulationPricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = inputs_->outputAdditionalResults() ? "true" : "false";
    edCopy->globalParameters()["RunType"] = "Exposure";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders;
    std::vector<QuantLib::ext::shared_ptr<LegBuilder>> extraLegBuilders;

    engineFactory_ = QuantLib::ext::make_shared<EngineFactory>(
            edCopy, analytic()->market(), configurations, inputs_->refDataManager(), *inputs_->iborFallbackConfig());

    return engineFactory_;
}

void CalibrationAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("Calibration: Build Simulation Model (continueOnCalibrationError = "
	<< std::boolalpha << continueOnCalibrationError << ")");
    ext::shared_ptr<Market> market = analytic()->market();
    QL_REQUIRE(market != nullptr, "Internal error, buildCrossAssetModel needs to be called after the market is built.");

    builder_ = ext::make_shared<CrossAssetModelBuilder>(
	market, analytic()->configurations().crossAssetModelData, inputs_->calibrationData(),
	inputs_->marketConfig("lgmcalibration"),
        inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
        inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
        inputs_->marketConfig("simulation"), false, continueOnCalibrationError, "",
        analytic()->configurations().crossAssetModelData->getSalvagingAlgorithm(), "xva cam building");

    model_ = *builder_->model();
}

void CalibrationAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
					  const std::set<std::string>& runTypes) {

    SavedSettings settings;
    
    string msg = "Running Calibration Analytic";
    LOG(msg << " with asof " << io::iso_date(inputs_->asof()));
    ProgressMessage(msg, 0, 1).log();

    Settings::instance().evaluationDate() = inputs_->asof();

    msg = "Calibration: Build Market";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    analytic()->buildMarket(loader);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    msg = "Calibration: Build Model";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    bool continueOnErr = false; // since this is the only thing we do here
    buildCrossAssetModel(continueOnErr);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    msg = "Calibration: Write Report";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();
    CONSOLE("OK");

    auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeCalibrationReport(*report, builder_);
    analytic()->reports()["CALIBRATION"]["calibration"] = report;
    
    ProgressMessage("Running Calibration Analytic", 1, 1).log();
}

} // namespace analytics
} // namespace ore
