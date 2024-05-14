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

/*! \file ored/portfolio/enginefactory.hpp
    \brief Pricing Engine Factory
    \ingroup tradedata
*/

#pragma once

#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/scripting/models/modelcg.hpp>

#include <qle/models/modelbuilder.hpp>

#include <ql/pricingengine.hpp>

#include <ql/shared_ptr.hpp>

#include <map>
#include <set>
#include <vector>

namespace ore {
namespace data {
using ore::data::Market;
using QuantLib::PricingEngine;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::tuple;

class Trade;
class LegBuilder;
class ReferenceDataManager;

/*! Market configuration contexts. Note that there is only one pricing context.
  If several are needed (for different trade types, different collateral
  currencies etc.), several engine factories should be set up for each such
  portfolio subset. */
enum class MarketContext { irCalibration, fxCalibration, eqCalibration, pricing };

//! Base PricingEngine Builder class for a specific model and engine
/*!
 *  The EngineBuilder is responsible for building pricing engines for a specific
 *  Model and Engine.
 *
 *  Each builder should implement a method with a signature
 *  @code
 *  QuantLib::ext::shared_ptr<PricingEngine> engine (...);
 *  @endcode
 *  The exact parameters of each method can vary depending on the type of engine.
 *
 *  An EngineBuilder can cache engines and return the same PricingEngine multiple
 *  times, alternatively the Builder can build a unique PricingEngine each time it
 *  is called.
 *
 *  For example a swap engine builder can have the interface
 *  @code
 *  QuantLib::ext::shared_ptr<PricingEngine> engine (const Currency&);
 *  @endcode
 *  and so returns the same (cached) engine every time it is asked for a particular
 *  currency.
 *
 *  The interface of each type of engine builder can be different, then there can
 *  be further sub-classes for different models and engines.
 *
 *  EngineBuilders are registered in an EngineFactory, multiple engine builders for
 *  the same trade type can be registered with the EngineFactory and it will select
 *  the appropriate one based on configuration.
 *
 *  Each EngineBuilder must return it's Model and Engine.

    \ingroup tradedata
 */
class EngineBuilder {
public:
    /*! Constructor that takes a model and engine name
     *  @param model the model name
     *  @param engine the engine name
     *  @param tradeTypes a set of trade types
     */
    EngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes)
        : model_(model), engine_(engine), tradeTypes_(tradeTypes) {}

    //! Virtual destructor
    virtual ~EngineBuilder() {}

    //! Return the model name
    const string& model() const { return model_; }
    //! Return the engine name
    const string& engine() const { return engine_; }
    //! Return the possible trade types
    const set<string>& tradeTypes() const { return tradeTypes_; }

    //! Return a configuration (or the default one if key not found)
    const string& configuration(const MarketContext& key) {
        if (configurations_.count(key) > 0) {
            return configurations_.at(key);
        } else {
            return Market::defaultConfiguration;
        }
    }

    //! reset the builder (e.g. clear cache)
    virtual void reset() {}

    //! Initialise this Builder with the market and parameters to use
    /*! This method should not be called directly, it is called by the EngineFactory
     *  before it is returned.
     */
    void init(const QuantLib::ext::shared_ptr<Market> market, const map<MarketContext, string>& configurations,
              const map<string, string>& modelParameters, const map<string, string>& engineParameters,
              const std::map<std::string, std::string>& globalParameters = {}) {
        market_ = market;
        configurations_ = configurations;
        modelParameters_ = modelParameters;
        engineParameters_ = engineParameters;
        globalParameters_ = globalParameters;
    }

    //! return model builders
    const set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>& modelBuilders() const { return modelBuilders_; }

    /*! retrieve engine parameter p, first look for p_qualifier, if this does not exist fall back to p */
    std::string engineParameter(const std::string& p, const std::vector<std::string>& qualifiers = {},
                                const bool mandatory = true, const std::string& defaultValue = "") const;
    /*! retrieve model parameter p, first look for p_qualifier, if this does not exist fall back to p */
    std::string modelParameter(const std::string& p, const std::vector<std::string>& qualifiers = {},
                               const bool mandatory = true, const std::string& defaultValue = "") const;

protected:
    string model_;
    string engine_;
    set<string> tradeTypes_;
    QuantLib::ext::shared_ptr<Market> market_;
    map<MarketContext, string> configurations_;
    map<string, string> modelParameters_;
    map<string, string> engineParameters_;
    std::map<std::string, std::string> globalParameters_;
    set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>> modelBuilders_;
};

//! Delegating Engine Builder
/* Special interface to consolidate different trade builders for one product. See AsianOption for a use case. */
class DelegatingEngineBuilder : public EngineBuilder {
public:
    using EngineBuilder::EngineBuilder;
    virtual QuantLib::ext::shared_ptr<ore::data::Trade> build(const ore::data::Trade*,
                                                      const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) = 0;
    virtual std::string effectiveTradeType() const = 0;
};

//! Engine/ Leg Builder Factory - notice that both engine and leg builders are allowed to maintain a state
class EngineBuilderFactory : public QuantLib::Singleton<EngineBuilderFactory, std::integral_constant<bool, true>> {
    std::vector<std::function<QuantLib::ext::shared_ptr<EngineBuilder>()>> engineBuilderBuilders_;
    std::vector<std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                               const std::vector<Date>& grid)>>
        amcEngineBuilderBuilders_;
    std::vector<std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model,
                                                               const std::vector<Date>& grid)>>
        amcCgEngineBuilderBuilders_;
    std::vector<std::function<QuantLib::ext::shared_ptr<LegBuilder>()>> legBuilderBuilders_;
    mutable boost::shared_mutex mutex_;

