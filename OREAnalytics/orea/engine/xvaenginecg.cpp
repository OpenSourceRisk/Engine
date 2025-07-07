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
#include <orea/engine/simpledynamicsimm.hpp>
#include <orea/engine/xvaenginecg.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <orea/simm/simmconfigurationisdav2_6_5.hpp>

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
                         const bool enableDynamicIM, const Size dynamicIMStepSize, const Size regressionOrder,
                         const Real regressionVarianceCutoff, const bool tradeLevelBreakDown, const bool useRedBlocks,
                         const bool useExternalComputeDevice, const bool externalDeviceCompatibilityMode,
                         const bool useDoublePrecisionForExternalCalculation, const std::string& externalComputeDevice,
                         const bool usePythonIntegration, const bool continueOnCalibrationError,
                         const bool allowModelFallbacks, const bool continueOnError, const std::string& context)
    : mode_(mode), asof_(asof), loader_(loader), curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams),
      simMarketData_(simMarketData), engineData_(engineData), crossAssetModelData_(crossAssetModelData),
      scenarioGeneratorData_(scenarioGeneratorData), portfolio_(portfolio), marketConfiguration_(marketConfiguration),
      marketConfigurationInCcy_(marketConfigurationInCcy), sensitivityData_(sensitivityData),
      referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig), bumpCvaSensis_(bumpCvaSensis),
      enableDynamicIM_(enableDynamicIM), dynamicIMStepSize_(dynamicIMStepSize), regressionOrder_(regressionOrder),
      regressionVarianceCutoff_(regressionVarianceCutoff), tradeLevelBreakDown_(tradeLevelBreakDown),
      useRedBlocks_(useRedBlocks), useExternalComputeDevice_(useExternalComputeDevice),
      externalDeviceCompatibilityMode_(externalDeviceCompatibilityMode),
      useDoublePrecisionForExternalCalculation_(useDoublePrecisionForExternalCalculation),
      externalComputeDevice_(externalComputeDevice), usePythonIntegration_(usePythonIntegration),
      continueOnCalibrationError_(continueOnCalibrationError), allowModelFallbacks_(allowModelFallbacks),
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
    // TODO: hardcoded "allowChangingFallbacks" to false here, we might want to revisit this when calculating e.g.
    // stress
    camBuilder_ = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        simMarketObs_, crossAssetModelData_, marketConfigurationInCcy_, marketConfiguration_, marketConfiguration_,
        marketConfiguration_, marketConfiguration_, marketConfiguration_, false, continueOnCalibrationError_,
        std::string(), "xva engine cg - cam builder", false, allowModelFallbacks_);

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

    // note: - these must be fine enough for Euler, e.g. weekly over the whole simulation period

    valuationDates_ = scenarioGeneratorData_->getGrid()->valuationDates();
    closeOutDates_ = scenarioGeneratorData_->getGrid()->closeOutDates();
    if (scenarioGeneratorData_->withCloseOutLag() && scenarioGeneratorData_->withMporStickyDate()) {
        std::vector<Date> tmp = scenarioGeneratorData_->getGrid()->valuationDates();
        simulationDates_ = std::set<Date>(valuationDates_.begin(), valuationDates_.end());
        stickyCloseOutDates_ = scenarioGeneratorData_->getGrid()->closeOutDates();
    } else {
        std::vector<Date> tmp = scenarioGeneratorData_->getGrid()->dates();
        simulationDates_ = std::set<Date>(tmp.begin(), tmp.end());
        stickyCloseOutDates_.clear();
    }

    QL_REQUIRE(stickyCloseOutDates_.empty() || stickyCloseOutDates_.size() == simulationDates_.size(),
               "XvaEngineCG::buildCam(): sticky close out dates size ("
                   << stickyCloseOutDates_.size() << ") do not match simulation dates size (" << simulationDates_.size()
                   << ") - internal error!");

    Size timeStepsPerYear =
        scenarioGeneratorData_->timeStepsPerYear() == Null<Size>() ? 0 : scenarioGeneratorData_->timeStepsPerYear();

    // note: projectedStateProcessIndices can be removed from GaussianCamCG constructor most probably?
    model_ = QuantLib::ext::make_shared<GaussianCamCG>(
        camBuilder_->model(), scenarioGeneratorData_->samples(), currencies, curves, fxSpots, irIndices, infIndices,
        indices, indexCurrencies, simulationDates_, iborFallbackConfig_, std::vector<Size>(),
        std::vector<std::string>(), stickyCloseOutDates_, timeStepsPerYear);
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

    auto factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, simMarket_, configurations, referenceData_, iborFallbackConfig_,
        EngineBuilderFactory::instance().generateAmcCgEngineBuilders(
            model_, std::vector<Date>(simulationDates_.begin(), simulationDates_.end())),
        true);

    portfolio_->build(factory, "xva engine cg", true);

    timing_pf_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build trades (" << portfolio_->size() << ") done.");
}

void XvaEngineCG::buildCgPartB() {
    DLOG("XvaEngineCG: build computation graph for all trades (part B)");

    // Build computation graph for all trades ("part B") and
    // - store npv, amc npv nodes

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    for (auto const& [id, trade] : portfolio_->trades()) {

        double multiplier = trade->instrument()->multiplier() * trade->instrument()->multiplier2();

        auto engine = QuantLib::ext::dynamic_pointer_cast<AmcCgPricingEngine>(
            trade->instrument()->qlInstrument()->pricingEngine());
        QL_REQUIRE(engine, "XvaEngineCG: expected to get AmcCgPricingEngine, trade '"
                               << id << "' has a different or no engine attached.");
        if (useRedBlocks_)
            g->startRedBlock();

        // trigger setupArguments
        if (!trade->instrument()->qlInstrument()->isCalculated())
            trade->instrument()->qlInstrument()->recalculate();

        std::vector<TradeExposure> tradeExposure;
        TradeExposureMetaInfo metaInfo;

        engine->buildComputationGraph(false, &tradeExposure, &metaInfo);

        for (auto& t : tradeExposure)
            t.multiplier = multiplier;

        tradeExposureMetaInfo_.push_back(metaInfo);

        {
            std::vector<TradeExposure> tmpValuation(valuationDates_.size() + 1);
            tmpValuation[0] = tradeExposure[0];
            for (std::size_t i = 0; i < valuationDates_.size(); ++i) {
                if (closeOutDates_.empty() || !stickyCloseOutDates_.empty()) {
                    tmpValuation[i + 1] = tradeExposure[i + 1];
                } else {
                    std::size_t index =
                        std::distance(simulationDates_.begin(),
                                      std::find(simulationDates_.begin(), simulationDates_.end(), valuationDates_[i]));
                    tmpValuation[i + 1] = tradeExposure[index + 1];
                }
            }
            tradeExposureValuation_.push_back(tmpValuation);
        }

        if (!closeOutDates_.empty()) {

            if (!stickyCloseOutDates_.empty()) {
                engine->buildComputationGraph(true, &tradeExposure, &metaInfo);
                for (auto& t : tradeExposure)
                    t.multiplier = multiplier;
            }

            std::vector<TradeExposure> tmpCloseOut(closeOutDates_.size() + 1);
            tmpCloseOut[0] = tradeExposure[0];
            for (std::size_t i = 0; i < closeOutDates_.size(); ++i) {
                if (!stickyCloseOutDates_.empty()) {
                    tmpCloseOut[i + 1] = tradeExposure[i + 1];
                } else {
                    std::size_t index =
                        std::distance(simulationDates_.begin(),
                                      std::find(simulationDates_.begin(), simulationDates_.end(), closeOutDates_[i]));
                    tmpCloseOut[i + 1] = tradeExposure[index + 1];
                }
            }
            tradeExposureCloseOut_.push_back(tmpCloseOut);
        }

        if (useRedBlocks_)
            g->endRedBlock();
    }

    timing_partb_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build computation graph for all trades done - graph size is "
         << model_->computationGraph()->size());
}

