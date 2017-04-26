/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/all.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <boost/make_shared.hpp>

#include <tuple>

using std::get;

namespace ore {
namespace data {

EngineFactory::EngineFactory(const boost::shared_ptr<EngineData>& engineData, const boost::shared_ptr<Market>& market,
                             const map<MarketContext, string>& configurations)
    : market_(market), engineData_(engineData), configurations_(configurations) {
    LOG("Building EngineFactory");

    addDefaultBuilders();
}

void EngineFactory::registerBuilder(const boost::shared_ptr<EngineBuilder>& builder) {
    const string& modelName = builder->model();
    const string& engineName = builder->engine();
    const set<string>& tradeTypes = builder->tradeTypes();
    LOG("EngineFactory registering builder for model:" << modelName << " and engine:" << engineName);
    for (auto const& t : tradeTypes) {
        LOG("Possible Trade Type: " << t);
    }
    builders_[make_tuple(modelName, engineName, tradeTypes)] = builder;
}

boost::shared_ptr<EngineBuilder> EngineFactory::builder(const string& tradeType) {
    // Check that we have a model/engine for tradetype
    QL_REQUIRE(engineData_->hasProduct(tradeType),
               "EngineFactory does not have a model/engine for trade type " << tradeType);

    // Find a builder for the model/engine/tradetype tuple (if there are several, the first is taken)
    auto key = make_tuple(engineData_->model(tradeType), engineData_->engine(tradeType), tradeType);
    auto it = std::find_if(builders_.begin(), builders_.end(),
                           [&key](const pair<tuple<string, string, set<string>>, boost::shared_ptr<EngineBuilder>>& k) {
                               return get<0>(k.first) == get<0>(key) && get<1>(k.first) == get<1>(key) &&
                                      (std::find(get<2>(k.first).begin(), get<2>(k.first).end(), get<2>(key)) !=
                                       get<2>(k.first).end());
                           });
    QL_REQUIRE(it != builders_.end(),
               "No EngineBuilder for " << get<0>(key) << "/" << get<1>(key) << "/" << get<2>(key));

    boost::shared_ptr<EngineBuilder> builder = it->second;

    builder->init(market_, configurations_, engineData_->modelParameters(tradeType),
                  engineData_->engineParameters(tradeType));

    return builder;
}

void EngineFactory::addDefaultBuilders() {
    registerBuilder(boost::make_shared<SwapEngineBuilder>());
    registerBuilder(boost::make_shared<SwapEngineBuilderOptimised>());
    registerBuilder(boost::make_shared<CrossCurrencySwapEngineBuilder>());

    registerBuilder(boost::make_shared<EuropeanSwaptionEngineBuilder>());
    registerBuilder(boost::make_shared<LGMGridBermudanSwaptionEngineBuilder>());

    registerBuilder(boost::make_shared<FxForwardEngineBuilder>());
    registerBuilder(boost::make_shared<FxOptionEngineBuilder>());

    registerBuilder(boost::make_shared<CapFloorEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredIborLegEngineBuilder>());

    registerBuilder(boost::make_shared<EquityForwardEngineBuilder>());
    registerBuilder(boost::make_shared<EquityOptionEngineBuilder>());

    registerBuilder(boost::make_shared<BondDiscountingEngineBuilder>());
}

} // namespace data
} // namespace ore
