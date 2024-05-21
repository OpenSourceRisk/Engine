/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/utilities/log.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <boost/make_shared.hpp>
#include <boost/algorithm/string/join.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace data {

namespace {
std::string getParameter(const std::map<std::string, std::string>& m, const std::string& p,
                         const std::vector<std::string>& qs, const bool mandatory, const std::string& defaultValue) {
    // first look for p_q if one or several qualifiers are given
    for (auto const& q : qs) {
        if (!q.empty()) {
            auto r = m.find(p + "_" + q);
            if (r != m.end())
                return r->second;
        }
    }
    // no qualifier given, or fall back on p because p_q was not found
    auto r = m.find(p);
    if (r != m.end()) {
        return r->second;
    }
    // if parameter is mandatory throw, otherwise return the default value
    if (mandatory) {
        QL_FAIL("parameter " << p << " not found (qualifier list was \"" << boost::algorithm::join(qs, ", ") << "\")");
    }
    return defaultValue;
}
} // namespace

std::string EngineBuilder::engineParameter(const std::string& p, const std::vector<std::string>& qualifiers,
                                           const bool mandatory, const std::string& defaultValue) const {
    return getParameter(engineParameters_, p, qualifiers, mandatory, defaultValue);
}

std::string EngineBuilder::modelParameter(const std::string& p, const std::vector<std::string>& qualifiers,
                                          const bool mandatory, const std::string& defaultValue) const {
    return getParameter(modelParameters_, p, qualifiers, mandatory, defaultValue);
}

void EngineBuilderFactory::addEngineBuilder(const std::function<QuantLib::ext::shared_ptr<EngineBuilder>()>& builder,
                                            const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    auto tmp = builder();
    auto key = make_tuple(tmp->model(), tmp->engine(), tmp->tradeTypes());
    auto it = std::remove_if(engineBuilderBuilders_.begin(), engineBuilderBuilders_.end(),
                             [&key](std::function<QuantLib::ext::shared_ptr<EngineBuilder>()>& b) {
                                 auto tmp = b();
                                 return key == std::make_tuple(tmp->model(), tmp->engine(), tmp->tradeTypes());
                             });
    QL_REQUIRE(it == engineBuilderBuilders_.end() || allowOverwrite,
               "EngineBuilderFactory::addEngineBuilder(" << tmp->model() << "/" << tmp->engine() << "/"
                                                         << boost::algorithm::join(tmp->tradeTypes(), ",")
                                                         << "): builder for given key already exists.");
    engineBuilderBuilders_.erase(it, engineBuilderBuilders_.end());
    engineBuilderBuilders_.push_back(builder);
}

void EngineBuilderFactory::addAmcEngineBuilder(
    const std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                         const std::vector<Date>& grid)>& builder,
    const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    auto tmp = builder(nullptr, {});
    auto key = make_tuple(tmp->model(), tmp->engine(), tmp->tradeTypes());
    auto it = std::remove_if(
        amcEngineBuilderBuilders_.begin(), amcEngineBuilderBuilders_.end(),
        [&key](std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                              const std::vector<Date>& grid)>& b) {
            auto tmp = b(nullptr, {});
            return key == std::make_tuple(tmp->model(), tmp->engine(), tmp->tradeTypes());
        });
    QL_REQUIRE(it == amcEngineBuilderBuilders_.end() || allowOverwrite,
               "EngineBuilderFactory::addAmcEngineBuilder(" << tmp->model() << "/" << tmp->engine() << "/"
                                                            << boost::algorithm::join(tmp->tradeTypes(), ",")
                                                            << "): builder for given key already exists.");
    amcEngineBuilderBuilders_.erase(it, amcEngineBuilderBuilders_.end());
    amcEngineBuilderBuilders_.push_back(builder);
}

