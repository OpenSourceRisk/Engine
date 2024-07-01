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

#include <orea/app/reportwriter.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/npvsensicube.hpp>
#include <orea/cube/sensitivitycube.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/engine/xvaenginecg.hpp>
#include <orea/scenario/deltascenariofactory.hpp>

#include <ored/report/inmemoryreport.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingenginecg.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/ad/backwardderivatives.hpp>
#include <qle/ad/forwardderivatives.hpp>
#include <qle/ad/forwardevaluation.hpp>
#include <qle/ad/ssaform.hpp>
#include <qle/math/randomvariable_ops.hpp>
#include <qle/methods/multipathvariategenerator.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/weighted_sum.hpp>
#include <boost/timer/timer.hpp>

namespace ore {
namespace analytics {

XvaEngineCG::Mode parseXvaEngineCgMode(const std::string& s) {
    if (s == "Disabled") {
        return XvaEngineCG::Mode::Disabled;
    } else if (s == "CubeGeneration") {
        return XvaEngineCG::Mode::CubeGeneration;
    } else if (s == "Full") {
        return XvaEngineCG::Mode::Full;
    } else {
        QL_FAIL("parseXvaEngineCgMode(" << s << "): not recognized, expected 'CubeGeneration' or 'Full'.");
    }
}

std::ostream& operator<<(std::ostream& os, XvaEngineCG::Mode m) {
    if (m == XvaEngineCG::Mode::Disabled)
        return os << "Disabled";
    else if (m == XvaEngineCG::Mode::CubeGeneration)
        return os << "CubeGeneration";
    else if (m == XvaEngineCG::Mode::Full)
        return os << "Full";
    else
        QL_FAIL(
            "operator<<(" << static_cast<int>(m)
                          << "): enum with this integer representation is not handled. Internal error, contact dev.");
}

namespace {
std::size_t numberOfStochasticRvs(const std::vector<RandomVariable>& v) {
    return std::count_if(v.begin(), v.end(),
                         [](const RandomVariable& r) { return r.initialised() && !r.deterministic(); });
}
} // namespace

XvaEngineCG::~XvaEngineCG() {
    if (externalCalculationId_)
        ComputeEnvironment::instance().context().disposeCalculation(externalCalculationId_);
}

XvaEngineCG::XvaEngineCG(const Mode mode, const Size nThreads, const Date& asof,
                         const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
                         const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                         const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                         const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                         const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                         const QuantLib::ext::shared_ptr<ore::analytics::CrossAssetModelData>& crossAssetModelData,
                         const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGeneratorData>& scenarioGeneratorData,
                         const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                         const string& marketConfiguration, const string& marketConfigurationInCcy,
                         const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData,
                         const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                         const IborFallbackConfig& iborFallbackConfig, const bool bumpCvaSensis,
                         const bool useExternalComputeDevice, const bool externalDeviceCompatibilityMode,
                         const bool useDoublePrecisionForExternalCalculation, const std::string& externalComputeDevice,
                         const bool continueOnCalibrationError, const bool continueOnError, const std::string& context)
    : mode_(mode), asof_(asof), loader_(loader), curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams),
      simMarketData_(simMarketData), engineData_(engineData), crossAssetModelData_(crossAssetModelData),
      scenarioGeneratorData_(scenarioGeneratorData), portfolio_(portfolio), marketConfiguration_(marketConfiguration),
      marketConfigurationInCcy_(marketConfigurationInCcy), sensitivityData_(sensitivityData),
      referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig), bumpCvaSensis_(bumpCvaSensis),
      useExternalComputeDevice_(useExternalComputeDevice),
      externalDeviceCompatibilityMode_(externalDeviceCompatibilityMode),
      useDoublePrecisionForExternalCalculation_(useDoublePrecisionForExternalCalculation),
      externalComputeDevice_(externalComputeDevice), continueOnCalibrationError_(continueOnCalibrationError),
      continueOnError_(continueOnError), context_(context) {

    // Just for performance testing, duplicate the trades in input portfolio as specified by env var N

    if (auto param_N = getenv("XVA_ENGINE_CG_N")) {
        portfolio_ = QuantLib::ext::make_shared<Portfolio>();
        std::string pfxml = portfolio->toXMLString();
        for (Size i = 0; i < atoi(param_N); ++i) {
            auto p = QuantLib::ext::make_shared<Portfolio>();
            p->fromXMLString(pfxml);
            for (auto const& [id, t] : p->trades()) {
                t->id() += "_" + std::to_string(i + 1);
                portfolio_->add(t);
            }
        }
    }
}

