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

XvaRunner::XvaRunner(Date asof, const string& baseCurrency, const boost::shared_ptr<Portfolio>& portfolio,
                     const boost::shared_ptr<NettingSetManager>& netting,
                     const boost::shared_ptr<EngineData>& engineData,
                     const boost::shared_ptr<CurveConfigurations>& curveConfigs,
                     const boost::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
                     const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                     const boost::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
                     const boost::shared_ptr<CrossAssetModelData>& crossAssetModelData,
                     std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders,
                     std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders,
                     const boost::shared_ptr<ReferenceDataManager>& referenceData,
                     const IborFallbackConfig& iborFallbackConfig, Real dimQuantile, Size dimHorizonCalendarDays,
                     map<string, bool> analytics, string calculationType, string dvaName, string fvaBorrowingCurve,
                     string fvaLendingCurve, bool fullInitialCollateralisation, bool storeFlows)
    : asof_(asof), baseCurrency_(baseCurrency), portfolio_(portfolio), netting_(netting), engineData_(engineData),
      curveConfigs_(curveConfigs), todaysMarketParams_(todaysMarketParams),
      simMarketData_(simMarketData), scenarioGeneratorData_(scenarioGeneratorData),
      crossAssetModelData_(crossAssetModelData), extraLegBuilders_(extraLegBuilders),
      extraEngineBuilders_(extraEngineBuilders), referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig),
      dimQuantile_(dimQuantile), dimHorizonCalendarDays_(dimHorizonCalendarDays), analytics_(analytics),
      inputCalculationType_(calculationType), dvaName_(dvaName), fvaBorrowingCurve_(fvaBorrowingCurve),
      fvaLendingCurve_(fvaLendingCurve), fullInitialCollateralisation_(fullInitialCollateralisation),
      storeFlows_(storeFlows) {

    if (analytics_.size() == 0) {
        WLOG("post processor analytics not set, using defaults");
        analytics_["dim"] = true;
        analytics_["mva"] = true;
        analytics_["kva"] = false;
        analytics_["cvaSensi"] = true;
    }
}

boost::shared_ptr<ScenarioSimMarketParameters>
XvaRunner::projectSsmData(const std::set<std::string>& currencyFilter) const {
    QL_FAIL("XvaRunner::projectSsmData() is only available in ORE+");
}

boost::shared_ptr<ore::analytics::ScenarioGenerator>
XvaRunner::getProjectedScenarioGenerator(const boost::optional<std::set<std::string>>& currencyFilter,
                                         const boost::shared_ptr<Market>& market,
                                         const boost::shared_ptr<ScenarioSimMarketParameters>& projectedSsmData,
                                         const boost::shared_ptr<ScenarioFactory>& sf, const bool continueOnErr) const {
    QL_REQUIRE(!currencyFilter,
               "XvaRunner::getProjectedScenarioGenerator() with currency filter is only available in ORE+");
    ScenarioGeneratorBuilder sgb(scenarioGeneratorData_);
    return sgb.build(model_, sf, projectedSsmData, asof_, market, Market::defaultConfiguration);
}

void XvaRunner::buildCamModel(const boost::shared_ptr<ore::data::Market>& market, bool continueOnErr) {

    LOG("XvaRunner::buildCamModel() called");

    Settings::instance().evaluationDate() = asof_;
    CrossAssetModelBuilder modelBuilder(market, crossAssetModelData_, Market::defaultConfiguration,
                                        Market::defaultConfiguration, Market::defaultConfiguration,
                                        Market::defaultConfiguration, Market::defaultConfiguration,
                                        Market::defaultConfiguration, ActualActual(ActualActual::ISDA), false, continueOnErr);
    model_ = *modelBuilder.model();
}

