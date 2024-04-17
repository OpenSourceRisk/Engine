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

/*! \file orea/app/pnlexplainanalytic.hpp
    \brief ORE Analytics Manager
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>

namespace ore {
namespace analytics {

class PNLExplainAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "PNL_EXPLAIN";
    PNLExplainAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }

    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    virtual void setUpConfigurations() override;
};

class PNLExplainAnalytic : public Analytic {
public:
    static constexpr const char* sensiLookupKey = "SENSI";
    PNLExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<PNLExplainAnalyticImpl>(inputs), {"PNL_EXPLAIN"}, inputs, true, true) {
        auto sensiAnalytic = boost::make_shared<PricingAnalytic>(inputs_);
        addDependentAnalytic(sensiLookupKey, sensiAnalytic);
    }
};

} // namespace analytics
} // namespace ore
