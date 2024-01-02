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
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/engine/decomposedsensitivitystream.hpp>
#include <ored/utilities/currencyhedgedequityindexdecomposition.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

DecomposedSensitivityStream::DecomposedSensitivityStream(
    const boost::shared_ptr<SensitivityStream>& ss,
    std::map<std::string, std::map<std::string, double>> defaultRiskDecompositionWeights,
    const std::set<std::string>& eqComDecompositionTradeIds,
    const std::map<std::string, std::map<std::string, double>>& currencyHedgedIndexQuantities,
    const boost::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
    const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const boost::shared_ptr<ore::data::Market>& todaysMarket)
    : ss_(ss), defaultRiskDecompositionWeights_(defaultRiskDecompositionWeights),
      eqComDecompositionTradeIds_(eqComDecompositionTradeIds),
      currencyHedgedIndexQuantities_(currencyHedgedIndexQuantities), refDataManager_(refDataManager),
      curveConfigs_(curveConfigs), todaysMarket_(todaysMarket) {
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

    bool tradeIdValidSurvival =
        defaultRiskDecompositionWeights_.find(record.tradeId) != defaultRiskDecompositionWeights_.end();
    bool tradeIdValid = eqComDecompositionTradeIds_.find(record.tradeId) != eqComDecompositionTradeIds_.end();
    bool isNotCrossGamma = record.isCrossGamma() == false;
    bool isSurvivalProbSensi = record.key_1.keytype == RiskFactorKey::KeyType::SurvivalProbability;
    bool isEquitySpotSensi = record.key_1.keytype == RiskFactorKey::KeyType::EquitySpot;
    bool isCommoditySpotSensi = record.key_1.keytype == RiskFactorKey::KeyType::CommodityCurve;
    
    
    bool hasEquityIndexRefData =
        refDataManager_ != nullptr && refDataManager_->hasData("EquityIndex", record.key_1.name);
    bool hasCurrencyHedgedIndexRefData =
        refDataManager_ != nullptr && refDataManager_->hasData("CurrencyHedgedEquityIndex", record.key_1.name);
    bool hasCommodityRefData = refDataManager_ != nullptr && refDataManager_->hasData("CommodityIndex", record.key_1.name);

    try {
        if (isSurvivalProbSensi && tradeIdValidSurvival && isNotCrossGamma) {
            return decomposeSurvivalProbability(record);
        } else if (isEquitySpotSensi && tradeIdValid && hasEquityIndexRefData && isNotCrossGamma) {
            return decomposeEquityRisk(record);
        } else if (isEquitySpotSensi && tradeIdValid && hasCurrencyHedgedIndexRefData && isNotCrossGamma) {
            return decomposeCurrencyHedgedIndexRisk(record);
        } else if ((isEquitySpotSensi || isCommoditySpotSensi) && tradeIdValid && hasCommodityRefData && isNotCrossGamma) {
            return decomposeCommodityRisk(record);
        } else if ((isEquitySpotSensi || isCommoditySpotSensi) && tradeIdValid && isNotCrossGamma) {
            // Error no ref data
        }
    } catch (const std::exception& e) {
        
    } catch (...) {
    
    }
    return {record};
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
    auto eqRefData = refDataManager_->getData("EquityIndex", sr.key_1.name);
    auto decomposeEquityIndexData = boost::dynamic_pointer_cast<ore::data::EquityIndexReferenceDatum>(eqRefData);
    if (decomposeEquityIndexData != nullptr) {
        auto decompResults =
            decomposeEqComIndexRisk(sr.delta, decomposeEquityIndexData, ore::data::CurveSpec::CurveType::Equity);
        return createDecompositionRecords(sr, decompResults);
    } else {
        auto subFields = std::map<std::string, std::string>({{"tradeId", sr.tradeId}});
        StructuredAnalyticsErrorMessage("CRIF Generation", "Equity index decomposition failed",
                                        "Cannot decompose equity index delta (" + sr.key_1.name +
                                            ") for trade: no reference data found. Continuing without decomposition.",
                                        subFields)
            .log();
        return {sr};
    }
}