void XvaRunner::bufferSimulationPaths() {

    LOG("XvaRunner::bufferSimulationPaths() called");

    auto stateProcess = model_->stateProcess();
    auto pathGen = MultiPathGeneratorFactory().build(scenarioGeneratorData_->sequenceType(), stateProcess,
                                                     scenarioGeneratorData_->getGrid()->timeGrid(),
                                                     scenarioGeneratorData_->seed(), scenarioGeneratorData_->ordering(),
                                                     scenarioGeneratorData_->directionIntegers());

    if (!bufferedPaths_) {
        bufferedPaths_ = boost::make_shared<std::vector<std::vector<Path>>>(
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

void XvaRunner::buildSimMarket(const boost::shared_ptr<ore::data::Market>& market,
    const boost::optional<std::set<std::string>>& currencyFilter, const bool continueOnErr) {
    LOG("XvaRunner::buildSimMarket() called");

    Settings::instance().evaluationDate() = asof_;

    boost::shared_ptr<ScenarioSimMarketParameters> projectedSsmData;
    if (currencyFilter) {
        projectedSsmData = projectSsmData(*currencyFilter);
    }
    else {
        projectedSsmData = simMarketData_;
    }

    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    boost::shared_ptr<ScenarioGenerator> sg =
        getProjectedScenarioGenerator(currencyFilter, market, projectedSsmData, sf, continueOnErr);
    simMarket_ = boost::make_shared<ScenarioSimMarket>(market, projectedSsmData, Market::defaultConfiguration,
                                                       *curveConfigs_, *todaysMarketParams_, true, false, true, false,
                                                       iborFallbackConfig_, false);
    simMarket_->scenarioGenerator() = sg;

    for (auto b : extraEngineBuilders_)
        b->reset();

    DLOG("build scenario data");

    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(
        scenarioGeneratorData_->getGrid()->valuationDates().size(), scenarioGeneratorData_->samples());
    simMarket_->aggregationScenarioData() = scenarioData_;

    auto ed = boost::make_shared<EngineData>(*engineData_);
    ed->globalParameters()["RunType"] = "Exposure";
    simFactory_ = boost::make_shared<EngineFactory>(ed, simMarket_, map<MarketContext, string>(), extraEngineBuilders_,
                                                    extraLegBuilders_, referenceData_, iborFallbackConfig_);
}

void XvaRunner::buildCube(const boost::optional<std::set<std::string>>& tradeIds, const bool continueOnErr) {

    LOG("XvaRunner::buildCube called");

    Settings::instance().evaluationDate() = asof_;    

    boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
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
    for (auto const& t : portfolio_->trades()) {
        try {
            t->build(simFactory_);
        } catch (...) {
            // we don't care, this is just to reset the portfolio, the real build is below
        }
    }

    portfolio->build(simFactory_);

    DLOG("build calculators");

    std::vector<boost::shared_ptr<ValuationCalculator>> calculators;
    boost::shared_ptr<NPVCalculator> npvCalculator = boost::make_shared<NPVCalculator>(baseCurrency_);
    if (scenarioGeneratorData_->withCloseOutLag()) {
        // depth 2: NPV and close-out NPV
        cube_ = getNpvCube(asof_, portfolio->ids(), scenarioGeneratorData_->getGrid()->valuationDates(),
                           scenarioGeneratorData_->samples(), 2);
        cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(scenarioGeneratorData_->getGrid());
        // default date value stored at index 0, close-out value at index 1
        calculators.push_back(boost::make_shared<MPORCalculator>(npvCalculator, 0, 1));
        calculationType_ = "NoLag";
        if (calculationType_ != inputCalculationType_) {
            ALOG("Forcing calculation type " << calculationType_ << " for simulations with close-out grid");
        }
    } else {
        if (storeFlows_) {
            // regular, depth 2: NPV and cash flow
            cube_ = getNpvCube(asof_, portfolio->ids(), scenarioGeneratorData_->getGrid()->dates(),
                               scenarioGeneratorData_->samples(), 2);
            calculators.push_back(
                boost::make_shared<CashflowCalculator>(baseCurrency_, asof_, scenarioGeneratorData_->getGrid(), 1));
        } else
            // regular, depth 1
            cube_ = getNpvCube(asof_, portfolio->ids(), scenarioGeneratorData_->getGrid()->dates(),
                               scenarioGeneratorData_->samples(), 1);

        cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>();
        calculators.push_back(npvCalculator);
        calculationType_ = inputCalculationType_;
    }

    DLOG("get netting cube");

    nettingCube_ = getNettingSetCube(calculators, portfolio);

    DLOG("run valuation engine");

    ValuationEngine engine(asof_, scenarioGeneratorData_->getGrid(), simMarket_);
    engine.buildCube(portfolio, cube_, calculators, scenarioGeneratorData_->withMporStickyDate(), nettingCube_);
}

void XvaRunner::generatePostProcessor(const boost::shared_ptr<Market>& market,
                                      const boost::shared_ptr<NPVCube>& npvCube,
                                      const boost::shared_ptr<NPVCube>& nettingCube,
                                      const boost::shared_ptr<AggregationScenarioData>& scenarioData,
                                      const bool continueOnErr,
                                      const std::map<std::string, QuantLib::Real>& currentIM) {

    LOG("XvaRunner::generatePostProcessor called");

    QL_REQUIRE(analytics_.size() > 0, "analytics map not set");

    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator =
        getDimCalculator(npvCube, cubeInterpreter_, scenarioData_, model_, nettingCube, currentIM);

    postProcess_ = boost::make_shared<PostProcess>(portfolio_, netting_, market, "", npvCube, scenarioData, analytics_,
                                                   baseCurrency_, "None", 1.0, 0.95, calculationType_, dvaName_,
                                                   fvaBorrowingCurve_, fvaLendingCurve_, dimCalculator,
                                                   cubeInterpreter_, fullInitialCollateralisation_);
}

void XvaRunner::runXva(const boost::shared_ptr<Market>& market, bool continueOnErr,
                       const std::map<std::string, QuantLib::Real>& currentIM) {

    LOG("XvaRunner::runXva called");

    buildCamModel(market);
    buildSimMarket(market, boost::none, true);
    buildCube(boost::none, continueOnErr);
    generatePostProcessor(market, npvCube(), nettingCube(), aggregationScenarioData(), continueOnErr, currentIM);
}

boost::shared_ptr<DynamicInitialMarginCalculator> XvaRunner::getDimCalculator(
    const boost::shared_ptr<NPVCube>& cube, const boost::shared_ptr<CubeInterpretation>& cubeInterpreter,
    const boost::shared_ptr<AggregationScenarioData>& scenarioData,
    const boost::shared_ptr<QuantExt::CrossAssetModel>& model, const boost::shared_ptr<NPVCube>& nettingCube,
    const std::map<std::string, QuantLib::Real>& currentIM) {

    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator;
    Size dimRegressionOrder = 0;
    vector<string> dimRegressors;           // FIXME: empty vector means regression vs netting set NPV
    Size dimLocalRegressionEvaluations = 0; // skip local regression
    Real dimLocalRegressionBandwidth = 0.25;

    dimCalculator = boost::make_shared<RegressionDynamicInitialMarginCalculator>(
        portfolio_, cube, cubeInterpreter, scenarioData, dimQuantile_, dimHorizonCalendarDays_, dimRegressionOrder,
        dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth, currentIM);

    return dimCalculator;
}

std::vector<std::string> XvaRunner::getNettingSetIds(const boost::shared_ptr<Portfolio>& portfolio) const {
    // collect netting set ids from portfolio
    std::set<std::string> nettingSetIds;
    for (auto const& t : portfolio == nullptr ? portfolio_->trades() : portfolio->trades())
        nettingSetIds.insert(t->envelope().nettingSetId());
    return std::vector<std::string>(nettingSetIds.begin(), nettingSetIds.end());
}

boost::shared_ptr<NPVCube> XvaRunner::getNpvCube(const Date& asof, const std::vector<std::string>& ids,
                                                 const std::vector<Date>& dates, const Size samples,
                                                 const Size depth) const {
    if (depth == 1) {
        return boost::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
    } else {
        return boost::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples, depth, 0.0f);
    }
}

} // namespace analytics
} // namespace ore
