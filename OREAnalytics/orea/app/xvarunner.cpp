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
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>

using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

XvaRunner::XvaRunner(Date asof, const string& baseCurrency, const boost::shared_ptr<Portfolio>& portfolio,
                     const boost::shared_ptr<NettingSetManager>& netting,
                     const boost::shared_ptr<EngineData>& engineData,
                     const boost::shared_ptr<CurveConfigurations>& curveConfigs,
                     const boost::shared_ptr<Conventions>& conventions,
                     const boost::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
                     const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                     const boost::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
                     const boost::shared_ptr<CrossAssetModelData>& crossAssetModelData,
                     std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders,
                     std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders,
                     const boost::shared_ptr<ReferenceDataManager>& referenceData, Real dimQuantile,
                     Size dimHorizonCalendarDays, map<string, bool> analytics, string calculationType, string dvaName,
                     string fvaBorrowingCurve, string fvaLendingCurve, bool fullInitialCollateralisation,
                     bool storeFlows)
    : asof_(asof), baseCurrency_(baseCurrency), portfolio_(portfolio), netting_(netting), engineData_(engineData),
      curveConfigs_(curveConfigs), conventions_(conventions), todaysMarketParams_(todaysMarketParams),
      simMarketData_(simMarketData), scenarioGeneratorData_(scenarioGeneratorData),
      crossAssetModelData_(crossAssetModelData), extraLegBuilders_(extraLegBuilders),
      extraEngineBuilders_(extraEngineBuilders), referenceData_(referenceData), dimQuantile_(dimQuantile),
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

    // collect netting set ids from portfolio
    std::set<std::string> nettingSetIds;
    for (auto const& t : portfolio_->trades())
        nettingSetIds.insert(t->envelope().nettingSetId());
    nettingSetIds_ = std::vector<std::string>(nettingSetIds.begin(), nettingSetIds.end());
}

void XvaRunner::prepareSimulation(const boost::shared_ptr<Market>& market, const bool continueOnErr) {
    LOG("XvaRunner::buildCamModel called");
    // ensure date is reset
    Settings::instance().evaluationDate() = asof_;

    CrossAssetModelBuilder modelBuilder(market, crossAssetModelData_, Market::defaultConfiguration,
                                        Market::defaultConfiguration, Market::defaultConfiguration,
                                        Market::defaultConfiguration, Market::defaultConfiguration, ActualActual(),
                                        false, continueOnErr);
    model_ = *modelBuilder.model();

    ScenarioGeneratorBuilder sgb(scenarioGeneratorData_);
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    boost::shared_ptr<ScenarioGenerator> sg = sgb.build(model_, sf, simMarketData_, asof_, market, "");

    simMarket_ = boost::make_shared<ScenarioSimMarket>(market, simMarketData_, *conventions_, "", *curveConfigs_,
                                                       *todaysMarketParams_, true);
    simMarket_->scenarioGenerator() = sg;

    for (auto b : extraEngineBuilders_)
        b->reset();

    // TODO: Is this needed? PMMarket is in ORE Plus
    // Rebuild portfolio linked to SSM (wrapped)
    // auto simPmMarket = boost::make_shared<PreciousMetalMarket>(simMarket);
    simFactory_ = boost::make_shared<EngineFactory>(engineData_, simMarket_, map<MarketContext, string>(),
                                                    extraEngineBuilders_, extraLegBuilders_, referenceData_);
}