void XvaEngineCG::buildT0Market() {
    LOG("XvaEngineCG: build init market");
    boost::timer::cpu_timer timer;
    initMarket_ = QuantLib::ext::make_shared<ore::data::TodaysMarket>(
        asof_, todaysMarketParams_, loader_, curveConfigs_, continueOnError_, true, true, referenceData_, false,
        iborFallbackConfig_, false, true);
    timing_t0_ = timer.elapsed().wall;
}

void XvaEngineCG::buildSsm() {
    LOG("XvaEngineCG: build sim market");

    boost::timer::cpu_timer timer;

    // note: set useSpreadedTermStructures == true here even if sensi config does not have that
    simMarket_ = QuantLib::ext::make_shared<ore::analytics::ScenarioSimMarket>(
        initMarket_, simMarketData_, marketConfiguration_, *curveConfigs_, *todaysMarketParams_, continueOnError_, true,
        false, false, iborFallbackConfig_, true);

    simMarketObs_ = static_pointer_cast<ore::data::Market>(simMarket_);

    timing_ssm_ = timer.elapsed().wall;
}

void XvaEngineCG::buildCam() {
    LOG("XvaEngineCG: build cam model builder");

    boost::timer::cpu_timer timer;

    // note: sim market has one config only, no in-ccy config to calibrate IR components
    camBuilder_ = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        simMarketObs_, crossAssetModelData_, marketConfigurationInCcy_, marketConfiguration_, marketConfiguration_,
        marketConfiguration_, marketConfiguration_, marketConfiguration_, false, continueOnCalibrationError_,
        std::string(), SalvagingAlgorithm::Spectral, "xva engine cg - cam builder");

    // Set up gaussian cam cg model

    LOG("XvaEngineCG: build cam cg model");

    QL_REQUIRE(
        crossAssetModelData_->discretization() == CrossAssetModel::Discretization::Euler,
        "XvaEngineCG: cam is required to use discretization 'Euler', please update simulation parameters accordingly.");

    std::vector<std::string> currencies;                                                         // from cam
    std::vector<Handle<YieldTermStructure>> curves;                                              // from cam
    std::vector<Handle<Quote>> fxSpots;                                                          // from cam
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>> irIndices; // from trade building
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>
        infIndices;                           // from trade building
    std::vector<std::string> indices;         // from trade building
    std::vector<std::string> indexCurrencies; // from trade building

    // note: for the PoC we populate the containers with hardcoded values ... temp hack ...
    currencies.push_back("EUR");
    curves.push_back(camBuilder_->model()->irModel(0)->termStructure());
    irIndices.push_back(std::make_pair("EUR-EURIBOR-6M", *simMarket_->iborIndex("EUR-EURIBOR-6M")));

    // note: these must be fine enough for Euler, e.g. weekly over the whole simulation period
    simulationDates_ = std::set<Date>(scenarioGeneratorData_->getGrid()->dates().begin(),
                                      scenarioGeneratorData_->getGrid()->dates().end());
    // note: this should be added to CrossAssetModelData
    Size timeStepsPerYear = 1;

    // note: projectedStateProcessIndices can be removed from GaussianCamCG constructor most probably?
    model_ = QuantLib::ext::make_shared<GaussianCamCG>(
        camBuilder_->model(), scenarioGeneratorData_->samples(), currencies, curves, fxSpots, irIndices, infIndices,
        indices, indexCurrencies, simulationDates_, timeStepsPerYear, iborFallbackConfig_, std::vector<Size>(),
        std::vector<std::string>(), true);

    timing_parta_ = timer.elapsed().wall;
}

