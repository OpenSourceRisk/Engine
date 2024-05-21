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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/range/adaptor/indexed.hpp>

using namespace std;
using namespace QuantLib;
using namespace boost::accumulators;

namespace {
 
using TradeSensiCache = map<Size, map<Size, pair<Real, Real>>>;
using ore::analytics::SensitivityRecord;

void cacheTradeSensitivities(TradeSensiCache& cache, ore::analytics::SensitivityStream& ss,
                             const set<SensitivityRecord>& srs,
                             const vector<string>& tradeIds) {

    // Reset the stream to ensure at start.
    ss.reset();

    // One pass of sensitivity records to populate the trade level cache.
    while (SensitivityRecord sr = ss.next()) {

        // Sensitivity record is only relevant if it is in our set of trade IDs
        auto itTrade = find(tradeIds.begin(), tradeIds.end(), sr.tradeId);
        if (itTrade == tradeIds.end())
            continue;

        // This sensitivity record should be in the set of passed in sensitivity records.
        auto itSr = find_if(srs.begin(), srs.end(), [&sr](const SensitivityRecord& other) {
            return sr.key_1 == other.key_1 && sr.key_2 == other.key_2;
        });

        // Positions for the cache keys.
        auto posTrade = distance(tradeIds.begin(), itTrade);
        auto posSr = distance(srs.begin(), itSr);

        // Add the sensitivity record values to the cache.
        auto itCacheSr = cache.find(posSr);
        if (itCacheSr == cache.end()) {
            cache[posSr][posTrade] = make_pair(sr.delta, sr.gamma);
        } else {
            auto itCacheTrade = itCacheSr->second.find(posTrade);
            if (itCacheTrade == itCacheSr->second.end()) {
                itCacheSr->second[posTrade] = make_pair(sr.delta, sr.gamma);
            } else {
                auto& p = itCacheTrade->second;
                p.first += sr.delta;
                p.second += sr.gamma;
            }
        }
    }

    // Reset the stream to ensure at start.
    ss.reset();
}

}

namespace ore {
namespace analytics {

void CovarianceCalculator::initialise(const set<pair<RiskFactorKey, Size>>& keys) {
    // Set up the boost accumulators that will calculate the covariance between the time series of historical shifts for
    // each relevant risk factor key i.e. the risk factor keys in the set keys over the benchmark period
    
    for (auto ito = keys.begin(); ito != keys.end(); ito++) {
        accCov_[make_pair(ito->second, ito->second)] = accumulator();
        for (auto iti = keys.begin(); iti != ito; iti++) {
            accCov_[make_pair(iti->second, ito->second)] = accumulator();
        }
    }
}

void CovarianceCalculator::updateAccumulators(const ext::shared_ptr<NPVCube>& shiftCube, Date startDate, Date endDate, Size index) {
    TLOG("Updating Covariance accumlators for sensitivity record " << index);
    if (covariancePeriod_.contains(startDate) &&
        covariancePeriod_.contains(endDate)) {
        // Update the covariance accumulators if in benchmark period
        for (auto it = accCov_.begin(); it != accCov_.end(); it++) {
            Real oShift = shiftCube->get(it->first.first, 0, index);
            if (it->first.first == it->first.second) {
                it->second(oShift, covariate1 = oShift);
            } else {
                Real iShift = shiftCube->get(it->first.second, 0, index);
                it->second(oShift, covariate1 = iShift);
            }
        }
    }
}

void CovarianceCalculator::populateCovariance(const std::set<std::pair<RiskFactorKey, QuantLib::Size>>& keys) {
    LOG("Populate the covariance matrix with the calculated covariances");
    covariance_ = Matrix(keys.size(), keys.size());
    Size i = 0;
    for (auto ito = keys.begin(); ito != keys.end(); ito++) {
        covariance_[i][i] = boost::accumulators::covariance(accCov_.at(make_pair(ito->second, ito->second)));
        Size j = 0;
        for (auto iti = keys.begin(); iti != ito; iti++) {
            covariance_[i][j] = covariance_[j][i] =
                boost::accumulators::covariance(accCov_.at(make_pair(iti->second, ito->second)));
            j++;
        }
        i++;
    }
}

void PNLCalculator::populatePNLs(const std::vector<Real>& allPnls,
                                 const std::vector<Real>& allFoPnls,
                                 const std::vector<Date>& startDates,
                                 const std::vector<Date>& endDates) {
    QL_REQUIRE(allPnls.size() == allFoPnls.size(), "PNLs and first order PNLs must be the same size");

    pnls_.reserve(allPnls.size());
    foPnls_.reserve(allPnls.size());

    for (Size i = 0; i < allPnls.size(); i++) {
        // Backtesting P&L vectors
        if (pnlPeriod_.contains(startDates[i]) && pnlPeriod_.contains(endDates[i])) {
            pnls_.push_back(allPnls[i]);
            foPnls_.push_back(allFoPnls[i]);
        }
    }
    pnls_.shrink_to_fit();
    foPnls_.shrink_to_fit();
}

void PNLCalculator::populateTradePNLs(const TradePnLStore& allPnls, const TradePnLStore& foPnls) {
    tradePnls_ = allPnls;
    foTradePnls_ = foPnls;
}

