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

#include <orea/app/analytics/analyticfactory.hpp>

#include <ql/errors.hpp>

using namespace std;

namespace ore {
namespace analytics {

std::map<std::string, std::pair<std::set<std::string>, QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>>> AnalyticFactory::getBuilders() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return builders_;
}

std::pair<std::string,
    QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>> AnalyticFactory::getBuilder(const std::string& analyticName) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    // check if matching main analytic
    auto b = builders_.find(analyticName);
    if (b != builders_.end())
        return std::make_pair(b->first, b->second.second);
    
    // then check subanalytics
    for (const auto& ba : builders_) {
        if (ba.second.first.size() > 0) {
            auto sa = ba.second.first.find(analyticName);
            if (sa != ba.second.first.end())
                return std::make_pair(ba.first, ba.second.second);
        }
    }
    WLOG("AnalyticFactory::getBuilder(" << analyticName << "): no builder found");
    return std::make_pair(analyticName, nullptr);
}

void AnalyticFactory::addBuilder(const std::string& className, const std::set<std::string>& subAnalytics,
    const QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>& builder, const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(builders_.insert(std::make_pair(className, std::make_pair(subAnalytics, builder))).second ||
                   allowOverwrite,
               "AnalyticFactory: duplicate builder for className '" << className << "'.");
}

std::pair<std::string, QuantLib::ext::shared_ptr<Analytic>> AnalyticFactory::build(const string& subAnalytic,
    const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) const {
    auto builder = getBuilder(subAnalytic);
    QuantLib::ext::shared_ptr<Analytic> a;
    if (builder.second)
        a = builder.second->build(inputs);
    return std::make_pair(builder.first, a);
}

} // namespace analytics
} // namespace ore