void XvaEngineCG::buildPortfolio() {
    LOG("XvaEngineCG: build trades (" << portfolio_->size() << ") against global cam cg model");

    boost::timer::cpu_timer timer;

    QL_REQUIRE(engineData_, "XvaEngineCG: no engine data given.");
    auto edCopy = QuantLib::ext::make_shared<EngineData>(*engineData_);
    edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = marketConfigurationInCcy_;
    configurations[MarketContext::fxCalibration] = marketConfiguration_;
    configurations[MarketContext::pricing] = marketConfiguration_;
    auto factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, simMarket_, configurations, referenceData_, iborFallbackConfig_,
        EngineBuilderFactory::instance().generateAmcCgEngineBuilders(model_,
                                                                     scenarioGeneratorData_->getGrid()->dates()),
        true);

    portfolio_->build(factory, "xva engine cg", true);

    timing_pf_ = timer.elapsed().wall;
}

void XvaEngineCG::buildCgPartB() {
    LOG("XvaEngineCG: build computation graph for all trades");

    // Build computation graph for all trades ("part B") and
    // - store npv, amc npv nodes

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    for (auto const& [id, trade] : portfolio_->trades()) {
        auto engine = QuantLib::ext::dynamic_pointer_cast<AmcCgPricingEngine>(
            trade->instrument()->qlInstrument()->pricingEngine());
        QL_REQUIRE(engine,
                   "XvaEngineCG: expected to get AmcCgPricingEngine, trade '" << id << "' has a different engine.");
        g->startRedBlock();
        // trigger setupArguments
        if (!trade->instrument()->qlInstrument()->isCalculated())
            trade->instrument()->qlInstrument()->recalculate();
        engine->buildComputationGraph();
        std::vector<std::size_t> tmp;
        tmp.push_back(g->variable(engine->npvName() + "_0"));
        for (std::size_t i = 0; i < simulationDates_.size(); ++i) {
            tmp.push_back(g->variable("_AMC_NPV_" + std::to_string(i)));
        }
        amcNpvNodes_.push_back(tmp);
        g->endRedBlock();
    }

    timing_partb_ = timer.elapsed().wall;
}

void XvaEngineCG::buildCgPartC() {
    LOG("XvaEngineCG: add exposure nodes to graph");

    // Add nodes that sum the exposure over trades, both pathwise and conditional expectations
    // This constitutes part C of the computation graph spanning "trade m range end ... lastExposureNode"
    // - pfPathExposureNodes: path amc sim values, aggregated over trades
    // - pfExposureNodes:     the corresponding conditional expectations

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    std::vector<std::size_t> pfPathExposureNodes;
    std::vector<std::size_t> tradeSum(portfolio_->trades().size());
    for (Size i = 0; i < simulationDates_.size() + 1; ++i) {
        for (Size j = 0; j < portfolio_->trades().size(); ++j) {
            tradeSum[j] = amcNpvNodes_[j][i];
        }
        pfPathExposureNodes.push_back(cg_add(*g, tradeSum));
        pfExposureNodes_.push_back(model_->npv(
            pfPathExposureNodes.back(), i == 0 ? model_->referenceDate() : *std::next(simulationDates_.begin(), i - 1),
            cg_const(*g, 1.0), boost::none, ComputationGraph::nan, ComputationGraph::nan));
    }

    timing_partc_ = timer.elapsed().wall;
}

void XvaEngineCG::buildCgPP() {
    LOG("XvaEngineCG: add cg post processor");

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    // Add post processor
    // This constitues part D of the computation graph from lastExposureNode ... g->size()
    // The cvaNode is the ultimate result w.r.t. which we want to compute sensitivities

    // note: very simplified calculation, for testing, just multiply the EPE on each date by fixed default prob
    auto defaultCurve = simMarket_->defaultCurve("BANK")->curve();
    model_->registerWith(defaultCurve);
    cvaNode_ = cg_const(*g, 0.0);
    for (Size i = 0; i < simulationDates_.size(); ++i) {
        Date d = i == 0 ? model_->referenceDate() : *std::next(simulationDates_.begin(), i - 1);
        Date e = *std::next(simulationDates_.begin(), i);
        std::size_t defaultProb =
            addModelParameter(*g, model_->modelParameterFunctors(), "__defaultprob_" + std::to_string(i),
                              [defaultCurve, d, e]() { return defaultCurve->defaultProbability(d, e); });
        cvaNode_ = cg_add(*g, cvaNode_, cg_mult(*g, defaultProb, cg_max(*g, pfExposureNodes_[i], cg_const(*g, 0.0))));
    }

    timing_partd_ = timer.elapsed().wall;
}

