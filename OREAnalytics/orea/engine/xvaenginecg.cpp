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
#include <qle/methods/cclgmfxoptionvegaparconverter.hpp>
#include <qle/methods/lgmswaptionvegaparconverter.hpp>
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
                         const bool tradeLevelBreakDown, const bool useRedBlocks, const bool useExternalComputeDevice,
                         const bool externalDeviceCompatibilityMode,
                         const bool useDoublePrecisionForExternalCalculation, const std::string& externalComputeDevice,
                         const bool continueOnCalibrationError, const bool continueOnError, const std::string& context)
    : mode_(mode), asof_(asof), loader_(loader), curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams),
      simMarketData_(simMarketData), engineData_(engineData), crossAssetModelData_(crossAssetModelData),
      scenarioGeneratorData_(scenarioGeneratorData), portfolio_(portfolio), marketConfiguration_(marketConfiguration),
      marketConfigurationInCcy_(marketConfigurationInCcy), sensitivityData_(sensitivityData),
      referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig), bumpCvaSensis_(bumpCvaSensis),
      enableDynamicIM_(enableDynamicIM), dynamicIMStepSize_(dynamicIMStepSize), regressionOrder_(regressionOrder),
      tradeLevelBreakDown_(tradeLevelBreakDown), useRedBlocks_(useRedBlocks),
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
               "XvaEngineCG::buildCam(): sticky close out dates ("
                   << stickyCloseOutDates_.size() << ") do not match simulation dates (" << simulationDates_.size()
                   << ") - internal error!");

    // note: this should be added to CrossAssetModelData
    Size timeStepsPerYear = 1;

    // note: projectedStateProcessIndices can be removed from GaussianCamCG constructor most probably?
    model_ = QuantLib::ext::make_shared<GaussianCamCG>(
        camBuilder_->model(), scenarioGeneratorData_->samples(), currencies, curves, fxSpots, irIndices, infIndices,
        indices, indexCurrencies, simulationDates_, timeStepsPerYear, iborFallbackConfig_, std::vector<Size>(),
        std::vector<std::string>(), stickyCloseOutDates_);
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

std::size_t XvaEngineCG::getAmcNpvIndexForValuationDate(const std::size_t i) const {
    if (closeOutDates_.empty()) {
        // there are no close-out dates -> __AMC_NPV_i is refering to the valuation date
        return i;
    } else if (stickyCloseOutDates_.empty()) {
        // we have close-out dates, but not sticky -> __AMC_NPV_i refers to the simulation date i
        if (auto s = simulationDates_.find(valuationDates_[i]); s != simulationDates_.end()) {
            return std::distance(simulationDates_.begin(), s);
        }
        QL_FAIL("XvaEngineCG::getAmcNpvIndexForValuationDate(" << i << "): no simulation date found for valuation date "
                                                               << valuationDates_[i]);
    } else {
        // close-out dates, sticky -> __AMC_NPV_i contains valuation date , then sticky close-out date values
        return i;
    }
}

std::size_t XvaEngineCG::getAmcNpvIndexForCloseOutDate(const std::size_t i) const {
    if (closeOutDates_.empty()) {
        // there are no close-out dates -> error
        QL_FAIL("XvaEngineCG::getAmcNpvIndexForCloseOutDate(i): internal error, no close-out dates are given.");
    } else if (stickyCloseOutDates_.empty()) {
        // we have close-out dates, but not sticky -> __AMC_NPV_i refers to the simulation date i
        if (auto s = simulationDates_.find(closeOutDates_[i]); s != simulationDates_.end()) {
            return std::distance(simulationDates_.begin(), s);
        }
        QL_FAIL("XvaEngineCG::getAmcNpvIndexForCloseOutDate(" << i << "): no simulation date found for valuation date "
                                                              << closeOutDates_[i]);
    } else {
        // close-out dates, sticky -> __AMC_NPV_i contains valuation date , then sticky close-out date values
        return valuationDates_.size() + i;
    }
}

