/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/app/xvarunner.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>

#include <algorithm>

using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

XvaRunner::XvaRunner(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                     Date asof, const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                     const QuantLib::ext::shared_ptr<NettingSetManager>& netting,
                     const QuantLib::ext::shared_ptr<CollateralBalances>& collateralBalances,
                     const QuantLib::ext::shared_ptr<EngineData>& engineData,
                     const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs,
                     const QuantLib::ext::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
                     const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                     const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
                     const QuantLib::ext::shared_ptr<CrossAssetModelData>& crossAssetModelData,
                     const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                     const IborFallbackConfig& iborFallbackConfig, Real dimQuantile, Size dimHorizonCalendarDays,
                     map<string, bool> analytics, string calculationType, string dvaName, string fvaBorrowingCurve,
                     string fvaLendingCurve, bool fullInitialCollateralisation, bool storeFlows)
    : inputs_(inputs), asof_(asof), baseCurrency_(baseCurrency), portfolio_(portfolio), netting_(netting),
      collateralBalances_(collateralBalances), engineData_(engineData),
      curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams), simMarketData_(simMarketData),
      scenarioGeneratorData_(scenarioGeneratorData), crossAssetModelData_(crossAssetModelData),
      referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig), dimQuantile_(dimQuantile),
      dimHorizonCalendarDays_(dimHorizonCalendarDays), analytics_(analytics), inputCalculationType_(calculationType),
      dvaName_(dvaName), fvaBorrowingCurve_(fvaBorrowingCurve), fvaLendingCurve_(fvaLendingCurve),
      fullInitialCollateralisation_(fullInitialCollateralisation), storeFlows_(storeFlows) {

    if (analytics_.size() == 0) {
        WLOG("post processor analytics not set, using defaults");
        analytics_["dim"] = true;
        analytics_["mva"] = true;
        analytics_["kva"] = false;
        analytics_["cvaSensi"] = true;
    }
}

QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>
XvaRunner::projectSsmData(const std::set<std::string>& currencyFilter) const {
    QL_FAIL("XvaRunner::projectSsmData() is only available in ORE+");
}

QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>
XvaRunner::getProjectedScenarioGenerator(const boost::optional<std::set<std::string>>& currencyFilter,
                                         const QuantLib::ext::shared_ptr<Market>& market,
                                         const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& projectedSsmData,
                                         const QuantLib::ext::shared_ptr<ScenarioFactory>& sf, const bool continueOnErr) const {
    QL_REQUIRE(!currencyFilter,
               "XvaRunner::getProjectedScenarioGenerator() with currency filter is only available in ORE+");
    ScenarioGeneratorBuilder sgb(scenarioGeneratorData_);
    return sgb.build(model_, sf, projectedSsmData, asof_, market, Market::defaultConfiguration);
}

void XvaRunner::buildCamModel(const QuantLib::ext::shared_ptr<ore::data::Market>& market, bool continueOnErr) {

    LOG("XvaRunner::buildCamModel() called");

    Settings::instance().evaluationDate() = asof_;
    CrossAssetModelBuilder modelBuilder(
        market, crossAssetModelData_, Market::defaultConfiguration, Market::defaultConfiguration,
        Market::defaultConfiguration, Market::defaultConfiguration, Market::defaultConfiguration,
        Market::defaultConfiguration, false, continueOnErr, "", SalvagingAlgorithm::None, "xva cam building");
    model_ = *modelBuilder.model();
}

void XvaRunner::bufferSimulationPaths() {

    LOG("XvaRunner::bufferSimulationPaths() called");

    auto stateProcess = model_->stateProcess();
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(stateProcess)) {
        tmp->resetCache(scenarioGeneratorData_->getGrid()->timeGrid().size() - 1);
    }
    auto pathGen = MultiPathGeneratorFactory().build(scenarioGeneratorData_->sequenceType(), stateProcess,
                                                     scenarioGeneratorData_->getGrid()->timeGrid(),
                                                     scenarioGeneratorData_->seed(), scenarioGeneratorData_->ordering(),
                                                     scenarioGeneratorData_->directionIntegers());

    if (!bufferedPaths_) {
        bufferedPaths_ = QuantLib::ext::make_shared<std::vector<std::vector<Path>>>(
            scenarioGeneratorData_->samples(), std::vector<Path>(stateProcess->size(), TimeGrid()));
    }

    for (Size p = 0; p < scenarioGeneratorData_->samples(); ++p) {
        const MultiPath& path = pathGen->next().value;
        for (Size d = 0; d < stateProcess->size(); ++d) {
            (*bufferedPaths_)[p][d] = path[d];
        }
    }

    LOG("XvaRunner::bufferSimulationPaths() finished");
}