void XvaEngineCG::getExternalContext() {
    LOG("XvaEngineCG: get external context");
    if (useExternalComputeDevice_) {
        ComputeEnvironment::instance().selectContext(externalComputeDevice_);
        externalComputeDeviceSettings_.debug = false;
        externalComputeDeviceSettings_.useDoublePrecision = useDoublePrecisionForExternalCalculation_;
        externalComputeDeviceSettings_.rngSequenceType = scenarioGeneratorData_->sequenceType();
        externalComputeDeviceSettings_.rngSeed = scenarioGeneratorData_->seed();
        externalComputeDeviceSettings_.regressionOrder = 4;
        externalCalculationId_ =
            ComputeEnvironment::instance()
                .context()
                .initiateCalculation(model_->size(), externalCalculationId_, 0, externalComputeDeviceSettings_)
                .first;
        DLOG("XvaEngineCG: initiated new external calculation id " << externalCalculationId_);
    }
}

void XvaEngineCG::setupValueContainers() {
    auto g = model_->computationGraph();
    values_ = std::vector<RandomVariable>(g->size(), RandomVariable(model_->size(), 0.0));
    derivatives_ = std::vector<RandomVariable>(g->size(), RandomVariable(model_->size(), 0.0));
    if (useExternalComputeDevice_)
        valuesExternal_ = std::vector<ExternalRandomVariable>(g->size());
}

void XvaEngineCG::doForwardEvaluation() {

    LOG("XvaEngineCG: do forward evaluation");

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    // Populate constants and model parameters

    baseModelParams_ = model_->modelParameters();
    populateConstants(values_, valuesExternal_);
    populateModelParameters(baseModelParams_, values_, valuesExternal_);
    timing_popparam_ = timer.elapsed().wall;

    // Populate random variates

    populateRandomVariates(values_, valuesExternal_);
    timing_poprv_ = timer.elapsed().wall - timing_popparam_;

    rvMemMax_ = numberOfStochasticRvs(values_) + numberOfStochasticRvs(derivatives_);

    // Do a forward evaluation, keep the following values nodes
    // - constants
    // - model parameters
    // - values needed for derivatives (except in red blocks, by their definition)
    // - red block dependencies
    // - the random variates for bump sensis
    // - the pfExposureNodes to dump out the epe profile

    opNodeRequirements_ = getRandomVariableOpNodeRequirements();
    Real eps = 0.0; // smoothing parameter for indicator functions
    if (useExternalComputeDevice_) {
        opsExternal_ = getExternalRandomVariableOps();
        gradsExternal_ = getExternalRandomVariableGradients();
    } else {
        ops_ = getRandomVariableOps(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial, bumpCvaSensis_ ? eps : 0.0,
                                    Null<Real>()); // todo set regression variance cutoff
        grads_ = getRandomVariableGradients(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial, eps);
    }

    keepNodes_ = std::vector<bool>(g->size(), false);

    if (sensitivityData_) {

        // do we need to keep the next 3 for both backward derivatives and bump sensis?

        for (auto const& c : g->constants()) {
            keepNodes_[c.second] = true;
        }

        for (auto const& [n, v] : baseModelParams_) {
            keepNodes_[n] = true;
        }

        for (auto const n : g->redBlockDependencies()) {
            keepNodes_[n] = true;
        }

        // make sure we can revalue for bump sensis

        if (bumpCvaSensis_) {
            for (auto const& rv : model_->randomVariates())
                for (auto const& v : rv)
                    keepNodes_[v] = true;
        }
    }

    for (auto const& n : pfExposureNodes_) {
        keepNodes_[n] = true;
    }

    if (cvaNode_ != ComputationGraph::nan)
        keepNodes_[cvaNode_] = true;

    std::vector<bool> rvOpAllowsPredeletion = QuantExt::getRandomVariableOpAllowsPredeletion();

    std::vector<std::vector<double>> externalOutput;
    std::vector<double*> externalOutputPtr;
    if (useExternalComputeDevice_) {
        forwardEvaluation(*g, valuesExternal_, opsExternal_, ExternalRandomVariable::deleter, !bumpCvaSensis_,
                          opNodeRequirements_, keepNodes_, 0, ComputationGraph::nan, false,
                          ExternalRandomVariable::preDeleter, rvOpAllowsPredeletion);
        for (Size i = 0; i < pfExposureNodes_.size(); ++i) {
            valuesExternal_[pfExposureNodes_[i]].declareAsOutput();
        }
        if (cvaNode_ != ComputationGraph::nan)
            valuesExternal_[cvaNode_].declareAsOutput();
        externalOutput.resize(pfExposureNodes_.size() + 1, std::vector<double>(model_->size()));
        externalOutputPtr.resize(externalOutput.size());
        std::transform(externalOutput.begin(), externalOutput.end(), externalOutputPtr.begin(),
                       [](std::vector<double>& v) { return &v[0]; });
        ComputeEnvironment::instance().context().finalizeCalculation(externalOutputPtr);
        // could skip this and use externalOutput directly below, but it's more convenient to copy the results to values
        for (Size i = 0; i < pfExposureNodes_.size(); ++i) {
            values_[pfExposureNodes_[i]] = RandomVariable(model_->size(), externalOutputPtr[i]);
        }
        if (cvaNode_ != ComputationGraph::nan)
            values_[cvaNode_] = RandomVariable(model_->size(), externalOutputPtr.back());
    } else {
        forwardEvaluation(*g, values_, ops_, RandomVariable::deleter, !bumpCvaSensis_, opNodeRequirements_, keepNodes_);
    }

    rvMemMax_ = std::max(rvMemMax_, numberOfStochasticRvs(values_) + numberOfStochasticRvs(derivatives_));
    timing_fwd_ = timer.elapsed().wall - timing_poprv_;
}

