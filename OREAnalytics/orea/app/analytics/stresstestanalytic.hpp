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

/*! \file orea/app/analytics/stresstestanalytic.hpp
    \brief ORE Stresstest Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/scenario/stressscenariodata.hpp>
namespace ore {
namespace analytics {
class StressTestAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "STRESS";

    StressTestAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }

    void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;

    void setUpConfigurations() override;
};

class StressTestAnalytic : public Analytic {
public:
    StressTestAnalytic(const boost::shared_ptr<InputParameters>& inputs);
};

} // namespace analytics
} // namespace ore