void XvaRunner::buildSimMarket(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
    const boost::optional<std::set<std::string>>& currencyFilter, const bool continueOnErr) {
    LOG("XvaRunner::buildSimMarket() called");

    Settings::instance().evaluationDate() = asof_;

    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> projectedSsmData;
    if (currencyFilter) {
        projectedSsmData = projectSsmData(*currencyFilter);
    }
    else {
        projectedSsmData = simMarketData_;
    }

    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> sg =
        getProjectedScenarioGenerator(currencyFilter, market, projectedSsmData, sf, continueOnErr);
    simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(market, projectedSsmData, Market::defaultConfiguration,
                                                       *curveConfigs_, *todaysMarketParams_, true, false, true, false,
                                                       iborFallbackConfig_, false);
    simMarket_->scenarioGenerator() = sg;

    DLOG("build scenario data");

    scenarioData_.linkTo(QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(
        scenarioGeneratorData_->getGrid()->valuationDates().size(), scenarioGeneratorData_->samples()));
    simMarket_->aggregationScenarioData() = *scenarioData_;

    auto ed = QuantLib::ext::make_shared<EngineData>(*engineData_);
    ed->globalParameters()["RunType"] = "Exposure";
    simFactory_ = QuantLib::ext::make_shared<EngineFactory>(ed, simMarket_, map<MarketContext, string>(), referenceData_,
                                                    iborFallbackConfig_);
}

void XvaRunner::buildCube(const boost::optional<std::set<std::string>>& tradeIds, const bool continueOnErr) {

    LOG("XvaRunner::buildCube called");

    Settings::instance().evaluationDate() = asof_;    

    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    if (tradeIds) {
        for (auto const& t : *tradeIds) {
            QL_REQUIRE(portfolio_->has(t), "XvaRunner::buildCube(): portfolio does not contain trade with id '"
                                               << t << "' specified in the filter");
            portfolio->add(portfolio_->get(t));
        }
    } else {
        portfolio = portfolio_;
    }

    DLOG("build portfolio");

    // FIXME why do we need this? portfolio_->reset() is not sufficient to ensure XVA simulation run fast (and this is
    // called before)
    for (auto const& [tid, t] : portfolio_->trades()) {
        try {
            t->build(simFactory_);
        } catch (...) {
            // we don't care, this is just to reset the portfolio, the real build is below
        }
    }

    portfolio->build(simFactory_);

    DLOG("build calculators");

    std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    QuantLib::ext::shared_ptr<NPVCalculator> npvCalculator = QuantLib::ext::make_shared<NPVCalculator>(baseCurrency_);
    cubeInterpreter_ = QuantLib::ext::make_shared<CubeInterpretation>(storeFlows_, scenarioGeneratorData_->withCloseOutLag(),
                                                              scenarioData_, scenarioGeneratorData_->getGrid());
    if (scenarioGeneratorData_->withCloseOutLag()) {
        // depth 2: NPV and close-out NPV
        cube_ = getNpvCube(asof_, portfolio->ids(), scenarioGeneratorData_->getGrid()->valuationDates(),
                           scenarioGeneratorData_->samples(), 2);
        calculators.push_back(QuantLib::ext::make_shared<MPORCalculator>(npvCalculator, cubeInterpreter_->defaultDateNpvIndex(),
                                                                 cubeInterpreter_->closeOutDateNpvIndex()));
        calculationType_ = "NoLag";
        if (calculationType_ != inputCalculationType_) {
            ALOG("Forcing calculation type " << calculationType_ << " for simulations with close-out grid");
        }
    } else {
        if (storeFlows_) {
            // regular, depth 2: NPV and cash flow
            cube_ = getNpvCube(asof_, portfolio->ids(), scenarioGeneratorData_->getGrid()->dates(),
                               scenarioGeneratorData_->samples(), 2);
            calculators.push_back(QuantLib::ext::make_shared<CashflowCalculator>(
                baseCurrency_, asof_, scenarioGeneratorData_->getGrid(), cubeInterpreter_->mporFlowsIndex()));
        } else
            // regular, depth 1
            cube_ = getNpvCube(asof_, portfolio->ids(), scenarioGeneratorData_->getGrid()->dates(),
                               scenarioGeneratorData_->samples(), 1);
        calculators.push_back(npvCalculator);
        calculationType_ = inputCalculationType_;
    }

    DLOG("get netting cube");

    nettingCube_ = getNettingSetCube(calculators, portfolio);

    DLOG("run valuation engine");

    ValuationEngine engine(asof_, scenarioGeneratorData_->getGrid(), simMarket_);
    engine.buildCube(portfolio, cube_, calculators, scenarioGeneratorData_->withMporStickyDate(), nettingCube_);
}

