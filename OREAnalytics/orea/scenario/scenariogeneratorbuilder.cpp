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
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>

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

boost::shared_ptr<ScenarioGenerator>
ScenarioGeneratorBuilder::build(boost::shared_ptr<QuantExt::CrossAssetModel> model,
                                boost::shared_ptr<ScenarioFactory> scenarioFactory,
                                boost::shared_ptr<ScenarioSimMarketParameters> marketConfig, Date asof,
                                boost::shared_ptr<ore::data::Market> initMarket, const std::string& configuration) {

    LOG("ScenarioGeneratorBuilder::build() started");

    QL_REQUIRE(initMarket != NULL, "ScenarioGeneratorBuilder: initMarket is null");

    boost::shared_ptr<StochasticProcess> stateProcess = model->stateProcess(data_->discretization());

    boost::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen;
    if (data_->sequenceType() == ScenarioGeneratorData::SequenceType::MersenneTwister)
        pathGen = boost::make_shared<MultiPathGeneratorMersenneTwister>(stateProcess, data_->grid()->timeGrid(),
                                                                        data_->seed(), false);
    else if (data_->sequenceType() == ScenarioGeneratorData::SequenceType::MersenneTwisterAntithetic)
        pathGen = boost::make_shared<MultiPathGeneratorMersenneTwister>(stateProcess, data_->grid()->timeGrid(),
                                                                        data_->seed(), true);
    else if (data_->sequenceType() == ScenarioGeneratorData::SequenceType::Sobol)
        pathGen = boost::make_shared<QuantExt::MultiPathGeneratorSobol>(stateProcess, data_->grid()->timeGrid());
    // TODO expose ordering parameter to xml configuration (we hard code Steps / JoeKuoD7 for now)
    else if (data_->sequenceType() == ScenarioGeneratorData::SequenceType::SobolBrownianBridge)
        pathGen = boost::make_shared<QuantExt::MultiPathGeneratorSobolBrownianBridge>(
            stateProcess, data_->grid()->timeGrid(), SobolBrownianGenerator::Steps, data_->seed(), SobolRsg::JoeKuoD7);
    else
        QL_FAIL("Sequence type " << data_->sequenceType() << " not covered");

    // boost::shared_ptr<CrossAssetModelScenarioGenerator> scenGen =
    boost::shared_ptr<ScenarioGenerator> scenGen = boost::make_shared<CrossAssetModelScenarioGenerator>(
        model, pathGen, scenarioFactory, marketConfig, asof, data_->grid(), initMarket, configuration);
    LOG("ScenarioGeneratorBuilder::build() done");

    return scenGen;
}
}
}
