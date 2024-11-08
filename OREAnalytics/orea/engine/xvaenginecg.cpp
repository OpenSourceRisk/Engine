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
      continueOnError_(continueOnError), context_(context) {}

void XvaEngineCG::buildT0Market() {
    DLOG("XvaEngineCG: build init market");
    boost::timer::cpu_timer timer;
    initMarket_ = QuantLib::ext::make_shared<ore::data::TodaysMarket>(
        asof_, todaysMarketParams_, loader_, curveConfigs_, continueOnError_, true, true, referenceData_, false,
        iborFallbackConfig_, false, true);
    timing_t0_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build init market done");
}

void XvaEngineCG::buildSsm() {
    DLOG("XvaEngineCG: build sim market");

    boost::timer::cpu_timer timer;

    // note: set useSpreadedTermStructures == true here even if sensi config does not have that
    simMarket_ = QuantLib::ext::make_shared<ore::analytics::ScenarioSimMarket>(
        initMarket_, simMarketData_, marketConfiguration_, *curveConfigs_, *todaysMarketParams_, continueOnError_, true,
        false, false, iborFallbackConfig_, true);

    simMarketObs_ = static_pointer_cast<ore::data::Market>(simMarket_);

    timing_ssm_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build sim market done");
}

void XvaEngineCG::buildCam() {
    DLOG("XvaEngineCG: build cam model builder");

    boost::timer::cpu_timer timer;

    // note: sim market has one config only, no in-ccy config to calibrate IR components
    camBuilder_ = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
	simMarketObs_, crossAssetModelData_, marketConfigurationInCcy_, marketConfiguration_, marketConfiguration_,
        marketConfiguration_, marketConfiguration_, marketConfiguration_, false, continueOnCalibrationError_,
        std::string(), "xva engine cg - cam builder");

    // Set up gaussian cam cg model

    DLOG("XvaEngineCG: build cam cg model");

    QL_REQUIRE(
        crossAssetModelData_->discretization() == CrossAssetModel::Discretization::Euler,
        "XvaEngineCG: cam is required to use discretization 'Euler', please update simulation parameters accordingly.");

    std::vector<std::string> currencies(1, crossAssetModelData_->domesticCurrency());
    std::vector<Handle<YieldTermStructure>> curves;
    std::vector<Handle<Quote>> fxSpots;
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>> irIndices;
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>> infIndices;
    std::vector<std::string> indices;
    std::vector<std::string> indexCurrencies;

    for (auto const& ccy : crossAssetModelData_->currencies()) {
        if (ccy != crossAssetModelData_->domesticCurrency())
            currencies.push_back(ccy);
    }

    QL_REQUIRE(!currencies.empty(),
               "XvaEngineCG::buildCam(): check simulation setup, there has to be one currency at least.");

    for (auto const& ccy : currencies) {
        curves.push_back(simMarket_->discountCurve(ccy));
    }

    for (Size i = 1; i < currencies.size(); ++i) {
        fxSpots.push_back(simMarket_->fxSpot(currencies[i] + currencies[0]));
        // we provide them, although we probably do not really need to
        indices.push_back("FX-GENERIC-" + currencies[i] + "-" + currencies[0]);
        indexCurrencies.push_back(currencies[i]);
    }

    for (auto const& ind : simMarketData_->indices()) {
        irIndices.push_back(std::make_pair(ind, *simMarket_->iborIndex(ind)));
    }

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
    // this is actually necessary, FIXME why? There is a calculate() missing in the model impl. then?
    model_->calculate();

    timing_parta_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build cam cg model done - graph size is " << model_->computationGraph()->size());
}

