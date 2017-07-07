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
#include <ql/quotes/all.hpp>
#include <qle/termstructures/all.hpp>

#include <map>

using namespace QuantLib;
using std::vector;
using std::map;
using std::string;

namespace ore {
namespace analytics {

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
    ScenarioSimMarket(boost::shared_ptr<ScenarioGenerator>& scenarioGenerator, boost::shared_ptr<Market>& initMarket,
                      boost::shared_ptr<ScenarioSimMarketParameters>& parameters, Conventions conventions,
                      const std::string& configuration = Market::defaultConfiguration);

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

    //! Return the fixing manager
    const boost::shared_ptr<FixingManager>& fixingManager() const override { return fixingManager_; }

private:
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<ScenarioSimMarketParameters> parameters_;
    boost::shared_ptr<AggregationScenarioData> asd_;
    boost::shared_ptr<FixingManager> fixingManager_;
    boost::shared_ptr<ScenarioFilter> filter_;

    std::map<RiskFactorKey, boost::shared_ptr<SimpleQuote>> simData_;
};
} // namespace analytics
} // namespace ore
