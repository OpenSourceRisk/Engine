/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/engine/xvaenginecg.hpp>

#include <ored/scripting/engines/scriptedinstrumentpricingenginecg.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/ad/backwardderivatives.hpp>
#include <qle/ad/forwardevaluation.hpp>
#include <qle/ad/ssaform.hpp>
#include <qle/methods/multipathvariategenerator.hpp>

#include <boost/timer/timer.hpp>

namespace ore {
namespace analytics {

XvaEngineCG::XvaEngineCG(const Size nThreads, const Date& asof, const boost::shared_ptr<ore::data::Loader>& loader,
                         const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                         const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                         const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                         const boost::shared_ptr<ore::data::EngineData>& engineData,
                         const boost::shared_ptr<ore::analytics::CrossAssetModelData>& crossAssetModelData,
                         const boost::shared_ptr<ore::analytics::ScenarioGeneratorData>& scenarioGeneratorData,
                         const boost::shared_ptr<ore::data::Portfolio>& portfolio, const string& marketConfiguration,
                         const string& marketConfigurationInCcy,
                         const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData,
                         const boost::shared_ptr<ReferenceDataManager>& referenceData,
                         const IborFallbackConfig& iborFallbackConfig, const bool continueOnCalibrationError,
                         const bool continueOnError, const std::string& context)
    : nThreads_(nThreads), asof_(asof), loader_(loader), curveConfigs_(curveConfigs),
      todaysMarketParams_(todaysMarketParams), simMarketData_(simMarketData), engineData_(engineData),
      crossAssetModelData_(crossAssetModelData), scenarioGeneratorData_(scenarioGeneratorData), portfolio_(portfolio),
      marketConfiguration_(marketConfiguration), marketConfigurationInCcy_(marketConfigurationInCcy),
      sensitivityData_(sensitivityData), referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig),
      continueOnCalibrationError_(continueOnCalibrationError), continueOnError_(continueOnError), context_(context) {

    // for performace testing, duplicate trades in input porfolio as specified by env var N
    portfolio_ = boost::make_shared<Portfolio>();
    std::string pfxml = portfolio->toXMLString();
    for (Size i = 0; i < atoi(getenv("N")); ++i) {
        auto p = boost::make_shared<Portfolio>();
        p->fromXMLString(pfxml);
        for (auto const& [id, t] : p->trades()) {
            t->id() += "_" + std::to_string(i + 1);
            portfolio_->add(t);
        }
    }
    // end of portfolio duplication, to be removed later obviously

    LOG("XvaEngineCG started");
    boost::timer::cpu_timer timer;

    // 1 build T0 market

    DLOG("XvaEngineCG: build init market");

    initMarket_ = boost::make_shared<ore::data::TodaysMarket>(asof_, todaysMarketParams_, loader_, curveConfigs_,
                                                              continueOnError_, true, true, referenceData_, false,
                                                              iborFallbackConfig_, false, true);

    boost::timer::nanosecond_type timing1 = timer.elapsed().wall;

    // 2 build sim market

    DLOG("XvaEngineCG: build sim market");

    // note: take use spreaded term structures from sensitivity config?
    simMarket_ = boost::make_shared<ore::analytics::ScenarioSimMarket>(
        initMarket_, simMarketData_, marketConfiguration_, *curveConfigs_, *todaysMarketParams_, continueOnError_, true,
        false, false, iborFallbackConfig_, true);

    boost::timer::nanosecond_type timing2 = timer.elapsed().wall;

    // 3 set up cam builder against sim market

    DLOG("XvaEngineCG: build cam model builder");

    // note: sim market has one config only, no in-ccy config to calibrate IR components
    camBuilder_ = boost::make_shared<CrossAssetModelBuilder>(
        simMarket_, crossAssetModelData_, marketConfigurationInCcy_, marketConfiguration_, marketConfiguration_,
        marketConfiguration_, marketConfiguration_, marketConfiguration_, false, continueOnCalibrationError_,
        std::string(), SalvagingAlgorithm::Spectral, "xva engine cg - cam builder");

    // 4 set up gaussian cam cg model
    // this constitues part A of the computation graph 0 ... lastModelNode

    DLOG("XvaEngineCG: build cam cg model");

    QL_REQUIRE(
        crossAssetModelData_->discretization() == CrossAssetModel::Discretization::Euler,
        "XvaEngineCG: cam is required to use discretization 'Euler', please update simulation parameters accordingly.");

    std::vector<std::string> currencies;                                                   // from cam
    std::vector<Handle<YieldTermStructure>> curves;                                        // from cam
    std::vector<Handle<Quote>> fxSpots;                                                    // from cam
    std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>> irIndices;   // from trade building
    std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>> infIndices; // from trade building
    std::vector<std::string> indices;                                                      // from trade building
    std::vector<std::string> indexCurrencies;                                              // from trade building

    // 4b for the PoC we populate the containers with hardcoded values ... TEMPORARY HACK ...

    currencies.push_back("EUR");
    curves.push_back(camBuilder_->model()->irModel(0)->termStructure());
    irIndices.push_back(std::make_pair("EUR-EURIBOR-6M", *initMarket_->iborIndex("EUR-EURIBOR-6M")));

    // note: these must be fine enough for Euler, e.g. weekly over the whole simulation period
    std::set<Date> simulationDates(scenarioGeneratorData_->getGrid()->dates().begin(),
                                   scenarioGeneratorData_->getGrid()->dates().end());
    // note: this should be added to CrossAssetModelData
    Size timeStepsPerYear = 1;

    // note: projectedStateProcessIndices can be removed from GaussianCamCG constructor most probably
    model_ = boost::make_shared<GaussianCamCG>(camBuilder_->model(), scenarioGeneratorData_->samples(), currencies,
                                               curves, fxSpots, irIndices, infIndices, indices, indexCurrencies,
                                               simulationDates, timeStepsPerYear, iborFallbackConfig,
                                               std::vector<Size>(), std::vector<std::string>(), true);
    model_->calculate();

    auto g = model_->computationGraph();
    Size lastModelNode = g->size();

    std::cout << "Computation Graph size after model build: " << g->size() << std::endl;
    DLOG("Built computation graph for model, size is " << g->size());
    TLOGGERSTREAM(ssaForm(*g, getRandomVariableOpLabels()));

    boost::timer::nanosecond_type timing3 = timer.elapsed().wall;

    // 4c build trades with global cg cam model
    // this constitutes part B of the computation graph, B = Union of Bi where 
    // lastModelNode       ... trade 1 range end = B1
    // trade 2 range start ... trade 2 range end = B2
    // ...
    // trade n range start ... trade n range end = Bn

    DLOG("XvaEngineCG: build trades using global cam cg model");

    auto edCopy = boost::make_shared<EngineData>(*engineData_);
    edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = marketConfigurationInCcy_;
    configurations[MarketContext::fxCalibration] = marketConfiguration_;
    configurations[MarketContext::pricing] = marketConfiguration_;
    auto factory =
        boost::make_shared<EngineFactory>(edCopy, simMarket_, configurations, referenceData_, iborFallbackConfig_,
                                          EngineBuilderFactory::instance().generateAmcCgEngineBuilders(
                                              model_, scenarioGeneratorData_->getGrid()->dates()),
                                          true);

    portfolio_->build(factory, "xva engine cg", true);

    boost::timer::nanosecond_type timing4 = timer.elapsed().wall;

    // 5 add to computation graph for all trades and store npv, amc npv nodes, node range for each trade

    DLOG("XvaEngineCG: add to computation graph for all trades");

    std::map<std::string, std::vector<std::size_t>> amcNpvNodes; // including time zero npv
    std::map<std::string, std::pair<std::size_t, std::size_t>> tradeNodeRanges;

    for (auto const& [id, trade] : portfolio_->trades()) {
        auto qlInstr = boost::dynamic_pointer_cast<ScriptedInstrument>(trade->instrument()->qlInstrument());
        QL_REQUIRE(qlInstr, "XvaEngineCG: expeced trade to provide ScriptedInstrument, trade '" << id << "' does not.");
        auto engine = boost::dynamic_pointer_cast<ScriptedInstrumentPricingEngineCG>(qlInstr->pricingEngine());
        QL_REQUIRE(engine, "XvaEngineCG: expected to get ScriptedInstrumentPricingEngineCG, trade '"
                               << id << "' has a different engine.");
        g->setPrefix(id + "_");
        std::size_t firstNode = g->size();
        engine->buildComputationGraph();
        std::size_t lastNode = g->size();
        tradeNodeRanges[id] = std::make_pair(firstNode, lastNode);
        std::vector<std::size_t> tmp;
        tmp.push_back(g->variable(engine->npvName() + "_0"));
        for (std::size_t i = 0; i < simulationDates.size(); ++i) {
            tmp.push_back(g->variable("_AMC_NPV_" + std::to_string(i)));
        }
        amcNpvNodes[id] = tmp;
        std::cout << "trade " << id << " node range: " << firstNode << " - " << lastNode << std::endl;
    }

    // 5b add exposure sum-over-trades nodes (pathwise and conditional expectations)
    // this constitutes part C of the computation graph spanning "trade n range end ... lastExposureNode"
    // it sums over trade exposures and building conditional expectations:
    // - pfPathExposureNodes: sum over trades of path amc sim values
    // - pfExposureNodes:     the corresponding conditional expectations

    std::vector<std::size_t> pfPathExposureNodes, pfExposureNodes;
    for (Size i = 0; i < simulationDates.size() + 1; ++i) {
        std::size_t sumNode = cg_const(*g, 0.0);
        for (auto const& [_, v] : amcNpvNodes) {
            sumNode = cg_add(*g, sumNode, v[i]);
        }
        pfPathExposureNodes.push_back(sumNode);
        pfExposureNodes.push_back(
            model_->npv(sumNode, i == 0 ? model_->referenceDate() : *std::next(simulationDates.begin(), i - 1),
                        cg_const(*g, 1.0), boost::none, ComputationGraph::nan, ComputationGraph::nan));
    }

    Size lastExposureNode = g->size();

    DLOG("Extended computation graph for trades, size is " << g->size());
    TLOGGERSTREAM(ssaForm(*g, getRandomVariableOpLabels()));

    std::cout << "graph size after adding sum-over-trade nodes (pathwise and conditional expectation): " << g->size() << std::endl;

    boost::timer::nanosecond_type timing5 = timer.elapsed().wall;

    // 6 populate random variates

    DLOG("XvaEngineCG: populate random variates");

    std::vector<RandomVariable> values(g->size(), RandomVariable(model_->size(), 0.0));

    auto const& rv = model_->randomVariates();
    if (!rv.empty()) {
        auto gen = makeMultiPathVariateGenerator(scenarioGeneratorData_->sequenceType(), rv.size(), rv.front().size(),
                                                 scenarioGeneratorData_->seed(), scenarioGeneratorData_->ordering(),
                                                 scenarioGeneratorData_->directionIntegers());
        for (Size path = 0; path < model_->size(); ++path) {
            auto p = gen->next();
            for (Size j = 0; j < rv.front().size(); ++j) {
                for (Size k = 0; k < rv.size(); ++k) {
                    values[rv[k][j]].set(path, p.value[j][k]);
                }
            }
        }
        DLOG("generated rvs for " << rv.size() << " underlyings and " << rv.front().size() << " time steps.");
    }

    boost::timer::nanosecond_type timing6 = timer.elapsed().wall;

    // 7 populate constants and model parameters

    DLOG("XvaEngineCG: populate constants and model parameters");

    for (auto const& c : g->constants()) {
        values[c.second] = RandomVariable(model_->size(), c.first);
    }

    baseModelParams_ = model_->modelParameters();
    for (auto const& [n, v] : baseModelParams_) {
        values[n] = RandomVariable(model_->size(), v);
    }

    DLOG("set " << g->constants().size() << " constants and " << baseModelParams_.size() << " model parameters.");

    boost::timer::nanosecond_type timing7 = timer.elapsed().wall;

    // 8 do forward evaluation for all trades, keep nodes in part A and pf exposure nodes, pf path exposure nodes
    //   for the pp which starts its computatino on the exposure nodes

    DLOG("XvaEngineCG: run forward evaluation");

    std::vector<bool> keepNodes(g->size(), false);
    std::fill(keepNodes.begin(), std::next(keepNodes.begin(), lastModelNode), true);
    for (auto const& n : pfPathExposureNodes) {
        keepNodes[n] = true;
    }
    for (auto const& n : pfExposureNodes) {
        keepNodes[n] = true;
    }
    for (auto const& c : g->constants()) {
        keepNodes[c.second] = true;
    }
    for (auto const& [n, v] : baseModelParams_) {
        keepNodes[n] = true;
    }

    // note. regression order and polynom type should ultimately come from st pe config or xva analytics config (?)
    ops_ = getRandomVariableOps(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial);
    grads_ = getRandomVariableGradients(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial);
    opNodeRequirements_ = getRandomVariableOpNodeRequirements();

    forwardEvaluation(*g, values, ops_, RandomVariable::deleter, false, opNodeRequirements_, keepNodes);

    DLOG("ran forward evaluation.");

    boost::timer::nanosecond_type timing8 = timer.elapsed().wall;

    // 8b dump epe / ene profile out

    // for (Size i = 0; i < simulationDates.size() + 1; ++i) {
    //     std::cout << ore::data::to_string(i == 0 ? model_->referenceDate() : *std::next(simulationDates.begin(), i - 1))
    //               << "," << expectation(values[pfExposureNodes[i]]) << ","
    //               << expectation(max(values[pfExposureNodes[i]], RandomVariable(model_->size(), 0.0))) << ","
    //               << expectation(max(-values[pfExposureNodes[i]], RandomVariable(model_->size(), 0.0))) << "\n";
    // }
    // std::cout << std::flush;

    // 9 build the postprocessing computation graph
    //   this constitues part D of the computation graph from lastExposureNode ... g->size()
    //   the cvaNode is the ultimate result w.r.t. which we want to compute sensitivities

    // note: very simplified calculation, for testing, just multiply the EPE on each date by fixed default prob
    Size defaultProbNode = cg_const(*g, 0.0004);
    Size cvaNode = cg_const(*g, 0.0);
    for (Size i = 0; i < simulationDates.size() + 1; ++i) {
        cvaNode =
            cg_add(*g, cvaNode, cg_mult(*g, defaultProbNode, cg_max(*g, pfExposureNodes[i], cg_const(*g, 0.0))));
    }

    boost::timer::nanosecond_type timing9 = timer.elapsed().wall;

    std::cout << "graph size after adding pp: " << g->size() << std::endl;

    // 10 do forward evaluation on postprocessing graph D
    //    on that part, keep all nodes needed for derivatives in part D, and the cva node

    std::vector<bool> activeNodes(g->size(), false);
    std::fill(std::next(activeNodes.begin(), lastExposureNode), activeNodes.end(), true);

    values.resize(g->size(), RandomVariable(model_->size(), 0.0));
    keepNodes.resize(g->size(), false);

    keepNodes[cvaNode] = true;

    // need to set the constants again, since we did not keep them and also added new ones in the pp
    for (auto const& c : g->constants()) {
        values[c.second] = RandomVariable(model_->size(), c.first);
    }

    forwardEvaluation(*g, values, ops_, RandomVariable::deleter, true, opNodeRequirements_, keepNodes, activeNodes);

    boost::timer::nanosecond_type timing10 = timer.elapsed().wall;

    std::cout << "CVA: " << expectation(values[cvaNode]) << std::endl;

    // 11 do backward derivatives run from CVA node on pp graph D

    std::vector<RandomVariable> derivatives(g->size(), RandomVariable(model_->size(), 0.0));
    std::vector<bool> keepNodesDerivatives(g->size(), false);

    std::fill(activeNodes.begin(), std::next(activeNodes.begin(), lastExposureNode), false);
    for (auto const& n : pfPathExposureNodes) {
        activeNodes[n] = true;
        keepNodesDerivatives[n] = true;
    }
    for (auto const& n : pfExposureNodes)
        activeNodes[n] = true;
    std::fill(std::next(activeNodes.begin(), lastExposureNode), activeNodes.end(), true);

    derivatives[cvaNode] = RandomVariable(model_->size(), 1.0);
    backwardDerivatives(*g, values, derivatives, grads_, RandomVariable::deleter, keepNodesDerivatives, activeNodes,
                        ops_[RandomVariableOpCode::ConditionalExpectation]);

    boost::timer::nanosecond_type timing11 = timer.elapsed().wall;

    // 11b roll back the derivatives for each trade, starting from the derivatives calc'ed in the previous step

    std::map<std::string, std::map<std::size_t, double>> tradeModelParamDerivatives;

    std::fill(keepNodesDerivatives.begin(), keepNodesDerivatives.end(), false);
    for (auto const& [n, _] : baseModelParams_)
        keepNodesDerivatives[n] = true;

    auto setToZeroDeleter =
        std::function<void(RandomVariable&)>([this](RandomVariable& x) { x = RandomVariable(model_->size(), 0.0); });

    std::vector<bool> activeNodes2(g->size(), false);
    for (auto const& [id, trade] : portfolio_->trades()) {

        auto range = tradeNodeRanges[id];

        std::fill(activeNodes2.begin(), std::next(activeNodes2.begin(), range.first), false);
        std::fill(std::next(activeNodes2.begin(), range.first), std::next(activeNodes2.begin(), range.second), true);
        std::fill(std::next(activeNodes2.begin(), range.second), activeNodes2.end(), false);
        forwardEvaluation(*g, values, ops_, RandomVariable::deleter, true, opNodeRequirements_, {}, activeNodes2);

        std::fill(activeNodes.begin(), std::next(activeNodes.begin(), lastModelNode), true);
        std::fill(std::next(activeNodes.begin(), lastModelNode), std::next(activeNodes.begin(), range.first), false);
        std::fill(std::next(activeNodes.begin(), range.first), std::next(activeNodes.begin(), range.second), true);
        std::fill(std::next(activeNodes.begin(), range.second), activeNodes.end(), false);
        for (auto const& n : pfPathExposureNodes)
            activeNodes[n] = true;
        backwardDerivatives(*g, values, derivatives, grads_, setToZeroDeleter, keepNodesDerivatives, activeNodes,
                            ops_[RandomVariableOpCode::ConditionalExpectation]);

        for (auto const& [n, v] : baseModelParams_) {
            tradeModelParamDerivatives[id][n] = expectation(derivatives[n]).at(0);
            setToZeroDeleter(derivatives[n]);
        }

        for (Size i = range.first; i < range.second; ++i) {
            setToZeroDeleter(values[i]);
        }

        std::cout << "computed derivatives for trade " << id << std::endl;
    }

    boost::timer::nanosecond_type timing12 = timer.elapsed().wall;

    // 12 delete values and derivatives vectors, they are not needed from this point on

    values.clear();
    derivatives.clear();

    // 13 Output statistics

    std::cout << "Computation graph size:   " << g->size() << std::endl;
    std::cout << "Peak mem usage:           " << ore::data::os::getPeakMemoryUsageBytes() / 1024 / 1024 << " MB"
              << std::endl;
    std::cout << "T0 market build:          " << timing1 / 1E6 << " ms" << std::endl;
    std::cout << "Sim market build:         " << (timing2 - timing1) / 1E6 << " ms" << std::endl;
    std::cout << "Model CG build:           " << (timing3 - timing2) / 1E6 << " ms" << std::endl;
    std::cout << "Portfolio build:          " << (timing4 - timing3) / 1E6 << " ms" << std::endl;
    std::cout << "Trade CG build:           " << (timing5 - timing4) / 1E6 << " ms" << std::endl;
    std::cout << "RV gen:                   " << (timing6 - timing5) / 1E6 << " ms" << std::endl;
    std::cout << "model params / const set  " << (timing7 - timing6) / 1E6 << " ms" << std::endl;
    std::cout << "forward eval              " << (timing8 - timing7) / 1E6 << " ms" << std::endl;
    std::cout << "pp CG build               " << (timing9 - timing8) / 1E6 << " ms" << std::endl;
    std::cout << "forward eval on pp CG     " << (timing10 - timing9) / 1E6 << " ms" << std::endl;
    std::cout << "backward deriv on pp CG   " << (timing11 - timing10) / 1E6 << " ms" << std::endl;
    std::cout << "forward/backward on trds  " << (timing12 - timing11) / 1E6 << " ms" << std::endl;
    std::cout << "total                     " << timing12 / 1E6 << " ms" << std::endl;
}

} // namespace analytics
} // namespace ore