void XvaEngineCG::populateAsd() {
    // todo
}

void XvaEngineCG::populateNpvOutputCube() {
    // todo
}

void XvaEngineCG::generateXvaReports() {
    LOG("XvaEngineCG: Write epe report.");
    epeReport_ = QuantLib::ext::make_shared<InMemoryReport>();
    epeReport_->addColumn("Date", Date()).addColumn("EPE", double(), 4).addColumn("ENE", double(), 4);

    for (Size i = 0; i < simulationDates_.size() + 1; ++i) {
        epeReport_->next();
        epeReport_->add(i == 0 ? model_->referenceDate() : *std::next(simulationDates_.begin(), i - 1))
            .add(expectation(max(values_[pfExposureNodes_[i]], RandomVariable(model_->size(), 0.0))).at(0))
            .add(expectation(max(-values_[pfExposureNodes_[i]], RandomVariable(model_->size(), 0.0))).at(0));
    }
    epeReport_->end();
}

void XvaEngineCG::calculateSensitivities() {
    LOG("XvaEngineCG: calculate sensitivities.");

    QL_REQUIRE(cvaNode_ != ComputationGraph::nan,
               "XvaEngineCG::calculateSensitivities(): no cva node set, internal error.");

    Real cva = expectation(values_[cvaNode_]).at(0);
    DLOG("XvaEngineCG: Calcuated CVA (node " << cvaNode_ << ") = " << cva);

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    if (sensitivityData_) {

        LOG("XvaEngineCG: Calculate sensitivities (bump = " << std::boolalpha << bumpCvaSensis_ << ")");

        // Do backward derivatives run

        std::vector<double> modelParamDerivatives(baseModelParams_.size());

        if (!bumpCvaSensis_) {

            LOG("XvaEngineCG: run backward derivatives");

            derivatives_[cvaNode_] = RandomVariable(model_->size(), 1.0);

            std::vector<bool> keepNodesDerivatives(g->size(), false);

            for (auto const& [n, _] : baseModelParams_)
                keepNodesDerivatives[n] = true;

            // backward derivatives run

            backwardDerivatives(*g, values_, derivatives_, grads_, RandomVariable::deleter, keepNodesDerivatives, ops_,
                                opNodeRequirements_, keepNodes_, RandomVariableOpCode::ConditionalExpectation,
                                ops_[RandomVariableOpCode::ConditionalExpectation]);

            // read model param derivatives

            Size i = 0;
            for (auto const& [n, v] : baseModelParams_) {
                modelParamDerivatives[i++] = expectation(derivatives_[n]).at(0);
            }

            // get mem consumption

            rvMemMax_ = std::max(rvMemMax_, numberOfStochasticRvs(values_) + numberOfStochasticRvs(derivatives_));

            LOG("XvaEngineCG: got " << modelParamDerivatives.size()
                                    << " model parameter derivatives from run backward derivatives");

            timing_bwd_ = timer.elapsed().wall;

            // Delete values and derivatives vectors, they are not needed from this point on
            // except we are doing a full revaluation!

            values_.clear();
            derivatives_.clear();
        }

        // generate sensitivity scenarios

        LOG("XvaEngineCG: running sensi scenarios");

        sensiScenarioGenerator_ = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
            sensitivityData_, simMarket_->baseScenario(), simMarketData_, simMarket_,
            QuantLib::ext::make_shared<DeltaScenarioFactory>(simMarket_->baseScenario()), false, std::string(),
            continueOnError_, simMarket_->baseScenarioAbsolute());

        simMarket_->scenarioGenerator() = sensiScenarioGenerator_;

        sensiResultCube_ = QuantLib::ext::make_shared<DoublePrecisionSensiCube>(std::set<std::string>{"CVA"}, asof_,
                                                                                sensiScenarioGenerator_->samples());
        sensiResultCube_->setT0(cva, 0, 0);

        model_->alwaysForwardNotifications();

        std::vector<std::vector<double>> externalOutput;
        std::vector<double*> externalOutputPtr;
        if (useExternalComputeDevice_) {
            externalOutput.resize(pfExposureNodes_.size() + 1, std::vector<double>(model_->size()));
            externalOutputPtr.resize(externalOutput.size());
        }

        Size activeScenarios = 0;
        for (Size sample = 0; sample < sensiResultCube_->samples(); ++sample) {

            // update sim market to next scenario

            simMarket_->preUpdate();
            simMarket_->updateScenario(asof_);
            simMarket_->postUpdate(asof_, false);

            // recalibrate the model

            camBuilder_->recalibrate();

            Real sensi = 0.0;

            // calculate sensi if model was notified of a change

            if (!model_->isCalculated()) {

                model_->calculate();
                ++activeScenarios;

                if (!bumpCvaSensis_) {

                    // calcuate CVA sensi using ad derivatives

                    auto modelParameters = model_->modelParameters();
                    Size i = 0;
                    boost::accumulators::accumulator_set<
                        double, boost::accumulators::stats<boost::accumulators::tag::weighted_sum>, double>
                        acc;
                    for (auto const& [n, v0] : baseModelParams_) {
                        Real v1 = modelParameters[i].second;
                        acc(modelParamDerivatives[i], boost::accumulators::weight = (v1 - v0));
                        ++i;
                    }
                    sensi = boost::accumulators::weighted_sum(acc);

                } else {

                    // calcuate CVA sensi doing full recalc of CVA

                    if (useExternalComputeDevice_) {
                        ComputeEnvironment::instance().context().initiateCalculation(
                            model_->size(), externalCalculationId_, 0, externalComputeDeviceSettings_);
                        populateConstants(values_, valuesExternal_);
                        populateModelParameters(model_->modelParameters(), values_, valuesExternal_);
                        ComputeEnvironment::instance().context().finalizeCalculation(externalOutputPtr);
                        values_[cvaNode_] = RandomVariable(model_->size(), externalOutputPtr.back());
                    } else {
                        populateModelParameters(model_->modelParameters(), values_, valuesExternal_);
                        forwardEvaluation(*g, values_, ops_, RandomVariable::deleter, true, opNodeRequirements_,
                                          keepNodes_);
                    }
                    sensi = expectation(values_[cvaNode_]).at(0) - cva;
                }
            }

            // set result in cube

            sensiResultCube_->set(cva + sensi, 0, 0, sample, 0);
        }

        timing_sensi_ = timer.elapsed().wall - timing_bwd_;

        LOG("XvaEngineCG: finished running " << sensiResultCube_->samples() << " sensi scenarios, thereof "
                                             << activeScenarios << " active.");
    } // if sensi data is given
}