std::pair<std::set<std::size_t>, std::set<std::set<std::size_t>>>
XvaEngineCG::getRegressors(const std::size_t dateIndex, const Date& obsDate, const std::set<std::size_t>& tradeIds,
                           const bool isValuation) {
    std::set<std::set<std::size_t>> pfRegressorGroups;
    std::set<std::size_t> pfRegressors;
    for (auto const j : tradeIds) {
        std::set<std::size_t> tradeRegressors = isValuation ? tradeExposureValuation_[j][dateIndex].regressors
                                                            : tradeExposureCloseOut_[j][dateIndex].regressors;
        pfRegressorGroups.insert(tradeRegressors);
        pfRegressors.insert(tradeRegressors.begin(), tradeRegressors.end());
    }

    std::set<std::set<std::size_t>> pfRegressorPosGroups;
    for (const auto& g : pfRegressorGroups) {
        std::set<std::size_t> group;
        std::transform(g.begin(), g.end(), std::inserter(group, group.end()), [&pfRegressors](std::size_t v) {
            return std::distance(pfRegressors.begin(), pfRegressors.find(v));
        });
        pfRegressorPosGroups.insert(group);
    }

    return std::make_pair(pfRegressors, pfRegressorPosGroups);
}

std::size_t XvaEngineCG::createPortfolioExposureNode(const std::size_t dateIndex, const bool isValuationDate) {

    auto g = model_->computationGraph();

    // determine valuation date and if applicable closeout date

    Date valuationDate = dateIndex == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), dateIndex - 1);
    Date closeOutDate;
    if (!isValuationDate) {
        closeOutDate = dateIndex == 0 ? model_->referenceDate() : closeOutDates_[dateIndex - 1];
    }

    // determine the obs date for conditional expectations below

    Date obsDate = valuationDate;
    if (stickyCloseOutDates_.empty() && !isValuationDate)
        obsDate = closeOutDate;

    // enable stickyness for the following calculations, if applicable

    model_->useStickyCloseOutDates(!stickyCloseOutDates_.empty() && !isValuationDate);

    // build the vector of nodes over which we sum the npvs

    std::vector<std::size_t> tradeSumIndividual;
    std::vector<std::size_t> tradeSumPlain;
    std::set<std::size_t> plainTradeIds;
    for (Size j = 0; j < portfolio_->trades().size(); ++j) {
        TradeExposure& exposure =
            isValuationDate ? tradeExposureValuation_[j][dateIndex] : tradeExposureCloseOut_[j][dateIndex];
        if (exposure.targetConditionalExpectation == ComputationGraph::nan) {
            tradeSumPlain.push_back(
                cg_mult(*g, cg_const(*g, exposure.multiplier), exposure.componentPathValues.front()));
            plainTradeIds.insert(j);
        } else {
            tradeSumIndividual.push_back(
                cg_mult(*g, cg_const(*g, exposure.multiplier), exposure.targetConditionalExpectation));
        }
    }

    // get the regressors and regressor pos groups

    auto [pfRegressors, pfRegressorPosGroups] = getRegressors(dateIndex, obsDate, plainTradeIds, isValuationDate);

    // build the portfolio exposure nodes

    std::size_t plainTradePathExposure = cg_add(*g, tradeSumPlain);
    std::size_t plainTradeExposure =
        model_->npv(plainTradePathExposure, obsDate, cg_const(*g, 1.0), std::nullopt, {}, pfRegressors);
    std::size_t individualTradeExposure = cg_add(*g, tradeSumIndividual);

    pfRegressorPosGroups_[plainTradeExposure] = pfRegressorPosGroups;

    std::size_t totalExposure = cg_add(*g, plainTradeExposure, individualTradeExposure);

    // diable stickyness again

    model_->useStickyCloseOutDates(false);

    // return the pathwise and conditional expectation nodes

    return totalExposure;
}

std::size_t XvaEngineCG::createTradeExposureNode(const std::size_t dateIndex, const std::size_t tradeIndex,
                                                 const bool isValuationDate) {

    auto g = model_->computationGraph();

    Date valuationDate = dateIndex == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), dateIndex - 1);
    Date closeOutDate;
    if (!isValuationDate) {
        closeOutDate = dateIndex == 0 ? model_->referenceDate() : closeOutDates_[dateIndex - 1];
    }
    Date obsDate = valuationDate;
    if (stickyCloseOutDates_.empty() && !isValuationDate)
        obsDate = closeOutDate;

    model_->useStickyCloseOutDates(!stickyCloseOutDates_.empty() && !isValuationDate);

    TradeExposure& exposure = isValuationDate ? tradeExposureValuation_[tradeIndex][dateIndex]
                                              : tradeExposureCloseOut_[tradeIndex][dateIndex];

    std::size_t result;

    if (exposure.targetConditionalExpectation == ComputationGraph::nan) {
        result = model_->npv(cg_mult(*g, cg_const(*g, exposure.multiplier), exposure.componentPathValues.front()),
                             obsDate, cg_const(*g, 1.0), {}, {},
                             isValuationDate ? tradeExposureValuation_[tradeIndex][dateIndex].regressors
                                             : tradeExposureCloseOut_[tradeIndex][dateIndex].regressors);
    } else {
        result = cg_mult(*g, cg_const(*g, exposure.multiplier), exposure.targetConditionalExpectation);
    }

    model_->useStickyCloseOutDates(false);

    return result;
}

void XvaEngineCG::buildCgPartC() {
    DLOG("XvaEngineCG: add exposure nodes to graph (part C)");

    // This constitutes part C of the computation graph spanning "trade m range end ... lastExposureNode"

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    if (mode_ == Mode::Full || !tradeLevelBreakDown_) {
        for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
            pfExposureValuation_.push_back(createPortfolioExposureNode(i, true));
            if (!closeOutDates_.empty()) {
                pfExposureCloseOut_.push_back(createPortfolioExposureNode(i, false));
            }
        }
    }

    if (tradeLevelBreakDown_) {
        for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
            tradeExposureNodes_.push_back(std::vector<std::size_t>(portfolio_->trades().size()));
            if (!closeOutDates_.empty())
                tradeExposureCloseOutNodes_.push_back(std::vector<std::size_t>(portfolio_->trades().size()));
            for (Size j = 0; j < portfolio_->trades().size(); ++j) {
                tradeExposureNodes_.back()[j] = createTradeExposureNode(i, j, true);
                if (!closeOutDates_.empty())
                    tradeExposureCloseOutNodes_.back()[j] = createTradeExposureNode(i, j, false);
            }
        }
    }

    timing_partc_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: add exposure nodes to graph done - graph size is "
         << g->size() << ", generateTradeLevelExposure = " << std::boolalpha << tradeLevelBreakDown_);
}

void XvaEngineCG::buildCgDynamicIM() {
    DLOG("XvaEngineCG: add dynamic im nodes to graph (part C2)");

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
        dynamicImInfo_.push_back({});
        std::map<std::set<ModelCG::ModelParameter>, std::vector<std::size_t>> plainTradeExposures;
        for (Size j = 0; j < portfolio_->trades().size(); ++j) {
            if (tradeExposureValuation_[j][i].targetConditionalExpectation == ComputationGraph::nan) {
                dynamicImInfo_.back().plainTradeIds.insert(j);
                plainTradeExposures[tradeExposureMetaInfo_[j].relevantModelParameters].push_back(
                    cg_mult(*g, cg_const(*g, tradeExposureValuation_[j][i].multiplier),
                            tradeExposureValuation_[j][i].componentPathValues.front()));
            } else {
                dynamicImInfo_.back().individualTradeIds.insert(j);
            }
        }

        for (auto const& [s, v] : plainTradeExposures) {
            dynamicImInfo_.back().plainTradeSumGrouped[s] = cg_add(*g, v);
        }

        std::tie(dynamicImInfo_.back().plainTradeRegressors, dynamicImInfo_.back().plainTradeRegressorGroups) =
            getRegressors(i, i == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), i - 1),
                          dynamicImInfo_.back().plainTradeIds, true);
    }

    timing_partc2_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: add dynamic im nodes to graph done - graph size is " << g->size());
}

