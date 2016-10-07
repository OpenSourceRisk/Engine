/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file scenario/lgmscenariogenerator.hpp
    \brief Scenario generation using LGM paths
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariogenerator.hpp>
#include <qle/models/lgm.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/simulation/dategrid.hpp>

using namespace QuantLib;

namespace openriskengine {
namespace analytics {

//! Scenario Generator using LGM model paths
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
class LgmScenarioGenerator : public ScenarioPathGenerator {
public:
    //! Constructor
    LgmScenarioGenerator(boost::shared_ptr<QuantExt::LGM> model,
                         boost::shared_ptr<QuantExt::MultiPathGeneratorBase> multiPathGenerator,
                         boost::shared_ptr<ScenarioFactory> scenarioFactory,
                         boost::shared_ptr<ScenarioSimMarketParameters> simMarketConfig, Date today,
                         openriskengine::analytics::DateGrid grid);
    //! Destructor
    ~LgmScenarioGenerator(){};
    std::vector<boost::shared_ptr<Scenario>> nextPath();
    void reset() { pathGenerator_->reset(); }

private:
    boost::shared_ptr<QuantExt::LGM> model_;
    boost::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator_;
    boost::shared_ptr<ScenarioFactory> scenarioFactory_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;
};
}
}