std::vector<SensitivityRecord>
DecomposedSensitivityStream::decomposeCurrencyHedgedIndexRisk(const SensitivityRecord& sr) const {

    boost::shared_ptr<ore::data::CurrencyHedgedEquityIndexDecomposition> decomposeCurrencyHedgedIndexHelper;
    decomposeCurrencyHedgedIndexHelper =
        loadCurrencyHedgedIndexDecomposition(sr.key_1.name, refDataManager_, curveConfigs_);
    if (decomposeCurrencyHedgedIndexHelper != nullptr) {
        QL_REQUIRE(currencyHedgedIndexQuantities_.count(sr.tradeId) > 0,
                   "CurrencyHedgedIndexDecomposition failed, there is no index quantity for trade "
                       << sr.tradeId << " and equity index EQ-" << sr.key_1.name);
        QL_REQUIRE(currencyHedgedIndexQuantities_.at(sr.tradeId).count("EQ-" + sr.key_1.name) > 0,
                   "CurrencyHedgedIndexDecomposition failed, there is no index quantity for trade "
                       << sr.tradeId << " and equity index EQ-" << sr.key_1.name);
        QL_REQUIRE(todaysMarket_ != nullptr,
                   "CurrencyHedgedIndexDecomposition failed, there is no market given quantity for trade "
                       << sr.tradeId);

        Date today = QuantLib::Settings::instance().evaluationDate();

        auto quantity = currencyHedgedIndexQuantities_.at(sr.tradeId).find("EQ-" + sr.key_1.name)->second;

        QL_REQUIRE(quantity != QuantLib::Null<double>(),
                   "CurrencyHedgedIndexDecomposition failed, index quantity cannot be NULL.");

        double unhedgedDelta =
            decomposeCurrencyHedgedIndexHelper->unhedgedDelta(sr.delta, quantity, today, todaysMarket_);

        auto decompResults =
            decomposeEqComIndexRisk(unhedgedDelta, decomposeCurrencyHedgedIndexHelper->underlyingRefData(),
                                    ore::data::CurveSpec::CurveType::Equity);

        // Correct FX Delta from FxForwards
        for (const auto& [ccy, fxRisk] :
             decomposeCurrencyHedgedIndexHelper->fxSpotRiskFromForwards(quantity, today, todaysMarket_)) {
            decompResults.fxRisk[ccy] = decompResults.fxRisk[ccy] - fxRisk;
        }
        return createDecompositionRecords(sr, decompResults);
    } else {
        auto subFields = std::map<std::string, std::string>({{"tradeId", sr.tradeId}});
        StructuredAnalyticsErrorMessage("CRIF Generation", "Equity index decomposition failed",
                                        "Cannot decompose equity index delta (" + sr.key_1.name +
                                            ") for trade: no reference data found. Continuing without decomposition.",
                                        subFields)
            .log();
        return {sr};
    }
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
                                                     ore::data::CurveSpec::CurveType curveType) const {
    QL_REQUIRE(ird, "Can not decompose equity risk, no EquityIndexReferenceData giving");
    QL_REQUIRE(curveConfigs_, "Can not decompose equity risk, no CurveConfig giving");
    QL_REQUIRE(curveType == ore::data::CurveSpec::CurveType::Equity ||
                   curveType == ore::data::CurveSpec::CurveType::Commodity,
               "internal error decomposeEquityRisk supports only Equity and Commodity curves");

    EqComIndexDecompositionResults results;

    if (curveConfigs_->has(curveType, ird->id())) {
        results.indexCurrency = curveType == ore::data::CurveSpec::CurveType::Equity
                                    ? curveConfigs_->equityCurveConfig(ird->id())->currency()
                                    : curveConfigs_->commodityCurveConfig(ird->id())->currency();
    } else if (curveConfigs_->hasEquityCurveConfig(ird->id())) {
        // if we use a equity curve as proxy  fall back to lookup the currency from the proxy config
        results.indexCurrency = curveConfigs_->equityCurveConfig(ird->id())->currency();
    } else {
        QL_FAIL("Cannot perform equity risk decomposition find currency index " + ird->id() + " from curve configs.");
    }

    for (auto c : ird->underlyings()) {
        results.equityDelta[c.first] += delta * c.second;
        // try look up currency in reference data and add if FX delta risk if necessary
        if (curveConfigs_->has(curveType, c.first)) {
            auto constituentCcy = curveType == ore::data::CurveSpec::CurveType::Equity
                                      ? curveConfigs_->equityCurveConfig(c.first)->currency()
                                      : curveConfigs_->commodityCurveConfig(c.first)->currency();
            results.fxRisk[constituentCcy] += delta * c.second;
        } else {
            StructuredAnalyticsErrorMessage("CRIF Generation", "Equity index decomposition",
                                            "Cannot find currency for equity " + c.first +
                                                " from curve configs, fallback to use index currency (" +
                                                results.indexCurrency + ")")
                .log();
            results.fxRisk[results.indexCurrency] += delta * c.second;
        }
    }
    return results;
}

std::vector<SensitivityRecord> DecomposedSensitivityStream::createDecompositionRecords(
    const SensitivityRecord& sr,
    const DecomposedSensitivityStream::EqComIndexDecompositionResults& decompResults) const {
    std::vector<SensitivityRecord> records;
    for (auto [underlying, delta] : decompResults.equityDelta) {
        RiskFactorKey underlyingKey(sr.key_1.keytype, underlying, sr.key_1.index);
        records.push_back(SensitivityRecord(sr.tradeId, sr.isPar, underlyingKey, "", sr.shift_1, RiskFactorKey(), "",
                                            sr.shift_2, sr.currency, sr.baseNpv, delta, 0.0));
    }
    // Add aggregated FX Deltas
    for (auto [ccy, delta] : decompResults.fxRisk) {
        if (ccy != decompResults.indexCurrency) {
            RiskFactorKey underlyingKey(RiskFactorKey::KeyType::FXSpot, ccy + "USD", 0);
            records.push_back(SensitivityRecord(sr.tradeId, sr.isPar, underlyingKey, "", sr.shift_1, RiskFactorKey(),
                                                "", sr.shift_2, sr.currency, sr.baseNpv, delta, 0.0));
        }
    }
    return records;
}

} // namespace analytics
} // namespace ore
