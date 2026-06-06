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

#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/sacvaanalytic.hpp>
#include <orea/app/inputparameters.hpp>
#include <orea/engine/standardapproachcvacalculator.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/engine/sacvasensitivityloader.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/parsers.hpp>

using RFType = ore::analytics::RiskFactorKey::KeyType;

namespace ore {
namespace analytics {

void SaCvaVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {

    auto inputPath = inputs->setupVariables().inputPath_;

    vector<string> analyticStrs = {"sacva", "bacva", "setup"};
    inputs->loadParameterXML<NettingSetManager>(nettingSetManager_, analyticStrs, "csaFile");

    // Forward the netting set manager to the xva section so the dependent XVA analytic can find it
    if (nettingSetManager_)
        inputs->setNettingSetManager(nettingSetManager_);

    // Load counterparty manager from sacva/bacva sections (needed by the SA-CVA calculator)
    inputs->loadParameterXML<CounterpartyManager>(counterpartyManager_, analyticStrs, "counterpartyFile");
    if (counterpartyManager_)
        inputs->setCounterpartyManager(counterpartyManager_);

    std::string tmp;

    // Load simulationConfigFile from sacva section and forward to the simulation section
    // so the XVA sub-analytic can pick it up for the exposure sim market
    inputs->loadParameter<std::string>(tmp, "sacva", "simulationConfigFile");
    if (!tmp.empty()) {
        LOG("Loading simulationConfigFile from sacva section: " << tmp);
        inputs->setExposureSimMarketParams(tmp);
    }

    // Load scenarioGeneratorData from sacva section and forward to the simulation section
    // so the XVA sub-analytic can pick it up (otherwise ConfigurationBuilder defaults to 1000 samples)
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "scenarioGeneratorData");
    if (!tmp.empty()) {
        LOG("Loading scenarioGeneratorData from sacva section: " << tmp);
        inputs->setScenarioGeneratorData(tmp);
    }

    // Load crossAssetModelData from sacva section and forward to the simulation section
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "crossAssetModelData");
    if (!tmp.empty()) {
        LOG("Loading crossAssetModelData from sacva section: " << tmp);
        inputs->setCrossAssetModelData(tmp);
    }

    // Load dimModel from sacva section and forward to the xva section
    // so the XVA sub-analytic applies Dynamic Initial Margin (e.g. DeltaVaR)
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "dimModel");
    if (!tmp.empty()) {
        LOG("Loading dimModel from sacva section: " << tmp);
        inputs->setDimModel(tmp);
        inputs->setDimAnalytic(true);
    }

    // Load dimScaling from the sacva section and forward it to the xva section.
    // When a dimModel is configured for SA-CVA but no dimScaling is supplied (e.g. the Restore
    // JSON path, where parameters are looked up flat and dimScaling is not part of the request
    // body), default to 1.0 (unscaled). SA-CVA only uses DIM to derive CVA sensitivities, so an
    // unscaled DIM is the correct default; an explicitly supplied dimScaling always takes precedence.
    Real dimScalingValue = QuantLib::Null<Real>();
    inputs->loadParameter<Real>(dimScalingValue, "sacva", "dimScaling", false, ore::data::parseReal);
    if (dimScalingValue == QuantLib::Null<Real>() && !tmp.empty())
        dimScalingValue = 1.0;
    if (dimScalingValue != QuantLib::Null<Real>()) {
        LOG("Forwarding dimScaling to xva section: " << dimScalingValue);
        inputs->setDimScaling(dimScalingValue);
    }

    // Forward storeSensis from sacva to simulation section
    // so the XVA sub-analytic creates nettingSetCube and sensitivityStorageManager for DIM
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "storeSensis");
    if (!tmp.empty() && ore::data::parseBool(tmp)) {
        LOG("Loading storeSensis from sacva section");
        inputs->setStoreSensis(true);
    }

    // Load xvaSensiSimMarketParams from sacva section (for XVA sensitivity analytic)
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "xvaSensiSimMarketParams");
    if (!tmp.empty()) {
        LOG("Loading xvaSensiSimMarketParams from sacva section: " << tmp);
        inputs->setXvaSensiSimMarketParams(tmp);
    }

    // Load xvaSensiScenarioData from sacva section (for XVA sensitivity analytic)
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "xvaSensiScenarioData");
    if (!tmp.empty()) {
        LOG("Loading xvaSensiScenarioData from sacva section: " << tmp);
        inputs->setXvaSensiScenarioData(tmp);
    }

    // Load sensitivity input files
    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "saCvaNetSensitivitiesFile");
    if (!tmp.empty()) {
        std::string file = (inputs->setupVariables().inputPath_ / tmp).generic_string();
        LOG("Loading aggregated SA-CVA sensitivity input from file" << file);
        inputs->setSaCvaNetSensitivitiesFromFile(file);
    } else {
        inputs->loadParameter<std::string>(tmp, "sacva", "cvaSensitivitiesFile");
        if (!tmp.empty()) {
            std::string file = (inputs->setupVariables().inputPath_ / tmp).generic_string();
            LOG("Loading granular cva sensitivity input from file" << file);
            inputs->setCvaSensitivitiesFromFile(file);
        }
    }

    tmp = {};
    inputs->loadParameter<std::string>(tmp, "sacva", "nameMappingInputFile");
    if (!tmp.empty()) {
        std::string fileName = (inputPath / tmp).generic_string();
        LOG("simmNameMapper file name: " << fileName);
        inputs->setSimmNameMapperFromFile(fileName);
    }else{
        auto nameMapper = QuantLib::ext::make_shared<SimmBasicNameMapper>();
        inputs->setSimmNameMapper(nameMapper);
    }
}

void SaCvaAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->xvaSensiScenarioData();
}

void SaCvaAnalyticImpl::buildDependencies() {
    auto sensiAnalytic =
        AnalyticFactory::instance().build("XVA_SENSITIVITY", inputs_, analytic()->analyticsManager(), true);
    if (sensiAnalytic.second)
        addDependentAnalytic(sensiLookupKey, sensiAnalytic.second, true);
}

void SaCvaAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                    const std::set<std::string>& runTypes) {

    LOG("SaCvaAnalyticImpl::runAnalytic called");
    SaCvaNetSensitivities cvaSensis = inputs_->saCvaNetSensitivities();

    auto sacvaVars = ext::dynamic_pointer_cast<SaCvaVariables>(inputVariables_);

    // Generate sensitivities here if not provided
    if (cvaSensis.size() == 0) {
	    //auto sensiAnalytic = AnalyticFactory::instance().build(analyticsLabel, inputs_).second;
        auto sensiAnalytic = dependentAnalytic(sensiLookupKey);
        sensiAnalytic->runAnalytic(loader, {"XVA_SENSITIVITY"});

	    // Get the par cva sensitivity cube stream that we have just cached after running the sensi analytic

        auto xsai = static_cast<XvaSensitivityAnalyticImpl*>(sensiAnalytic->impl().get());
        auto pss = xsai->parCvaSensiCubeStream();
	    QL_REQUIRE(pss, "parCvaSensiCubeStream has not been populated by the xva sensitivity analytic");

	    // Use the loader to map and aggregate the par sensitivity input.
	    // We pass scenario data to cvaLoader.loadFromRawSensis(), because the loader needs the original shift types
	    // and especially shift sizes to compute the capital calulator inputs from sensitivities, in particular:
	    // - yield curve sensis have to be calculated using abolsute shifts of 1bp = 0.0001, and the input into the
	    //   capital calculator is sensitivity divided by ABSOLUTE shift size 0.0001, i.e. a partial derivative proxy;
	    // - FX rate, FX and yield vol sensis have to be calculated using RELATIVE shifts of 1% = 0.01, and the
	    //   input into the capital calculator is sensitivity divided by shift size 0.01
	    // See https://www.bis.org/basel_framework/chapter/MAR/50.htm
        SaCvaSensitivityLoader cvaLoader;
        cvaLoader.loadFromRawSensis(pss, inputs_->baseCurrency(), analytic()->configurations().sensiScenarioData,
                                    inputs_->counterpartyManager());
        cvaSensis = cvaLoader.netRecords();

        CONSOLEW("SA-CVA: Scaled CVA Sensitivity Report");
	    auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
	    ReportWriter(inputs_->reportNaString()).writeCvaSensiReport(cvaLoader.cvaSensitivityRecords(), *cvaSensiReport);
	    analytic()->addReport(label(), "cva_sensitivities", cvaSensiReport);
	    CONSOLE("OK");
    }

    // Report the net CVA sensis, even if we loaded them from a report
    CONSOLEW("SA-CVA: SACVA Sensitivity Report");
    auto saCvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeSaCvaSensiReport(cvaSensis, *saCvaSensiReport);
    analytic()->addReport(label(), "sacva_sensitivities", saCvaSensiReport);
    CONSOLE("OK");

    // Create the SA-CVA result reports we want to be populated by the sacva calculator below
    auto summaryReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    auto detailReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<Report>> reports;
    reports[StandardApproachCvaCalculator::ReportType::Summary] = summaryReport;
    reports[StandardApproachCvaCalculator::ReportType::Detail] = detailReport;

    // Call the SA-CVA calculator, given CVA sensitivities
    CONSOLEW("SA-CVA: Capital Reports");
    auto sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(
        inputs_->baseCurrency(), cvaSensis, inputs_->counterpartyManager(), reports,
        inputs_->useUnhedgedCvaSensis(), inputs_->cvaPerfectHedges());
    sacva->calculate();
    CONSOLE("OK");

    analytic()->addReport(label(), "sacva", summaryReport);
    analytic()->addReport(label(), "sacvadetail", detailReport);

    LOG("SaCvaAnalyticImpl::runAnalytic done");
}

} // namespace analytics
} // namespace ore
