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

/*! \file scenario/crossassetmodelscenariogenerator.hpp
    \brief Scenario generation using cross asset model paths
    \ingroup scenario
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/simulation/dategrid.hpp>

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetmodelimpliedfxvoltermstructure.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

namespace ore {
using namespace data;
namespace analytics {

//! Scenario Generator using cross asset model paths
/*!
  The generator expects
  - a calibrated model,
  - an associated multi path generator (i.e. providing paths for all factors
    of the model ordered as described in the model),
  - a scenario factory that provides building scenario class instances,
  - the configuration of market curves to be simulated
  - a simulation date grid that starts in the future, i.e. does not include today's date
  - the associated time grid including t=0

  \ingroup scenario
 */
class CrossAssetModelScenarioGenerator : public ScenarioPathGenerator {
public:
    //! Constructor
    CrossAssetModelScenarioGenerator(boost::shared_ptr<QuantExt::CrossAssetModel> model,
                                     boost::shared_ptr<QuantExt::MultiPathGeneratorBase> multiPathGenerator,
                                     boost::shared_ptr<ScenarioFactory> scenarioFactory,
                                     boost::shared_ptr<ScenarioSimMarketParameters> simMarketConfig,
                                     QuantLib::Date today, boost::shared_ptr<ore::analytics::DateGrid> grid,
                                     boost::shared_ptr<ore::data::Market> initMarket,
                                     const std::string& configuration = Market::defaultConfiguration);
    //! Default destructor
    ~CrossAssetModelScenarioGenerator(){};
    std::vector<boost::shared_ptr<Scenario>> nextPath();
    void reset() { pathGenerator_->reset(); }

private:
    boost::shared_ptr<QuantExt::CrossAssetModel> model_;
    boost::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator_;
    boost::shared_ptr<ScenarioFactory> scenarioFactory_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;
    boost::shared_ptr<ore::data::Market> initMarket_;
    const std::string configuration_;
    std::vector<RiskFactorKey> discountCurveKeys_, indexCurveKeys_;
    std::vector<RiskFactorKey> fxKeys_, eqKeys_, eqirCurveKeys_;
    std::vector<boost::shared_ptr<QuantExt::CrossAssetModelImpliedFxVolTermStructure>> fxVols_;
};
}
}
