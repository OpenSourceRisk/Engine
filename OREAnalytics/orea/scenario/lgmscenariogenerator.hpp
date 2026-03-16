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

/*! \file scenario/lgmscenariogenerator.hpp
    \brief Scenario generation using LGM paths
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <ored/utilities/dategrid.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/lgm.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;

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
    LgmScenarioGenerator(QuantLib::ext::shared_ptr<QuantExt::LGM> model,
                         QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> multiPathGenerator,
                         QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                         QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig, Date today, DateGrid grid);
    //! Destructor
    ~LgmScenarioGenerator(){};
    std::vector<QuantLib::ext::shared_ptr<Scenario>> nextPath() override;
    void reset() override { pathGenerator_->reset(); }

private:
    QuantLib::ext::shared_ptr<QuantExt::LGM> model_;
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator_;
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;
};
} // namespace analytics
} // namespace ore
