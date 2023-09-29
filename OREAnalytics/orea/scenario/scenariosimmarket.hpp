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

/*! \file scenario/scenariosimmarket.hpp
    \brief A Market class that can be updated by Scenarios
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>

#include <map>

namespace ore {
namespace analytics {
using namespace QuantLib;
using std::map;
using std::string;
using std::vector;

//! Map a yield curve type to a risk factor key type
RiskFactorKey::KeyType yieldCurveRiskFactor(const ore::data::YieldCurveType y);

//! A scenario filter can exclude certain key from updating the scenario
/*! Override this class with to provide custom filtering, by default all keys
 *  are allowed.
 */
class ScenarioFilter {
public:
    ScenarioFilter() {}
    virtual ~ScenarioFilter() {}

    //! Allow this key to be updated
    virtual bool allow(const RiskFactorKey& key) const { return true; }
};

//! Simulation Market updated with discrete scenarios
/*! If useSpreadedTermStructures is true, spreaded term structures over the initMarket for supported risk factors will
  be generated. This is used by the SensitivityScenarioGenerator.

  If cacheSimData is true, the scenario application is optimised. This requires that all scenarios are SimpleScenario
  instances with identical key structure in their data.

  If allowPartialScenarios is true, the check that all simData_ is touched by a scenario is disabled.
 */
class ScenarioSimMarket : public analytics::SimMarket {
public:
    //! Constructor
    explicit ScenarioSimMarket(const bool handlePseudoCurrencies) : SimMarket(handlePseudoCurrencies) {}

    ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                      const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
                      const std::string& configuration = Market::defaultConfiguration,
                      const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                      const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                      const bool continueOnError = false, const bool useSpreadedTermStructures = false,
                      const bool cacheSimData = false, const bool allowPartialScenarios = false,
                      const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                      const bool handlePseudoCurrencies = true);

    ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                      const boost::shared_ptr<ScenarioSimMarketParameters>& parameters,
                      const boost::shared_ptr<FixingManager>& fixingManager,
                      const std::string& configuration = Market::defaultConfiguration,
                      const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                      const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                      const bool continueOnError = false, const bool useSpreadedTermStructures = false,
                      const bool cacheSimData = false, const bool allowPartialScenarios = false,
                      const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                      const bool handlePseudoCurrencies = true);

    //! Set scenario generator
    virtual boost::shared_ptr<ScenarioGenerator>& scenarioGenerator() { return scenarioGenerator_; }
    //! Get scenario generator
    virtual const boost::shared_ptr<ScenarioGenerator>& scenarioGenerator() const { return scenarioGenerator_; }

    //! Set aggregation data
    virtual boost::shared_ptr<AggregationScenarioData>& aggregationScenarioData() { return asd_; }
    //! Get aggregation data
    virtual const boost::shared_ptr<AggregationScenarioData>& aggregationScenarioData() const { return asd_; }

    //! Set scenarioFilter
    virtual boost::shared_ptr<ScenarioFilter>& filter() { return filter_; }
    //! Get scenarioFilter
    virtual const boost::shared_ptr<ScenarioFilter>& filter() const { return filter_; }

    //! Update
    // virtual void update(const Date&) override;
    virtual void preUpdate() override;
    virtual void updateScenario(const Date&) override;
    virtual void updateDate(const Date&) override;
    virtual void postUpdate(const Date& d, bool withFixings) override;
    virtual void updateAsd(const Date&) override;

    //! Reset sim market to initial state
    virtual void reset() override;

    /*! Scenario representing the initial state of the market. If useSpreadedTermStructures = false, this scenario
      contains absolute values for all risk factor keys. If useSpreadedTermStructures = true, this scenario contains
      spread values for all risk factor keys which support spreaded term structures and absolute values for the other
      risk factor keys. The spread values will typically be zero (e.g. for vol risk factors) or 1 (e.g. for rate curve
      risk factors, since we use discount factors there). */
    virtual boost::shared_ptr<Scenario> baseScenario() const { return baseScenario_; }

    /*! Scenario representing the initial state of the market. This scenario contains absolute values for all risk factor
      types, no matter whether useSpreadedTermStructures is true or false. */
    virtual boost::shared_ptr<Scenario> baseScenarioAbsolute() const { return baseScenarioAbsolute_; }

    //! Return the fixing manager
    const boost::shared_ptr<FixingManager>& fixingManager() const override { return fixingManager_; }

    //! is risk factor key simulated by this sim market instance?
    virtual bool isSimulated(const RiskFactorKey::KeyType& factor) const;

protected:
    void applyScenario(const boost::shared_ptr<Scenario>& scenario);

    void writeSimData(std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>>& simDataTmp,
                      std::map<RiskFactorKey, Real>& absoluteSimDataTmp);

    void addYieldCurve(const boost::shared_ptr<Market>& initMarket, const std::string& configuration,
                       const RiskFactorKey::KeyType rf, const string& key, const vector<Period>& tenors,
                       bool& simDataWritten, bool simulate = true, bool spreaded = false);

    /*! Given a yield curve spec ID, \p yieldSpecId, return the corresponding yield term structure
    from the \p market. If \p market is `nullptr`, then the yield term structure is taken from
    this ScenarioSimMarket instance.
    */
    QuantLib::Handle<QuantLib::YieldTermStructure>
    getYieldCurve(const std::string& yieldSpecId, const ore::data::TodaysMarketParameters& todaysMarketParams,
                  const std::string& configuration, const boost::shared_ptr<ore::data::Market>& market = nullptr) const;

    /*! add a single swap index to the market, return true if successful */
    bool addSwapIndexToSsm(const std::string& indexName, const bool continueOnError);

    const boost::shared_ptr<ScenarioSimMarketParameters> parameters_;
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<AggregationScenarioData> asd_;
    boost::shared_ptr<FixingManager> fixingManager_;
    boost::shared_ptr<ScenarioFilter> filter_;

    std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>> simData_;
    boost::shared_ptr<Scenario> baseScenario_;
    boost::shared_ptr<Scenario> baseScenarioAbsolute_;

    std::vector<boost::shared_ptr<SimpleQuote>> cachedSimData_;
    std::vector<bool> cachedSimDataActive_;

    std::set<RiskFactorKey::KeyType> nonSimulatedFactors_;

    // if generate spread scenario values for keys, we store the absolute values in this map
    // so that we can set up the base scenario with absolute values
    bool useSpreadedTermStructures_;
    std::map<RiskFactorKey, Real> absoluteSimData_;

    bool cacheSimData_;
    bool allowPartialScenarios_;
    IborFallbackConfig iborFallbackConfig_;

    // for delta scenario application
    std::set<ore::analytics::RiskFactorKey> diffToBaseKeys_;

    mutable boost::shared_ptr<Scenario> currentScenario_;
};
} // namespace analytics
} // namespace ore