void XvaEngineCG::buildCgPP() {
    DLOG("XvaEngineCG: add cg post processor (part D)");

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    // Add post processor
    // This constitues part D of the computation graph from lastExposureNode ... g->size()
    // The cvaNode is the ultimate result w.r.t. which we want to compute sensitivities

    // note: very simplified calculation, for testing, just multiply the EPE on each date by fixed default prob
    auto defaultCurve = simMarket_->defaultCurve("BANK")->curve();
    model_->registerWith(defaultCurve);
    cvaNode_ = cg_const(*g, 0.0);
    for (Size i = 0; i < valuationDates_.size(); ++i) {
        Date d = i == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), i - 1);
        Date e = *std::next(valuationDates_.begin(), i);
        std::size_t defaultProb =
            addModelParameter(*g, model_->modelParameters(),
                              ModelCG::ModelParameter(ModelCG::ModelParameter::Type ::defaultProb, {}, {}, d),
                              [defaultCurve, d, e]() { return defaultCurve->defaultProbability(d, e); });
        cvaNode_ =
            cg_add(*g, cvaNode_, cg_mult(*g, defaultProb, cg_max(*g, pfExposureValuation_[i], cg_const(*g, 0.0))));
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
    xvaDerivatives_ = std::vector<RandomVariable>(g->size(), RandomVariable(model_->size(), 0.0));
    dynamicIMDerivatives_ = std::vector<RandomVariable>(g->size(), RandomVariable(model_->size(), 0.0));

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

    baseModelParams_.clear();
    for (auto const& p : model_->modelParameters()) {
        baseModelParams_.push_back(std::make_pair(p.node(), p.eval()));
    }
    populateConstants(values_, valuesExternal_);
    populateModelParameters(baseModelParams_, values_, valuesExternal_);
    timing_popparam_ = timer.elapsed().wall;

    // Populate random variates

    populateRandomVariates(values_, valuesExternal_);
    timing_poprv_ = timer.elapsed().wall - timing_popparam_;

    rvMemMax_ = numberOfStochasticRvs(values_) + numberOfStochasticRvs(xvaDerivatives_) +
                numberOfStochasticRvs(dynamicIMDerivatives_);

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
        ops_ = getRandomVariableOps(model_->size(), regressionOrder_, QuantLib::LsmBasisSystem::Monomial,
                                    (sensitivityData_ && bumpCvaSensis_) ? eps : 0.0, regressionVarianceCutoff_,
                                    pfRegressorPosGroups_, usePythonIntegration_);
        grads_ = getRandomVariableGradients(model_->size(), regressionOrder_, QuantLib::LsmBasisSystem::Monomial, eps);
    }

    bool keepValuesForDerivatives = (!bumpCvaSensis_ && sensitivityData_) || enableDynamicIM_;

    keepNodes_ = std::vector<bool>(g->size(), false);

    for (auto const& c : g->constants()) {
        keepNodes_[c.second] = true;
    }

    for (auto const& [n, v] : baseModelParams_) {
        keepNodes_[n] = true;
    }

    for (auto const& n : pfExposureValuation_) {
        keepNodes_[n] = true;
    }

    for (auto const& n : pfExposureCloseOut_) {
        keepNodes_[n] = true;
    }

    if (enableDynamicIM_) {
        for (std::size_t i = 0; i < valuationDates_.size(); ++i) {
            for (std::size_t j = 0; j < tradeExposureMetaInfo_.size(); ++j) {
                for (auto const n :
                     dependentNodes(*g, tradeExposureValuation_[j][i + 1].componentPathValues.back() + 1,
                                    tradeExposureValuation_[j][i + 1].targetConditionalExpectationDerivatives + 1)) {
                    keepNodes_[n] = true;
                }
            }
        }
    }

    if (tradeLevelBreakDown_) {
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

    for (auto const& v : asdFx_)
        for (auto const& n : v)
            keepNodes_[n] = true;

    for (auto const& v : asdIndex_)
        for (auto const& n : v)
            keepNodes_[n] = true;

    if (keepValuesForDerivatives) {
        for (auto const n : g->redBlockDependencies()) {
            keepNodes_[n] = true;
        }
    }

    if (bumpCvaSensis_) {
        for (auto const& rv : model_->randomVariates())
            for (auto const& v : rv)
                keepNodes_[v] = true;
    }

    std::vector<bool> rvOpAllowsPredeletion = QuantExt::getRandomVariableOpAllowsPredeletion();

    if (useExternalComputeDevice_) {
        if (firstRun_) {
            forwardEvaluation(*g, valuesExternal_, opsExternal_, ExternalRandomVariable::deleter,
                              keepValuesForDerivatives, opNodeRequirements_, keepNodes_, 0, ComputationGraph::nan,
                              false, ExternalRandomVariable::preDeleter, rvOpAllowsPredeletion);
            externalOutputNodes_.insert(externalOutputNodes_.end(), pfExposureValuation_.begin(),
                                        pfExposureValuation_.end());
            externalOutputNodes_.insert(externalOutputNodes_.end(), pfExposureCloseOut_.begin(),
                                        pfExposureCloseOut_.end());
            externalOutputNodes_.insert(externalOutputNodes_.end(), asdNumeraire_.begin(), asdNumeraire_.end());
            for (auto const& v : asdFx_)
                externalOutputNodes_.insert(externalOutputNodes_.end(), v.begin(), v.end());
            for (auto const& v : asdIndex_)
                externalOutputNodes_.insert(externalOutputNodes_.end(), v.begin(), v.end());
            if (tradeLevelBreakDown_) {
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
        forwardEvaluation(*g, values_, ops_, RandomVariable::deleter, keepValuesForDerivatives, opNodeRequirements_,
                          keepNodes_);
    }

    rvMemMax_ = std::max(rvMemMax_, numberOfStochasticRvs(values_) + numberOfStochasticRvs(xvaDerivatives_)) +
                numberOfStochasticRvs(dynamicIMDerivatives_);
    timing_fwd_ = timer.elapsed().wall - timing_poprv_;

    DLOG("XvaEngineCG: do forward evaluation done");
}

void XvaEngineCG::buildAsdNodes() {
    DLOG("XvaEngineCG: build asd nodes.");

    // we need the numeraire to populate the npv output cube
    if (asd_ == nullptr && npvOutputCube_ == nullptr)
        return;

    asdNumeraire_.resize(valuationDates_.size());
    asdFx_.resize(simMarketData_->additionalScenarioDataCcys().size(),
                  std::vector<std::size_t>(valuationDates_.size()));
    asdIndex_.resize(simMarketData_->additionalScenarioDataIndices().size(),
                     std::vector<std::size_t>(valuationDates_.size()));

    std::size_t dateIndex = 0;
    for (auto const& date : valuationDates_) {

        // numeraire
        asdNumeraire_[dateIndex] = model_->numeraire(date);

        // see above, we have set the numeraire node now, the rest is needed for asd only
        if (asd_ == nullptr)
            continue;

        // fx spots
        std::size_t ccyIndex = 0;
        for (auto const& ccy : simMarketData_->additionalScenarioDataCcys()) {
            if (ccy != simMarketData_->baseCcy()) {
                asdFx_[ccyIndex++][dateIndex] =
                    model_->eval("FX-GENERIC-" + ccy + "-" + simMarketData_->baseCcy(), date, Null<Date>());
            }
        }

        // index fixings
        std::size_t indIndex = 0;
        for (auto const& ind : simMarketData_->additionalScenarioDataIndices()) {
            asdIndex_[indIndex++][dateIndex] = model_->eval(ind, date, Null<Date>());
        }

        // set credit states: TODO not yet provided in model_
        QL_REQUIRE(simMarketData_->additionalScenarioDataNumberOfCreditStates() == 0,
                   "XvaEngineCG::buildAsdNodes(): credit states currently not provided by GaussianCamCG, we have "
                   "implement this!");

        ++dateIndex;
    }

    DLOG("XvaEngineCG: building asd nodes done ("
         << scenarioGeneratorData_->getGrid()->timeGrid().size() - 1 << " timesteps, "
         << simMarketData_->additionalScenarioDataCcys().size() << " asd ccys, "
         << simMarketData_->additionalScenarioDataIndices().size() << " asd indices)");
}

void XvaEngineCG::populateAsd() {
    if (asd_ == nullptr)
        return;

    DLOG("XvaEngineCG: populate asd.");

    boost::timer::cpu_timer timer;

    for (Size k = 0; k < valuationDates_.size(); ++k) {
        // set numeraire
        for (std::size_t i = 0; i < model_->size(); ++i) {
            asd_->set(k, i, values_[asdNumeraire_[k]][i], AggregationScenarioDataType::Numeraire);
        }

        // set fx spots
        std::size_t ccyIndex = 0;
        for (auto const& ccy : simMarketData_->additionalScenarioDataCcys()) {
            if (ccy != simMarketData_->baseCcy()) {
                for (std::size_t i = 0; i < model_->size(); ++i) {
                    asd_->set(k, i, values_[asdFx_[ccyIndex][k]][i], AggregationScenarioDataType::FXSpot, ccy);
                }
                ++ccyIndex;
            }
        }

        // set index fixings
        std::size_t indIndex = 0;
        for (auto const& ind : simMarketData_->additionalScenarioDataIndices()) {
            for (std::size_t i = 0; i < model_->size(); ++i) {
                asd_->set(k, i, values_[asdIndex_[indIndex][k]][i], AggregationScenarioDataType::IndexFixing, ind);
            }
            ++indIndex;
        }
    }

    timing_asd_ = timer.elapsed().wall;

    DLOG("XvaEngineCG: populate asd done.");
}

void XvaEngineCG::populateNpvOutputCube() {
    if (npvOutputCube_ == nullptr)
        return;

    DLOG("XvaEngineCG: populate npv output cube.");

    boost::timer::cpu_timer timer;

    QL_REQUIRE(npvOutputCube_->samples() == model_->size(),
               "populateNpvOutputCube(): cube sample size ("
                   << npvOutputCube_->samples() << ") does not match model size (" << model_->size() << ")");

    // if we don't generate the exposure on trade level, but are forced to populate the npv cube on trade
    // level, we assign the same fraction of the portfolio amount to each trade

    Real multiplier = tradeLevelBreakDown_ ? 1.0 : 1.0 / static_cast<Real>(portfolio_->trades().size());

    std::size_t tradePos = 0;
    for (const auto& [id, index] : portfolio_->trades()) {

        auto getNode = [this, tradePos](std::size_t dateIndex, bool isCloseOut) {
            return tradeLevelBreakDown_
                       ? (isCloseOut ? tradeExposureCloseOutNodes_[dateIndex][tradePos]
                                     : tradeExposureNodes_[dateIndex][tradePos])
                       : (isCloseOut ? pfExposureCloseOut_[dateIndex] : pfExposureValuation_[dateIndex]);
        };

        auto cubeTradeIdx = npvOutputCube_->idsAndIndexes().find(id);
        QL_REQUIRE(cubeTradeIdx != npvOutputCube_->idsAndIndexes().end(),
                   "XvaEngineCG::populateNpvOutputCube(): trade id '"
                       << id << "' from portfolio is not present in output cube - internal error.");

        npvOutputCube_->setT0(values_[getNode(0, false)][0] * multiplier, cubeTradeIdx->second);
        for (Size i = 0; i < valuationDates_.size(); ++i) {
            for (Size j = 0; j < npvOutputCube_->samples(); ++j) {
                npvOutputCube_->set(values_[getNode(i + 1, false)][j] * multiplier, cubeTradeIdx->second, i, j);
            }
        }
        for (Size i = 0; i < closeOutDates_.size(); ++i) {
            for (Size j = 0; j < npvOutputCube_->samples(); ++j) {
                /* ore convention: the close-out value in the output cube should be multiplied by the numeraire value at
                 * the associated valuation date */
                npvOutputCube_->set(values_[getNode(i + 1, true)][j] * values_[asdNumeraire_[i]][j] * multiplier,
                                    cubeTradeIdx->second, i, j, 1);
            }
        }
        ++tradePos;
    }

    timing_outcube_ = timer.elapsed().wall;

    DLOG("XvaEngineCG: populate npv output cube done.");
}

void XvaEngineCG::populateDynamicIMOutputCube() {
    if (dynamicIMOutputCube_ == nullptr || !enableDynamicIM_)
        return;

    DLOG("XvaEngineCG: populate dynamic IM output cube.");

    boost::timer::cpu_timer timer;

    for (auto const& [ns, im] : dynamicIM_) {

        auto nidx = dynamicIMOutputCube_->idsAndIndexes().find(ns);
        QL_REQUIRE(nidx != dynamicIMOutputCube_->idsAndIndexes().end(),
                   "XvaEngineCG::populateDynamicIMOutputCube(): netting set "
                       << ns << " not found in output cube, this is an internal error.");

        // dynamicIMOutputCube_->setT0(im[0].at(0), 0);
        dynamicIMOutputCube_->setT0(im[0].at(0), nidx->second, 0);
	dynamicIMOutputCube_->setT0(dynamicDeltaIM_[ns][0].at(0), nidx->second, 1);
	dynamicIMOutputCube_->setT0(dynamicVegaIM_[ns][0].at(0), nidx->second, 2);
	dynamicIMOutputCube_->setT0(dynamicCurvatureIM_[ns][0].at(0), nidx->second, 3);

        for (Size i = 0; i < valuationDates_.size(); ++i) {
            for (Size k = 0; k < im[i + 1].size(); ++k) {
                // // convention in the output cube is im deflated by numeraire
                // dynamicIMOutputCube_->set(im[i + 1][k] / values_[asdNumeraire_[i]][k], nidx->second, i, k, 0);
                // dynamicIMOutputCube_->set(dynamicDeltaIM_[ns][i + 1][k] / values_[asdNumeraire_[i]][k], nidx->second, i, k, 1);
                // dynamicIMOutputCube_->set(dynamicVegaIM_[ns][i + 1][k] / values_[asdNumeraire_[i]][k], nidx->second, i, k, 2);
                // dynamicIMOutputCube_->set(dynamicCurvatureIM_[ns][i + 1][k] / values_[asdNumeraire_[i]][k], nidx->second, i, k, 3);
                dynamicIMOutputCube_->set(im[i + 1][k], nidx->second, i, k, 0);
                dynamicIMOutputCube_->set(dynamicDeltaIM_[ns][i + 1][k], nidx->second, i, k, 1);
                dynamicIMOutputCube_->set(dynamicVegaIM_[ns][i + 1][k], nidx->second, i, k, 2);
                dynamicIMOutputCube_->set(dynamicCurvatureIM_[ns][i + 1][k], nidx->second, i, k, 3);
            }
        }
    }

    timing_imcube_ = timer.elapsed().wall;

    DLOG("XvaEngineCG: populate dynamic im output cube done.");
}

void XvaEngineCG::generateXvaReports() {
    DLOG("XvaEngineCG: Write epe report.");
    epeReport_ = QuantLib::ext::make_shared<InMemoryReport>();
    epeReport_->addColumn("Date", Date()).addColumn("EPE", double(), 4).addColumn("ENE", double(), 4);
    constexpr double tol = 4E-5;
    for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
        double epe = expectation(max(values_[pfExposureValuation_[i]], RandomVariable(model_->size(), 0.0))).at(0);
        double ene = expectation(max(-values_[pfExposureValuation_[i]], RandomVariable(model_->size(), 0.0))).at(0);
        if (std::abs(epe) > tol || std::abs(ene) > tol) {
            epeReport_->next();
            epeReport_->add(i == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), i - 1))
                .add(epe)
                .add(ene);
        }
    }
    epeReport_->end();
}