void XvaEngineCG::generateSensiReports() {
    LOG("XvaEngineCG: write sensi report.");
    sensiReport_ = QuantLib::ext::make_shared<InMemoryReport>();
    auto sensiCube = QuantLib::ext::make_shared<SensitivityCube>(
        sensiResultCube_, sensiScenarioGenerator_->scenarioDescriptions(), sensiScenarioGenerator_->shiftSizes(),
        sensiScenarioGenerator_->shiftSizes(), sensiScenarioGenerator_->shiftSchemes());
    auto sensiStream = QuantLib::ext::make_shared<SensitivityCubeStream>(sensiCube, simMarketData_->baseCcy());
    ReportWriter().writeScenarioReport(*sensiReport_, {sensiCube}, 0.0);
}

void XvaEngineCG::cleanUpAfterCalcs() {
    values_.clear();
    derivatives_.clear();
    valuesExternal_.clear();
}

void XvaEngineCG::outputGraphStats() {
    auto g = model_->computationGraph();
    LOG("XvaEngineCG: graph building complete, size is " << g->size());
    LOG("XvaEngineCG: got " << g->redBlockDependencies().size() << " red block dependencies.");
    numberOfRedNodes_ = 0;
    for (auto const& r : g->redBlockRanges()) {
        DLOG("XvaEngineCG: red block range " << r.first << " ... " << r.second);
        numberOfRedNodes_ += r.second - r.first;
    }
}

