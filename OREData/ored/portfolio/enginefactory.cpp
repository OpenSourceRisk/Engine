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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/all.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/legbuilders.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

EngineFactory::EngineFactory(const boost::shared_ptr<EngineData>& engineData, const boost::shared_ptr<Market>& market,
                             const map<MarketContext, string>& configurations,
                             const std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders,
                             const std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders)
    : market_(market), engineData_(engineData), configurations_(configurations) {
    LOG("Building EngineFactory");

    addDefaultBuilders();
    addExtraBuilders(extraEngineBuilders, extraLegBuilders);
}

void EngineFactory::registerBuilder(const boost::shared_ptr<EngineBuilder>& builder) {
    const string& modelName = builder->model();
    const string& engineName = builder->engine();
    LOG("EngineFactory registering builder for model:" << modelName << " and engine:" << engineName);
    builders_[make_tuple(modelName, engineName, builder->tradeTypes())] = builder;
}

boost::shared_ptr<EngineBuilder> EngineFactory::builder(const string& tradeType) {
    // Check that we have a model/engine for tradetype
    QL_REQUIRE(engineData_->hasProduct(tradeType), "EngineFactory does not have a "
                                                   "model/engine for trade type "
                                                       << tradeType);

    // Find a builder for the model/engine/tradeType
    const string& model = engineData_->model(tradeType);
    const string& engine = engineData_->engine(tradeType);
    typedef pair<tuple<string, string, set<string>>, boost::shared_ptr<EngineBuilder>> map_type;
    auto it =
        std::find_if(builders_.begin(), builders_.end(), [&model, &engine, &tradeType](const map_type& v) -> bool {
            const set<string>& types = std::get<2>(v.first);
            return std::get<0>(v.first) == model && std::get<1>(v.first) == engine &&
                   std::find(types.begin(), types.end(), tradeType) != types.end();
        });
    QL_REQUIRE(it != builders_.end(), "No EngineBuilder for " << model << "/" << engine << "/" << tradeType);

    boost::shared_ptr<EngineBuilder> builder = it->second;

    builder->init(market_, configurations_, engineData_->modelParameters(tradeType),
                  engineData_->engineParameters(tradeType));

    return builder;
}

void EngineFactory::registerLegBuilder(const boost::shared_ptr<LegBuilder>& legBuilder) {
    LOG("EngineFactory registering builder for leg type " << legBuilder->legType());
    legBuilders_[legBuilder->legType()] = legBuilder;
}

boost::shared_ptr<LegBuilder> EngineFactory::legBuilder(const string& legType) {
    auto it = legBuilders_.find(legType);
    QL_REQUIRE(it != legBuilders_.end(), "No LegBuilder for " << legType);
    return it->second;
}

Disposable<set<std::pair<string, boost::shared_ptr<ModelBuilder>>>> EngineFactory::modelBuilders() const {
    set<std::pair<string, boost::shared_ptr<ModelBuilder>>> res;
    for (auto const& b : builders_) {
        res.insert(b.second->modelBuilders().begin(), b.second->modelBuilders().end());
    }
    return res;
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
    registerBuilder(boost::make_shared<CmsSpreadCouponPricerBuilder>());

    registerBuilder(boost::make_shared<YoYCapFloorEngineBuilder>());

    registerBuilder(boost::make_shared<EquityForwardEngineBuilder>());
    registerBuilder(boost::make_shared<EquityOptionEngineBuilder>());

    registerBuilder(boost::make_shared<BondDiscountingEngineBuilder>());

    registerBuilder(boost::make_shared<AnalyticHaganCmsCouponPricerBuilder>());
    registerBuilder(boost::make_shared<NumericalHaganCmsCouponPricerBuilder>());
    registerBuilder(boost::make_shared<LinearTSRCmsCouponPricerBuilder>());

    registerBuilder(boost::make_shared<MidPointCdsEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityForwardEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityOptionEngineBuilder>());

    registerLegBuilder(boost::make_shared<FixedLegBuilder>());
    registerLegBuilder(boost::make_shared<FloatingLegBuilder>());
    registerLegBuilder(boost::make_shared<CashflowLegBuilder>());
    registerLegBuilder(boost::make_shared<CPILegBuilder>());
    registerLegBuilder(boost::make_shared<YYLegBuilder>());
    registerLegBuilder(boost::make_shared<CMSLegBuilder>());
    registerLegBuilder(boost::make_shared<CMSSpreadLegBuilder>());
    registerLegBuilder(boost::make_shared<EquityLegBuilder>());
}


void EngineFactory::addExtraBuilders(const std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders,
    const std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders) {

    if (extraEngineBuilders.size() > 0) {
        LOG("adding " << extraEngineBuilders.size() << " extra engine builders");
        for (auto eb : extraEngineBuilders)
            registerBuilder(eb);
    }
    if (extraLegBuilders.size() > 0) {
        LOG("adding " << extraLegBuilders.size() << " extra leg builders");
        for (auto elb : extraLegBuilders)
            registerLegBuilder(elb);
    }
}

} // namespace data
} // namespace ore