void XvaEngineCG::dynamicImAddToPathSensis(const std::set<ModelCG::ModelParameter>& parameterGroup, const Date& valDate,
                                           const double t, const std::map<std::string, std::size_t>& currencyLookup,
                                           const std::vector<IrDeltaParConverter>& irDeltaConverter,
                                           const std::vector<LgmSwaptionVegaParConverter>& irVegaConverter,
                                           const std::vector<CcLgmFxOptionVegaParConverter>& fxVegaConverter,
                                           std::vector<std::vector<RandomVariable>>& pathIrDelta,
                                           std::vector<RandomVariable>& pathFxDelta,
                                           std::vector<std::vector<RandomVariable>>& pathIrVega,
                                           std::vector<std::vector<RandomVariable>>& pathFxVega) {

    for (auto const& p : model_->modelParameters()) {

        // if the model parameter is not wanted for the current parameter group, we ignore its contribution
        if (parameterGroup.find(ModelCG::ModelParameter(p.type(), p.qualifier())) == parameterGroup.end()) {
            continue;
        }

        // zero rate sensi for T - t as seen from val date t is - ( T - t ) *  P(0,T) * d NPV / d P(0,T)

        if (p.type() == ModelCG::ModelParameter::Type::dsc && p.time() > t &&
            (p.date2() > valDate || p.date2() == Date())) {
            std::size_t ccyIndex = currencyLookup.at(p.qualifier());
            std::size_t bucket = std::min<std::size_t>(
                irDeltaConverter[ccyIndex].times().size() - 1,
                std::distance(irDeltaConverter[ccyIndex].times().begin(),
                              std::lower_bound(irDeltaConverter[ccyIndex].times().begin(),
                                               irDeltaConverter[ccyIndex].times().end(), p.time() - t)));
            Real w1 = 0.0, w2 = 1.0;
            if (bucket > 0) {
                w1 = (irDeltaConverter[ccyIndex].times()[bucket] - (p.time() - t)) /
                     (irDeltaConverter[ccyIndex].times()[bucket] -
                      (bucket == 0 ? 0.0 : irDeltaConverter[ccyIndex].times()[bucket - 1]));
                w2 = 1.0 - w1;
                pathIrDelta[ccyIndex][bucket - 1] += RandomVariable(model_->size(), -(p.time() - t) * 1E-4 * w1) *
                                                     values_[p.node()] * dynamicIMDerivatives_[p.node()];
            }
            pathIrDelta[ccyIndex][bucket] += RandomVariable(model_->size(), -(p.time() - t) * 1E-4 * w2) *
                                             values_[p.node()] * dynamicIMDerivatives_[p.node()];
        }

        /* fx spot sensi as seen from val date t for a relative shift r is r * d NPV / d ln fxSpot,
           we use r = 0.01 */

        if (p.type() == ModelCG::ModelParameter::Type::logFxSpot) {
            std::size_t ccyIndex = currencyLookup.at(p.qualifier());
            QL_REQUIRE(ccyIndex > 0,
                       "XvaEngineCG::calculateDynamicIM(): internal error, logFxSpot qualifier is equal to base ccy");
            pathFxDelta[ccyIndex - 1] += RandomVariable(model_->size(), 0.01) * dynamicIMDerivatives_[p.node()];
        }

        // ir vega, we collect the sensi w.r.t. zeta, unit shift per unit time

        if (p.type() == ModelCG::ModelParameter::Type::lgm_zeta && p.time() > t) {
            std::size_t ccyIndex = currencyLookup.at(p.qualifier());
            Real tte = p.time() - model_->actualTimeFromReference(valDate);
            std::size_t bucket = std::min<std::size_t>(
                irVegaConverter[ccyIndex].optionTimes().size() - 1,
                std::distance(irVegaConverter[ccyIndex].optionTimes().begin(),
                              std::lower_bound(irVegaConverter[ccyIndex].optionTimes().begin(),
                                               irVegaConverter[ccyIndex].optionTimes().end(), tte)));
            Real w1 = 0.0, w2 = 1.0;
            if (bucket > 0) {
                w1 = (irVegaConverter[ccyIndex].optionTimes()[bucket] - tte) /
                     (irVegaConverter[ccyIndex].optionTimes()[bucket] -
                      (bucket == 0 ? 0.0 : irVegaConverter[ccyIndex].optionTimes()[bucket - 1]));
                w2 = 1.0 - w1;
                pathIrVega[ccyIndex][bucket - 1] += RandomVariable(model_->size(), w1) *
                                                    dynamicIMDerivatives_[p.node()] *
                                                    RandomVariable(model_->size(), tte);
            }
            pathIrVega[ccyIndex][bucket] += RandomVariable(model_->size(), w2) * dynamicIMDerivatives_[p.node()] *
                                            RandomVariable(model_->size(), tte);
        }

        // fx vega, we want the sensi w.r.t. an absolute shift of 0.01

        if (p.type() == ModelCG::ModelParameter::Type::fxbs_sigma && p.time() >= t) {
            std::size_t ccyIndex = currencyLookup.at(p.qualifier());
            QL_REQUIRE(ccyIndex > 0,
                       "XvaEngineCG::calculateDynamicIM(): internal error, fxbs_sigma qualifier is equal to base ccy");
            Real tte = p.time() - model_->actualTimeFromReference(valDate);
            std::size_t bucket = std::min<std::size_t>(
                fxVegaConverter[ccyIndex - 1].optionTimes().size() - 1,
                std::distance(fxVegaConverter[ccyIndex - 1].optionTimes().begin(),
                              std::upper_bound(fxVegaConverter[ccyIndex - 1].optionTimes().begin(),
                                               fxVegaConverter[ccyIndex - 1].optionTimes().end(), tte)));
            pathFxVega[ccyIndex - 1][bucket] += dynamicIMDerivatives_[p.node()];
        }
    }
}

