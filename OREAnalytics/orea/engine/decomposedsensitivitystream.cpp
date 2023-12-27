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

#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/engine/decomposedsensitivitystream.hpp>
#include <ored/utilities/currencyhedgedequityindexdecomposition.hpp>

using QuantLib::Real;
using std::fabs;

namespace ore {
namespace analytics {

DecomposedSensitivityStream::DecomposedSensitivityStream(
    const boost::shared_ptr<SensitivityStream>& ss,
    std::map<std::string, std::map<std::string, double>> defaultRiskDecompositionWeights,
    const std::set<std::string>& eqComDecompositionTradeIds,
    const std::map<std::string, double>& currencyHedgedIndexQuantities,
    const boost::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
    const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs)
    : ss_(ss), defaultRiskDecompositionWeights_(defaultRiskDecompositionWeights),
      eqComDecompositionTradeIds_(eqComDecompositionTradeIds),
      currencyHedgedIndexQuantities_(currencyHedgedIndexQuantities), refDataManager_(refDataManager),
      curveConfigs_(curveConfigs) {
    reset();
    decompose_ = !defaultRiskDecompositionWeights_.empty() ||
                 (!eqComDecompositionTradeIds_.empty() && refDataManager_ != nullptr);
}

//! Returns the next SensitivityRecord in the stream after filtering
SensitivityRecord DecomposedSensitivityStream::next() {
    if (!decompose_)
        return ss_->next();
    // No decomposed records left, so continue to the next record
    if (itCurrent_ == decomposedRecords_.end()) {
        decomposedRecords_ = decompose(ss_->next());
        itCurrent_ = decomposedRecords_.begin();
    }
    return *(itCurrent_++);
}

std::vector<SensitivityRecord> DecomposedSensitivityStream::decompose(const SensitivityRecord& record) const {
    std::vector<SensitivityRecord> results;
    if (record.key_1.keytype == RiskFactorKey::KeyType::SurvivalProbability && record.isCrossGamma() == false &&
        defaultRiskDecompositionWeights_.find(record.tradeId) != defaultRiskDecompositionWeights_.end()) {
        return decomposeSurvivalProbability(record);
    } else if (record.key_1.keytype == RiskFactorKey::KeyType::EquitySpot && record.isCrossGamma() == false &&
               eqComDecompositionTradeIds_.find(record.tradeId) != eqComDecompositionTradeIds_.end()) {
        return decomposeEquityRisk(record);
    } else if (record.key_1.keytype == RiskFactorKey::KeyType::CommodityCurve && record.isCrossGamma() == false &&
               eqComDecompositionTradeIds_.find(record.tradeId) != eqComDecompositionTradeIds_.end()) {
        return decomposeCommodityRisk(record);
    } else {
        return {record};
    }
}

std::vector<SensitivityRecord>
DecomposedSensitivityStream::decomposeSurvivalProbability(const SensitivityRecord& record) const {
    std::vector<SensitivityRecord> results;
    auto decompRecord = record;
    for (const auto& [constituent, weight] : defaultRiskDecompositionWeights_.at(record.tradeId)) {
        decompRecord.key_1 = RiskFactorKey(record.key_1.keytype, constituent, record.key_1.index);
        decompRecord.delta = record.delta * weight;
        decompRecord.gamma = record.gamma * weight;
        results.push_back(decompRecord);
    }
    return results;
}

std::vector<SensitivityRecord> DecomposedSensitivityStream::decomposeEquityRisk(const SensitivityRecord& sr) const {

    boost::shared_ptr<ore::data::EquityIndexReferenceDatum> decomposeEquityIndexData;
    boost::shared_ptr<ore::data::CurrencyHedgedEquityIndexDecomposition> decomposeCurrencyHedgedIndexHelper;

    try {
        auto eqRefData = refDataManager_->getData("EquityIndex", sr.key_1.name);
        decomposeEquityIndexData = boost::dynamic_pointer_cast<ore::data::EquityIndexReferenceDatum>(eqRefData);
    } catch (std::exception& e) {
        WLOG("Error getting Equity Index constituents for " << sr.key_1.name << ": " << e.what());
    }

    try {
        decomposeCurrencyHedgedIndexHelper =
            loadCurrencyHedgedIndexDecomposition(sr.key_1.name, refDataManager_, curveConfigs_);
    } catch (const std::exception& e) {
        WLOG("Error getting Currency Equity Index constituents for " << sr.key_1.name << ": " << e.what());
        decomposeCurrencyHedgedIndexHelper = nullptr;
    }

    try {
        if (decomposeEquityIndexData != nullptr) {
            auto decompResults =  decomposeEqComIndexRisk(sr.delta, decomposeEquityIndexData, ore::data::CurveSpec::CurveType::Equity);
            return createDecompositionRecords(sr, decompResults);
        } else if (decomposeCurrencyHedgedIndexHelper != nullptr &&
                   currencyHedgedIndexQuantities_.count(sr.tradeId) > 0) {
        }
    } catch (...) {
    }
    return {sr};
}

std::vector<SensitivityRecord>
DecomposedSensitivityStream::decomposeCommodityRisk(const SensitivityRecord& record) const {}

void DecomposedSensitivityStream::reset() {
    ss_.reset();
    decomposedRecords_.clear();
    itCurrent_ = decomposedRecords_.begin();
}

DecomposedSensitivityStream::EqComIndexDecompositionResults
DecomposedSensitivityStream::decomposeEqComIndexRisk(double delta,
                                                     const boost::shared_ptr<ore::data::IndexReferenceDatum>& ird,
                                                     ore::data::CurveSpec::CurveType curveType) const;

std::vector<SensitivityRecord> DecomposedSensitivityStream::createDecompositionRecords(
    const SensitivityRecord& sr,
    const DecomposedSensitivityStream::EqComIndexDecompositionResults& decompResults) const;

} // namespace analytics
} // namespace ore