 const bool PNLCalculator::isInTimePeriod(Date startDate, Date endDate) {
    return pnlPeriod_.contains(startDate) && pnlPeriod_.contains(endDate);
}

void HistoricalSensiPnlCalculator::populateSensiShifts(QuantLib::ext::shared_ptr<NPVCube>& cube, const vector<RiskFactorKey>& keys,
    ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator) {

    hisScenGen_->reset();
    QuantLib::ext::shared_ptr<Scenario> baseScenario = hisScenGen_->baseScenario();

    set<string> keyNames;
    std::map<std::string, RiskFactorKey> keyNameMapping;
    for (auto k : keys) {
        keyNames.insert(ore::data::to_string(k));
        keyNameMapping.insert({ore::data::to_string(k), k});
    }

    cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        baseScenario->asof(), keyNames, vector<Date>(1, baseScenario->asof()), hisScenGen_->numScenarios());

    // Loop over each historical scenario which represents the market move from t_i to
    // t_i + mpor applied to the base scenario for all i in historical period of scenario generator
    for (Size i = 0; i < hisScenGen_->numScenarios(); i++) {
        QuantLib::ext::shared_ptr<Scenario> scenario = hisScenGen_->next(baseScenario->asof());

        Size j = 0;
        for (const auto& [_, key] : keyNameMapping) {
            try {
                Real shift = shiftCalculator->shift(key, *baseScenario, *scenario);
                cube->set(shift, j, 0, i);
            } catch (const std::exception& e) {
                StructuredAnalyticsErrorMessage(
                    "HistocialSensiPnlCalculator",
                    "Shift calcuation failed. Check consistency of simulation and sensi config.",
                    "Error retrieving sensi key '" + ore::data::to_string(key) + "' from ssm scenario: '" + e.what())
                    .log();
            }
            j++;
        }
    }
}

void HistoricalSensiPnlCalculator::calculateSensiPnl(
    const set<SensitivityRecord>& srs, const vector<RiskFactorKey>& rfKeys, ext::shared_ptr<NPVCube>& shiftCube, 
    const vector<ext::shared_ptr<PNLCalculator>>& pnlCalculators,
    const ext::shared_ptr<CovarianceCalculator>& covarianceCalculator,
    const vector<string>& tradeIds, const bool includeGammaMargin, 
    const bool includeDeltaMargin, const bool tradeLevel) {    

    // Set of relevant keys from sensitivity records, needed for covariance matrix
    // Add the index of the key location in sensi shift cube
    set<pair<RiskFactorKey, Size>> keys;
    for (auto it = rfKeys.begin(); it != rfKeys.end(); it++) {
        auto it1 = shiftCube->idsAndIndexes().find(ore::data::to_string(*it));
        QL_REQUIRE(it1 != shiftCube->idsAndIndexes().end(),
                   "Could not find key " << *it << " in sensi shift cube keys");
        keys.insert(make_pair(*it, it1->second));
    }
    // Create an index pair for each sensitivity record. The first element holds the index position
    // in the sensi shift cube of key_1. The second element holds the index of key_2 for cross
    // gamma record.
    vector<pair<Size, Size>> srsIndex;
    for (auto it = srs.begin(); it != srs.end(); it++) {
        auto it1 = shiftCube->idsAndIndexes().find(ore::data::to_string(it->key_1));
        QL_REQUIRE(it1 != shiftCube->idsAndIndexes().end(),
                   "Could not find key " << it->key_1 << " in sensi shift cube keys");
        Size ind_1 = it1->second;
        Size ind_2 = Size();
        if (it->isCrossGamma()) {
            auto it2 = shiftCube->idsAndIndexes().find(ore::data::to_string(it->key_2));
            QL_REQUIRE(it2 != shiftCube->idsAndIndexes().end(),
                       "Could not find key " << it->key_2 << " in sensi shift cube keys");
            ind_2 = it2->second;
        }
        srsIndex.push_back(make_pair(ind_1, ind_2));
    }

    if (covarianceCalculator)
        covarianceCalculator->initialise(keys);

    // we require a sensitivity stream to run at trade level
    bool runTradeLevel = tradeLevel && sensitivityStream_;

    // Local P&L vectors to hold _all_ historical P&Ls
    Size nScenarios = hisScenGen_->numScenarios();
    Size nCalculators = pnlCalculators.size();
    vector<Real> allPnls(hisScenGen_->numScenarios(), 0.0);
    vector<Real> allFoPnls(hisScenGen_->numScenarios(), 0.0);
    
    //                    calculators,scenarios,  trades
    using TradePnLStore = std::vector<std::vector<QuantLib::Real>>;
    std::vector<TradePnLStore> tradePnls, foTradePnls;

    // We may need to store trade level P&Ls.
    if (runTradeLevel) {
        tradePnls.clear();
        tradePnls.reserve(nCalculators);
        foTradePnls.clear();
        foTradePnls.reserve(nCalculators);
        for (Size i = 0; i < nCalculators; i++) {
            tradePnls.push_back(std::vector<std::vector<QuantLib::Real>>());
            tradePnls.at(i).reserve(nScenarios);
            foTradePnls.push_back(std::vector<std::vector<QuantLib::Real>>());
            foTradePnls.at(i).reserve(nScenarios);
        }
    }

    hisScenGen_->reset();
    QuantLib::ext::shared_ptr<Scenario> baseScenario = hisScenGen_->baseScenario();

    // If we have been asked for a trade level P&L contribution report or detail report, store the trade level
    // sensitivities. We store them in a container here that is easily looked up in the loop below.
    TradeSensiCache tradeSensiCache;
    if (runTradeLevel) {
        cacheTradeSensitivities(tradeSensiCache, *sensitivityStream_, srs, tradeIds);
    }

    // Loop over each historical scenario
    for (Size i = 0; i < hisScenGen_->numScenarios(); i++) {

        // Add trade level P&L vector if needed.
        if (runTradeLevel) {
            for (Size j = 0; j < pnlCalculators.size(); j++) {
                bool inPeriod =
                    pnlCalculators.at(j)->isInTimePeriod(hisScenGen_->startDates()[i], hisScenGen_->endDates()[i]);
                if (inPeriod) {
                    tradePnls.at(j).push_back(vector<Real>(tradeIds.size(), 0.0));
                    foTradePnls.at(j).push_back(vector<Real>(tradeIds.size(), 0.0));
                }
            }
        }

        for (const auto elem : srs | boost::adaptors::indexed(0)) {

            const auto& sr = elem.value();
            auto j = elem.index();

            if (!sr.isCrossGamma()) {

                Real shift = shiftCube->get(srsIndex[j].first, 0, i);
                Real deltaPnl = shift * sr.delta;
                Real gammaPnl = 0.5 * shift * shift * sr.gamma;
                // Update the first order P&L
                allFoPnls[i] += deltaPnl;
                // If backtesting curvature margin, we exclude deltas i.e. 1st order effects from the sensi P&L
                if (includeDeltaMargin)
                    allPnls[i] += deltaPnl;
                // If backtesting delta margin, we exclude gammas i.e. second order effects from the sensi P&L
                if (includeGammaMargin)
                    allPnls[i] += gammaPnl;

                for (Size k = 0; k < pnlCalculators.size(); k++) {
                    if (pnlCalculators.at(k)->isInTimePeriod(hisScenGen_->startDates()[i],
                                                             hisScenGen_->endDates()[i])) {
                        pnlCalculators.at(k)->writePNL(i, true, sr.key_1, shift, sr.delta, sr.gamma, deltaPnl,
                                                       gammaPnl);
                        if (!tradeSensiCache.empty()) {
                            auto itSr = tradeSensiCache.find(j);
                            if (itSr != tradeSensiCache.end()) {
                                for (const auto& kv : itSr->second) {
                                    const auto& tradeId = tradeIds[kv.first];
                                    Real tradeDelta = kv.second.first;
                                    Real tradeDeltaPnl = shift * tradeDelta;
                                    Real tradeGamma = kv.second.second;
                                    Real tradeGammaPnl = 0.5 * shift * shift * tradeGamma;
                                    // Attempt to write trade level P&L contribution row.
                                    pnlCalculators.at(k)->writePNL(i, true, sr.key_1, shift, tradeDelta, tradeGamma,
                                                                   tradeDeltaPnl, tradeGammaPnl, RiskFactorKey(), 0.0,
                                                                   tradeId);
                                    // Update the sensitivity based trade level P&Ls
                                    if (runTradeLevel) {
                                        foTradePnls.at(k).back()[kv.first] += tradeDeltaPnl;
                                        if (includeDeltaMargin)
                                            tradePnls.at(k).back()[kv.first] += tradeDeltaPnl;
                                        if (includeGammaMargin)
                                            tradePnls.at(k).back()[kv.first] += tradeGammaPnl;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                Real shift_1 = shiftCube->get(srsIndex[j].first, 0, i);
                Real shift_2 = shiftCube->get(srsIndex[j].second, 0, i);
                Real gammaPnl = shift_1 * shift_2 * sr.gamma;

                // If backtesting delta margin, we exclude gammas i.e. second order effects from the sensi P&L
                if (includeGammaMargin)
                    allPnls[i] += gammaPnl;
                                
                for (Size j = 0; j < pnlCalculators.size(); j++) {
                    const auto& c = pnlCalculators[j];
                    if (c->isInTimePeriod(hisScenGen_->startDates()[i], hisScenGen_->endDates()[i])) {
                        c->writePNL(i, true, sr.key_1, shift_1, sr.delta, sr.gamma, 0.0, gammaPnl, sr.key_2, shift_2);
                        if (!tradeSensiCache.empty()) {
                            auto itSr = tradeSensiCache.find(j);
                            if (itSr != tradeSensiCache.end()) {
                                for (const auto& kv : itSr->second) {
                                    const auto& tradeId = tradeIds[kv.first];
                                    Real tradeGamma = kv.second.second;
                                    Real tradeGammaPnl = shift_1 * shift_2 * tradeGamma;
                                    // Attempt to write trade level P&L contribution row.
                                    c->writePNL(i, true, sr.key_1, shift_1, 0.0, tradeGamma, 0.0, tradeGammaPnl,
                                                  sr.key_2, shift_2, tradeId);
                                    // Update the sensitivity based trade level P&Ls
                                    if (runTradeLevel && includeGammaMargin) {
                                        tradePnls.at(j).back()[kv.first] += tradeGammaPnl;
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }
        if (covarianceCalculator)
            covarianceCalculator->updateAccumulators(shiftCube, hisScenGen_->startDates()[i], hisScenGen_->endDates()[i], i);
    }
    if (covarianceCalculator)
        covarianceCalculator->populateCovariance(keys);

    LOG("Populate the sensitivity backtesting P&L vectors");
    for (Size j = 0; j < pnlCalculators.size(); j++) {
        pnlCalculators.at(j)->populatePNLs(allPnls, allFoPnls, hisScenGen_->startDates(), hisScenGen_->endDates());
        if (runTradeLevel)
            pnlCalculators.at(j)->populateTradePNLs(tradePnls.at(j), foTradePnls.at(j));
    }
}

} // namespace analytics
} // namespace ore