void XvaEngineCG::dynamicImAddToConvertedSensis(
    const std::size_t ccyIndex, const std::vector<RandomVariable>& tmpIrDelta,
    const std::vector<RandomVariable>& tmpIrVega, const std::vector<RandomVariable>& tmpFxVega,
    const RandomVariable& tmpFxDelta, const std::vector<IrDeltaParConverter>& irDeltaConverter,
    const std::vector<LgmSwaptionVegaParConverter>& irVegaConverter,
    const std::vector<CcLgmFxOptionVegaParConverter>& fxVegaConverter,
    std::vector<std::vector<RandomVariable>>& conditionalIrDelta, std::vector<RandomVariable>& conditionalFxDelta,
    std::vector<std::vector<RandomVariable>>& conditionalIrVega,
    std::vector<std::vector<RandomVariable>>& conditionalFxVega) {

    for (std::size_t b = 0; b < irDeltaConverter[ccyIndex].times().size(); ++b) {
        // conditionalIrDelta[ccyIndex][b] = tmpIrDelta[b]; // no conversion
        for (std::size_t z = 0; z < irDeltaConverter[ccyIndex].times().size(); ++z) {
            conditionalIrDelta[ccyIndex][b] +=
                RandomVariable(model_->size(), irDeltaConverter[ccyIndex].dzerodpar()(z, b)) * tmpIrDelta[z];
        }
    }

    for (std::size_t b = 0; b < irVegaConverter[ccyIndex].optionTimes().size(); ++b) {
        // conditionalIrVega[ccyIndex][b] = tmpIrVega[b]; // no conversion
        for (std::size_t z = 0; z < irVegaConverter[ccyIndex].optionTimes().size(); ++z) {
            conditionalIrVega[ccyIndex][b] +=
                RandomVariable(model_->size(), irVegaConverter[ccyIndex].dzerodpar()(z, b) * 1E-4) * tmpIrVega[z];
        }
    }

    if (ccyIndex > 0) {

        // fx delta

        conditionalFxDelta[ccyIndex - 1] += tmpFxDelta;

        // fx vega (including par conversion)

        for (std::size_t b = 0; b < fxVegaConverter[ccyIndex - 1].optionTimes().size(); ++b) {
            // conditionalFxVega[ccyIndex - 1][b] = tmpFxVega[b]; // no conversion
            for (std::size_t z = 0; z < fxVegaConverter[ccyIndex - 1].optionTimes().size(); ++z) {
                conditionalFxVega[ccyIndex - 1][b] +=
                    RandomVariable(model_->size(), fxVegaConverter[ccyIndex - 1].dzerodpar()(z, b) * 1E-2) *
                    tmpFxVega[z];
            }
        }
    }
}

