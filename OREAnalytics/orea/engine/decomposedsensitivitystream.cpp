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

namespace {

double fxRiskShiftSize(const std::string ccy, const std::string baseCcy,
                       boost::shared_ptr<SensitivityScenarioData> ssd) {
    auto fxpair = ccy + baseCcy;
    auto fxShiftSizeIt = ssd->fxShiftData().find(fxpair);
    QL_REQUIRE(fxShiftSizeIt != ssd->fxShiftData().end(), "Couldn't find shiftsize for " << fxpair);
    QL_REQUIRE(fxShiftSizeIt->second.shiftType == "Relative",
               "Requires a relative fxSpot shift for index decomposition");
    return fxShiftSizeIt->second.shiftSize;
}

double eqRiskShiftSize(const std::string equityName, boost::shared_ptr<SensitivityScenarioData> ssd) {
    auto eqShiftSizeIt = ssd->equityShiftData().find(equityName);
    QL_REQUIRE(eqShiftSizeIt != ssd->equityShiftData().end(), "Couldn't find a shift size for " << equityName);
    QL_REQUIRE(eqShiftSizeIt->second.shiftType == "Relative",
               "Requires a relative eqSpot shift for index decomposition");
    return eqShiftSizeIt->second.shiftSize;
}

} // namespace

