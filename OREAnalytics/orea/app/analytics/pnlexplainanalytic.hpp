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
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/pnlanalytic.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>

namespace ore {
namespace analytics {

class PnlExplainAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "PNL_EXPLAIN";
    static constexpr const char* sensiLookupKey = "SENSI";
    static constexpr const char* pnlLookupKey = "PNL";
    PnlExplainAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs) {
        setLabel(LABEL);
        
        auto sensiAnalytic = AnalyticFactory::instance().build("SENSITIVITY", inputs_);
        if (sensiAnalytic.second)
            addDependentAnalytic(sensiLookupKey, sensiAnalytic.second);

        auto pnlAnalytic = AnalyticFactory::instance().build("PNL", inputs_);
        if (pnlAnalytic.second)
            addDependentAnalytic(pnlLookupKey, pnlAnalytic.second);
    }

    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    virtual void setUpConfigurations() override;
};

class PnlExplainAnalytic : public Analytic {
public:
    PnlExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<PnlExplainAnalyticImpl>(inputs), {"PNL_EXPLAIN"}, inputs, true, true) {

    }
};

} // namespace analytics
} // namespace ore