RandomVariable XvaEngineCG::dynamicImCombineComponents(const std::vector<const RandomVariable*>& componentDerivatives,
                                                       const std::size_t tradeId, const std::size_t timeStep) {

    std::size_t nComponents = tradeExposureValuation_[tradeId][timeStep].componentPathValues.size();

    auto g = model_->computationGraph();

    std::vector<RandomVariable> tmp(g->size());

    std::size_t startNode = tradeExposureValuation_[tradeId][timeStep].componentPathValues.back() + 1;
    std::size_t endNode = tradeExposureValuation_[tradeId][timeStep].targetConditionalExpectationDerivatives;

    // set the values we need for evaluation

    for (auto const n : dependentNodes(*g, startNode, endNode)) {
        tmp[n] = values_[n];
    }

    // set the component derivatives

    for (std::size_t c = 0; c < nComponents; ++c) {
        tmp[tradeExposureValuation_[tradeId][timeStep].componentPathValues[c]] = *componentDerivatives[c];
    }

    // combine the components

    std::vector<bool> keepNodes(g->size(), false);
    keepNodes[endNode] = true;

    forwardEvaluation(*g, tmp, ops_, RandomVariable::deleter, false, {}, keepNodes, startNode, endNode + 1);

    return tmp[endNode];
}

