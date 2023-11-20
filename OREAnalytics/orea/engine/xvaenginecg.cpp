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

    // Just for performance testing, duplicate the trades in input portfolio as specified by env var N

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

    // Start Engine

    LOG("XvaEngineCG started");
    boost::timer::cpu_timer timer;

    // Build T0 market

    DLOG("XvaEngineCG: build init market");

    initMarket_ = boost::make_shared<ore::data::TodaysMarket>(asof_, todaysMarketParams_, loader_, curveConfigs_,
                                                              continueOnError_, true, true, referenceData_, false,
                                                              iborFallbackConfig_, false, true);

    boost::timer::nanosecond_type timing1 = timer.elapsed().wall;

    // Build sim market

    DLOG("XvaEngineCG: build sim market");

    // note: take "use spreaded term structures" from sensitivity config instead of hardcoding to true here?
    simMarket_ = boost::make_shared<ore::analytics::ScenarioSimMarket>(
        initMarket_, simMarketData_, marketConfiguration_, *curveConfigs_, *todaysMarketParams_, continueOnError_, true,
        false, false, iborFallbackConfig_, true);

    boost::timer::nanosecond_type timing2 = timer.elapsed().wall;

    // Set up cam builder against sim market

    DLOG("XvaEngineCG: build cam model builder");

    // note: sim market has one config only, no in-ccy config to calibrate IR components
    camBuilder_ = boost::make_shared<CrossAssetModelBuilder>(
        simMarket_, crossAssetModelData_, marketConfigurationInCcy_, marketConfiguration_, marketConfiguration_,
        marketConfiguration_, marketConfiguration_, marketConfiguration_, false, continueOnCalibrationError_,
        std::string(), SalvagingAlgorithm::Spectral, "xva engine cg - cam builder");

    // Set up gaussian cam cg model
    // This constitues part A of the computation graph 0 ... lastModelNode

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

    // note: for the PoC we populate the containers with hardcoded values ... temp hack ...
    currencies.push_back("EUR");
    curves.push_back(camBuilder_->model()->irModel(0)->termStructure());
    irIndices.push_back(std::make_pair("EUR-EURIBOR-6M", *initMarket_->iborIndex("EUR-EURIBOR-6M")));

    // note: these must be fine enough for Euler, e.g. weekly over the whole simulation period
    std::set<Date> simulationDates(scenarioGeneratorData_->getGrid()->dates().begin(),
                                   scenarioGeneratorData_->getGrid()->dates().end());
    // note: this should be added to CrossAssetModelData
    Size timeStepsPerYear = 1;

    // note: projectedStateProcessIndices can be removed from GaussianCamCG constructor most probably?
    model_ = boost::make_shared<GaussianCamCG>(camBuilder_->model(), scenarioGeneratorData_->samples(), currencies,
                                               curves, fxSpots, irIndices, infIndices, indices, indexCurrencies,
                                               simulationDates, timeStepsPerYear, iborFallbackConfig,
                                               std::vector<Size>(), std::vector<std::string>(), true);
    model_->calculate(); // trigger model build

    auto g = model_->computationGraph();
    Size lastModelNode = g->size();

    DLOG("XvaEngineCG: Built computation graph part A (model), size is now " << g->size());

    boost::timer::nanosecond_type timing3 = timer.elapsed().wall;

    // Build trades against global cg cam model
    // This constitutes part B of the computation graph, B = Union of Bi where
    // lastModelNode       ... trade 1 range end = B1
    // trade 2 range start ... trade 2 range end = B2
    // ...
    // trade m range start ... trade n range end = Bm

    DLOG("XvaEngineCG: build trades against global cam cg model");

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

    // Build computation graph for all trades ("part B") and
    // - store npv, amc npv nodes,
    // - node range for each trade
    // - predecessors in part A on which trades depend

    DLOG("XvaEngineCG: build computation graph for all trades");

    std::map<std::string, std::vector<std::size_t>> amcNpvNodes; // includes time zero npv
    std::map<std::string, std::pair<std::size_t, std::size_t>> tradeNodeRanges;

    std::set<std::size_t> tradeModelDependencies;
    g->recordPredecessors(&tradeModelDependencies);

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
        DLOG("XvaEngineCG: node range for trade " << id << ": " << firstNode << " ... " << lastNode);
    }

    g->recordPredecessors();

    // remove dependencies which are not in A
    tradeModelDependencies.erase(
        std::lower_bound(tradeModelDependencies.begin(), tradeModelDependencies.end(), lastModelNode),
        tradeModelDependencies.end());

    std::size_t lastTradeNode = g->size();

    boost::timer::nanosecond_type timing5 = timer.elapsed().wall;

    // Add nodes that sum the exposure over trades, both pathwise and conditional expectations
    // This constitutes part C of the computation graph spanning "trade m range end ... lastExposureNode"
    // - pfPathExposureNodes: path amc sim values, aggregated over trades
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

    DLOG("XvaEngineCG: added aggregation over trade expsosure nodes and conditional expectation, size is "
         << g->size());

    boost::timer::nanosecond_type timing6 = timer.elapsed().wall;

    // Add post processor
    // This constitues part D of the computation graph from lastExposureNode ... g->size()
    // The cvaNode is the ultimate result w.r.t. which we want to compute sensitivities

    // note: very simplified calculation, for testing, just multiply the EPE on each date by fixed default prob
    Size defaultProbNode = cg_const(*g, 0.0004);
    Size cvaNode = cg_const(*g, 0.0);
    for (Size i = 0; i < simulationDates.size() + 1; ++i) {
        cvaNode = cg_add(*g, cvaNode, cg_mult(*g, defaultProbNode, cg_max(*g, pfExposureNodes[i], cg_const(*g, 0.0))));
    }

    boost::timer::nanosecond_type timing7 = timer.elapsed().wall;

    // Create values and derivatives containers

    std::vector<RandomVariable> values(g->size(), RandomVariable(model_->size(), 0.0));
    std::vector<RandomVariable> derivatives(g->size(), RandomVariable(model_->size(), 0.0));

    // Populate random variates

    DLOG("XvaEngineCG: populate random variates");

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
        DLOG("XvaEngineCG: generated rvs for " << rv.size() << " underlyings and " << rv.front().size()
                                               << " time steps.");
    }

    boost::timer::nanosecond_type timing8 = timer.elapsed().wall;

    // Populate constants and model parameters

    DLOG("XvaEngineCG: populate constants and model parameters");

    for (auto const& c : g->constants()) {
        values[c.second] = RandomVariable(model_->size(), c.first);
    }

    baseModelParams_ = model_->modelParameters();
    for (auto const& [n, v] : baseModelParams_) {
        values[n] = RandomVariable(model_->size(), v);
    }

    DLOG("XvaEngineCG: set " << g->constants().size() << " constants and " << baseModelParams_.size()
                             << " model parameters.");

    boost::timer::nanosecond_type timing9 = timer.elapsed().wall;

    // Do a forward evaluation, keep the following values nodes
    // - constants
    // - model parameters
    // - all nodes in part A needed for derivatives and trade model dependencies
    // - all nodes in part C needed for derivatives and pf exposure nodes
    // - all nodes in part D needed for derivatives and cva node

    DLOG("XvaEngineCG: run forward evaluation");

    ops_ = getRandomVariableOps(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial);
    grads_ = getRandomVariableGradients(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial);
    opNodeRequirements_ = getRandomVariableOpNodeRequirements();

    std::vector<bool> keepNodes(g->size(), false);

    for (auto const& c : g->constants()) {
        keepNodes[c.second] = true;
    }

    for (auto const& [n, v] : baseModelParams_) {
        keepNodes[n] = true;
    }

    for(auto const& n: tradeModelDependencies) {
        keepNodes[n] = true;
    }

    for (auto const& n : pfExposureNodes) {
        keepNodes[n] = true;
    }

    keepNodes[cvaNode] = true;

    std::vector<bool> nodesA(g->size(), false);
    std::vector<bool> nodesB(g->size(), false);
    std::vector<bool> nodesC(g->size(), false);
    std::vector<bool> nodesD(g->size(), false);

    std::fill(nodesA.begin(), std::next(nodesA.begin(), lastModelNode), true);
    std::fill(std::next(nodesB.begin(), lastModelNode), std::next(nodesB.begin(), lastTradeNode), true);
    std::fill(std::next(nodesC.begin(), lastTradeNode), std::next(nodesC.begin(), lastExposureNode), true);
    std::fill(std::next(nodesD.begin(), lastExposureNode), nodesD.end(), true);

    // part A
    forwardEvaluation(*g, values, ops_, RandomVariable::deleter, true, opNodeRequirements_, keepNodes, nodesA);

    // part B
    forwardEvaluation(*g, values, ops_, RandomVariable::deleter, false, opNodeRequirements_, keepNodes, nodesB);

    // part C
    forwardEvaluation(*g, values, ops_, RandomVariable::deleter, true, opNodeRequirements_, keepNodes, nodesC);

    // part D
    forwardEvaluation(*g, values, ops_, RandomVariable::deleter, true, opNodeRequirements_, keepNodes, nodesD);

    DLOG("XvaEngineCG: forward evaluation finished.");

    boost::timer::nanosecond_type timing10 = timer.elapsed().wall;

    // Dump epe / ene profile out

    for (Size i = 0; i < simulationDates.size() + 1; ++i) {
        std::cout << ore::data::to_string(i == 0 ? model_->referenceDate() : *std::next(simulationDates.begin(), i - 1))
                  << "," << expectation(values[pfExposureNodes[i]]) << ","
                  << expectation(max(values[pfExposureNodes[i]], RandomVariable(model_->size(), 0.0))) << ","
                  << expectation(max(-values[pfExposureNodes[i]], RandomVariable(model_->size(), 0.0))) << "\n";
    }
    std::cout << std::flush;

    DLOG("XvaEngineCG: Calcuated CVA = " << values[cvaNode]);

    // Do backward derivatives run from CVA node on pp graph D up to path exposure nodes in C

    DLOG("XvaEngineCG: run backward derivatives");

    derivatives[cvaNode] = RandomVariable(model_->size(), 1.0);

    std::vector<bool> keepNodesDerivatives(g->size(), false);
    std::vector<bool> nodesDPlusExposure(nodesD);

    for (auto const& [n, _] : baseModelParams_)
        keepNodesDerivatives[n] = true;

    for (auto const& n : tradeModelDependencies)
        keepNodesDerivatives[n] = true;

    for (auto const& n : pfPathExposureNodes) {
        keepNodesDerivatives[n] = true;
        nodesDPlusExposure[n] = true;
    }

    for (auto const& n : pfExposureNodes) {
        nodesDPlusExposure[n] = true;
    }

    backwardDerivatives(*g, values, derivatives, grads_, RandomVariable::deleter, keepNodesDerivatives,
                        nodesDPlusExposure, RandomVariableOpCode::ConditionalExpectation,
                        ops_[RandomVariableOpCode::ConditionalExpectation]);

    boost::timer::nanosecond_type timing11 = timer.elapsed().wall;

    // Roll back the derivatives for each trade, starting from the derivatives calc'ed in the previous step
    // This requires a forward eval for each trade on its graph Bj

    std::vector<bool> tradeActiveNodes(g->size(), false);
    for (auto const& [id, trade] : portfolio_->trades()) {
        auto range = tradeNodeRanges[id];

        std::fill(tradeActiveNodes.begin(), tradeActiveNodes.end(), false);
        std::fill(std::next(tradeActiveNodes.begin(), range.first), std::next(tradeActiveNodes.begin(), range.second),
                  true);
        forwardEvaluation(*g, values, ops_, RandomVariable::deleter, true, opNodeRequirements_, keepNodes,
                          tradeActiveNodes);

        for (auto const& n : tradeModelDependencies)
            tradeActiveNodes[n] = true;
        for (auto const& n : pfPathExposureNodes)
            tradeActiveNodes[n] = true;
        backwardDerivatives(*g, values, derivatives, grads_, RandomVariable::deleter, keepNodesDerivatives,
                            tradeActiveNodes, RandomVariableOpCode::ConditionalExpectation,
                            ops_[RandomVariableOpCode::ConditionalExpectation]);

        for (Size i = range.first; i < range.second; ++i) {
            if (!keepNodes[i])
                RandomVariable::deleter(values[i]);
        }
    }

    boost::timer::nanosecond_type timing12 = timer.elapsed().wall;

    // Roll back derivatives on model part A

    backwardDerivatives(*g, values, derivatives, grads_, RandomVariable::deleter, keepNodesDerivatives, nodesA);

    std::map<std::size_t, double> modelParamDerivatives;
    for (auto const& [n, v] : baseModelParams_) {
        modelParamDerivatives[n] = expectation(derivatives[n]).at(0);
    }

    DLOG("XvaEngineCG: got " << modelParamDerivatives.size() << " model parameter derivatives from run backward derivatives");

    boost::timer::nanosecond_type timing13 = timer.elapsed().wall;

    // Delete values and derivatives vectors, they are not needed from this point on

    values.clear();
    derivatives.clear();

    // 13 Output statistics

    std::cout << "Computation graph size   : " << g->size() << std::endl;
    std::cout << "Peak mem usage           : " << ore::data::os::getPeakMemoryUsageBytes() / 1024 / 1024 << " MB"
              << std::endl;
    std::cout << "T0 market build          : " << timing1 / 1E6 << " ms" << std::endl;
    std::cout << "Sim market build         : " << (timing2 - timing1) / 1E6 << " ms" << std::endl;
    std::cout << "Part A CG build          : " << (timing3 - timing2) / 1E6 << " ms" << std::endl;
    std::cout << "Portfolio build          : " << (timing4 - timing3) / 1E6 << " ms" << std::endl;
    std::cout << "Part B CG build          : " << (timing5 - timing4) / 1E6 << " ms" << std::endl;
    std::cout << "Part C CG build          : " << (timing6 - timing5) / 1E6 << " ms" << std::endl;
    std::cout << "Part D CG build          : " << (timing7 - timing6) / 1E6 << " ms" << std::endl;
    std::cout << "RV gen                   : " << (timing8 - timing7) / 1E6 << " ms" << std::endl;
    std::cout << "Const and Model params   : " << (timing9 - timing8) / 1E6 << " ms" << std::endl;
    std::cout << "Forward eval             : " << (timing10- timing9) / 1E6 << " ms" << std::endl;
    std::cout << "Backward deriv D         : " << (timing11- timing10) / 1E6 << " ms" << std::endl;
    std::cout << "Forward Backward B       : " << (timing12- timing11) / 1E6 << " ms" << std::endl;
    std::cout << "Backward A               : " << (timing13 - timing12) / 1E6 << " ms" << std::endl;
    std::cout << "total                    : " << timing12 / 1E6 << " ms" << std::endl;
}

} // namespace analytics
} // namespace ore
