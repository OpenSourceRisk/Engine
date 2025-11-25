/*
  Copyright (C) 2021 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*!
  \file orea/app/hwhistoricalcalibrationmodelbuilder.hpp
  \Model builder for Hull-White historical calibration model
 */

#pragma once

#include <qle/models/hwhistoricalcalibrationmodel.hpp>
#include <ored/model/hwhistoricalcalibrationmodeldata.hpp>
#include <ored/marketdata/market.hpp>


namespace ore {
namespace data {

class HwHistoricalCalibrationModelBuilder {
public:
    HwHistoricalCalibrationModelBuilder(QuantLib::ext::shared_ptr<HwHistoricalCalibrationModelData>& data, const bool runPcaCalibration = true,
                                        const bool runMeanReversionCalibration = false, const bool continueOnError = false);

private:
    void performCalculations();
    void buildModel();
    void extractOutputs() const;

    QuantLib::ext::shared_ptr<HwHistoricalCalibrationModelData>& data_;
    std::unique_ptr<QuantExt::HwHistoricalCalibrationModel> model_;
    bool runPcaCalibration_;
    bool runMeanReversionCalibration_;
    bool continueOnError_;

    std::ostringstream qleLog_;
};

} // namespace data
} // namespace ore
