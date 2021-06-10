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
#include <ql/quotes/all.hpp>
#include <qle/termstructures/averageoisratehelper.hpp>
#include <qle/termstructures/basistwoswaphelper.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>
#include <qle/termstructures/blackvariancecurve3.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>
#include <qle/termstructures/blackvolsurfacewithatm.hpp>
#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <qle/termstructures/datedstrippedoptionlet.hpp>
#include <qle/termstructures/datedstrippedoptionletadapter.hpp>
#include <qle/termstructures/datedstrippedoptionletbase.hpp>
#include <qle/termstructures/defaultprobabilityhelpers.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>
#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
#include <qle/termstructures/dynamicoptionletvolatilitystructure.hpp>
#include <qle/termstructures/dynamicstype.hpp>
#include <qle/termstructures/dynamicswaptionvolmatrix.hpp>
#include <qle/termstructures/dynamicyoyoptionletvolatilitystructure.hpp>
#include <qle/termstructures/equityvolconstantspread.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>
#include <qle/termstructures/fxsmilesection.hpp>
#include <qle/termstructures/fxvannavolgasmilesection.hpp>
#include <qle/termstructures/hazardspreadeddefaulttermstructure.hpp>
#include <qle/termstructures/immfraratehelper.hpp>
#include <qle/termstructures/interpolateddiscountcurve.hpp>
#include <qle/termstructures/interpolateddiscountcurve2.hpp>
#include <qle/termstructures/interpolateddiscountcurvelinearzero.hpp>
#include <qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp>
#include <qle/termstructures/oibasisswaphelper.hpp>
#include <qle/termstructures/oiccbasisswaphelper.hpp>
#include <qle/termstructures/oisratehelper.hpp>
#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>
#include <qle/termstructures/spreadedoptionletvolatility.hpp>
#include <qle/termstructures/spreadedsmilesection.hpp>
#include <qle/termstructures/spreadedswaptionvolatility.hpp>
#include <qle/termstructures/staticallycorrectedyieldtermstructure.hpp>
#include <qle/termstructures/strippedoptionletadapter2.hpp>
#include <qle/termstructures/subperiodsswaphelper.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/tenorbasisswaphelper.hpp>
#include <qle/termstructures/yoyinflationcurveobservermoving.hpp>
#include <qle/termstructures/yoyinflationcurveobserverstatic.hpp>
#include <qle/termstructures/yoyinflationoptionletvolstripper.hpp>
#include <qle/termstructures/yoyoptionletvolatilitysurface.hpp>
#include <qle/termstructures/zeroinflationcurveobservermoving.hpp>
#include <qle/termstructures/zeroinflationcurveobserverstatic.hpp>

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
class ScenarioSimMarket : public analytics::SimMarket {
public:
    //! Constructor
    ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                      const boost::shared_ptr<ScenarioSimMarketParameters>& parameters, const Conventions& conventions,
                      const std::string& configuration = Market::defaultConfiguration,
                      const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                      const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                      const bool continueOnError = false);

    ScenarioSimMarket(const boost::shared_ptr<Market>& initMarket,
                      const boost::shared_ptr<ScenarioSimMarketParameters>& parameters, const Conventions& conventions,
                      const boost::shared_ptr<FixingManager>& fixingManager,
                      const std::string& configuration = Market::defaultConfiguration,
                      const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                      const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                      const bool continueOnError = false);

    //! Set scenario generator
    boost::shared_ptr<ScenarioGenerator>& scenarioGenerator() { return scenarioGenerator_; }
    //! Get scenario generator
    const boost::shared_ptr<ScenarioGenerator>& scenarioGenerator() const { return scenarioGenerator_; }

    //! Set aggregation data
    boost::shared_ptr<AggregationScenarioData>& aggregationScenarioData() { return asd_; }
    //! Get aggregation data
    const boost::shared_ptr<AggregationScenarioData>& aggregationScenarioData() const { return asd_; }

    //! Set scenarioFilter
    boost::shared_ptr<ScenarioFilter>& filter() { return filter_; }
    //! Get scenarioFilter
    const boost::shared_ptr<ScenarioFilter>& filter() const { return filter_; }

    //! Update market snapshot and relevant fixing history
    void update(const Date& d) override;

    //! Reset sim market to initial state
    virtual void reset() override;

    //! Scenario representing the initial state of the market
    boost::shared_ptr<Scenario> baseScenario() const { return baseScenario_; }

    //! Return the fixing manager
    const boost::shared_ptr<FixingManager>& fixingManager() const override { return fixingManager_; }

    //! is risk factor key simulated by this sim market instance?
    bool isSimulated(const RiskFactorKey::KeyType& factor) const;

protected:
    virtual void applyScenario(const boost::shared_ptr<Scenario>& scenario);
    void addYieldCurve(const boost::shared_ptr<Market>& initMarket, const std::string& configuration,
                       const RiskFactorKey::KeyType rf, const string& key, const vector<Period>& tenors,
                       const std::string& dc, bool simulate = true, const string& interpolation = "LogLinear");

    /*! Given a yield curve spec ID, \p yieldSpecId, return the corresponding yield term structure
    from the \p market. If \p market is `nullptr`, then the yield term structure is taken from
    this ScenarioSimMarket instance.
    */
    QuantLib::Handle<QuantLib::YieldTermStructure>
    getYieldCurve(const std::string& yieldSpecId, const ore::data::TodaysMarketParameters& todaysMarketParams,
                  const std::string& configuration, const boost::shared_ptr<ore::data::Market>& market = nullptr) const;

    const boost::shared_ptr<ScenarioSimMarketParameters> parameters_;
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<AggregationScenarioData> asd_;
    boost::shared_ptr<FixingManager> fixingManager_;
    boost::shared_ptr<ScenarioFilter> filter_;

    std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>> simData_;
    boost::shared_ptr<Scenario> baseScenario_;

    std::set<RiskFactorKey::KeyType> nonSimulatedFactors_;
};
} // namespace analytics
} // namespace ore