void XvaRunner::buildCube(const boost::optional<std::set<std::string>>& tradeIds, const bool continueOnErr) {

    LOG("XvaRunner::buildCube called");

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

    portfolio->reset();
    portfolio->build(simFactory_);

    DLOG("build calculators");

    std::vector<boost::shared_ptr<ValuationCalculator>> calculators;
    boost::shared_ptr<NPVCalculator> npvCalculator = boost::make_shared<NPVCalculator>(baseCurrency_);
    if (scenarioGeneratorData_->withCloseOutLag()) {
        // depth 2: NPV and close-out NPV
        cube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>(asof_, portfolio_->ids(),
                                                                 scenarioGeneratorData_->grid()->valuationDates(),
                                                                 scenarioGeneratorData_->samples(),
                                                                 2); // depth 2: default date and close-out date NPV
        cubeInterpreter_ = boost::make_shared<MporGridCubeInterpretation>(scenarioGeneratorData_->grid());
        // default date value stored at index 0, close-out value at index 1
        calculators.push_back(boost::make_shared<MPORCalculator>(npvCalculator, 0, 1));
        calculationType_ = "NoLag";
        if (calculationType_ != inputCalculationType_) {
            ALOG("Forcing calculation type " << calculationType_ << " for simulations with close-out grid");
        }
    } else {
        if (storeFlows_) {
            // regular, depth 2: NPV and cash flow
            cube_ = boost::make_shared<SinglePrecisionInMemoryCubeN>(asof_, portfolio_->ids(),
                                                                     scenarioGeneratorData_->grid()->dates(),
                                                                     scenarioGeneratorData_->samples(), 2, 0.0f);
            calculators.push_back(
                boost::make_shared<CashflowCalculator>(baseCurrency_, asof_, scenarioGeneratorData_->grid(), 1));
        } else
            // regular, depth 1
            cube_ = boost::make_shared<SinglePrecisionInMemoryCube>(asof_, portfolio_->ids(),
                                                                    scenarioGeneratorData_->grid()->dates(),
                                                                    scenarioGeneratorData_->samples(), 0.0f);

        cubeInterpreter_ = boost::make_shared<RegularCubeInterpretation>();
        calculators.push_back(npvCalculator);
        calculationType_ = inputCalculationType_;
    }

    DLOG("get netting cube");

    nettingCube_ = getNettingSetCube(calculators);

    DLOG("build scenario data");

    scenarioData_ = boost::make_shared<InMemoryAggregationScenarioData>(
        scenarioGeneratorData_->grid()->valuationDates().size(), scenarioGeneratorData_->samples());

    simMarket_->aggregationScenarioData() = scenarioData_; // ??? simMarket gets agg data?

    DLOG("run valuation engine");

    ValuationEngine engine(asof_, scenarioGeneratorData_->grid(), simMarket_);
    engine.buildCube(portfolio, cube_, calculators, scenarioGeneratorData_->withMporStickyDate(), nettingCube_);
}

void XvaRunner::generatePostProcessor(const boost::shared_ptr<Market>& market,
                                      const boost::shared_ptr<NPVCube>& npvCube,
                                      const boost::shared_ptr<NPVCube>& nettingCube) {
    LOG("XvaRunner::generatePostProcessor called");
    QL_REQUIRE(analytics_.size() > 0, "analytics map not set");
    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator =
        getDimCalculator(npvCube, cubeInterpreter_, scenarioData_, model_, nettingCube);
    postProcess_ = boost::make_shared<PostProcess>(portfolio_, netting_, market, "", npvCube, scenarioData_, analytics_,
                                                   baseCurrency_, "None", 1.0, 0.95, calculationType_, dvaName_,
                                                   fvaBorrowingCurve_, fvaLendingCurve_, dimCalculator,
                                                   cubeInterpreter_, fullInitialCollateralisation_);
}

void XvaRunner::runXva(const boost::shared_ptr<Market>& market, bool continueOnErr) {
    LOG("XvaRunner::runXva called");
    prepareSimulation(market, continueOnErr);
    buildCube(boost::none, continueOnErr);
    generatePostProcessor(market, npvCube(), nettingCube());
}

boost::shared_ptr<DynamicInitialMarginCalculator> XvaRunner::getDimCalculator(
    const boost::shared_ptr<NPVCube>& cube, const boost::shared_ptr<CubeInterpretation>& cubeInterpreter,
    const boost::shared_ptr<AggregationScenarioData>& scenarioData,
    const boost::shared_ptr<QuantExt::CrossAssetModel>& model, const boost::shared_ptr<NPVCube>& nettingCube) {

    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator;
    Size dimRegressionOrder = 0;
    vector<string> dimRegressors;           // FIXME: empty vector means regression vs netting set NPV
    Size dimLocalRegressionEvaluations = 0; // skip local regression
    Real dimLocalRegressionBandwidth = 0.25;

    dimCalculator = boost::make_shared<RegressionDynamicInitialMarginCalculator>(
        portfolio_, cube, cubeInterpreter, scenarioData, dimQuantile_, dimHorizonCalendarDays_, dimRegressionOrder,
        dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth);

    return dimCalculator;
}

} // namespace analytics
} // namespace ore
