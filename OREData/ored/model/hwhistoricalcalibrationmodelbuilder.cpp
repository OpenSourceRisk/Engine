/*
  Copyright (C) 2021 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ored/model/hwhistoricalcalibrationmodelbuilder.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

HwHistoricalCalibrationModelBuilder::HwHistoricalCalibrationModelBuilder(QuantLib::ext::shared_ptr<HwHistoricalCalibrationModelData>& data,
                                                                         const bool runPcaCalibration,
                                                                         const bool runMeanReversionCalibration,
                                                                         const bool continueOnError)
    : data_(data), runPcaCalibration_(runPcaCalibration), runMeanReversionCalibration_(runMeanReversionCalibration),
      continueOnError_(continueOnError) {
    performCalculations();
}

void HwHistoricalCalibrationModelBuilder::performCalculations() {

    DLOG("HwHistoricalCalibrationModelBuilder: starting performCalculations");

    buildModel();

    // PCA stage
    if (runPcaCalibration_) {
        DLOG("HwHistoricalCalibrationModelBuilder: starting PCA calibration");
        try {
            model_->pcaCalibration(data_->varianceRetained());
        } catch (std::exception& e) {
            if (!continueOnError_)
                QL_FAIL("PCA calibration failed: " << e.what());
            WLOG("PCA calibration error ignored: " << e.what());
        }
    } else {
        QL_REQUIRE(!data_->eigenValues().empty() && !data_->eigenVectors().empty(),
                   "No eigenvalues and eigenvectors found from input files.");
        DLOG("HwHistoricalCalibrationModelBuilder: PCA skipped, eigenvalues and eigenvectors read from input files.");
    }

    // Mean Reversion stage
    if (runMeanReversionCalibration_) {
        try {
            model_->meanReversionCalibration(data_->basisFunctionNumber(), data_->kappaUpperBound(),
                                             data_->haltonMaxGuess());
        } catch (std::exception& e) {
            if (!continueOnError_)
                QL_FAIL("Mean reversion calibration failed: " << e.what());
            WLOG("Mean reversion calibration error ignored: " << e.what());
        }
    }
    // retrieve log
    std::string logLine;
    std::istringstream logInput(qleLog_.str());
    while (std::getline(logInput, logLine)) {
        if (!logLine.empty()) {
            LOG(logLine);
        }
    }

    extractOutputs();
    DLOG("HwHistoricalCalibrationModelBuilder: performCalculations finished");
}

void HwHistoricalCalibrationModelBuilder::buildModel() {
    LOG("Start building HwHistoricalCalibratonModel");
    if (runPcaCalibration_)
        model_ = std::make_unique<QuantExt::HwHistoricalCalibrationModel>(data_->asOf(), data_->curveTenors(),
                                                                          data_->lambda(), data_->useForwardRate(),
                                                                          data_->irCurves(), data_->fxSpots(), &qleLog_);
    else
        model_ = std::make_unique<QuantExt::HwHistoricalCalibrationModel>(
            data_->asOf(), data_->curveTenors(), data_->useForwardRate(), data_->principalComponents(),
            data_->eigenValues(), data_->eigenVectors(), &qleLog_);
    LOG("Building HwHistoricalCalibratonModel done");
}

void HwHistoricalCalibrationModelBuilder::extractOutputs() const { 
    if (runPcaCalibration_) {
        data_->setPcaResults(model_->eigenValue(), model_->eigenVector(), model_->principalComponent(),
                             model_->fxSigma(), model_->rho());
    }
    if (runMeanReversionCalibration_) {
        data_->setMeanReversionResults(model_->kappa(), model_->v(), model_->irSigma(), model_->irKappa());
    }
}

} // namespace data
} // namespace ore
