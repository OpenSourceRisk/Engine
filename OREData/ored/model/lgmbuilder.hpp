/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/model/lgmbuilder.hpp
    \brief Build an lgm model
    \ingroup models
*/

#pragma once

#include <ored/model/irlgmdata.hpp>
#include <ored/model/irmodelbuilder.hpp>

#include <qle/models/lgm.hpp>

namespace ore {
namespace data {

//! Builder for a Linear Gauss Markov model component
class LgmBuilder : public IrModelBuilder {
public:
    LgmBuilder(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<IrLgmData>& data,
        const std::string& configuration = Market::defaultConfiguration, Real bootstrapTolerance = 0.001,
        const bool continueOnError = false, const std::string& referenceCalibrationGrid = "",
        const bool setCalibrationInfo = false, const std::string& id = "unknown",
        BlackCalibrationHelper::CalibrationErrorType calibrationErrorType = BlackCalibrationHelper::RelativePriceError,
        const bool allowChangingFallbacksUnderScenarios = false, const bool allowModelFallbacks = false);

private:
    void initParametrization() const override;
    void calibrate() const override;
    QuantLib::ext::shared_ptr<PricingEngine> getPricingEngine() const override;

    bool setCalibrationInfo_ = false;

    mutable bool parametrizationInitialized_ = false;
};

} // namespace data
} // namespace ore
