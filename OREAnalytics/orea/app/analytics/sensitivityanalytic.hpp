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

/*! \file orea/app/analytics/sensitivityanalytic.hpp
    \brief ORE Sensitivity Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
namespace ore {
namespace analytics {
class SensitivityAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SENSITIVITY";

    SensitivityAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }

    void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;

    void setUpConfigurations() override;
};

class SensitivityAnalytic : public Analytic {
public:
    SensitivityAnalytic(const boost::shared_ptr<InputParameters>& inputs, boost::optional<bool> parSensiRun = {},
                        boost::optional<bool> alignPillars = {}, boost::optional<bool> outputJacobi = {},
                        boost::optional<bool> optimiseRiskFactors = {})
        : Analytic(std::make_unique<SensitivityAnalyticImpl>(inputs), {"SENSITIVITY"}, inputs, false, false, false,
                   false),
          parSensi_(parSensiRun.get_value_or(inputs->parSensi())),
          alignPillars_(alignPillars.get_value_or(inputs->alignPillars())),
          outputJacobi_(outputJacobi.get_value_or(inputs->outputJacobi())),
          optimiseRiskFactors_(optimiseRiskFactors.get_value_or(inputs->optimiseRiskFactors())) {}

    const ParSensitivityAnalysis::ParContainer& parSensitivities() const { return parSensitivities_; }
    void setParSensitivities(const ParSensitivityAnalysis::ParContainer& sensitivities) {
        parSensitivities_ = sensitivities;
    }

    bool alignPillars() const { return alignPillars_; }
    bool parSensi() const { return parSensi_; }
    bool outputJacobi() const { return outputJacobi_; }
    bool optimiseRiskFactors() const { return optimiseRiskFactors_; }
private:
    bool parSensi_;
    bool alignPillars_;
    bool outputJacobi_;
    bool optimiseRiskFactors_;
    ParSensitivityAnalysis::ParContainer parSensitivities_;
};

} // namespace analytics
} // namespace ore