void XvaEngineCG::buildCgPartB() {
    DLOG("XvaEngineCG: build computation graph for all trades");

    // Build computation graph for all trades ("part B") and
    // - store npv, amc npv nodes

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    for (auto const& [id, trade] : portfolio_->trades()) {

        std::size_t multiplier = cg_const(*g, trade->instrument()->multiplier() * trade->instrument()->multiplier2());

        auto engine = QuantLib::ext::dynamic_pointer_cast<AmcCgPricingEngine>(
            trade->instrument()->qlInstrument()->pricingEngine());
        QL_REQUIRE(engine, "XvaEngineCG: expected to get AmcCgPricingEngine, trade '"
                               << id << "' has a different or no engine attached.");
        if (useRedBlocks_)
            g->startRedBlock();

        if (!trade->instrument()->qlInstrument()->isCalculated())
            trade->instrument()->qlInstrument()->recalculate(); // trigger setupArguments
        engine->buildComputationGraph(false, false);

        {
            std::vector<std::size_t> tmp;
            tmp.push_back(cg_mult(*g, multiplier, g->variable(engine->npvName() + "_0")));
            for (std::size_t i = 0; i < valuationDates_.size(); ++i) {
                tmp.push_back(cg_mult(*g, multiplier,
                                      g->variable("_AMC_NPV_" + std::to_string(getAmcNpvIndexForValuationDate(i)))));
            }
            amcNpvNodes_.push_back(tmp);
        }

        if (!closeOutDates_.empty()) {

            // note: hardcode reevaluate exercise decision to false (expose to config?)
            model_->useStickyCloseOutDates(true);
            engine->buildComputationGraph(true, false);
            model_->useStickyCloseOutDates(false);

            std::vector<std::size_t> tmp;
            tmp.push_back(cg_mult(*g, multiplier, g->variable(engine->npvName() + "_0")));
            for (std::size_t i = 0; i < closeOutDates_.size(); ++i) {
                tmp.push_back(cg_mult(*g, multiplier,
                                      g->variable("_AMC_NPV_" + std::to_string(getAmcNpvIndexForCloseOutDate(i)))));
            }
            amcNpvCloseOutNodes_.push_back(tmp);
        }

        if (useRedBlocks_)
            g->endRedBlock();
        tradeCurrencyGroup_.push_back(engine->relevantCurrencies());
    }

    timing_partb_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: build computation graph for all trades done - graph size is "
         << model_->computationGraph()->size());
}