void XvaEngineCG::outputTimings() {
    LOG("XvaEngineCG: graph size               : " << model_->computationGraph()->size());
    LOG("XvaEngineCG: red nodes                : " << numberOfRedNodes_);
    LOG("XvaEngineCG: red node dependendices   : " << model_->computationGraph()->redBlockDependencies().size());
    LOG("XvaEngineCG: Peak mem usage           : " << ore::data::os::getPeakMemoryUsageBytes() / 1024 / 1024 << " MB");
    LOG("XvaEngineCG: Peak theoretical rv mem  : " << static_cast<double>(rvMemMax_) / 1024 / 1024 * 8 * model_->size()
                                                   << " MB");
    LOG("XvaEngineCG: T0 market build          : " << std::fixed << std::setprecision(1) << timing_t0_ / 1E6 << " ms");
    LOG("XvaEngineCG: Sim market build         : " << std::fixed << std::setprecision(1) << timing_ssm_ / 1E6 << " ms");
    LOG("XvaEngineCG: Part A CG build          : " << std::fixed << std::setprecision(1) << timing_parta_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Portfolio build          : " << std::fixed << std::setprecision(1) << timing_pf_ / 1E6 << " ms");
    LOG("XvaEngineCG: Part B CG build          : " << std::fixed << std::setprecision(1) << timing_partb_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Part C CG build          : " << std::fixed << std::setprecision(1) << timing_partc_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Part D CG build          : " << std::fixed << std::setprecision(1) << timing_partd_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Const and Model params   : " << std::fixed << std::setprecision(1) << timing_popparam_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: RV gen                   : " << std::fixed << std::setprecision(1) << timing_poprv_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Forward eval             : " << std::fixed << std::setprecision(1) << timing_fwd_ / 1E6 << " ms");
    LOG("XvaEngineCG: Backward deriv           : " << std::fixed << std::setprecision(1) << timing_bwd_ / 1E6 << " ms");
    LOG("XvaEngineCG: Sensi Cube Gen           : " << std::fixed << std::setprecision(1) << timing_sensi_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: total                    : " << std::fixed << std::setprecision(1) << timing_total_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: all done.");
}

void XvaEngineCG::run() {

    LOG("XvaEngineCG::run(): firstRun is " << std::boolalpha << firstRun_);
    boost::timer::cpu_timer timer;

    if (firstRun_) {
        buildT0Market();
    }

    if (firstRun_ || offsetScenario_.get() != simMarket_->offsetScenario().get()) {
        buildSsm();
    }

    if (firstRun_) {
        buildCam();
        buildPortfolio();
        buildCgPartB();
        buildCgPartC();
    }

    if (firstRun_ && mode_ == Mode::Full) {
        buildCgPP();
    }

    if (firstRun_) {
        outputGraphStats();
    }

    getExternalContext();

    setupValueContainers();
    doForwardEvaluation();

    populateAsd();
    populateNpvOutputCube();

    if (mode_ == Mode::Full) {
        generateXvaReports();
        calculateSensitivities();
        generateSensiReports();
    }

    outputTimings();
}

void XvaEngineCG::setOffsetScenario(const QuantLib::ext::shared_ptr<Scenario>& offsetScenario) {
    offsetScenario_ = offsetScenario;
}

void XvaEngineCG::setAggregationScenarioData(
    const QuantLib::ext::shared_ptr<ore::analytics::AggregationScenarioData>& asd) {
    asd_ = asd;
}

void XvaEngineCG::setNpvOutputCube(const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& npvOutputCube) {
    npvOutputCube_ = npvOutputCube;
}

void XvaEngineCG::populateRandomVariates(std::vector<RandomVariable>& values,
                                         std::vector<ExternalRandomVariable>& valuesExternal) const {

    DLOG("XvaEngineCG: populate random variates");

    auto const& rv = model_->randomVariates();
    if (!rv.empty()) {
        if (useExternalComputeDevice_) {
            auto gen = ComputeEnvironment::instance().context().createInputVariates(rv.size(), rv.front().size());
            for (Size k = 0; k < rv.size(); ++k) {
                for (Size j = 0; j < rv.front().size(); ++j)
                    valuesExternal[rv[k][j]] = ExternalRandomVariable(gen[k][j]);
            }
        } else {
            if (scenarioGeneratorData_->sequenceType() == QuantExt::SequenceType::MersenneTwister &&
                externalDeviceCompatibilityMode_) {
                // use same order for rng generation as it is (usually) done on external devices
                // this is mainly done to be able to reconcile results produced on external devices
                auto rng = std::make_unique<MersenneTwisterUniformRng>(scenarioGeneratorData_->seed());
                QuantLib::InverseCumulativeNormal icn;
                for (Size j = 0; j < rv.front().size(); ++j) {
                    for (Size i = 0; i < rv.size(); ++i) {
                        for (Size path = 0; path < model_->size(); ++path) {
                            values[rv[i][j]].set(path, icn(rng->nextReal()));
                        }
                    }
                }
            } else {
                // use the 'usual' path generation that we also use elsewhere
                auto gen =
                    makeMultiPathVariateGenerator(scenarioGeneratorData_->sequenceType(), rv.size(), rv.front().size(),
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
            }
            DLOG("XvaEngineCG: generated rvs for " << rv.size() << " underlyings and " << rv.front().size()
                                                   << " time steps.");
        }
    }
}

void XvaEngineCG::populateConstants(std::vector<RandomVariable>& values,
                                    std::vector<ExternalRandomVariable>& valuesExternal) const {

    DLOG("XvaEngineCG: populate constants");

    auto g = model_->computationGraph();
    for (auto const& c : g->constants()) {
        if (useExternalComputeDevice_) {
            valuesExternal[c.second] = ExternalRandomVariable(c.first);
        } else {
            values[c.second] = RandomVariable(model_->size(), c.first);
        }
    }

    DLOG("XvaEngineCG: set " << g->constants().size() << " constants");
}

void XvaEngineCG::populateModelParameters(const std::vector<std::pair<std::size_t, double>>& modelParameters,
                                          std::vector<RandomVariable>& values,
                                          std::vector<ExternalRandomVariable>& valuesExternal) const {

    DLOG("XvaEngineCG: populate model parameters");

    for (auto const& [n, v] : modelParameters) {
        if (useExternalComputeDevice_) {
            valuesExternal[n] = ExternalRandomVariable(v);
        } else {
            values[n] = RandomVariable(model_->size(), v);
        }
    }

    DLOG("XvaEngineCG: set " << modelParameters.size() << " model parameters.");
}

} // namespace analytics
} // namespace ore
