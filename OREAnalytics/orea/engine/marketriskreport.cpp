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

void MarketRiskReport::calculate(ext::shared_ptr<MarketRiskReport::Reports>& reports) {

    // Cube to store Sensi Shifts and vector of keys used in cube, per portfolio
    map<string, boost::shared_ptr<NPVCube>> sensiShiftCube;
    ext::shared_ptr<SensitivityAggregator> sensiAgg;
    if (sensiBased_) {
        // Create a sensitivity aggregator. Will be used if running sensi-based backtest.
        sensiAgg = ext::make_shared<SensitivityAggregator>(tradeIdGroups_);        
    }

    while (ext::shared_ptr<MarketRiskGroup> riskGroup = riskGroups_->next()) {
        ext::shared_ptr<ScenarioFilter> filter = createScenarioFilter(riskGroup);

        // If this filter disables all risk factors, move to next risk group
        if (disablesAll(filter))
            continue;

        updateFilter(riskGroup, filter);

        if (sensiBased_)
            sensiAgg->aggregate(*sensiArgs_->sensitivityStream_, filter);
        
        bool runSensiBased = sensiBased_;

        tradeGroups_->reset();
        while (ext::shared_ptr<TradeGroup> tradeGroup = tradeGroups_->next()) {
            tradeIdIdxPairs_ = tradeIdGroups_.at(tradeGroupKey(tradeGroup)); 
        
            reset(riskGroup);

            // Set of trade IDs in this trade group.
            vector<string> tradeIds;
            transform(tradeIdIdxPairs_.begin(), tradeIdIdxPairs_.end(), back_inserter(tradeIds),
                      [](const pair<string, Size>& elem) { return elem.first; });

            set<SensitivityRecord> srs;

            if (runSensiBased && sensiAgg) {
                map<string, boost::shared_ptr<NPVCube>>::iterator scube;

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
                            scube = sensiShiftCube.insert({portfolio, boost::shared_ptr<NPVCube>()}).first;
                            vector<RiskFactorKey> riskFactorKeys;
                            transform(deltas_.begin(), deltas_.end(), back_inserter(riskFactorKeys),
                                      [](const pair<RiskFactorKey, Real>& kv) { return kv.first; });
                            sensiPnlCalculator_->populateSensiShifts(scube->second, riskFactorKeys,
                                                                     sensiArgs_->shiftCalculator_);
                        }
                    }
                }

                if (runSensiBased) {
                    if (deltaKeys.size() > 0) {
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
                            if (salvage_) {
                                DLOG("Covariance matrix is not salvaged, check for positive semi-definiteness");
                                SymmetricSchurDecomposition ssd(covarianceMatrix_);
                                Real evMin = ssd.eigenvalues().back();
                                QL_REQUIRE(
                                    evMin > 0.0 || close_enough(evMin, 0.0),
                                    "ParametricVar: input covariance matrix is not positive semi-definite, smallest "
                                    "eigenvalue is "
                                        << evMin);
                                DLOG("Smallest eigenvalue is " << evMin);
                                salvage_ = boost::make_shared<QuantExt::NoCovarianceSalvage>();
                            }
                        } else
                            covCalculator = ext::make_shared<CovarianceCalculator>(period_.value());

                        vector<ext::shared_ptr<PNLCalculator>> pnlCalculators;
                        addPnlCalculators(pnlCalculators);

                        includeDeltaMargin_ = includeDeltaMargin(riskGroup);
                        includeGammaMargin_ = includeGammaMargin(riskGroup);
                        //  bool haveDetailTrd = btArgs_->reports_.count(ReportType::DetailTrade) == 1;

                        if (covCalculator && pnlCalculators.size() > 0)
                            sensiPnlCalculator_->calculateSensiPnl(srs, deltaKeys, scube->second, pnlCalculators,
                                                                   covCalculator, tradeIds, includeGammaMargin_,
                                                                   includeDeltaMargin_, requireTradePnl_);
                    }
                    handleSensiResults(reports, riskGroup, tradeGroup);
                }
            }
        }

        // Reset the sensitivity aggregator before changing the risk filter
        sensiAgg->reset();
    }
}

void MarketRiskReport::reset(const ext::shared_ptr<MarketRiskGroup>& riskGroup) {
    deltas_.clear();
    gammas_.clear();
    covarianceMatrix_ = Matrix();
    tradeIdIdxPairs_.clear();
}

} // namespace analytics
} // namespace ore
