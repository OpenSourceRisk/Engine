/*
  Copyright (C) 2021 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ored/model/hwhistoricalcalibrationmodelbuilder.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

HwHistoricalCalibrationModelBuilder::HwHistoricalCalibrationModelBuilder(HwHistoricalCalibrationModelData& data,
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
            model_->pcaCalibration(data_.varianceRetained);
        } catch (std::exception& e) {
            if (!continueOnError_)
                QL_FAIL("PCA calibration failed: " << e.what());
            WLOG("PCA calibration error ignored: " << e.what());
        }
    } else {
        QL_REQUIRE(!data_.eigenValues.empty() && !data_.eigenVectors.empty(),
                   "No eigenvalues and eigenvectors found from input files.");
        DLOG("HwHistoricalCalibrationModelBuilder: PCA skipped, eigenvalues and eigenvectors read from input files.");
    }

    // Mean Reversion stage
    if (runMeanReversionCalibration_) {
        try {
            model_->meanReversionCalibration(data_.basisFunctionNumber, data_.kappaUpperBound, data_.haltonMaxGuess);
        } catch (std::exception& e) {
            if (!continueOnError_)
                QL_FAIL("Mean reversion calibration failed: " << e.what());
            WLOG("Mean reversion calibration error ignored: " << e.what());
        }
    }

    extractOutputs();
    DLOG("HwHistoricalCalibrationModelBuilder: performCalculations finished");
}

void HwHistoricalCalibrationModelBuilder::buildModel() {
    LOG("Start building HwHistoricalCalibratonModel");
    if (runPcaCalibration_)
        model_ = std::make_unique<QuantExt::HwHistoricalCalibrationModel>(
            data_.asOf, data_.curveTenors, data_.lambda, data_.useForwardRate, data_.irCurves, data_.fxSpots);
    else
        model_ = std::make_unique<QuantExt::HwHistoricalCalibrationModel>(
            data_.asOf, data_.curveTenors, data_.useForwardRate, data_.eigenValues, data_.eigenVectors);
    LOG("Building HwHistoricalCalibratonModel done");
}

void HwHistoricalCalibrationModelBuilder::extractOutputs() const { 
    if (runPcaCalibration_) {
        data_.principalComponents = model_->principalComponent();
        data_.eigenValues = model_->eigenValue();
        data_.eigenVectors = model_->eigenVector();
        data_.rho = model_->rho();
        data_.fxSigma = model_->fxSigma();
    }
    if (runMeanReversionCalibration_) {
        data_.kappa = model_->kappa();
        data_.v = model_->v();
        data_.irSigma = model_->irSigma();
        data_.irKappa = model_->irKappa();
    }
}

} // namespace data
} // namespace ore