void EngineBuilderFactory::addAmcCgEngineBuilder(
    const std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model,
                                                         const std::vector<Date>& grid)>& builder,
    const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    auto tmp = builder(nullptr, {});
    auto key = make_tuple(tmp->model(), tmp->engine(), tmp->tradeTypes());
    auto it = std::remove_if(
        amcCgEngineBuilderBuilders_.begin(), amcCgEngineBuilderBuilders_.end(),
        [&key](std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model,
                                                              const std::vector<Date>& grid)>& b) {
            auto tmp = b(nullptr, {});
            return key == std::make_tuple(tmp->model(), tmp->engine(), tmp->tradeTypes());
        });
    QL_REQUIRE(it == amcCgEngineBuilderBuilders_.end() || allowOverwrite,
               "EngineBuilderFactory::addAmcCgEngineBuilder(" << tmp->model() << "/" << tmp->engine() << "/"
                                                              << boost::algorithm::join(tmp->tradeTypes(), ",")
                                                              << "): builder for given key already exists.");
    amcCgEngineBuilderBuilders_.erase(it, amcCgEngineBuilderBuilders_.end());
    amcCgEngineBuilderBuilders_.push_back(builder);
}

void EngineBuilderFactory::addLegBuilder(const std::function<QuantLib::ext::shared_ptr<LegBuilder>()>& builder,
                                         const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    auto key = builder()->legType();
    auto it =
        std::remove_if(legBuilderBuilders_.begin(), legBuilderBuilders_.end(),
                       [&key](std::function<QuantLib::ext::shared_ptr<LegBuilder>()>& b) { return key == b()->legType(); });
    QL_REQUIRE(it == legBuilderBuilders_.end() || allowOverwrite,
               "EngineBuilderFactory::addLegBuilder(" << key << "): builder for given key already exists.");
    legBuilderBuilders_.erase(it, legBuilderBuilders_.end());
    legBuilderBuilders_.push_back(builder);
}

std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> EngineBuilderFactory::generateEngineBuilders() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> result;
    for (auto const& b : engineBuilderBuilders_)
        result.push_back(b());
    return result;
}

std::vector<QuantLib::ext::shared_ptr<EngineBuilder>>
EngineBuilderFactory::generateAmcEngineBuilders(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                const std::vector<Date>& grid) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> result;
    for (auto const& b : amcEngineBuilderBuilders_)
        result.push_back(b(cam, grid));
    return result;
}

std::vector<QuantLib::ext::shared_ptr<EngineBuilder>>
EngineBuilderFactory::generateAmcCgEngineBuilders(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model,
                                                  const std::vector<Date>& grid) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> result;
    for (auto const& b : amcCgEngineBuilderBuilders_)
        result.push_back(b(model, grid));
    return result;
}

std::vector<QuantLib::ext::shared_ptr<LegBuilder>> EngineBuilderFactory::generateLegBuilders() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    std::vector<QuantLib::ext::shared_ptr<LegBuilder>> result;
    for (auto const& b : legBuilderBuilders_)
        result.push_back(b());
    return result;
}

EngineFactory::EngineFactory(const QuantLib::ext::shared_ptr<EngineData>& engineData, const QuantLib::ext::shared_ptr<Market>& market,
                             const map<MarketContext, string>& configurations,
                             const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                             const IborFallbackConfig& iborFallbackConfig,
                             const std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders,
                             const bool allowOverwrite)
    : market_(market), engineData_(engineData), configurations_(configurations), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig) {
    LOG("Building EngineFactory");

    addDefaultBuilders();
    addExtraBuilders(extraEngineBuilders, {}, allowOverwrite);
}

void EngineFactory::registerBuilder(const QuantLib::ext::shared_ptr<EngineBuilder>& builder, const bool allowOverwrite) {
    const string& modelName = builder->model();
    const string& engineName = builder->engine();
    auto key = make_tuple(modelName, engineName, builder->tradeTypes());
    if (allowOverwrite)
        builders_.erase(key);
    QL_REQUIRE(builders_.insert(make_pair(key, builder)).second,
               "EngineFactory: duplicate engine builder for (" << modelName << "/" << engineName << "/"
                                                               << boost::algorithm::join(builder->tradeTypes(), ",")
                                                               << ") - this is an internal error.");
}