DecomposedSensitivityStream::DecomposedSensitivityStream(
    const boost::shared_ptr<SensitivityStream>& ss, const std::string& baseCurrency,
    std::map<std::string, std::map<std::string, double>> defaultRiskDecompositionWeights,
    const std::set<std::string>& eqComDecompositionTradeIds,
    const std::map<std::string, std::map<std::string, double>>& currencyHedgedIndexQuantities,
    const boost::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
    const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const boost::shared_ptr<SensitivityScenarioData>& scenarioData,
    const boost::shared_ptr<ore::data::Market>& todaysMarket)
    : ss_(ss), baseCurrency_(baseCurrency), defaultRiskDecompositionWeights_(defaultRiskDecompositionWeights),
      eqComDecompositionTradeIds_(eqComDecompositionTradeIds),
      currencyHedgedIndexQuantities_(currencyHedgedIndexQuantities), refDataManager_(refDataManager),
      curveConfigs_(curveConfigs), ssd_(scenarioData), todaysMarket_(todaysMarket) {
    reset();
    decompose_ = !defaultRiskDecompositionWeights_.empty() || !eqComDecompositionTradeIds_.empty();
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
    bool hasCommodityRefData =
        refDataManager_ != nullptr && refDataManager_->hasData("CommodityIndex", record.key_1.name);

    try {
        if (isSurvivalProbSensi && tradeIdValidSurvival && isNotCrossGamma) {
            return decomposeSurvivalProbability(record);
        } else if (isEquitySpotSensi && tradeIdValid && hasEquityIndexRefData && isNotCrossGamma) {
            return decomposeEquityRisk(record);
        } else if (isEquitySpotSensi && tradeIdValid && hasCurrencyHedgedIndexRefData && isNotCrossGamma) {
            return decomposeCurrencyHedgedIndexRisk(record);
        } else if ((isEquitySpotSensi || isCommoditySpotSensi) && tradeIdValid && hasCommodityRefData &&
                   isNotCrossGamma) {
            return decomposeCommodityRisk(record);
        } else if ((isEquitySpotSensi || isCommoditySpotSensi) && tradeIdValid && isNotCrossGamma) {
            auto subFields = std::map<std::string, std::string>({{"tradeId", record.tradeId}});
            StructuredAnalyticsErrorMessage(
                "Sensitivity Decomposition", "Index decomposition failed",
                "Cannot decompose equity index delta (" + record.key_1.name +
                    ") for trade: no reference data found. Continuing without decomposition.",
                subFields)
                .log();
        }
    } catch (const std::exception& e) {
        auto subFields = std::map<std::string, std::string>({{"tradeId", record.tradeId}});
        StructuredAnalyticsErrorMessage(
            "Sensitivity Decomposition", "Index decomposition failed",
            "Cannot decompose equity index delta (" + record.key_1.name + ") for trade:" + e.what(), subFields)
            .log();
    } catch (...) {
        auto subFields = std::map<std::string, std::string>({{"tradeId", record.tradeId}});
        StructuredAnalyticsErrorMessage(
            "Sensitivity Decomposition", "Index decomposition failed",
            "Cannot decompose equity index delta (" + record.key_1.name + ") for trade: unkown error", subFields)
            .log();
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
    std::string indexName = sr.key_1.name;
    auto indexCurrency = curveCurrency(indexName, ore::data::CurveSpec::CurveType::Equity);
    if (refDataManager_->hasData("EquityIndex", indexName)) {
        auto refDatum = refDataManager_->getData("EquityIndex", indexName);
        auto indexRefDatum = boost::dynamic_pointer_cast<ore::data::IndexReferenceDatum>(refDatum);
        auto decompResults = decomposeIndex(sr.delta, indexRefDatum, ore::data::CurveSpec::CurveType::Equity);
        scaleFxRisk(decompResults.fxRisk, indexName);
        return createDecompositionRecords(
            decompResults.equityDelta, decompResults.fxRisk, decompResults.indexCurrency, sr);
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
    
    auto indexName = sr.key_1.name;
    auto indexCurrency = curveCurrency(indexName, ore::data::CurveSpec::CurveType::Equity);

    boost::shared_ptr<ore::data::CurrencyHedgedEquityIndexDecomposition> decomposeCurrencyHedgedIndexHelper;
    decomposeCurrencyHedgedIndexHelper =
        loadCurrencyHedgedIndexDecomposition(indexName, refDataManager_, curveConfigs_);
    if (decomposeCurrencyHedgedIndexHelper != nullptr) {
        QL_REQUIRE(currencyHedgedIndexQuantities_.count(sr.tradeId) > 0,
                   "CurrencyHedgedIndexDecomposition failed, there is no index quantity for trade "
                       << sr.tradeId << " and equity index EQ-" << indexName);
        QL_REQUIRE(currencyHedgedIndexQuantities_.at(sr.tradeId).count("EQ-" + indexName) > 0,
                   "CurrencyHedgedIndexDecomposition failed, there is no index quantity for trade "
                       << sr.tradeId << " and equity index EQ-" << indexName);
        QL_REQUIRE(todaysMarket_ != nullptr,
                   "CurrencyHedgedIndexDecomposition failed, there is no market given quantity for trade "
                       << sr.tradeId);

        Date today = QuantLib::Settings::instance().evaluationDate();

        auto quantity = currencyHedgedIndexQuantities_.at(sr.tradeId).find("EQ-" + indexName)->second;

        QL_REQUIRE(quantity != QuantLib::Null<double>(),
                   "CurrencyHedgedIndexDecomposition failed, index quantity cannot be NULL.");

        double hedgedExposure = sr.delta / eqRiskShiftSize(indexName, ssd_);

        double unhedgedExposure =
            decomposeCurrencyHedgedIndexHelper->unhedgedSpotExposure(hedgedExposure, quantity, today, todaysMarket_);

        double unhedgedDelta = unhedgedExposure * eqRiskShiftSize(indexName, ssd_);

        auto decompResults = decomposeIndex(unhedgedDelta, decomposeCurrencyHedgedIndexHelper->underlyingRefData(),
                                            ore::data::CurveSpec::CurveType::Equity);
        scaleFxRisk(decompResults.fxRisk, decomposeCurrencyHedgedIndexHelper->indexName());
        // Correct FX Delta from FxForwards
        for (const auto& [ccy, fxRisk] :
             decomposeCurrencyHedgedIndexHelper->fxSpotRiskFromForwards(quantity, today, todaysMarket_, 1.0)) {
            decompResults.fxRisk[ccy] = decompResults.fxRisk[ccy] - fxRisk * fxRiskShiftSize(ccy, baseCurrency_, ssd_);
        }
        // Convert into the correct currency
        return createDecompositionRecords(decompResults.equityDelta, decompResults.fxRisk,
                                          indexCurrency, sr);
    } else {
        auto subFields = std::map<std::string, std::string>({{"tradeId", sr.tradeId}});
        StructuredAnalyticsErrorMessage("CRIF Generation", "Equity index decomposition failed",
                                        "Cannot decompose equity index delta (" + indexName +
                                            ") for trade: no reference data found. Continuing without decomposition.",
                                        subFields)
            .log();
        return {sr};
    }
}

std::vector<SensitivityRecord> DecomposedSensitivityStream::decomposeCommodityRisk(const SensitivityRecord& sr) const {
    std::string indexName = sr.key_1.name;
    if (refDataManager_->hasData("CommodityIndex", indexName)) {
        auto refDatum = refDataManager_->getData("CommodityIndex", indexName);
        auto indexRefDatum = boost::dynamic_pointer_cast<ore::data::IndexReferenceDatum>(refDatum);
        auto decompResults = decomposeIndex(sr.delta, indexRefDatum, ore::data::CurveSpec::CurveType::Commodity);
        scaleFxRisk(decompResults.fxRisk, indexName);
        auto indexCurrency = curveCurrency(indexName, ore::data::CurveSpec::CurveType::Commodity);
        return createDecompositionRecords(
            decompResults.equityDelta, decompResults.fxRisk, decompResults.indexCurrency, sr);
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

void DecomposedSensitivityStream::reset() {
    ss_->reset();
    decomposedRecords_.clear();
    itCurrent_ = decomposedRecords_.begin();
}

DecomposedSensitivityStream::DecompositionResults
DecomposedSensitivityStream::decomposeIndex(double delta, const boost::shared_ptr<ore::data::IndexReferenceDatum>& ird,
                                            ore::data::CurveSpec::CurveType curveType) const {
    QL_REQUIRE(ird, "Can not decompose equity risk, no EquityIndexReferenceData giving");
    QL_REQUIRE(curveConfigs_, "Can not decompose equity risk, no CurveConfig giving");
    QL_REQUIRE(curveType == ore::data::CurveSpec::CurveType::Equity ||
                   curveType == ore::data::CurveSpec::CurveType::Commodity,
               "internal error decomposeEquityRisk supports only Equity and Commodity curves");

    DecompositionResults results;
    results.indexCurrency = curveCurrency(ird->id(), curveType);
    QL_REQUIRE(!results.indexCurrency.empty(),
               "Cannot perform equity risk decomposition find currency index " + ird->id() + " from curve configs.");

    for (const auto& [constituent, weight] : ird->underlyings()) {
        results.equityDelta[constituent] += delta * weight;
        // try look up currency in reference data and add if FX delta risk if necessary
        std::string constituentCcy = curveCurrency(constituent, curveType);
        if (constituentCcy.empty()) {
            constituentCcy = results.indexCurrency;
            StructuredAnalyticsErrorMessage("CRIF Generation", "Equity index decomposition",
                                            "Cannot find currency for equity " + constituent +
                                                " from curve configs, fallback to use index currency (" +
                                                results.indexCurrency + ")")
                .log();
        }
        if (constituentCcy != baseCurrency_) {
            results.fxRisk[constituentCcy] += delta * weight;
        }
    }
    return results;
}

std::vector<SensitivityRecord> DecomposedSensitivityStream::createDecompositionRecords(
    const std::map<std::string, double>& eqDeltas, const std::map<std::string, double>& fxDeltas,
    const std::string indexCurrency, const SensitivityRecord& sr) const {
    std::vector<SensitivityRecord> records;
    for (auto [underlying, delta] : eqDeltas) {
        RiskFactorKey underlyingKey(sr.key_1.keytype, underlying, sr.key_1.index);
        records.push_back(SensitivityRecord(sr.tradeId, sr.isPar, underlyingKey, sr.desc_1, sr.shift_1, RiskFactorKey(),
                                            "", sr.shift_2, sr.currency, sr.baseNpv, delta, 0.0));
    }
    // Add aggregated FX Deltas
    for (auto [ccy, delta] : fxDeltas) {
        if (ccy != indexCurrency && ccy != baseCurrency_) {
            RiskFactorKey underlyingKey(RiskFactorKey::KeyType::FXSpot, ccy + baseCurrency_, 0);
            records.push_back(SensitivityRecord(sr.tradeId, sr.isPar, underlyingKey, sr.desc_1, sr.shift_1,
                                                RiskFactorKey(), "", sr.shift_2, sr.currency, sr.baseNpv, delta, 0.0));
        }
    }
    return records;
}

void DecomposedSensitivityStream::scaleFxRisk(std::map<std::string, double>& fxRisk,
                                              const std::string& equityName) const {
    // Eq/Comm Shift to FX Shift Conversion
    auto eqShift = eqRiskShiftSize(equityName, ssd_);
    for (auto& [ccy, fxdelta] : fxRisk) {
        fxdelta = fxdelta * fxRiskShiftSize(ccy, baseCurrency_, ssd_) / eqShift;
    }
}

std::string DecomposedSensitivityStream::curveCurrency(const std::string& name,
                                                       ore::data::CurveSpec::CurveType curveType) const {
    std::string curveCurrency;
    if (curveConfigs_->has(curveType, name)) {
        curveCurrency = curveType == ore::data::CurveSpec::CurveType::Equity
                            ? curveConfigs_->equityCurveConfig(name)->currency()
                            : curveConfigs_->commodityCurveConfig(name)->currency();
    } else if (curveConfigs_->hasEquityCurveConfig(name)) {
        // if we use a equity curve as proxy  fall back to lookup the currency from the proxy config
        curveCurrency = curveConfigs_->equityCurveConfig(name)->currency();
    }
    return curveCurrency;
}
} // namespace analytics
} // namespace ore
