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

/*! \file orea/app/analytics/sacvaanalytic.hpp
    \brief Analytic for "Standard Approach CVA Capital" (SA-CCA) calculation SA-CVA
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/xvasensitivityanalytic.hpp>

namespace ore {
namespace analytics {

class SaCvaAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SA_CVA";
    static constexpr const char* sensiLookupKey = "SENSI";

    SaCvaAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);

        auto sensiAnalytic = AnalyticFactory::instance().build("XVA_SENSITIVITY", inputs_);
        if (sensiAnalytic.second)
            addDependentAnalytic(sensiLookupKey, sensiAnalytic.second);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

// static const std::set<std::string> sacvaAnalyticSubAnalytics {"SA_CVA", "XVA_SENSITIVITY"};

class SaCvaAnalytic : public Analytic {
public:
    SaCvaAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                  const QuantLib::ext::shared_ptr<Scenario>& offSetScenario = nullptr,
                  const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : Analytic(std::make_unique<SaCvaAnalyticImpl>(inputs), {"SA_CVA"}, inputs, true, true) {}
};

} // namespace analytics
} // namespace ore