QuantLib::ext::shared_ptr<EngineBuilder> EngineFactory::builder(const string& tradeType) {
    // Check that we have a model/engine for tradetype
    QL_REQUIRE(engineData_->hasProduct(tradeType),
               "No Pricing Engine configuration was provided for trade type " << tradeType);

    // Find a builder for the model/engine/tradeType
    const string& model = engineData_->model(tradeType);
    const string& engine = engineData_->engine(tradeType);
    typedef pair<tuple<string, string, set<string>>, QuantLib::ext::shared_ptr<EngineBuilder>> map_type;
    auto pred = [&model, &engine, &tradeType](const map_type& v) -> bool {
        const set<string>& types = std::get<2>(v.first);
        return std::get<0>(v.first) == model && std::get<1>(v.first) == engine &&
               std::find(types.begin(), types.end(), tradeType) != types.end();
    };
    auto it = std::find_if(builders_.begin(), builders_.end(), pred);
    QL_REQUIRE(it != builders_.end(), "No EngineBuilder for " << model << "/" << engine << "/" << tradeType);
    QL_REQUIRE(std::find_if(std::next(it, 1), builders_.end(), pred) == builders_.end(),
               "Ambiguous EngineBuilder for " << model << "/" << engine << "/" << tradeType);

    QuantLib::ext::shared_ptr<EngineBuilder> builder = it->second;
    string effectiveTradeType = tradeType;
    if(auto db = QuantLib::ext::dynamic_pointer_cast<DelegatingEngineBuilder>(builder))
	effectiveTradeType = db->effectiveTradeType();

    builder->init(market_, configurations_, engineData_->modelParameters(effectiveTradeType),
                  engineData_->engineParameters(effectiveTradeType), engineData_->globalParameters());

    return builder;
}

void EngineFactory::registerLegBuilder(const QuantLib::ext::shared_ptr<LegBuilder>& legBuilder, const bool allowOverwrite) {
    if (allowOverwrite)
        legBuilders_.erase(legBuilder->legType());
    QL_REQUIRE(legBuilders_.insert(make_pair(legBuilder->legType(), legBuilder)).second,
               "EngineFactory duplicate leg builder for '" << legBuilder->legType()
                                                           << "' - this is an internal error.");
}

QuantLib::ext::shared_ptr<LegBuilder> EngineFactory::legBuilder(const string& legType) {
    auto it = legBuilders_.find(legType);
    QL_REQUIRE(it != legBuilders_.end(), "No LegBuilder for " << legType);
    return it->second;
}

set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>> EngineFactory::modelBuilders() const {
    set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>> res;
    for (auto const& b : builders_) {
        res.insert(b.second->modelBuilders().begin(), b.second->modelBuilders().end());
    }
    return res;
}

void EngineFactory::addDefaultBuilders() {
    for(auto const& b: EngineBuilderFactory::instance().generateEngineBuilders())
        registerBuilder(b);
    for(auto const& b: EngineBuilderFactory::instance().generateLegBuilders())
        registerLegBuilder(b);
}

void EngineFactory::addExtraBuilders(const std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders,
                                     const std::vector<QuantLib::ext::shared_ptr<LegBuilder>> extraLegBuilders,
                                     const bool allowOverwrite) {

    if (extraEngineBuilders.size() > 0) {
        DLOG("adding " << extraEngineBuilders.size() << " extra engine builders");
        for (auto eb : extraEngineBuilders)
            registerBuilder(eb, allowOverwrite);
    }
    if (extraLegBuilders.size() > 0) {
        DLOG("adding " << extraLegBuilders.size() << " extra leg builders");
        for (auto elb : extraLegBuilders)
            registerLegBuilder(elb, allowOverwrite);
    }
}

} // namespace data
} // namespace ore
