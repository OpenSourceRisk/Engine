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

/*! \file scenario/scenariogeneratorbuilder.hpp
    \brief Build a scenariogenerator
    \ingroup scenario
*/

#pragma once

#include <vector>

#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>
#include <qle/methods/pathgeneratorfactory.hpp>

#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogeneratordata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;

//! Build a ScenarioGenerator
/*! ScenarioGeneratorBuilder builds a scenario generator
  based on the settings provided via the ScenarioGeneratorData object
  - state process
  - simulation date grid
  - multipath generator
  - scenario factory
  - fixing method

  \ingroup scenario
 */
class ScenarioGeneratorBuilder {
public:
    //! Default constructor
    ScenarioGeneratorBuilder() {}

    //! Constructor
    ScenarioGeneratorBuilder(QuantLib::ext::shared_ptr<ScenarioGeneratorData> data) : data_(data) {}

    //! Build function
    QuantLib::ext::shared_ptr<ScenarioGenerator>
    build(QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model, QuantLib::ext::shared_ptr<ScenarioFactory> sf,
          QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> marketConfig, Date asof,
          QuantLib::ext::shared_ptr<ore::data::Market> initMarket,
          const std::string& configuration = ore::data::Market::defaultConfiguration,
          const QuantLib::ext::shared_ptr<PathGeneratorFactory>& pf = QuantLib::ext::make_shared<MultiPathGeneratorFactory>());

private:
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> data_;
    boost::optional<std::set<std::string>> currencies_;
};
} // namespace analytics
} // namespace ore
