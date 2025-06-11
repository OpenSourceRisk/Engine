/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/correlationanalytic.hpp
    \brief Generate correlations from a set of historical scenarios
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/engine/correlationreport.hpp>

namespace ore {
namespace analytics {

class CorrelationAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "CORRELATION";
    static constexpr const char* sensiLookupKey = "SENSI";
    CorrelationAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void buildDependencies() override;
    virtual QuantLib::ext::shared_ptr<SensitivityStream>sensiStream(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
        return inputs_->sensitivityStream();
    };
        
protected:
    QuantLib::ext::shared_ptr<CorrelationReport> correlationReport_;
    void setCorrelationReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);
};


class CorrelationAnalytic : public Analytic {
public:
    CorrelationAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                 const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<CorrelationAnalyticImpl>(inputs), {"CORRELATION"}, inputs,
                   analyticsManager) {}
};

} // namespace analytics
} // namespace ore