public:
    void addEngineBuilder(const std::function<QuantLib::ext::shared_ptr<EngineBuilder>()>& builder,
                          const bool allowOverwrite = false);
    void addAmcEngineBuilder(
        const std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                             const std::vector<Date>& grid)>& builder,
        const bool allowOverwrite = false);
    void addAmcCgEngineBuilder(
        const std::function<QuantLib::ext::shared_ptr<EngineBuilder>(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model,
                                                             const std::vector<Date>& grid)>& builder,
        const bool allowOverwrite = false);
    void addLegBuilder(const std::function<QuantLib::ext::shared_ptr<LegBuilder>()>& builder,
                       const bool allowOverwrite = false);

    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> generateEngineBuilders() const;
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>>
    generateAmcEngineBuilders(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                              const std::vector<Date>& grid) const;
    std::vector<QuantLib::ext::shared_ptr<EngineBuilder>>
    generateAmcCgEngineBuilders(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model,
                                const std::vector<Date>& grid) const;
    std::vector<QuantLib::ext::shared_ptr<LegBuilder>> generateLegBuilders() const;
};

//! Pricing Engine Factory class
/*! A Pricing Engine Factory is used when building a portfolio, it provides
 *  QuantLib::PricingEngines to each of the Trade objects.
 *
 *  An Engine Factory is configured on two levels, both from EngineData.
 *  The first level is the type of builder (Model and Engine) to use for each
 *  trade type, so given a trade type one asks the factory for a builder and
 *  then the builder for a PricingEngine. Each builder must be registered with
 *  the factory and then the configuration defines which builder to use.
 *
 *  Secondly, the factory maintains builder specific parameters for each Model
 *  and Engine.

    \ingroup tradedata
 */
class EngineFactory {
public:
    //! Create an engine factory
    EngineFactory( //! Configuration data
        const QuantLib::ext::shared_ptr<EngineData>& data,
        //! The market that is passed to each builder
        const QuantLib::ext::shared_ptr<Market>& market,
        //! The market configurations that are passed to each builder
        const map<MarketContext, string>& configurations = std::map<MarketContext, string>(),
        //! reference data
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
        //! ibor fallback config
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
        //! additional engine builders
        const std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders = {},
        //! additional engine builders may overwrite existing builders with same key
        const bool allowOverwrite = false);

    //! Return the market used by this EngineFactory
    const QuantLib::ext::shared_ptr<Market>& market() const { return market_; };
    //! Return the market configurations used by this EngineFactory
    const map<MarketContext, string>& configurations() const { return configurations_; };
    //! Return a configuration (or the default one if key not found)
    const string& configuration(const MarketContext& key) {
        if (configurations_.count(key) > 0) {
            return configurations_.at(key);
        } else {
            return Market::defaultConfiguration;
        }
    }
    //! Return the EngineData parameters
    const QuantLib::ext::shared_ptr<EngineData> engineData() const { return engineData_; };
    //! Register a builder with the factory
    void registerBuilder(const QuantLib::ext::shared_ptr<EngineBuilder>& builder, const bool allowOverwrite = false);
    //! Return the reference data used by this EngineFactory
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData() const { return referenceData_; };
    //! Return the ibor fallback config
    const IborFallbackConfig& iborFallbackConfig() const { return iborFallbackConfig_; }

    //! Get a builder by trade type
    /*! This will look up configured model/engine for that trade type
        the returned builder can be cast to the type required for the tradeType.

        The factory will call EngineBuilder::init() before returning it.
     */
    QuantLib::ext::shared_ptr<EngineBuilder> builder(const string& tradeType);

    //! Register a leg builder with the factory
    void registerLegBuilder(const QuantLib::ext::shared_ptr<LegBuilder>& legBuilder, const bool allowOverwrite = false);

    //! Get a leg builder by leg type
    QuantLib::ext::shared_ptr<LegBuilder> legBuilder(const string& legType);

    //! Add a set of default engine and leg builders
    void addDefaultBuilders();
    //! Add a set of default engine and leg builders, overwrite existing builders with same key if specified
    void addExtraBuilders(const std::vector<QuantLib::ext::shared_ptr<EngineBuilder>> extraEngineBuilders,
                          const std::vector<QuantLib::ext::shared_ptr<LegBuilder>> extraLegBuilders,
                          const bool allowOverwrite = false);

    //! Clear all builders
    void clear() {
        builders_.clear();
        legBuilders_.clear();
    }

    //! return model builders
    set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>> modelBuilders() const;

private:
    QuantLib::ext::shared_ptr<Market> market_;
    QuantLib::ext::shared_ptr<EngineData> engineData_;
    map<MarketContext, string> configurations_;
    map<tuple<string, string, set<string>>, QuantLib::ext::shared_ptr<EngineBuilder>> builders_;
    map<string, QuantLib::ext::shared_ptr<LegBuilder>> legBuilders_;
    QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
};

//! Leg builder
class RequiredFixings;
class LegBuilder {
public:
    LegBuilder(const string& legType) : legType_(legType) {}
    virtual ~LegBuilder() {}
    virtual Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>&, RequiredFixings& requiredFixings,
                         const string& configuration, const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                         const bool useXbsCurves = false) const = 0;
    const string& legType() const { return legType_; }

private:
    const string legType_;
};

} // namespace data
} // namespace ore