void XvaEngineCG::calculateDynamicIM() {

    DLOG("XvaEngineCG: calculate dynamic im.");

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    // init result container

    std::set<std::string> nettingSetIds;
    for (auto const& [id, t] : portfolio_->trades())
        nettingSetIds.insert(t->envelope().nettingSetId());

    for (auto const& n : nettingSetIds) {
        dynamicIM_[n] = std::vector<RandomVariable>(valuationDates_.size() + 1, RandomVariable(model_->size()));
        dynamicDeltaIM_[n] = std::vector<RandomVariable>(valuationDates_.size() + 1, RandomVariable(model_->size()));
        dynamicVegaIM_[n] = std::vector<RandomVariable>(valuationDates_.size() + 1, RandomVariable(model_->size()));
        dynamicCurvatureIM_[n] =
            std::vector<RandomVariable>(valuationDates_.size() + 1, RandomVariable(model_->size()));
    }

    // sensi bucketing configuration

    const std::vector<QuantLib::Period> irDeltaTerms{2 * Weeks,  1 * Months, 3 * Months, 6 * Months,
                                                     1 * Years,  2 * Years,  3 * Years,  5 * Years,
                                                     10 * Years, 15 * Years, 20 * Years, 30 * Years};
    const std::vector<IrDeltaParConverter::InstrumentType> irDeltaInstruments{
        IrDeltaParConverter::InstrumentType::Deposit, IrDeltaParConverter::InstrumentType::Deposit,
        IrDeltaParConverter::InstrumentType::Deposit, IrDeltaParConverter::InstrumentType::Deposit,
        IrDeltaParConverter::InstrumentType::Swap,    IrDeltaParConverter::InstrumentType::Swap,
        IrDeltaParConverter::InstrumentType::Swap,    IrDeltaParConverter::InstrumentType::Swap,
        IrDeltaParConverter::InstrumentType::Swap,    IrDeltaParConverter::InstrumentType::Swap,
        IrDeltaParConverter::InstrumentType::Swap,    IrDeltaParConverter::InstrumentType::Swap};

    const std::vector<QuantLib::Period> irVegaTerms{2 * Weeks,  1 * Months, 3 * Months, 6 * Months,
                                                    1 * Years,  2 * Years,  3 * Years,  5 * Years,
                                                    10 * Years, 15 * Years, 20 * Years, 30 * Years};

    const std::vector<QuantLib::Period> irVegaUnderlyingTerms{30 * Years, 30 * Years, 30 * Years, 30 * Years,
                                                              29 * Years, 28 * Years, 27 * Years, 25 * Years,
                                                              20 * Years, 15 * Years, 10 * Years, 1 * Years};

    const std::vector<QuantLib::Period> fxVegaTerms{2 * Weeks,  1 * Months, 3 * Months, 6 * Months,
                                                    1 * Years,  2 * Years,  3 * Years,  5 * Years,
                                                    10 * Years, 15 * Years, 20 * Years, 30 * Years};

    // set up ir delta, vega and fx vega conversion matrices

    std::vector<IrDeltaParConverter> irDeltaConverter(model_->currencies().size());
    std::vector<LgmSwaptionVegaParConverter> irVegaConverter(model_->currencies().size());
    std::vector<CcLgmFxOptionVegaParConverter> fxVegaConverter(model_->currencies().size() - 1);

    for (std::size_t ccyIndex = 0; ccyIndex < model_->currencies().size(); ++ccyIndex) {

        irDeltaConverter[ccyIndex] =
            IrDeltaParConverter(irDeltaTerms, irDeltaInstruments,
                                *initMarket_->swapIndex(initMarket_->swapIndexBase(model_->currencies()[ccyIndex])),
                                [this](const Date& d) { return model_->actualTimeFromReference(d); });

        irVegaConverter[ccyIndex] = LgmSwaptionVegaParConverter(
            model_->cam()->lgm(ccyIndex), irVegaTerms, irVegaUnderlyingTerms,
            *initMarket_->swapIndex(initMarket_->swapIndexBase(model_->currencies()[ccyIndex])));

        if (ccyIndex > 0) {
            fxVegaConverter[ccyIndex - 1] = CcLgmFxOptionVegaParConverter(*model_->cam(), ccyIndex - 1, fxVegaTerms);
        }
    }

    // set up im calculator

    SimpleDynamicSimm imCalculator(model_->size(), model_->currencies(), irDeltaTerms, irVegaTerms, fxVegaTerms,
                                   QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_6_5>(
                                       QuantLib::ext::make_shared<SimmBucketMapperBase>(), 10));

    // calculate derivatives and derive dynamic im from them

    std::vector<bool> keepNodesDerivatives(g->size(), false);
    for (auto const& [n, v] : baseModelParams_) {
        keepNodesDerivatives[n] = true;
    }

    
    for (std::size_t i = 0; i < valuationDates_.size() + 1; i += dynamicIMStepSize_) {

        Date valDate = i == 0 ? model_->referenceDate() : valuationDates_[i - 1];
        Real t = model_->actualTimeFromReference(valDate);

        std::map<std::string, std::size_t> currencyLookup;
        std::size_t index = 0;
        for (auto const& c : model_->currencies())
            currencyLookup[c] = index++;

        /* calculate path derivatives for plain trades, grouped by model parameter groups to be able
           to filter out unwanted sensitivities that are artifacts of the simulation */

        std::vector<std::vector<RandomVariable>> pathIrDelta(
            model_->currencies().size(),
            std::vector<RandomVariable>(irDeltaTerms.size(), RandomVariable(model_->size())));
        std::vector<RandomVariable> pathFxDelta(model_->currencies().size() - 1, RandomVariable(model_->size()));
        std::vector<std::vector<RandomVariable>> pathIrVega(
            model_->currencies().size(),
            std::vector<RandomVariable>(irVegaTerms.size(), RandomVariable(model_->size())));
        std::vector<std::vector<RandomVariable>> pathFxVega(
            model_->currencies().size() - 1,
            std::vector<RandomVariable>(fxVegaTerms.size(), RandomVariable(model_->size())));

	// additional IM calculator results
	auto deltaMarginIr = QuantLib::ext::make_shared<RandomVariable>(model_->size(), 0.0);
	auto vegaMarginIr = QuantLib::ext::make_shared<RandomVariable>(model_->size(), 0.0);
	auto curvatureMarginIr = QuantLib::ext::make_shared<RandomVariable>(model_->size(), 0.0);
	auto deltaMarginFx = QuantLib::ext::make_shared<RandomVariable>(model_->size(), 0.0);
	auto vegaMarginFx = QuantLib::ext::make_shared<RandomVariable>(model_->size(), 0.0);
	auto curvatureMarginFx = QuantLib::ext::make_shared<RandomVariable>(model_->size(), 0.0);
	
        for (auto const& [parameterGroup, exposureNode] : dynamicImInfo_[i].plainTradeSumGrouped) {

            // init derivatives container

            for (auto& r : dynamicIMDerivatives_)
                r = RandomVariable(model_->size());

            dynamicIMDerivatives_[exposureNode].setAll(1.0);

            // run backward derivatives from n, note: we use eps = 0 in grads_ here!

            backwardDerivatives(*g, values_, dynamicIMDerivatives_, grads_, RandomVariable::deleter,
                                keepNodesDerivatives, ops_, opNodeRequirements_, keepNodes_,
                                RandomVariableOpCode::ConditionalExpectation,
                                ops_[RandomVariableOpCode::ConditionalExpectation]);

            dynamicImAddToPathSensis(parameterGroup, valDate, t, currencyLookup, irDeltaConverter, irVegaConverter,
                                     fxVegaConverter, pathIrDelta, pathFxDelta, pathIrVega, pathFxVega);

        } // loop over parameter groups

        // calculate conditional expectations on the aggregated sensis and convert to par if applicable

        std::vector<RandomVariable> tmpIrDelta(irDeltaTerms.size(), RandomVariable(model_->size()));
        RandomVariable tmpFxDelta(model_->size());
        std::vector<RandomVariable> tmpIrVega(irVegaTerms.size(), RandomVariable(model_->size()));
        std::vector<RandomVariable> tmpFxVega(fxVegaTerms.size(), RandomVariable(model_->size()));

        std::vector<std::vector<RandomVariable>> conditionalIrDelta(
            model_->currencies().size(),
            std::vector<RandomVariable>(irDeltaTerms.size(), RandomVariable(model_->size())));
        std::vector<RandomVariable> conditionalFxDelta(model_->currencies().size() - 1, RandomVariable(model_->size()));
        std::vector<std::vector<RandomVariable>> conditionalIrVega(
            model_->currencies().size(),
            std::vector<RandomVariable>(irVegaTerms.size(), RandomVariable(model_->size())));
        std::vector<std::vector<RandomVariable>> conditionalFxVega(
            model_->currencies().size() - 1,
            std::vector<RandomVariable>(fxVegaTerms.size(), RandomVariable(model_->size())));

        auto regressorGroups = dynamicImInfo_[i].plainTradeRegressorGroups;

        auto condExp = [this, &regressorGroups](const std::vector<const RandomVariable*>& args) {
            return randomVariableOpConditionalExpectation(model_->size(), regressionOrder_,
                                                          QuantLib::LsmBasisSystem::Monomial, regressionVarianceCutoff_,
                                                          regressorGroups, usePythonIntegration_, args);
        };

        // first entry is populated below with each regressand
        std::vector<const RandomVariable*> args(1);
        // second entry is the filter which we set to trivial here
        RandomVariable trivialFilter(model_->size(), 1.0);
        args.push_back(&trivialFilter);
        // the remaining entries are the regressors
        for (const auto& r : dynamicImInfo_[i].plainTradeRegressors)
            args.push_back(&values_[r]);

        for (std::size_t ccy = 0; ccy < model_->currencies().size(); ++ccy) {

            // ir delta

            for (std::size_t b = 0; b < irDeltaTerms.size(); ++b) {
                args[0] = &pathIrDelta[ccy][b];
                tmpIrDelta[b] = condExp(args);
            }

            // ir vega (including par conversion)

            for (std::size_t b = 0; b < irVegaTerms.size(); ++b) {
                args[0] = &pathIrVega[ccy][b];
                tmpIrVega[b] = condExp(args);
            }

            if (ccy > 0) {

                // fx delta
                args[0] = &pathFxDelta[ccy - 1];
                tmpFxDelta = condExp(args);

                // fx vega (including par conversion)

                for (std::size_t b = 0; b < fxVegaTerms.size(); ++b) {
                    args[0] = &pathFxVega[ccy - 1][b];
                    tmpFxVega[b] = condExp(args);
                }
            }

            dynamicImAddToConvertedSensis(ccy, tmpIrDelta, tmpIrVega, tmpFxVega, tmpFxDelta, irDeltaConverter,
                                          irVegaConverter, fxVegaConverter, conditionalIrDelta, conditionalFxDelta,
                                          conditionalIrVega, conditionalFxVega);
        }

        // handle individual trades

        for (std::size_t tradeId : dynamicImInfo_[i].individualTradeIds) {

            auto parameterGroup = tradeExposureMetaInfo_[tradeId].relevantModelParameters;

            // individual trade: loop over all components of the trade

            std::size_t nComponents = tradeExposureValuation_[tradeId][i].componentPathValues.size();

            std::vector<std::vector<std::vector<RandomVariable>>> pathIrDeltaC(
                nComponents, std::vector<std::vector<RandomVariable>>(
                                 model_->currencies().size(),
                                 std::vector<RandomVariable>(irDeltaTerms.size(), RandomVariable(model_->size()))));
            std::vector<std::vector<RandomVariable>> pathFxDeltaC(
                nComponents,
                std::vector<RandomVariable>(model_->currencies().size() - 1, RandomVariable(model_->size())));
            std::vector<std::vector<std::vector<RandomVariable>>> pathIrVegaC(
                nComponents, std::vector<std::vector<RandomVariable>>(
                                 model_->currencies().size(),
                                 std::vector<RandomVariable>(irVegaTerms.size(), RandomVariable(model_->size()))));
            std::vector<std::vector<std::vector<RandomVariable>>> pathFxVegaC(
                nComponents, std::vector<std::vector<RandomVariable>>(
                                 model_->currencies().size() - 1,
                                 std::vector<RandomVariable>(fxVegaTerms.size(), RandomVariable(model_->size()))));

            for (std::size_t comp = 0; comp < tradeExposureValuation_[tradeId][i].componentPathValues.size(); ++comp) {

                auto n = tradeExposureValuation_[tradeId][i].componentPathValues[comp];

                // individual trade: run backward derivatives over component

                for (auto& r : dynamicIMDerivatives_)
                    r = RandomVariable(model_->size());

                dynamicIMDerivatives_[n].setAll(1.0);

                backwardDerivatives(*g, values_, dynamicIMDerivatives_, grads_, RandomVariable::deleter,
                                    keepNodesDerivatives, ops_, opNodeRequirements_, keepNodes_,
                                    RandomVariableOpCode::ConditionalExpectation,
                                    ops_[RandomVariableOpCode::ConditionalExpectation]);

                dynamicImAddToPathSensis(parameterGroup, valDate, t, currencyLookup, irDeltaConverter, irVegaConverter,
                                         fxVegaConverter, pathIrDeltaC[comp], pathFxDeltaC[comp], pathIrVegaC[comp],
                                         pathFxVegaC[comp]);
            }

            // individual trade: run part of the cg that combines the components

            std::vector<const RandomVariable*> compDer(nComponents);

            // set to 0 because they are only set below for ccy > 0
            tmpFxDelta.setAll(0.0);
            for (auto& r : tmpFxVega)
                r.setAll(0.0);

            for (std::size_t ccy = 0; ccy < model_->currencies().size(); ++ccy) {

                for (std::size_t b = 0; b < irDeltaTerms.size(); ++b) {
                    for (std::size_t comp = 0; comp < nComponents; ++comp)
                        compDer[comp] = &pathIrDeltaC[comp][ccy][b];
                    tmpIrDelta[b] = dynamicImCombineComponents(compDer, tradeId, i) *
                                    RandomVariable(model_->size(), tradeExposureValuation_[tradeId][i].multiplier);
                }
                for (std::size_t b = 0; b < irVegaTerms.size(); ++b) {
                    for (std::size_t comp = 0; comp < nComponents; ++comp)
                        compDer[comp] = &pathIrVegaC[comp][ccy][b];
                    tmpIrVega[b] = dynamicImCombineComponents(compDer, tradeId, i) *
                                   RandomVariable(model_->size(), tradeExposureValuation_[tradeId][i].multiplier);
                }
                if (ccy > 0) {
                    for (std::size_t comp = 0; comp < nComponents; ++comp)
                        compDer[comp] = &pathFxDeltaC[comp][ccy - 1];
                    tmpFxDelta = dynamicImCombineComponents(compDer, tradeId, i) *
                                 RandomVariable(model_->size(), tradeExposureValuation_[tradeId][i].multiplier);
                    for (std::size_t b = 0; b < fxVegaTerms.size(); ++b) {
                        for (std::size_t comp = 0; comp < nComponents; ++comp)
                            compDer[comp] = &pathFxVegaC[comp][ccy - 1][b];
                        tmpFxVega[b] = dynamicImCombineComponents(compDer, tradeId, i) *
                                       RandomVariable(model_->size(), tradeExposureValuation_[tradeId][i].multiplier);
                    }
                }

                dynamicImAddToConvertedSensis(ccy, tmpIrDelta, tmpIrVega, tmpFxVega, tmpFxDelta, irDeltaConverter,
                                              irVegaConverter, fxVegaConverter, conditionalIrDelta, conditionalFxDelta,
                                              conditionalIrVega, conditionalFxVega);
            }

        } // loop over individual trades

        // scale ir vega for im calculation

        for (std::size_t ccy = 0; ccy < model_->currencies().size(); ++ccy) {
            for (std::size_t b = 0; b < irVegaConverter[ccy].optionTimes().size(); ++b) {
                conditionalIrVega[ccy][b] *=
                    RandomVariable(model_->size(), 1E4 * irVegaConverter[ccy].baseImpliedVols()[b]);
            }
        }

        // set results for this valuation date

        for (auto const& n : nettingSetIds) {
            dynamicIM_[n][i] =
	      imCalculator.value(conditionalIrDelta, conditionalIrVega, conditionalFxDelta, conditionalFxVega,
				 deltaMarginIr, vegaMarginIr, curvatureMarginIr,
				 deltaMarginFx, vegaMarginFx, curvatureMarginFx);
            if (deltaMarginIr && deltaMarginFx)
                dynamicDeltaIM_[n][i] = *deltaMarginIr + *deltaMarginFx;
            if (vegaMarginIr && vegaMarginFx)
                dynamicVegaIM_[n][i] = *vegaMarginIr + *vegaMarginFx;
            if (curvatureMarginIr && curvatureMarginFx)
                dynamicCurvatureIM_[n][i] = *curvatureMarginIr + *curvatureMarginFx;

            for (Size j = i + 1; j < std::min(i + dynamicIMStepSize_, valuationDates_.size() + 1); ++j) {
                dynamicIM_[n][j] = dynamicIM_[n][i];
		dynamicDeltaIM_[n][j] = dynamicDeltaIM_[n][i];
                dynamicVegaIM_[n][j] = dynamicVegaIM_[n][i];
                dynamicCurvatureIM_[n][j] = dynamicCurvatureIM_[n][i];
            }
        }

    } // loop over valuation dates

    timing_dynamicIM_ = timer.elapsed().wall;
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

            xvaDerivatives_[cvaNode_] = RandomVariable(model_->size(), 1.0);

            std::vector<bool> keepNodesDerivatives(g->size(), false);

            for (auto const& [n, v] : baseModelParams_)
                keepNodesDerivatives[n] = true;

            // backward derivatives run

            backwardDerivatives(*g, values_, xvaDerivatives_, grads_, RandomVariable::deleter, keepNodesDerivatives,
                                ops_, opNodeRequirements_, keepNodes_, RandomVariableOpCode::ConditionalExpectation,
                                ops_[RandomVariableOpCode::ConditionalExpectation]);

            // read model param derivatives

            Size i = 0;
            for (auto const& [n, v] : baseModelParams_) {
                modelParamDerivatives[i++] = expectation(xvaDerivatives_[n]).at(0);
            }

            // get mem consumption

            rvMemMax_ = std::max(rvMemMax_, numberOfStochasticRvs(values_) + numberOfStochasticRvs(xvaDerivatives_)) +
                        numberOfStochasticRvs(dynamicIMDerivatives_);

            DLOG("XvaEngineCG: got " << modelParamDerivatives.size()
                                     << " model parameter derivatives from run backward derivatives");

            timing_bwd_ = timer.elapsed().wall;

            // Delete values and derivatives vectors, they are not needed from this point on
            // except we are doing a full revaluation!

            values_.clear();
            xvaDerivatives_.clear();
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

                    std::vector<std::pair<std::size_t, double>> modelParameters;
                    for (auto const& p : model_->modelParameters()) {
                        modelParameters.push_back(std::make_pair(p.node(), p.eval()));
                    }

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

                    std::vector<std::pair<std::size_t, double>> modelParameters;
                    for (auto const& p : model_->modelParameters()) {
                        modelParameters.push_back(std::make_pair(p.node(), p.eval()));
                    }

                    if (useExternalComputeDevice_) {
                        ComputeEnvironment::instance().context().initiateCalculation(
                            model_->size(), externalCalculationId_, 0, externalComputeDeviceSettings_);
                        populateConstants(values_, valuesExternal_);
                        populateModelParameters(modelParameters, values_, valuesExternal_);
                        finalizeExternalCalculation();
                    } else {
                        populateModelParameters(modelParameters, values_, valuesExternal_);
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
    ReportWriter().writeScenarioReport(*sensiReport_, {sensiCube}, 1E-6);
}

void XvaEngineCG::cleanUpAfterCalcs() {
    values_.clear();
    dynamicIMDerivatives_.clear();
    xvaDerivatives_.clear();
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
    LOG("XvaEngineCG: Part C2 CG build         : " << std::fixed << std::setprecision(1) << timing_partc2_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Part D CG build          : " << std::fixed << std::setprecision(1) << timing_partd_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Const and Model params   : " << std::fixed << std::setprecision(1) << timing_popparam_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: RV gen                   : " << std::fixed << std::setprecision(1) << timing_poprv_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Forward eval             : " << std::fixed << std::setprecision(1) << timing_fwd_ / 1E6 << " ms");
    LOG("XvaEngineCG: DynamicIM             : " << std::fixed << std::setprecision(1) << timing_dynamicIM_ / 1E6
                                                << " ms");
    LOG("XvaEngineCG: Backward deriv           : " << std::fixed << std::setprecision(1) << timing_bwd_ / 1E6 << " ms");
    LOG("XvaEngineCG: Sensi Cube Gen           : " << std::fixed << std::setprecision(1) << timing_sensi_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Populate ASD             : " << std::fixed << std::setprecision(1) << timing_asd_ / 1E6 << " ms");
    LOG("XvaEngineCG: Populate NPV Outcube     : " << std::fixed << std::setprecision(1) << timing_outcube_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: Populate IM Outcube      : " << std::fixed << std::setprecision(1) << timing_imcube_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: total                    : " << std::fixed << std::setprecision(1) << timing_total_ / 1E6
                                                   << " ms");
    LOG("XvaEngineCG: all done.");
}

void XvaEngineCG::run() {

    LOG("XvaEngineCG::run(): firstRun is " << std::boolalpha << firstRun_);
    boost::timer::cpu_timer timer;

    updateProgress(0, 5);

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

    if (enableDynamicIM_) {
        buildCgDynamicIM();
    }

    if (firstRun_) {
        buildAsdNodes();
    }

    // the cg is final at this point

    if (firstRun_) {
        outputGraphStats();
    }

    getExternalContext();

    updateProgress(1, 5);

    setupValueContainers();
    doForwardEvaluation();

    // dump model parameters
    // for(auto const&p : model_->modelParameters())
    //     std::cout << p << "," << p.eval() << std::endl;

    updateProgress(2, 5);

    populateAsd();
    populateNpvOutputCube();

    updateProgress(3, 5);

    if (enableDynamicIM_) {
        calculateDynamicIM();
    }

    populateDynamicIMOutputCube();

    updateProgress(4, 5);

    if (mode_ == Mode::Full) {
        generateXvaReports();
        calculateSensitivities();
        generateSensiReports();
    }

    updateProgress(5, 5);

    timing_total_ = timer.elapsed().wall;
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

void XvaEngineCG::setDynamicIMOutputCube(
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& dynamicIMOutputCube) {
    dynamicIMOutputCube_ = dynamicIMOutputCube;
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
