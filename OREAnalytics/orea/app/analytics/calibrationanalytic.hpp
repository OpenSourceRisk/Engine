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

/*! \file orea/app/analytics/calibrationanalytic.hpp
    \brief Calibrate the cross asset model and export resulting model parameters
*/

#pragma once

#include <orea/app/analytic.hpp>

namespace ore {
namespace analytics {

class CalibrationAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "CALIBRATION";

    explicit CalibrationAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

protected:
    QuantLib::ext::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildCrossAssetModel(bool continueOnError);

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory_;
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<CrossAssetModelBuilder> builder_;
};

static const std::set<std::string> calibrationAnalyticSubAnalytics{};

class CalibrationAnalytic : public Analytic {
public:
    explicit CalibrationAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<CalibrationAnalyticImpl>(inputs), calibrationAnalyticSubAnalytics, inputs, false,
                   false, false, false) {}
};

} // namespace analytics
} // namespace ore
