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

#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/methods/pathgeneratorfactory.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <map>

using namespace ore::data;
using namespace std;

using namespace QuantExt;

namespace ore {
namespace analytics {

QuantLib::ext::shared_ptr<ScenarioGenerator>
ScenarioGeneratorBuilder::build(QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model,
                                QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                                QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> marketConfig, Date asof,
                                QuantLib::ext::shared_ptr<ore::data::Market> initMarket, const std::string& configuration,
                                const QuantLib::ext::shared_ptr<PathGeneratorFactory>& pf) {

    LOG("ScenarioGeneratorBuilder::build() called");

    QL_REQUIRE(initMarket != NULL, "ScenarioGeneratorBuilder: initMarket is null");

    // enable cache
    auto process = model->stateProcess();
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(data_->getGrid()->timeGrid().size() - 1);
    }

    auto pathGen = pf->build(data_->sequenceType(), process, data_->getGrid()->timeGrid(), data_->seed(),
                             data_->ordering(), data_->directionIntegers());

    return QuantLib::ext::make_shared<CrossAssetModelScenarioGenerator>(model, pathGen, scenarioFactory, marketConfig, asof,
                                                                data_->getGrid(), initMarket, configuration);
}
} // namespace analytics
} // namespace ore
