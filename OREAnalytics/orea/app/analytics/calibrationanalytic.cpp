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
#include <ored/model/modelparameter.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

#include <qle/models/cirppconstantfellerparametrization.hpp>
#include <qle/models/cirppconstantparametrization.hpp>

#include <iostream>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/******************************************************************
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
    LOG("Calibration: Build Simulation Model (continueOnCalibrationError = " << std::boolalpha
                                                                             << continueOnCalibrationError << ")");
    ext::shared_ptr<Market> market = analytic()->market();
    QL_REQUIRE(market != nullptr, "Internal error, buildCrossAssetModel needs to be called after the market is built.");

    builder_ = ext::make_shared<CrossAssetModelBuilder>(
        market, analytic()->configurations().crossAssetModelData, inputs_->marketConfig("lgmcalibration"),
        inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
        inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
        inputs_->marketConfig("simulation"), false, continueOnCalibrationError, "", "xva cam building");

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
    auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
    auto continueOnCalErr = globalParams.find("ContinueOnCalibrationError");
    bool continueOnErr = (continueOnCalErr != globalParams.end()) && parseBool(continueOnCalErr->second);
    buildCrossAssetModel(continueOnErr);
    CONSOLE("OK");
    ProgressMessage(msg, 1, 1).log();

    msg = "Calibration: Write Modified Model Data";
    LOG(msg);
    CONSOLEW(msg);
    ProgressMessage(msg, 0, 1).log();

    // Update Cross Asset Model Data: IR
    QuantLib::ext::shared_ptr<CrossAssetModelData> data = builder_->modelData();
    Array times, values;
    for (Size i = 0; i < data->irConfigs().size(); ++i) {
        ext::shared_ptr<IrModelData> irData = data->irConfigs()[i];
        ext::shared_ptr<Parametrization> irPara = model_->ir(i);
        if (auto lgmPara = ext::dynamic_pointer_cast<IrLgm1fParametrization>(irPara)) {
            // LGM flavours including HW adaptor
            DLOG("CamData, updating IrLgm1fParametrization:"
                 << " name=" << irData->name() << " qualifier=" << irData->qualifier());
            QL_REQUIRE(lgmPara->numberOfParameters() == 2, "2 lgm1f model parameters expected");
            ext::shared_ptr<LgmData> lgmData = boost::dynamic_pointer_cast<LgmData>(irData);
            QL_REQUIRE(lgmData, "Failed to cast IrModelData to LgmData");
            // overwrite initial values with calibration results
            times = lgmPara->parameterTimes(0);
            values = lgmPara->parameterValues(0);
            lgmData->aTimes() = std::vector<Real>(times.begin(), times.end());
            lgmData->aValues() = std::vector<Real>(values.begin(), values.end());
            lgmData->calibrateA() = false;
            times = lgmPara->parameterTimes(1);
            values = lgmPara->parameterValues(1);
            lgmData->hTimes() = std::vector<Real>(times.begin(), times.end());
            lgmData->hValues() = std::vector<Real>(values.begin(), values.end());
            lgmData->calibrateH() = false;
        } else if (auto hwPara = ext::dynamic_pointer_cast<IrHwParametrization>(irPara)) {
            // HW Multi-Factor
            StructuredAnalyticsWarningMessage("CalibrationAnalytic", "Parametrization not processed.",
                                              "HW parametrization not covered for IR model name=" + irData->name() +
                                                  " qualifier=" + irData->qualifier())
                .log();
        } else {
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for IR model, model data unchanged: name=" + irData->name() +
                    " qualifier=" + irData->qualifier())
                .log();
        }
    }

    // Update Cross Asset Model Data: FX
    for (Size i = 0; i < data->fxConfigs().size(); ++i) {
        ext::shared_ptr<FxBsData> fxData = data->fxConfigs()[i];
        ext::shared_ptr<Parametrization> para = model_->fx(i);
        if (auto fxPara = ext::dynamic_pointer_cast<FxBsParametrization>(para)) {
            LOG("CamData, updating FxBsParametrization:"
                << " foreign=" << fxData->foreignCcy() << " domestic=" << fxData->domesticCcy());
            QL_REQUIRE(fxPara->numberOfParameters() == 1, "1 fx model parameter expected");
            // overwrite initial values with calibration results
            times = fxPara->parameterTimes(0);
            values = fxPara->parameterValues(0);
            fxData->sigmaTimes() = std::vector<Real>(times.begin(), times.end());
            fxData->sigmaValues() = std::vector<Real>(values.begin(), values.end());
            // set calibration flags to false to ensure we reuse the calibration when loading this version
            fxData->calibrateSigma() = false;
        } else {
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for FX model, model data not changed: foreign=" +
                    fxData->foreignCcy() + " domestic=" + fxData->domesticCcy())
                .log();
        }
    }

    // Update Cross Asset Model Data: EQ
    for (Size i = 0; i < data->eqConfigs().size(); ++i) {
        ext::shared_ptr<EqBsData> eqData = data->eqConfigs()[i];
        ext::shared_ptr<Parametrization> para = model_->eq(i);
        if (auto eqPara = ext::dynamic_pointer_cast<EqBsParametrization>(para)) {
            LOG("CamData, updating EqBsParametrization:"
                << " name=" << eqData->eqName());
            QL_REQUIRE(eqPara->numberOfParameters() == 1, "1 equity model parameter expected");
            // overwrite initial values with calibration results
            times = eqPara->parameterTimes(0);
            values = eqPara->parameterValues(0);
            eqData->sigmaTimes() = std::vector<Real>(times.begin(), times.end());
            eqData->sigmaValues() = std::vector<Real>(values.begin(), values.end());
            // set calibration flags to false to ensure we reuse the calibration when loading this version
            eqData->calibrateSigma() = false;
        } else {
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for EQ model, model data not changed: name=" + eqData->eqName())
                .log();
        }
    }

    // Update Cross Asset Model Data: INF
    for (Size i = 0; i < data->infConfigs().size(); ++i) {
        ext::shared_ptr<InflationModelData> infData = data->infConfigs()[i];
        ext::shared_ptr<Parametrization> para = model_->inf(i);
        if (auto infPara = ext::dynamic_pointer_cast<InfDkParametrization>(para)) {
            // DK
            LOG("CamData, updating InfDkParametrization:"
                << " ccy=" << infData->currency() << " index=" << infData->index());
            QL_REQUIRE(infPara->numberOfParameters() == 2, "2 model parameters for INF DK");
            ext::shared_ptr<InfDkData> dkData = boost::dynamic_pointer_cast<InfDkData>(infData);
            QL_REQUIRE(dkData, "Failed to cast InflationModelData to InfDkData");
            // FIXME: Check order or model parameters
            times = infPara->parameterTimes(0);
            values = infPara->parameterValues(0);
            VolatilityParameter vp(*dkData->volatility().volatilityType(), false, dkData->volatility().type(),
                                   std::vector<Real>(times.begin(), times.end()),
                                   std::vector<Real>(values.begin(), values.end()));
            times = infPara->parameterTimes(1);
            values = infPara->parameterValues(1);
            ReversionParameter rp(dkData->reversion().reversionType(), false, dkData->reversion().type(),
                                  std::vector<Real>(times.begin(), times.end()),
                                  std::vector<Real>(values.begin(), values.end()));
            dkData->setVolatility(vp);
            dkData->setReversion(rp);
        } else if (auto infPara = ext::dynamic_pointer_cast<InfJyParameterization>(para)) {
            // JY
            LOG("CamData, updating InfJyParametrization:"
                << " ccy=" << infData->currency() << " index=" << infData->index());
            QL_REQUIRE(infPara->numberOfParameters() == 3, "3 model parameters expected for INF JY");
            ext::shared_ptr<InfJyData> jyData = boost::dynamic_pointer_cast<InfJyData>(infData);
            QL_REQUIRE(jyData, "Failed to cast InflationModelData to InfJyData");
            times = infPara->realRate()->parameterTimes(0);
            values = infPara->realRate()->parameterValues(0);
            VolatilityParameter rrv(*jyData->realRateVolatility().volatilityType(), false,
                                    jyData->realRateVolatility().type(), std::vector<Real>(times.begin(), times.end()),
                                    std::vector<Real>(values.begin(), values.end()));
            times = infPara->realRate()->parameterTimes(1);
            values = infPara->realRate()->parameterValues(1);
            ReversionParameter rrr(jyData->realRateReversion().reversionType(), false,
                                   jyData->realRateReversion().type(), std::vector<Real>(times.begin(), times.end()),
                                   std::vector<Real>(values.begin(), values.end()));
            times = infPara->index()->parameterTimes(0);
            values = infPara->index()->parameterValues(0);
            VolatilityParameter iv(*jyData->indexVolatility().volatilityType(), false, jyData->indexVolatility().type(),
                                   std::vector<Real>(times.begin(), times.end()),
                                   std::vector<Real>(values.begin(), values.end()));
            jyData->setRealRateReversion(rrr);
            jyData->setRealRateVolatility(rrv);
            jyData->setIndexVolatility(iv);
        } else {
            // Any others ?
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for INF model, model data not changed: ccy=" + infData->currency() +
                    " index=" + infData->index())
                .log();
        }
    }

    // Update Cross Asset Model Data: COM
    for (Size i = 0; i < data->comConfigs().size(); ++i) {
        ext::shared_ptr<CommoditySchwartzData> comData = data->comConfigs()[i];
        ext::shared_ptr<Parametrization> para = model_->com(i);
        if (auto comPara = ext::dynamic_pointer_cast<CommoditySchwartzParametrization>(para)) {
            LOG("CamData, updating CommoditySchwartzParametrization:"
                << " ccy=" << comData->currency() << " name=" << comData->name());
            QL_REQUIRE(comPara->numberOfParameters() == 2, "2 model parameters for COM");
            // overwrite initial values with calibration results
            comData->sigmaValue() = comPara->parameterValues(0).front();
            comData->kappaValue() = comPara->parameterValues(1).front();
            comData->calibrateSigma() = false;
            comData->calibrateKappa() = false;
        } else {
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for COM model, model data not changed: ccy=" + comData->currency() +
                    " name=" + comData->name())
                .log();
        }
    }

    // CAM supports two types of CR model data with associated parametrizations (LGM and CIR++)
    // So we loop over these two types separately

    // Update Cross Asset Model Data: CR LGM
    for (Size i = 0; i < data->crLgmConfigs().size(); ++i) {
        ext::shared_ptr<CrLgmData> crData = data->crLgmConfigs()[i];
        Size crComponentIndex = model_->crName(crData->name());
        ext::shared_ptr<Parametrization> para = model_->cr(crComponentIndex);
        if (auto crPara = ext::dynamic_pointer_cast<CrLgm1fParametrization>(para)) {
            LOG("CamData, updating Credit LGM Config: name=" << crData->name());
            QL_REQUIRE(crPara->numberOfParameters() == 2, "2 model parameters for CR LGM");
            times = crPara->parameterTimes(0);
            values = crPara->parameterValues(0);
            crData->aTimes() = std::vector<Real>(times.begin(), times.end());
            crData->aValues() = std::vector<Real>(values.begin(), values.end());
            crData->calibrateA() = false;
            times = crPara->parameterTimes(1);
            values = crPara->parameterValues(1);
            crData->hTimes() = std::vector<Real>(times.begin(), times.end());
            crData->hValues() = std::vector<Real>(values.begin(), values.end());
            crData->calibrateH() = false;
        } else {
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for CR LGM config, model data not changed: name=" + crData->name())
                .log();
        }
    }

    // Update Cross Asset Model Data: CR CIR++
    for (Size i = 0; i < data->crCirConfigs().size(); ++i) {
        ext::shared_ptr<CrCirData> crData = data->crCirConfigs()[i];
        Size crComponentIndex = model_->crName(crData->name());
        ext::shared_ptr<Parametrization> para = model_->cr(crComponentIndex);
        if (auto crPara = ext::dynamic_pointer_cast<CrCirppConstantParametrization>(para)) {
            LOG("CamData, updating Credit CIR++ Config: name=" << crData->name());
            QL_REQUIRE(crPara->numberOfParameters() == 4, "4 model parameters for CR CIR++");
            // parameter ordering: kappa, theta, sigma, v0
            // kappa:
            crData->reversionValue() = crPara->parameterValues(0).front();
            // theta:
            crData->longTermValue() = crPara->parameterValues(1).front();
            // sigma:
            crData->volatility() = crPara->parameterValues(2).front();
            // v0:
            crData->startValue() = crPara->parameterValues(3).front();
            crData->calibrationType() = ore::data::CalibrationType::None;
        } else if (auto crPara = ext::dynamic_pointer_cast<CrCirppConstantWithFellerParametrization>(para)) {
            LOG("CamData, updating Credit CIR++ Config: name=" << crData->name());
            QL_REQUIRE(crPara->numberOfParameters() == 4, "4 model parameters for CR CIR++");
            // parameter ordering: kappa, theta, sigma, v0 (as above)
            // kappa:
            crData->reversionValue() = crPara->parameterValues(0).front();
            // theta:
            crData->longTermValue() = crPara->parameterValues(1).front();
            // sigma:
            crData->volatility() = crPara->parameterValues(2).front();
            // v0:
            crData->startValue() = crPara->parameterValues(3).front();
            // switch off calibration:
            crData->calibrationType() = ore::data::CalibrationType::None;
        } else {
            StructuredAnalyticsWarningMessage(
                "CalibrationAnalytic", "Parametrization not processed.",
                "Matching parametrization not found for CR CIR++ config, model data not changed: name=" +
                    crData->name())
                .log();
        }
    }

    // Write CAM data to an xml file
    string fileName = inputs_->resultsPath().string() + "/calibration.xml";
    data->toFile(fileName);

    // Write CAM data as single XML string to an in-memory report
    string xml = data->toXMLStringUnformatted();
    auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeXmlReport(*report, "CrossAssetModel", xml);
    analytic()->reports()["CALIBRATION"]["calibration"] = report;

    CONSOLE("OK");

    ProgressMessage("Running Calibration Analytic", 1, 1).log();
}

} // namespace analytics
} // namespace ore