void XvaEngineCG::buildPortfolio() {
    DLOG("XvaEngineCG: build trades (" << portfolio_->size() << ").");

    boost::timer::cpu_timer timer;

    QL_REQUIRE(engineData_, "XvaEngineCG: no engine data given.");
    auto edCopy = QuantLib::ext::make_shared<EngineData>(*engineData_);
    edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = marketConfigurationInCcy_;
    configurations[MarketContext::fxCalibration] = marketConfiguration_;
    configurations[MarketContext::pricing] = marketConfiguration_;

    std::vector<Date> simDates, stickyCloseOutDates;
    if (scenarioGeneratorData_->withMporStickyDate()) {
        simDates = scenarioGeneratorData_->getGrid()->valuationDates();
        stickyCloseOutDates = scenarioGeneratorData_->getGrid()->closeOutDates();
    } else {
        simDates = scenarioGeneratorData_->getGrid()->dates();
    }

    auto factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, simMarket_, configurations, referenceData_, iborFallbackConfig_,
        EngineBuilderFactory::instance().generateAmcCgEngineBuilders(model_, simDates, stickyCloseOutDates), true);

    portfolio_->build(factory, "xva engine cg", true);

    timing_pf_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build trades (" << portfolio_->size() << ") done.");
}

void XvaEngineCG::buildCgPartB() {
    DLOG("XvaEngineCG: build computation graph for all trades");

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
    DLOG("XvaEngineCG: build computation graph for all trades done - graph size is "
         << model_->computationGraph()->size());
}

void XvaEngineCG::buildCgPartC() {
    DLOG("XvaEngineCG: add exposure nodes to graph");

    // Add nodes that sum the exposure over trades and add conditional expectations on pf level
    // Optionally, add conditional expectations on trade level (if we have to populate a legacy npv cube)
    // This constitutes part C of the computation graph spanning "trade m range end ... lastExposureNode"
    // - pfExposureNodes    :     the conditional expectations on pf level
    // - tradeExposureNodes :     the conditional expectations on trade level

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    std::vector<std::size_t> tradeSum(portfolio_->trades().size());
    for (Size i = 0; i < simulationDates_.size() + 1; ++i) {
        for (Size j = 0; j < portfolio_->trades().size(); ++j) {
            tradeSum[j] = amcNpvNodes_[j][i];
        }
        pfExposureNodes_.push_back(model_->npv(
            cg_add(*g, tradeSum), i == 0 ? model_->referenceDate() : *std::next(simulationDates_.begin(), i - 1),
            cg_const(*g, 1.0), boost::none, ComputationGraph::nan, ComputationGraph::nan));
    }

    if (generateTradeLevelExposure_) {
        for (Size i = 0; i < simulationDates_.size() + 1; ++i) {
            tradeExposureNodes_.push_back(std::vector<std::size_t>(portfolio_->trades().size()));
            for (Size j = 0; j < portfolio_->trades().size(); ++j) {
                tradeExposureNodes_.back()[j] = model_->npv(
                    amcNpvNodes_[j][i], i == 0 ? model_->referenceDate() : *std::next(simulationDates_.begin(), i - 1),
                    cg_const(*g, 1.0), boost::none, ComputationGraph::nan, ComputationGraph::nan);
            }
        }
    }

    timing_partc_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: add exposure nodes to graph done - graph size is "
         << g->size() << ", generateTradeLevelExposure = " << std::boolalpha << generateTradeLevelExposure_);
}

void XvaEngineCG::buildCgPP() {
    DLOG("XvaEngineCG: add cg post processor");

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
    DLOG("XvaEngineCG: add cg post processor done - graph size is " << g->size());
}

void XvaEngineCG::getExternalContext() {
    DLOG("XvaEngineCG: get external context");
    if (useExternalComputeDevice_) {
        ComputeEnvironment::instance().selectContext(externalComputeDevice_);
        externalComputeDeviceSettings_.debug = false;
        externalComputeDeviceSettings_.useDoublePrecision = useDoublePrecisionForExternalCalculation_;
        externalComputeDeviceSettings_.rngSequenceType = scenarioGeneratorData_->sequenceType();
        externalComputeDeviceSettings_.rngSeed = scenarioGeneratorData_->seed();
        externalComputeDeviceSettings_.regressionOrder = 4;
        bool newCalc;
        std::tie(externalCalculationId_, newCalc) = ComputeEnvironment::instance().context().initiateCalculation(
            model_->size(), externalCalculationId_, 0, externalComputeDeviceSettings_);
        DLOG("XvaEngineCG: initiated external calculation id " << externalCalculationId_ << ", newCalc = " << newCalc
                                                               << ", firstRun = " << firstRun_);
        QL_REQUIRE(newCalc == firstRun_, "XVaEngineCG::getExternalContext(): firstRun_ ("
                                             << std::boolalpha << firstRun_ << ") and newCalc (" << newCalc
                                             << ") are not consistent. Internal error.");
    }
}