void XvaRunner::generatePostProcessor(const QuantLib::ext::shared_ptr<Market>& market,
                                      const QuantLib::ext::shared_ptr<NPVCube>& npvCube,
                                      const QuantLib::ext::shared_ptr<NPVCube>& nettingCube,
                                      const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
                                      const bool continueOnErr,
                                      const std::map<std::string, QuantLib::Real>& currentIM) {

    LOG("XvaRunner::generatePostProcessor called");

    QL_REQUIRE(analytics_.size() > 0, "analytics map not set");

    QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator =
        getDimCalculator(npvCube, cubeInterpreter_, *scenarioData_, model_, nettingCube, currentIM);

    postProcess_ = QuantLib::ext::make_shared<PostProcess>(portfolio_, netting_, collateralBalances_,
                                                   market, "", npvCube, scenarioData, analytics_,
                                                   baseCurrency_, "None", 1.0, 0.95, calculationType_, dvaName_,
                                                   fvaBorrowingCurve_, fvaLendingCurve_, dimCalculator,
                                                   cubeInterpreter_, fullInitialCollateralisation_);
}

void XvaRunner::runXva(const QuantLib::ext::shared_ptr<Market>& market, bool continueOnErr,
                       const std::map<std::string, QuantLib::Real>& currentIM) {

    LOG("XvaRunner::runXva called");

    buildCamModel(market);
    buildSimMarket(market, boost::none, true);
    buildCube(boost::none, continueOnErr);
    generatePostProcessor(market, npvCube(), nettingCube(), *aggregationScenarioData(), continueOnErr, currentIM);
}

QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> XvaRunner::getDimCalculator(
    const QuantLib::ext::shared_ptr<NPVCube>& cube, const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpreter,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& model, const QuantLib::ext::shared_ptr<NPVCube>& nettingCube,
    const std::map<std::string, QuantLib::Real>& currentIM) {

    QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator;
    Size dimRegressionOrder = 0;
    vector<string> dimRegressors;           // FIXME: empty vector means regression vs netting set NPV
    Size dimLocalRegressionEvaluations = 0; // skip local regression
    Real dimLocalRegressionBandwidth = 0.25;

    dimCalculator = QuantLib::ext::make_shared<RegressionDynamicInitialMarginCalculator>(
        inputs_, portfolio_, cube, cubeInterpreter, scenarioData, dimQuantile_, dimHorizonCalendarDays_, dimRegressionOrder,
        dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth, currentIM);

    return dimCalculator;
}

std::set<std::string> XvaRunner::getNettingSetIds(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) const {
    // collect netting set ids from portfolio
    std::set<std::string> nettingSetIds;
    for (auto const& [tradeId,trade] : portfolio == nullptr ? portfolio_->trades() : portfolio->trades())
        nettingSetIds.insert(trade->envelope().nettingSetId());
    return nettingSetIds;
}

QuantLib::ext::shared_ptr<NPVCube> XvaRunner::getNpvCube(const Date& asof, const std::set<std::string>& ids,
                                                 const std::vector<Date>& dates, const Size samples,
                                                 const Size depth) const {
    if (depth == 1) {
        return QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
    } else {
        return QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples, depth, 0.0f);
    }
}

} // namespace analytics
} // namespace ore
