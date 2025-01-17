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

/*! \file orea/app/analytics/saccranalytic.hpp
    \brief ORE SACCR Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>
#include <orea/engine/saccr.hpp>

namespace ore {
namespace analytics {

class SaCcrAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SA_CCR";

    SaCcrAnalyticImpl(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs);

    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

class SaCcrAnalytic : public Analytic {
public:
    SaCcrAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs,
                  const QuantLib::ext::shared_ptr<ore::analytics::AnalyticsManager>& analyticsManager = nullptr)
        : Analytic(std::make_unique<SaCcrAnalyticImpl>(inputs), {"SA_CCR"}, inputs, analyticsManager) {}

    const QuantLib::ext::shared_ptr<SACCR> saccr() const { return saccr_; }
    void setSaccr(QuantLib::ext::shared_ptr<SACCR> saccr) { saccr_ = saccr; }

private:
    QuantLib::ext::shared_ptr<SACCR> saccr_;
};

} // namespace analytics
} // namespace ore
