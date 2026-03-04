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
#include <orea/app/inputvariables.hpp>

namespace ore {
namespace data {
class CrossAssetModelBuilder;
class HwHistoricalCalibrationModelData;
class HwHistoricalCalibrationModelBuilder;
}; // namespace data
}; // namespace ore

namespace ore {
namespace analytics {

class InputParameters;;

struct CalibrationVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;

    QuantLib::ext::shared_ptr<ore::data::EngineData> pricingEngine_;
    QuantLib::ext::shared_ptr<ore::data::CrossAssetModelData> crossAssetModelData_;

    std::string calibrationModel_;
    std::string hwCalibrationMode_;
    bool pcaCalibration_ = false;
    bool meanReversionCalibration_ = false;
    std::vector<std::string> foreignCurrencies_;
    std::vector<QuantLib::Period> curveTenors_;
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    std::string scenarioInputFile_;
    bool useForwardRate_ = false;
    QuantLib::Real lambda_ = 1.0;
    std::vector<std::string> pcaInputFiles_;
    QuantLib::Real varianceRetained_ = 0.0;
    QuantLib::Size basisFunctionNumber_ = 0;
    QuantLib::Real kappaUpperBound_ = 0.0;
    QuantLib::Size haltonMaxGuess_ = 0;
    std::string pcaOutputFileName_;
    std::string meanReversionOutputFileName_;
};

class CalibrationAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "CALIBRATION";

    explicit CalibrationAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs, QuantLib::ext::make_shared<CalibrationVariables>()) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

protected:
    QuantLib::ext::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildCrossAssetModel(bool continueOnError, bool allowModelFallbacks);
    void buildHwHistoricalCalibrationModelData();

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory_;
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<ore::data::CrossAssetModelBuilder> builder_;
    QuantLib::ext::shared_ptr<ore::data::HwHistoricalCalibrationModelData> hwHistoricalModelData_;
    QuantLib::ext::shared_ptr<ore::data::HwHistoricalCalibrationModelBuilder> hwHistoricalModelBuilder_;
};

static const std::set<std::string> calibrationAnalyticSubAnalytics{};

class CalibrationAnalytic : public Analytic {
public:
    explicit CalibrationAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                 const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<CalibrationAnalyticImpl>(inputs), calibrationAnalyticSubAnalytics, inputs, analyticsManager, false,
                   false, false, false) {}
    bool requiresMarketData() const override;
};

} // namespace analytics
} // namespace ore
