/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/marketdata/todaysmarket.hpp>
#include <orea/cube/cubewriter.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/engine/marketriskreport.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

using namespace QuantLib;
using namespace ore::data;

namespace ore {
namespace analytics {
using std::map;

std::ostream& operator<<(std::ostream& out, const ext::shared_ptr<MarketRiskGroup>& riskGroup) {
    return out << riskGroup->to_string();
}

std::ostream& operator<<(std::ostream& out, const ext::shared_ptr<TradeGroup>& tradeGroup) {
    return out << tradeGroup->to_string();
}

void MarketRiskReport::init() {

    if (multiThreadArgs_ && fullRevalArgs_ && !fullRevalArgs_->simMarket_)
        initSimMarket();

    // set run type in engine data, make a copy of this before
    if (fullRevalArgs_ && fullRevalArgs_->engineData_) {
        fullRevalArgs_->engineData_ = QuantLib::ext::make_shared<EngineData>(*fullRevalArgs_->engineData_);
        fullRevalArgs_->engineData_->globalParameters()["RunType"] = std::string("HistoricalPnL");
    }

    // save a sensi pnl calculator
    if (hisScenGen_) {
        // Build the filtered historical scenario generator
        hisScenGen_ = QuantLib::ext::make_shared<HistoricalScenarioGeneratorWithFilteredDates>(
            timePeriods(), hisScenGen_);

        if (fullRevalArgs_ && fullRevalArgs_->simMarket_)
            hisScenGen_->baseScenario() = fullRevalArgs_->simMarket_->baseScenario();
    }

    if (sensiArgs_ && hisScenGen_)
        sensiPnlCalculator_ =
            ext::make_shared<HistoricalSensiPnlCalculator>(hisScenGen_, sensiArgs_->sensitivityStream_);

    if (fullRevalArgs_) {
        LOG("Build the portfolio for full reval bt.");

        if (!multiThreadArgs_) {
            factory_ = QuantLib::ext::make_shared<EngineFactory>(fullRevalArgs_->engineData_, fullRevalArgs_->simMarket_,
                                                         map<MarketContext, string>(), fullRevalArgs_->referenceData_,
                                                         fullRevalArgs_->iborFallbackConfig_);

            DLOG("Building the portfolio");
            fullRevalArgs_->portfolio_->build(factory_, "historical pnl generation");
            DLOG("Portfolio built");

            LOG("Creating the historical P&L generator (dryRun=" << std::boolalpha << fullRevalArgs_->dryRun_ << ")");
            ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
                fullRevalArgs_->simMarket_->asofDate(), fullRevalArgs_->portfolio_->ids(),
                vector<Date>(1, fullRevalArgs_->simMarket_->asofDate()), hisScenGen_->numScenarios());

            histPnlGen_ = QuantLib::ext::make_shared<HistoricalPnlGenerator>(
                baseCurrency_, fullRevalArgs_->portfolio_, fullRevalArgs_->simMarket_, 
                hisScenGen_, cube, factory_->modelBuilders(), fullRevalArgs_->dryRun_);
        } else {
            histPnlGen_ = QuantLib::ext::make_shared<HistoricalPnlGenerator>(
                baseCurrency_, fullRevalArgs_->portfolio_, hisScenGen_,
                fullRevalArgs_->engineData_,
                multiThreadArgs_->nThreads_, multiThreadArgs_->today_, multiThreadArgs_->loader_,
                multiThreadArgs_->curveConfigs_, multiThreadArgs_->todaysMarketParams_,
                multiThreadArgs_->configuration_, multiThreadArgs_->simMarketData_, fullRevalArgs_->referenceData_,
                fullRevalArgs_->iborFallbackConfig_, fullRevalArgs_->dryRun_, multiThreadArgs_->context_);
        }
    }
}

void MarketRiskReport::initSimMarket() {
    QL_REQUIRE(multiThreadArgs_ && fullRevalArgs_, "MarketRiskBacktest: must be a multithreaded run");

    // called from the ctors that do not take a sim market as an input (the multithreaded ctors)
    auto initMarket = QuantLib::ext::make_shared<TodaysMarket>(
        multiThreadArgs_->today_, multiThreadArgs_->todaysMarketParams_, multiThreadArgs_->loader_,
        multiThreadArgs_->curveConfigs_, true, true, false, fullRevalArgs_->referenceData_, false,
        fullRevalArgs_->iborFallbackConfig_);

    fullRevalArgs_->simMarket_ = QuantLib::ext::make_shared<ore::analytics::ScenarioSimMarket>(
        initMarket, multiThreadArgs_->simMarketData_, multiThreadArgs_->configuration_,
        *multiThreadArgs_->curveConfigs_, *multiThreadArgs_->todaysMarketParams_, true, false, false, false,
        fullRevalArgs_->iborFallbackConfig_);
}

void MarketRiskReport::registerProgressIndicators() {
    if (histPnlGen_) {
        histPnlGen_->unregisterAllProgressIndicators();
        for (auto const& i : this->progressIndicators())
            histPnlGen_->registerProgressIndicator(i);
    }
}

void MarketRiskReport::calculate(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    registerProgressIndicators();
    
    LOG("Creating reports");
    createReports(reports);

    // Cube to store Sensi Shifts and vector of keys used in cube, per portfolio
    map<string, QuantLib::ext::shared_ptr<NPVCube>> sensiShiftCube;
    ext::shared_ptr<SensitivityAggregator> sensiAgg;
    if (sensiBased_)
        // Create a sensitivity aggregator. Will be used if running sensi-based backtest.
        sensiAgg = ext::make_shared<SensitivityAggregator>(tradeIdGroups_);
    
    bool runDetailTrd = runTradeDetail(reports);
    addPnlCalculators(reports);

    // Loop over all the risk groups
    riskGroups_->reset();
    Size currentRiskGroup = 0;
    while (ext::shared_ptr<MarketRiskGroup> riskGroup = riskGroups_->next()) {
        LOG("[progress] Processing RiskGroup " << ++currentRiskGroup << " out of " << riskGroups_->size()
                                                  << ") = " << riskGroup);

        ext::shared_ptr<ScenarioFilter> filter = createScenarioFilter(riskGroup);

        // If this filter disables all risk factors, move to next risk group
        if (disablesAll(filter))
            continue;

        updateFilter(riskGroup, filter);

        if (sensiBased_)
            sensiAgg->aggregate(*sensiArgs_->sensitivityStream_, filter);

        // If doing a full revaluation backtest, generate the cube under this filter
        if (fullReval_) {
            if (generateCube(riskGroup)) {
                histPnlGen_->generateCube(filter);
                if (fullRevalArgs_->writeCube_) {
                    CubeWriter writer(cubeFilePath(riskGroup));
                    writer.write(histPnlGen_->cube(), {});
                }
            }
        }
        
        bool runSensiBased = sensiBased_;

        // loop over all the trade groups
        tradeGroups_->reset();
        while (ext::shared_ptr<TradeGroup> tradeGroup = tradeGroups_->next()) {
            reset(riskGroup);

            // Only look at this trade group if there required
            if (!runTradeRiskGroup(tradeGroup, riskGroup))
                continue;

            MEM_LOG;
            LOG("Start processing for RiskGroup: " << riskGroup << ", TradeGroup: " << tradeGroup); 
            
            writePnl_ = tradeGroup->allLevel() && riskGroup->allLevel();
            tradeIdIdxPairs_ = tradeIdGroups_.at(tradeGroupKey(tradeGroup));

            // populate the tradeIds
            transform(tradeIdIdxPairs_.begin(), tradeIdIdxPairs_.end(), back_inserter(tradeIds_),
                      [](const pair<string, Size>& elem) { return elem.first; });

            set<SensitivityRecord> srs;

            if (runSensiBased && sensiAgg) {
                map<string, QuantLib::ext::shared_ptr<NPVCube>>::iterator scube;

                auto tradeGpId = tradeGroupKey(tradeGroup);
                srs = sensiAgg->sensitivities(tradeGpId);

                // Populate the deltas and gammas for a parametric VAR benchmark calculation
                sensiAgg->generateDeltaGamma(tradeGpId, deltas_, gammas_);
                vector<RiskFactorKey> deltaKeys;
                for (const auto& [rfk, _] : deltas_)
                    deltaKeys.push_back(rfk);

                string portfolio = portfolioId(tradeGroup);

                // riskGroups_ and tradeGroups_ are ordered so we should always run
                // [Product Class, Risk Class, Margin Type] = [All, All, All] first.
                // This populates every possible scenario shift to a cube for quicker
                // generation of Sensi PnLs.
                scube = sensiShiftCube.find(portfolio);
                if (sensiArgs_->shiftCalculator_) {
                    if (scube == sensiShiftCube.end()) {
                        DLOG("Populating Sensi Shifts for portfolio '" << portfolio << "'");

                        // if we have no senis for this run we skip this, and set runSensiBased to false
                        if (srs.size() == 0) {
                            ALOG("No senitivities found for RiskGroup = "
                                 << riskGroup << " and tradeGroup " << tradeGroup << "; Skipping Sensi based PnL.");
                            runSensiBased = false;
                        } else {
                            scube = sensiShiftCube.insert({portfolio, QuantLib::ext::shared_ptr<NPVCube>()}).first;
                            vector<RiskFactorKey> riskFactorKeys;
                            transform(deltas_.begin(), deltas_.end(), back_inserter(riskFactorKeys),
                                      [](const pair<RiskFactorKey, Real>& kv) { return kv.first; });
                            sensiPnlCalculator_->populateSensiShifts(scube->second, riskFactorKeys,
                                                                     sensiArgs_->shiftCalculator_);
                        }
                    }
                }

                if (runSensiBased) {
                    ext::shared_ptr<CovarianceCalculator> covCalculator;
                    // if a covariance matrix has been provided as an input we use that
                    if (sensiArgs_->covarianceInput_.size() > 0) {
                        std::vector<bool> sensiKeyHasNonZeroVariance(deltaKeys.size(), false);

                        // build global covariance matrix
                        covarianceMatrix_ = Matrix(deltaKeys.size(), deltaKeys.size(), 0.0);
                        Size unusedCovariance = 0;
                        for (const auto& c : sensiArgs_->covarianceInput_) {
                            auto k1 = std::find(deltaKeys.begin(), deltaKeys.end(), c.first.first);
                            auto k2 = std::find(deltaKeys.begin(), deltaKeys.end(), c.first.second);
                            if (k1 != deltaKeys.end() && k2 != deltaKeys.end()) {
                                covarianceMatrix_(k1 - deltaKeys.begin(), k2 - deltaKeys.begin()) = c.second;
                                if (k1 == k2)
                                    sensiKeyHasNonZeroVariance[k1 - deltaKeys.begin()] = true;
                            } else
                                ++unusedCovariance;
                        }
                        DLOG("Found " << sensiArgs_->covarianceInput_.size() << " covariance matrix entries, "
                                        << unusedCovariance
                                        << " do not match a portfolio sensitivity and will not be used.");
                        for (Size i = 0; i < sensiKeyHasNonZeroVariance.size(); ++i) {
                            if (!sensiKeyHasNonZeroVariance[i])
                                WLOG("Zero variance assigned to sensitivity key " << deltaKeys[i]);
                        }

                        // make covariance matrix positive semi-definite
                        DLOG("Covariance matrix has dimension " << deltaKeys.size() << " x " << deltaKeys.size());
                        if (salvage_ && !covarianceMatrix_.empty()) {
                            DLOG("Covariance matrix is not salvaged, check for positive semi-definiteness");
                            SymmetricSchurDecomposition ssd(covarianceMatrix_);
                            Real evMin = ssd.eigenvalues().back();
                            QL_REQUIRE(
                                evMin > 0.0 || close_enough(evMin, 0.0),
                                "ParametricVar: input covariance matrix is not positive semi-definite, smallest "
                                "eigenvalue is "
                                    << evMin);
                            DLOG("Smallest eigenvalue is " << evMin);
                            salvage_ = QuantLib::ext::make_shared<QuantExt::NoCovarianceSalvage>();
                        }
                    } else
                        covCalculator = ext::make_shared<CovarianceCalculator>(covariancePeriod());

                    includeDeltaMargin_ = includeDeltaMargin(riskGroup);
                    includeGammaMargin_ = includeGammaMargin(riskGroup);
                    //  bool haveDetailTrd = btArgs_->reports_.count(ReportType::DetailTrade) == 1;

                    if (covCalculator || pnlCalculators_.size() > 0) {
                        sensiPnlCalculator_->calculateSensiPnl(srs, deltaKeys, scube->second, pnlCalculators_,
                                                                covCalculator, tradeIds_, includeGammaMargin_,
                                                                includeDeltaMargin_, runDetailTrd);

                        covarianceMatrix_ = covCalculator->covariance();
                    }
                    handleSensiResults(reports, riskGroup, tradeGroup);
                }
            }
            // Do the full revaluation step
            if (runFullReval(riskGroup))                                
                handleFullRevalResults(reports, riskGroup, tradeGroup);

            writeSummary(reports, riskGroup, tradeGroup);
        }
        if (sensiBased_)
            // Reset the sensitivity aggregator before changing the risk filter
            sensiAgg->reset();
    }
    closeReports(reports);
}

void MarketRiskReport::enableCubeWrite(const string& cubeDir, const string& cubeFilename) {
    QL_REQUIRE(boost::find_first(cubeFilename, "FILTER"),
               "cube file name '" << cubeFilename << "' must contain 'FILTER'");
    fullRevalArgs_->writeCube_ = true;
    fullRevalArgs_->cubeDir_ = cubeDir;
    fullRevalArgs_->cubeFilename_ = cubeFilename;
}

void MarketRiskReport::reset(const ext::shared_ptr<MarketRiskGroup>& riskGroup) {
    deltas_.clear();
    gammas_.clear();
    covarianceMatrix_ = Matrix();
    tradeIdIdxPairs_.clear();
    tradeIds_.clear();
    for (const auto& p : pnlCalculators_)
        p->clear();
}

void MarketRiskReport::closeReports(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    for (const auto& r : reports->reports())
        r->end();    
}
} // namespace analytics
} // namespace ore