std::pair<std::size_t, std::size_t> XvaEngineCG::createPortfolioExposureNode(const std::size_t dateIndex,
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

    model_->useStickyCloseOutDates(!stickyCloseOutDates_.empty());
    std::set<std::set<std::size_t>> pfRegressorGroups;
    std::set<std::size_t> pfRegressors;
    std::vector<std::size_t> tradeSum(portfolio_->trades().size());
    for (Size j = 0; j < portfolio_->trades().size(); ++j) {
        tradeSum[j] = isValuationDate ? amcNpvNodes_[j][dateIndex] : amcNpvCloseOutNodes_[j][dateIndex];
        std::set<std::size_t> tradeRegressors = model_->npvRegressors(obsDate, tradeCurrencyGroup_[j]);
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

    std::size_t pfExposureNodePathwiseTmp = cg_add(*g, tradeSum);
    std::size_t pfExposureNodeTmp =
        model_->npv(pfExposureNodePathwiseTmp, obsDate, cg_const(*g, 1.0), std::nullopt, {}, pfRegressors);
    pfRegressorPosGroups_[pfExposureNodePathwiseTmp] = pfRegressorPosGroups;
    pfRegressorPosGroups_[pfExposureNodeTmp] = pfRegressorPosGroups;

    model_->useStickyCloseOutDates(false);
    return std::make_pair(pfExposureNodePathwiseTmp, pfExposureNodeTmp);
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

    model_->useStickyCloseOutDates(!stickyCloseOutDates_.empty());
    std::size_t res = model_->npv(
        isValuationDate ? amcNpvNodes_[tradeIndex][dateIndex] : amcNpvCloseOutNodes_[tradeIndex][dateIndex], obsDate,
        cg_const(*g, 1.0), {}, {}, model_->npvRegressors(obsDate, tradeCurrencyGroup_[tradeIndex]));
    model_->useStickyCloseOutDates(false);
    return res;
}

void XvaEngineCG::buildCgPartC() {
    DLOG("XvaEngineCG: add exposure nodes to graph");

    // Add nodes that sum the exposure over trades and add conditional expectations on pf level
    // Optionally, add conditional expectations on trade level (if we have to populate a legacy npv cube)
    // This constitutes part C of the computation graph spanning "trade m range end ... lastExposureNode"
    // - pfExposureNodes          :     the conditional expectations on pf level
    // - tradeExposureNodes       :     the conditional expectations on trade level
    // - prExposureNodesInflated  :     pfExposureNode times numeraire evaluated at associated sim time

    boost::timer::cpu_timer timer;
    auto g = model_->computationGraph();

    if (mode_ == Mode::Full || enableDynamicIM_ || !tradeLevelBreakDown_) {
        for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
            auto [n1, n2] = createPortfolioExposureNode(i, true);
            pfExposureNodesPathwise_.push_back(n1);
            pfExposureNodes_.push_back(n2);
            if (!closeOutDates_.empty()) {
                auto [_, n] = createPortfolioExposureNode(i, false);
                pfExposureCloseOutNodes_.push_back(n);
            }
            auto tmp = model_->numeraire(i == 0 ? model_->referenceDate() : valuationDates_[i - 1]);
            pfExposureNodesPathwiseInflated_.push_back(cg_mult(*g, pfExposureNodesPathwise_.back(), tmp));
            // copy over the regressor group from the pf exp node to the inflated version of the same node
            pfRegressorPosGroups_[pfExposureNodesPathwiseInflated_.back()] =
                pfRegressorPosGroups_[pfExposureNodes_.back()];
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

    // Add notes to store ir states, this is needed for dynamic delta calc (and only that)
    // In fact, this is probably not needed.

    // if (enableDynamicIM_) {
    //     irState_.resize(model_->currencies().size(), std::vector<std::size_t>(valuationDates_.size() + 1));
    //     for (Size ccy = 0; ccy < model_->currencies().size(); ++ccy) {
    //         for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
    //             irState_[ccy][i] =
    //                 model_->getInterpolatedIrState(i == 0 ? model_->referenceDate() : valuationDates_[i - 1], ccy);
    //         }
    //     }
    // }

    timing_partc_ = timer.elapsed().wall;
    DLOG("XvaEngineCG: add exposure nodes to graph done - graph size is "
         << g->size() << ", generateTradeLevelExposure = " << std::boolalpha << tradeLevelBreakDown_);
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
    for (Size i = 0; i < valuationDates_.size(); ++i) {
        Date d = i == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), i - 1);
        Date e = *std::next(valuationDates_.begin(), i);
        std::size_t defaultProb =
            addModelParameter(*g, model_->modelParameters(),
                              ModelCG::ModelParameter(ModelCG::ModelParameter::Type ::defaultProb, {}, {}, d),
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
        // todo set regression variance cutoff
        ops_ =
            getRandomVariableOps(model_->size(), regressionOrder_, QuantLib::LsmBasisSystem::Monomial,
                                 (sensitivityData_ && bumpCvaSensis_) ? eps : 0.0, Null<Real>(), pfRegressorPosGroups_);
        grads_ = getRandomVariableGradients(model_->size(), 4, QuantLib::LsmBasisSystem::Monomial, eps);
    }

    bool keepValuesForDerivatives = (!bumpCvaSensis_ && sensitivityData_) || enableDynamicIM_;

    keepNodes_ = std::vector<bool>(g->size(), false);

    for (auto const& c : g->constants()) {
        keepNodes_[c.second] = true;
    }

    for (auto const& [n, v] : baseModelParams_) {
        keepNodes_[n] = true;
    }

    for (auto const& n : pfExposureNodes_) {
        keepNodes_[n] = true;
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
            externalOutputNodes_.insert(externalOutputNodes_.end(), pfExposureNodes_.begin(), pfExposureNodes_.end());
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
                       : (isCloseOut ? pfExposureCloseOutNodes_[dateIndex] : pfExposureNodes_[dateIndex]);
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

        dynamicIMOutputCube_->setT0(im[0].at(0), 0);

        for (Size i = 0; i < valuationDates_.size(); ++i) {
            for (Size k = 0; k < im[i + 1].size(); ++k) {
                dynamicIMOutputCube_->set(im[i + 1][k], nidx->second, i, k, 0);
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

    for (Size i = 0; i < valuationDates_.size() + 1; ++i) {
        epeReport_->next();
        epeReport_->add(i == 0 ? model_->referenceDate() : *std::next(valuationDates_.begin(), i - 1))
            .add(expectation(max(values_[pfExposureNodes_[i]], RandomVariable(model_->size(), 0.0))).at(0))
            .add(expectation(max(-values_[pfExposureNodes_[i]], RandomVariable(model_->size(), 0.0))).at(0));
    }
    epeReport_->end();
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
    }

    // sensi bucketing configuration

    const std::vector<QuantLib::Period> irDeltaTerms{1 * Years, 5 * Years, 10 * Years, 30 * Years};

    const std::vector<QuantLib::Period> irVegaTerms{1 * Months, 6 * Months, 1 * Years,
                                                    5 * Years,  10 * Years, 20 * Years};
    const std::vector<QuantLib::Period> irVegaUnderlyingTerms{30 * Years, 30 * Years, 29 * Years,
                                                              25 * Years, 20 * Years, 10 * Years};

    const std::vector<QuantLib::Period> fxVegaTerms{1 * Months, 6 * Months, 1 * Years,
                                                    5 * Years,  10 * Years, 20 * Years};

    // set up ir delta times

    std::vector<double> irDeltaTimes;
    for (auto const& p : irDeltaTerms) {
        Date d = model_->referenceDate() + p;
        irDeltaTimes.push_back(model_->actualTimeFromReference(d));
    }

    // set up ir and fx vega conversion matrices

    std::vector<LgmSwaptionVegaParConverter> irVegaConverter(model_->currencies().size());
    std::vector<CcLgmFxOptionVegaParConverter> fxVegaConverter(model_->currencies().size() - 1);

    for (std::size_t ccyIndex = 0; ccyIndex < model_->currencies().size(); ++ccyIndex) {

        irVegaConverter[ccyIndex] = LgmSwaptionVegaParConverter(
            model_->cam()->lgm(ccyIndex), irVegaTerms, irVegaUnderlyingTerms,
            *initMarket_->swapIndex(initMarket_->swapIndexBase(model_->currencies()[ccyIndex])));

        // debug output

        // std::cout << "ir vega " << model_->currencies()[ccyIndex] << ":" << std::endl;
        // std::cout << "dpardzero = \n" << irVegaConverter[ccyIndex].dpardzero() << std::endl;
        // std::cout << "dzerodpar = \n" << irVegaConverter[ccyIndex].dzerodpar() << std::endl;

        // end debug output

        if (ccyIndex > 0) {
            fxVegaConverter[ccyIndex - 1] = CcLgmFxOptionVegaParConverter(*model_->cam(), ccyIndex - 1, fxVegaTerms);

            // debug output

            // std::cout << "fx vega " << model_->currencies()[ccyIndex + 1] << ":" << std::endl;
            // std::cout << "dpardzero = \n" << fxVegaConverter[ccyIndex].dpardzero() << std::endl;
            // std::cout << "dzerodpar = \n" << fxVegaConverter[ccyIndex].dzerodpar() << std::endl;

            // end debug output
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

        std::size_t n = pfExposureNodesPathwiseInflated_[i];

        Date valDate = i == 0 ? model_->referenceDate() : valuationDates_[i - 1];
        Real t = model_->actualTimeFromReference(valDate);

        // init derivatives container

        for (auto& r : dynamicIMDerivatives_)
            r = RandomVariable(model_->size());

        dynamicIMDerivatives_[n].setAll(1.0);

        // run backward derivatives from n, note: we use eps = 0 in grads_ here!

        backwardDerivatives(*g, values_, dynamicIMDerivatives_, grads_, RandomVariable::deleter, keepNodesDerivatives,
                            ops_, opNodeRequirements_, keepNodes_, RandomVariableOpCode::ConditionalExpectation,
                            ops_[RandomVariableOpCode::ConditionalExpectation]);

        // collect and aggregate the derivatives of interest (pathwise values)

        std::map<std::string, std::size_t> currencyLookup;
        std::size_t index = 0;
        for (auto const& c : model_->currencies())
            currencyLookup[c] = index++;

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

        for (auto const& p : model_->modelParameters()) {

            // debug output: expected path derivatives for all model parameters for time step 20 (5y 2021-02-05)

            // if (i == 0) {
            //     std::cout << p << "," << expectation(dynamicIMDerivatives_[p.node()]).at(0) << std::endl;
            // }

            // debug output end

            // zero rate sensi for T - t as seen from val date t is - ( T - t ) *  P(0,T) * d NPV / d P(0,T)

            if (p.type() == ModelCG::ModelParameter::Type::dsc && p.date() > valDate && p.date2() > valDate) {
                std::size_t ccyIndex = currencyLookup.at(p.qualifier());
                Real T = model_->actualTimeFromReference(p.date());
                std::size_t bucket = std::min<std::size_t>(
                    irDeltaTerms.size() - 1,
                    std::distance(irDeltaTimes.begin(), std::lower_bound(irDeltaTimes.begin(), irDeltaTimes.end(), T)));
                Real w1 = 0.0, w2 = 1.0;
                if (bucket > 0) {
                    w1 = (irDeltaTimes[bucket] - T) /
                         (irDeltaTimes[bucket] - (bucket == 0 ? 0.0 : irDeltaTimes[bucket - 1]));
                    w2 = 1.0 - w1;
                    pathIrDelta[ccyIndex][bucket - 1] += RandomVariable(model_->size(), -(T - t) * 1E-4 * w1) *
                                                         values_[p.node()] * dynamicIMDerivatives_[p.node()];
                }
                pathIrDelta[ccyIndex][bucket] += RandomVariable(model_->size(), -(T - t) * 1E-4 * w2) *
                                                 values_[p.node()] * dynamicIMDerivatives_[p.node()];
            }

            // fx spot sensi as seen from val date t for a relative shift r is r * d NPV / d ln fxSpot, we use r = 0.01

            if (p.type() == ModelCG::ModelParameter::Type::logFxSpot) {
                std::size_t ccyIndex = currencyLookup.at(p.qualifier());
                QL_REQUIRE(
                    ccyIndex > 0,
                    "XvaEngineCG::calculateDynamicIM(): internal error, logFxSpot qualifier is equal to base ccy");
                pathFxDelta[ccyIndex - 1] += RandomVariable(model_->size(), 0.01) * dynamicIMDerivatives_[p.node()];
            }

            // ir vega, we collect the sensi w.r.t. zeta, unit shift per unit time

            if (p.type() == ModelCG::ModelParameter::Type::lgm_zeta && p.date() > valDate) {
                std::size_t ccyIndex = currencyLookup.at(p.qualifier());
                Real tte = model_->actualTimeFromReference(p.date()) - model_->actualTimeFromReference(valDate);
                std::size_t bucket = std::min<std::size_t>(
                    irVegaTerms.size() - 1,
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

            if (p.type() == ModelCG::ModelParameter::Type::fxbs_sigma && p.date() >= valDate) {
                std::size_t ccyIndex = currencyLookup.at(p.qualifier());
                QL_REQUIRE(
                    ccyIndex > 0,
                    "XvaEngineCG::calculateDynamicIM(): internal error, fxbs_sigma qualifier is equal to base ccy");
                Real tte = model_->actualTimeFromReference(p.date()) - model_->actualTimeFromReference(valDate);
                std::size_t bucket = std::min<std::size_t>(
                    fxVegaTerms.size() - 1,
                    std::distance(fxVegaConverter[ccyIndex - 1].optionTimes().begin(),
                                  std::upper_bound(fxVegaConverter[ccyIndex - 1].optionTimes().begin(),
                                                   fxVegaConverter[ccyIndex - 1].optionTimes().end(), tte)));
                pathFxVega[ccyIndex - 1][bucket] += dynamicIMDerivatives_[p.node()];
            }
        }

        // calculate conditional expectations on the aggregated sensis and convert to par if applicable

        std::vector<std::vector<RandomVariable>> conditionalIrDelta(
            model_->currencies().size(),
            std::vector<RandomVariable>(irDeltaTerms.size(), RandomVariable(model_->size())));

        std::vector<RandomVariable> conditionalFxDelta(model_->currencies().size() - 1, RandomVariable(model_->size()));

        std::vector<RandomVariable> tmpIrVega(irVegaTerms.size(), RandomVariable(model_->size()));
        std::vector<std::vector<RandomVariable>> conditionalIrVega(
            model_->currencies().size(),
            std::vector<RandomVariable>(irVegaTerms.size(), RandomVariable(model_->size())));

        std::vector<RandomVariable> tmpFxVega(fxVegaTerms.size(), RandomVariable(model_->size()));
        std::vector<std::vector<RandomVariable>> conditionalFxVega(
            model_->currencies().size() - 1,
            std::vector<RandomVariable>(fxVegaTerms.size(), RandomVariable(model_->size())));

        // we use this node to determine the regressor, which is given as part of the predecessors of this node
        std::size_t n0 = pfExposureNodes_[i];

        for (std::size_t ccy = 0; ccy < model_->currencies().size(); ++ccy) {

            if (g->predecessors(n0).empty())
                continue;

            std::vector<const RandomVariable*> args(1); // first entry is populated below with each regressand
            for (std::size_t p = 1; p < g->predecessors(n0).size(); ++p)
                args.push_back(&values_[g->predecessors(n0)[p]]);

            // ir delta

            for (std::size_t b = 0; b < irDeltaTerms.size(); ++b) {
                args[0] = &pathIrDelta[ccy][b];
                conditionalIrDelta[ccy][b] = ops_[RandomVariableOpCode::ConditionalExpectation](args, n);
            }

            // ir vega (including par conversion)

            for (std::size_t b = 0; b < irVegaTerms.size(); ++b) {
                args[0] = &pathIrVega[ccy][b];
                tmpIrVega[b] = ops_[RandomVariableOpCode::ConditionalExpectation](args, n);
            }

            for (std::size_t b = 0; b < irVegaTerms.size(); ++b) {
                // conditionalIrVega[ccy][b] = tmpIrVega[b]; // no conversion
                for (std::size_t z = 0; z < irVegaTerms.size(); ++z) {
                    conditionalIrVega[ccy][b] +=
                        RandomVariable(model_->size(), irVegaConverter[ccy].dzerodpar()(z, b) * 1E-4) * tmpIrVega[z];
                }

                // multiply with atm vol for further processing in dynamic im model

                conditionalIrVega[ccy][b] *=
                    RandomVariable(model_->size(), 1E4 * irVegaConverter[ccy].baseImpliedVols()[b]);
            }

            if (ccy > 0) {

                // fx delta

                args[0] = &pathFxDelta[ccy - 1];
                conditionalFxDelta[ccy - 1] = ops_[RandomVariableOpCode::ConditionalExpectation](args, n);

                // fx vega (including par conversion)

                for (std::size_t b = 0; b < fxVegaTerms.size(); ++b) {
                    args[0] = &pathFxVega[ccy - 1][b];
                    tmpFxVega[b] = ops_[RandomVariableOpCode::ConditionalExpectation](args, n);
                }

                for (std::size_t b = 0; b < fxVegaTerms.size(); ++b) {
                    // conditionalFxVega[ccy - 1][b] = tmpFxVega[b]; // no conversion
                    for (std::size_t z = 0; z < fxVegaTerms.size(); ++z) {
                        conditionalFxVega[ccy - 1][b] +=
                            RandomVariable(model_->size(), fxVegaConverter[ccy - 1].dzerodpar()(z, b) * 1E-2) *
                            tmpFxVega[z];
                    }

                    // multiply with atm vol for further processing in dynamic im model

                    conditionalFxVega[ccy - 1][b] *=
                        RandomVariable(model_->size(), 1E2 * fxVegaConverter[ccy - 1].baseImpliedVols()[b]);
                }
            }

            // debug conditional ir delta vs. model state on time step 20

            // if (ccy == 0 && i == 20) {
            //     std::cout << "args size " << args.size() << std::endl;
            //     for (std::size_t j = 0; j < model_->size(); ++j) {
            //         std::cout << j << "," << args[2]->at(j) << "," << conditionalIrDelta[ccy].at(j) << std::endl;
            //     }
            // }

            // end debug
        }

        // output for debug: expected ir delta, fx delta, fx vega over all time steps

        // for (std::size_t ccy = 0; ccy < conditionalIrDelta.size(); ++ccy) {
        //     for (std::size_t b = 0; b < irDeltaTerms.size(); ++b) {
        //         std::cout << i << "," << "irDelta," << QuantLib::io::iso_date(valDate) << ","
        //                   << model_->currencies()[ccy] << "," << irDeltaTerms[b] << ","
        //                   << expectation(conditionalIrDelta[ccy][b]).at(0) << std::endl;
        //     }
        // }

        // for (std::size_t ccy = 0; ccy < conditionalFxDelta.size(); ++ccy) {
        //     std::cout << i << "," << "fxDelta," << QuantLib::io::iso_date(valDate) << ","
        //               << model_->currencies()[ccy + 1] << "," << expectation(conditionalFxDelta[ccy]).at(0)
        //               << std::endl;
        // }

        // for (std::size_t ccy = 1; ccy < 2/*conditionalIrVega.size()*/; ++ccy) {
        //     for (std::size_t b = 0; b < irVegaTerms.size(); ++b) {
        //         std::cout << i << "," << "irVega," << QuantLib::io::iso_date(valDate) << ","
        //                   << model_->currencies()[ccy] << "," << irVegaTerms[b] << ","
        //                   << expectation(conditionalIrVega[ccy][b]).at(0) << std::endl;
        //     }
        // }

        // for (std::size_t ccy = 0; ccy < conditionalFxVega.size(); ++ccy) {
        //     for (std::size_t b = 0; b < fxVegaTerms.size(); ++b) {
        //         std::cout << i << "," << "fxVega," << QuantLib::io::iso_date(valDate) << ","
        //                   << model_->currencies()[ccy + 1] << "," << fxVegaTerms[b] << ","
        //                   << expectation(conditionalFxVega[ccy][b]).at(0) << std::endl;
        //     }
        // }

        // end output for debug

        // set results for this valuation date

        for (auto const& n : nettingSetIds) {
            // debug (to prepare im calculator debug output)
            // std::cout << i << ",";
            // end debug
            dynamicIM_[n][i] =
                imCalculator.value(conditionalIrDelta, conditionalIrVega, conditionalFxDelta, conditionalFxVega);
            for (Size j = i + 1; j < std::min(i + dynamicIMStepSize_, valuationDates_.size() + 1); ++j) {
                dynamicIM_[n][j] = dynamicIM_[n][i];
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
    ReportWriter().writeScenarioReport(*sensiReport_, {sensiCube}, 0.0);
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