void XvaEngineCG::setupValueContainers() {
    DLOG("XvaEngineCG: setup value containers");
    auto g = model_->computationGraph();
    values_ = std::vector<RandomVariable>(g->size(), RandomVariable(model_->size(), 0.0));
    derivatives_ = std::vector<RandomVariable>(g->size(), RandomVariable(model_->size(), 0.0));
    if (useExternalComputeDevice_)
        valuesExternal_ = std::vector<ExternalRandomVariable>(g->size());
}

void XvaEngineCG::finalizeExternalCalculation() {
    std::vector<std::vector<double>> externalOutput(externalOutputNodes_.size(), std::vector<double>(model_->size()));
    std::vector<double*> externalOutputPtr(externalOutputNodes_.size());
    std::transform(externalOutput.begin(), externalOutput.end(), externalOutputPtr.begin(),
                   [](std::vector<double>& v) { return &v[0]; });
    ComputeEnvironment::instance().context().finalizeCalculation(externalOutputPtr);
    std::size_t i = 0;
    for (auto const& n : externalOutputNodes_) {
        values_[n] = RandomVariable(model_->size(), externalOutputPtr[i++]);
    }
}

void XvaEngineCG::doForwardEvaluation() {

    DLOG("XvaEngineCG: do forward evaluation");

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
        ops_ = getRandomVariableOps(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial,
                                    (sensitivityData_ && bumpCvaSensis_) ? eps : 0.0,
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

    if (generateTradeLevelExposure_) {
        for (auto const& n : tradeExposureNodes_) {
            for (auto const& m : n) {
                keepNodes_[m] = true;
            }
        }
    }

    if (cvaNode_ != ComputationGraph::nan)
        keepNodes_[cvaNode_] = true;

    for (auto const& n : asdNumeraire_)
        keepNodes_[n] = true;

    for (auto const& n : asdFx_)
        keepNodes_[n] = true;

    for (auto const& n : asdIndex_)
        keepNodes_[n] = true;

    std::vector<bool> rvOpAllowsPredeletion = QuantExt::getRandomVariableOpAllowsPredeletion();

    if (useExternalComputeDevice_) {
        if (firstRun_) {
            forwardEvaluation(*g, valuesExternal_, opsExternal_, ExternalRandomVariable::deleter,
                              !bumpCvaSensis_ && sensitivityData_, opNodeRequirements_, keepNodes_, 0,
                              ComputationGraph::nan, false, ExternalRandomVariable::preDeleter, rvOpAllowsPredeletion);
            externalOutputNodes_.insert(externalOutputNodes_.end(), pfExposureNodes_.begin(), pfExposureNodes_.end());
            externalOutputNodes_.insert(externalOutputNodes_.end(), asdNumeraire_.begin(), asdNumeraire_.end());
            externalOutputNodes_.insert(externalOutputNodes_.end(), asdFx_.begin(), asdFx_.end());
            externalOutputNodes_.insert(externalOutputNodes_.end(), asdIndex_.begin(), asdIndex_.end());
            if (generateTradeLevelExposure_) {
                for (auto const& n : tradeExposureNodes_) {
                    externalOutputNodes_.insert(externalOutputNodes_.end(), n.begin(), n.end());
                }
            }
            if (cvaNode_ != ComputationGraph::nan) {
                externalOutputNodes_.push_back(cvaNode_);
            }

            for (auto const& n : externalOutputNodes_)
                valuesExternal_[n].declareAsOutput();
        }
        finalizeExternalCalculation();
    } else {
        forwardEvaluation(*g, values_, ops_, RandomVariable::deleter, !bumpCvaSensis_ && sensitivityData_,
                          opNodeRequirements_, keepNodes_);
    }

    rvMemMax_ = std::max(rvMemMax_, numberOfStochasticRvs(values_) + numberOfStochasticRvs(derivatives_));
    timing_fwd_ = timer.elapsed().wall - timing_poprv_;

    DLOG("XvaEngineCG: do forward evaluation done");
}

void XvaEngineCG::buildAsdNodes() {
    DLOG("XvaEngineCG: build asd nodes.");

    if (asd_ == nullptr)
        return;

    for (Size k = 1; k < scenarioGeneratorData_->getGrid()->timeGrid().size(); ++k) {

        // only write asd on valuation dates
        if (!scenarioGeneratorData_->getGrid()->isValuationDate()[k - 1])
            continue;

        Date date = scenarioGeneratorData_->getGrid()->dates()[k - 1];

        // numeraire
        asdNumeraire_.push_back(model_->numeraire(date));

        // fx spots
        for (auto const& ccy : simMarketData_->additionalScenarioDataCcys()) {
            if (ccy != simMarketData_->baseCcy()) {
                asdFx_.push_back(
                    model_->eval("FX-GENERIC-" + ccy + "-" + simMarketData_->baseCcy(), date, Null<Date>()));
            }
        }

        // index fixings
        for (auto const& ind : simMarketData_->additionalScenarioDataIndices()) {
            asdIndex_.push_back(model_->eval(ind, date, Null<Date>()));
        }

        // set credit states: TODO not yet provided in model_
        QL_REQUIRE(simMarketData_->additionalScenarioDataNumberOfCreditStates() == 0,
                   "XvaEngineCG::buildAsdNodes(): credit states currently not provided by GaussianCamCG, we have "
                   "implement this!");
    }

    DLOG("XvaEngineCG: building asd nodes done ("
         << scenarioGeneratorData_->getGrid()->timeGrid().size() - 1 << " timesteps, "
         << simMarketData_->additionalScenarioDataCcys().size() << " asd ccys, "
         << simMarketData_->additionalScenarioDataIndices().size() << " asd indices)");
}

void XvaEngineCG::populateAsd() {
    DLOG("XvaEngineCG: populate asd.");

    if (asd_ == nullptr)
        return;

    for (Size k = 1; k < scenarioGeneratorData_->getGrid()->timeGrid().size(); ++k) {
        // only write asd on valuation dates
        if (!scenarioGeneratorData_->getGrid()->isValuationDate()[k - 1])
            continue;

        // set numeraire
        for (std::size_t i = 0; i < model_->size(); ++i) {
            asd_->set(k - 1, i, values_[asdNumeraire_[k - 1]][i], AggregationScenarioDataType::Numeraire);
        }

        // set fx spots
        for (auto const& ccy : simMarketData_->additionalScenarioDataCcys()) {
            if (ccy != simMarketData_->baseCcy()) {
                for (std::size_t i = 0; i < model_->size(); ++i) {
                    asd_->set(k - 1, i, values_[asdFx_[k - 1]][i], AggregationScenarioDataType::FXSpot, ccy);
                }
            }
        }

        // set index fixings
        for (auto const& ind : simMarketData_->additionalScenarioDataIndices()) {
            for (std::size_t i = 0; i < model_->size(); ++i) {
                asd_->set(k - 1, i, values_[asdIndex_[k - 1]][i], AggregationScenarioDataType::IndexFixing, ind);
            }
        }
    }

    DLOG("XvaEngineCG: populate asd done.");
}

void XvaEngineCG::populateNpvOutputCube() {
    DLOG("XvaEngineCG: populate npv output cube.");

    if (npvOutputCube_ == nullptr)
        return;

    QL_REQUIRE(npvOutputCube_->samples() == model_->size(),
               "populateNpvOutputCube(): cube sample size ("
                   << npvOutputCube_->samples() << ") does not match model size (" << model_->size() << ")");

    // if we don't generate the exposure on trade level, but are forced to populate the npv cube on trade
    // level, we assign the same fraction of the portfolio amount to each trade

    Real multiplier = generateTradeLevelExposure_ ? 1.0 : 1.0 / static_cast<Real>(portfolio_->trades().size());

    std::size_t tradePos = 0;
    for (const auto& [id, index] : portfolio_->trades()) {

        auto getNode = [this, tradePos](std::size_t dateIndex) {
            return generateTradeLevelExposure_ ? tradeExposureNodes_[dateIndex][tradePos] : pfExposureNodes_[dateIndex];
        };

        auto cubeTradeIdx = npvOutputCube_->idsAndIndexes().find(id);
        QL_REQUIRE(cubeTradeIdx != npvOutputCube_->idsAndIndexes().end(),
                   "XvaEngineCG::populateNpvOutputCube(): trade id '"
                       << id << "' from portfolio is not present in output cube - internal error.");

        npvOutputCube_->setT0(values_[getNode(0)][0] * multiplier, cubeTradeIdx->second);
        for (Size i = 0; i < simulationDates_.size(); ++i) {
            for (Size j = 0; j < npvOutputCube_->samples(); ++j) {
                npvOutputCube_->set(values_[getNode(i + 1)][j] * multiplier, cubeTradeIdx->second, i, j);
            }
        }
        ++tradePos;
    }

    DLOG("XvaEngineCG: populate npv output cube done.");
}

void XvaEngineCG::generateXvaReports() {
    DLOG("XvaEngineCG: Write epe report.");
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
    DLOG("XvaEngineCG: calculate sensitivities.");

    QL_REQUIRE(cvaNode_ != ComputationGraph::nan,
               "XvaEngineCG::calculateSensitivities(): no cva node set, internal error.");

    Real cva = expectation(values_[cvaNode_]).at(0);
    DLOG("XvaEngineCG: Calcuated CVA (node " << cvaNode_ << ") = " << cva);

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    if (sensitivityData_) {

        DLOG("XvaEngineCG: Calculate sensitivities (bump = " << std::boolalpha << bumpCvaSensis_ << ")");

        // Do backward derivatives run

        std::vector<double> modelParamDerivatives(baseModelParams_.size());

        if (!bumpCvaSensis_) {

            DLOG("XvaEngineCG: run backward derivatives");

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

            DLOG("XvaEngineCG: got " << modelParamDerivatives.size()
                                     << " model parameter derivatives from run backward derivatives");

            timing_bwd_ = timer.elapsed().wall;

            // Delete values and derivatives vectors, they are not needed from this point on
            // except we are doing a full revaluation!

            values_.clear();
            derivatives_.clear();
        }

        // generate sensitivity scenarios

        DLOG("XvaEngineCG: running sensi scenarios");

        sensiScenarioGenerator_ = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
            sensitivityData_, simMarket_->baseScenario(), simMarketData_, simMarket_,
            QuantLib::ext::make_shared<DeltaScenarioFactory>(simMarket_->baseScenario()), false, std::string(),
            continueOnError_, simMarket_->baseScenarioAbsolute());

        simMarket_->scenarioGenerator() = sensiScenarioGenerator_;

        sensiResultCube_ = QuantLib::ext::make_shared<DoublePrecisionSensiCube>(std::set<std::string>{"CVA"}, asof_,
                                                                                sensiScenarioGenerator_->samples());
        sensiResultCube_->setT0(cva, 0, 0);

        model_->alwaysForwardNotifications();

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
                        finalizeExternalCalculation();
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

        DLOG("XvaEngineCG: finished running " << sensiResultCube_->samples() << " sensi scenarios, thereof "
                                              << activeScenarios << " active.");
    } // if sensi data is given
}

void XvaEngineCG::generateSensiReports() {
    DLOG("XvaEngineCG: write sensi report.");
    if (!sensitivityData_)
        return;
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

    updateProgress(0, 4);

    generateTradeLevelExposure_ = npvOutputCube_ != nullptr;

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
        buildAsdNodes();
    }

    if (firstRun_) {
        outputGraphStats();
    }

    getExternalContext();

    updateProgress(1, 4);

    setupValueContainers();
    doForwardEvaluation();

    updateProgress(2, 4);

    populateAsd();
    populateNpvOutputCube();

    updateProgress(3, 4);

    if (mode_ == Mode::Full) {
        generateXvaReports();
        calculateSensitivities();
        generateSensiReports();
    }

    updateProgress(4, 4);

    outputTimings();

    cleanUpAfterCalcs();
    firstRun_ = false;
    LOG("XvaEngineCG::run(): finished.");
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
