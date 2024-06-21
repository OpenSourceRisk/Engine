/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
 Copyright (C) 2023 Oleg Kulkov
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

#include <ored/portfolio/tradefactory.hpp>

#include <ql/errors.hpp>

using namespace std;

namespace ore {
namespace data {

std::map<std::string, QuantLib::ext::shared_ptr<AbstractTradeBuilder>> TradeFactory::getBuilders() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return builders_;
}

QuantLib::ext::shared_ptr<AbstractTradeBuilder> TradeFactory::getBuilder(const std::string& className) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto b = builders_.find(className);
    QL_REQUIRE(b != builders_.end(), "TradeFactory::getBuilder(" << className << "): no builder found");
    return b->second;
}

void TradeFactory::addBuilder(const std::string& className, const QuantLib::ext::shared_ptr<AbstractTradeBuilder>& builder,
                              const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    {
        auto [it, ins] = builders_.try_emplace(className, builder);
        if (!ins) QL_REQUIRE(allowOverwrite && (it->second = builder),
                       "TradeFactory: duplicate builder for className '" << className << "'.");
    }
}

QuantLib::ext::shared_ptr<Trade> TradeFactory::build(const string& className) const { return getBuilder(className)->build(); }

} // namespace data
} // namespace ore
