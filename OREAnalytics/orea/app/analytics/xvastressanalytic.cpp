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

#include <orea/app/analytics/xvastressanalytic.hpp>

namespace ore {
namespace analytics {

XvaStressAnalyticImpl::XvaStressAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
}

void XvaStressAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                        const std::set<std::string>& runTypes) {}

void XvaStressAnalyticImpl::setUpConfigurations() {}

void XvaStressAnalyticImpl::buildScenarioSimMarket() {}

void XvaStressAnalyticImpl::buildCrossAssetModel(bool continueOnError) {}

void XvaStressAnalyticImpl::buildScenarioGenerator(bool continueOnError) {}

XvaStressAnalytic::XvaStressAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
    : Analytic(std::make_unique<XvaStressAnalyticImpl>(inputs), {"XVA_STRESS"}, inputs, true, false, true, true) {}

} // namespace analytics
} // namespace ore
