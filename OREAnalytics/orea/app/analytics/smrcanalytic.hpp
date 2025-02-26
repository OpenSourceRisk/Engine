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

/*! \file orea/app/analytics/smrcanalytic.hpp
    \brief ORE SMRC Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>

namespace ore {
namespace analytics {
    
class SmrcAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SMRC";

    SmrcAnalyticImpl(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs)
        : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    virtual void setUpConfigurations() override;
};

class SmrcAnalytic : public Analytic {
public:
    SmrcAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs,
                 const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<SmrcAnalyticImpl>(inputs), {"SMRC"}, inputs, analyticsManager) {}
};

} // namespace analytics
} // namespace ore
