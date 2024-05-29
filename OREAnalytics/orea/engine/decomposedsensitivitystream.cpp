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

namespace {} // namespace

DecomposedSensitivityStream::DecomposedSensitivityStream(
    const QuantLib::ext::shared_ptr<SensitivityStream>& ss, const std::string& baseCurrency,
    std::map<std::string, std::map<std::string, double>> defaultRiskDecompositionWeights,
    const std::set<std::string>& eqComDecompositionTradeIds,
    const std::map<std::string, std::map<std::string, double>>& currencyHedgedIndexQuantities,
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& scenarioData,
    const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket)
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

    bool tradeMarkedForDecompositionDefaultRisk =
        defaultRiskDecompositionWeights_.find(record.tradeId) != defaultRiskDecompositionWeights_.end();
    bool tradeMarkedForDecomposition =
        eqComDecompositionTradeIds_.find(record.tradeId) != eqComDecompositionTradeIds_.end();
    bool isNotCrossGamma = record.isCrossGamma() == false;
    bool isSurvivalProbSensi = record.key_1.keytype == RiskFactorKey::KeyType::SurvivalProbability;
    bool isEquitySpotSensi = record.key_1.keytype == RiskFactorKey::KeyType::EquitySpot;
    bool isCommoditySpotSensi = record.key_1.keytype == RiskFactorKey::KeyType::CommodityCurve;

    bool decomposeEquitySpot = tradeMarkedForDecomposition && isEquitySpotSensi && refDataManager_ != nullptr &&
                               refDataManager_->hasData("EquityIndex", record.key_1.name);
    bool decomposeCurrencyHedgedSpot = tradeMarkedForDecomposition && isEquitySpotSensi && refDataManager_ != nullptr &&
                                       refDataManager_->hasData("CurrencyHedgedEquityIndex", record.key_1.name);
    bool decomposeCommoditySpot = tradeMarkedForDecomposition && (isCommoditySpotSensi || isEquitySpotSensi) &&
                                  refDataManager_ != nullptr &&
                                  refDataManager_->hasData("CommodityIndex", record.key_1.name);

    if (isEquitySpotSensi && refDataManager_->hasData("Equity", record.key_1.name)) {
        auto eqRefData = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityReferenceDatum>(
                refDataManager_->getData("Equity", record.key_1.name));
        isEquitySpotSensi = eqRefData->equityData().isIndex;       
    }

    try {
        if (tradeMarkedForDecompositionDefaultRisk && isSurvivalProbSensi && isNotCrossGamma) {
            return decomposeSurvivalProbability(record);
        } else if (decomposeEquitySpot && isNotCrossGamma) {
            auto decompResults =
                indexDecomposition(record.delta, record.key_1.name, ore::data::CurveSpec::CurveType::Equity);
            return sensitivityRecords(decompResults.spotRisk, decompResults.fxRisk, decompResults.indexCurrency,
                                      record);
        } else if (decomposeCurrencyHedgedSpot && isNotCrossGamma) {
            return decomposeCurrencyHedgedIndexRisk(record);
        } else if (decomposeCommoditySpot && isNotCrossGamma) {
            auto decompResults =
                indexDecomposition(record.delta, record.key_1.name, ore::data::CurveSpec::CurveType::Commodity);
            return sensitivityRecords(decompResults.spotRisk, decompResults.fxRisk, decompResults.indexCurrency,
                                      record);
        } else if (tradeMarkedForDecomposition && (isCommoditySpotSensi || isEquitySpotSensi ) && isNotCrossGamma) {
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

//! Decompose
std::map<std::string, double> DecomposedSensitivityStream::constituentSpotRiskFromDecomposition(
    const double spotDelta, const std::map<std::string, double>& indexWeights) const {
    std::map<std::string, double> results;
    for (const auto& [constituent, weight] : indexWeights) {
        results[constituent] = weight * spotDelta;
    }
    return results;
}

std::map<std::string, double> DecomposedSensitivityStream::fxRiskFromDecomposition(
    const std::map<std::string, double>& spotRisk,
    const std::map<std::string, std::vector<std::string>>& constituentCurrencies,
    const std::map<std::string, double>& fxSpotShiftSize, const double eqShiftSize) const {
    std::map<std::string, double> results;
    for (const auto& [currency, constituents] : constituentCurrencies) {
        if (currency != baseCurrency_) {
            QL_REQUIRE(fxSpotShiftSize.count(currency) == 1, "Can not find fxSpotShiftSize for currency " << currency);
            for (const auto& constituent : constituents) {
                QL_REQUIRE(spotRisk.count(constituent) == 1, "Can not find spotDelta for " << constituent);
                results[currency] += spotRisk.at(constituent) * fxSpotShiftSize.at(currency) / eqShiftSize;
            }
        }
    }
    return results;
}

double DecomposedSensitivityStream::fxRiskShiftSize(const std::string ccy) const {
    auto fxpair = ccy + baseCurrency_;
    auto fxShiftSizeIt = ssd_->fxShiftData().find(fxpair);
    QL_REQUIRE(fxShiftSizeIt != ssd_->fxShiftData().end(), "Couldn't find shiftsize for " << fxpair);
    QL_REQUIRE(fxShiftSizeIt->second.shiftType == ore::analytics::ShiftType::Relative,
               "Requires a relative fxSpot shift for index decomposition");
    return fxShiftSizeIt->second.shiftSize;
}

std::map<std::string, double>
DecomposedSensitivityStream::fxRiskShiftSizes(const std::map<std::string, std::vector<std::string>>& currencies) const {
    std::map<std::string, double> results;
    for (const auto& [ccy,_] : currencies) {
        if (ccy != baseCurrency_) {
            double shiftSize = fxRiskShiftSize(ccy);
            results[ccy] = shiftSize;
        }
    }
    return results;
}

double DecomposedSensitivityStream::equitySpotShiftSize(const std::string name) const {
    auto eqShiftSizeIt = ssd_->equityShiftData().find(name);
    QL_REQUIRE(eqShiftSizeIt != ssd_->equityShiftData().end(), "Couldn't find a equity shift size for " << name);
    QL_REQUIRE(eqShiftSizeIt->second.shiftType == ore::analytics::ShiftType::Relative,
               "Requires a relative eqSpot shift for index decomposition");
    return eqShiftSizeIt->second.shiftSize;
}

double DecomposedSensitivityStream::assetSpotShiftSize(const std::string indexName,
                                                       const ore::data::CurveSpec::CurveType curveType) const {
    if (curveType == ore::data::CurveSpec::CurveType::Equity) {
        return equitySpotShiftSize(indexName);
    } else if (curveType == ore::data::CurveSpec::CurveType::Commodity) {
        return commoditySpotShiftSize(indexName);
    } else {
        QL_FAIL("unsupported curveType, got  "
                << curveType << ". Only Equity and Commodity curves are supported for decomposition.");
    }
}

double DecomposedSensitivityStream::commoditySpotShiftSize(const std::string name) const {
    auto commShiftSizeIt = ssd_->commodityCurveShiftData().find(name);
    if (commShiftSizeIt != ssd_->commodityCurveShiftData().end()) {
        QL_REQUIRE(commShiftSizeIt->second->shiftType == ore::analytics::ShiftType::Relative,
                   "Requires a relative eqSpot shift for index decomposition");
        return commShiftSizeIt->second->shiftSize;
    } else {
        LOG("Could not find a commodity shift size for commodity index "
            << name << ". Try to find a equity spot shift size as fallback")
        return equitySpotShiftSize(name);
    }
}

std::map<std::string, std::vector<std::string>>
DecomposedSensitivityStream::getConstituentCurrencies(const std::map<std::string, double>& constituents,
                                                      const std::string& indexCurrency,
                                                      const ore::data::CurveSpec::CurveType curveType) const {
    std::map<std::string, std::vector<std::string>> results;
    for (const auto& [constituent, _] : constituents) {
        auto ccy = curveCurrency(constituent, curveType);
        if (ccy.empty()) {
            ccy = indexCurrency;
            StructuredAnalyticsErrorMessage("CRIF Generation", "Equity index decomposition",
                                            "Cannot find currency for equity " + constituent +
                                                " from curve configs, fallback to use index currency (" +
                                                indexCurrency + ")")
                .log();
        }
        if (ccy != baseCurrency_) {
            results[ccy].push_back(constituent);
        }
        
    }
    return results;
}

DecomposedSensitivityStream::IndexDecompositionResult
DecomposedSensitivityStream::indexDecomposition(double delta, const std::string& indexName,
                                                const ore::data::CurveSpec::CurveType curveType) const {
    IndexDecompositionResult result;
    std::string refDataType = curveType == ore::data::CurveSpec::CurveType::Equity ? "EquityIndex" : "CommodityIndex";

    QL_REQUIRE(refDataManager_->hasData(refDataType, indexName),
               "Cannot decompose equity index delta ("
                   << indexName << ") for trade: no reference data found. Continuing without decomposition.");

    auto refDatum = refDataManager_->getData(refDataType, indexName);
    auto indexRefDatum = QuantLib::ext::dynamic_pointer_cast<ore::data::IndexReferenceDatum>(refDatum);
    std::string indexCurrency = curveCurrency(indexName, curveType);
    std::map<string, double> indexWeights = indexRefDatum->underlyings();
    auto spotRisk = constituentSpotRiskFromDecomposition(delta, indexWeights);
    auto currencies = getConstituentCurrencies(spotRisk, indexCurrency, curveType);
    auto fxShifts = fxRiskShiftSizes(currencies);
    double spotShift = assetSpotShiftSize(indexName, curveType);    
    auto fxRisk = fxRiskFromDecomposition(spotRisk, currencies, fxShifts, spotShift);
    result.spotRisk = spotRisk;
    result.fxRisk = fxRisk;
    result.indexCurrency = indexCurrency;
    return result;
}

std::vector<SensitivityRecord>
DecomposedSensitivityStream::decomposeCurrencyHedgedIndexRisk(const SensitivityRecord& sr) const {

    auto indexName = sr.key_1.name;
    auto indexCurrency = curveCurrency(indexName, ore::data::CurveSpec::CurveType::Equity);

    QuantLib::ext::shared_ptr<ore::data::CurrencyHedgedEquityIndexDecomposition> decomposeCurrencyHedgedIndexHelper;
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

        double assetSensiShift = assetSpotShiftSize(indexName, ore::data::CurveSpec::CurveType::Equity);

        double hedgedExposure = sr.delta / assetSensiShift;

        double unhedgedExposure =
            decomposeCurrencyHedgedIndexHelper->unhedgedSpotExposure(hedgedExposure, quantity, today, todaysMarket_);

        double unhedgedDelta = unhedgedExposure * assetSensiShift;

        auto decompResults =
            indexDecomposition(unhedgedDelta, decomposeCurrencyHedgedIndexHelper->underlyingIndexName(),
                               ore::data::CurveSpec::CurveType::Equity);

        // Correct FX Delta from FxForwards
        for (const auto& [ccy, fxRisk] :
             decomposeCurrencyHedgedIndexHelper->fxSpotRiskFromForwards(quantity, today, todaysMarket_, 1.0)) {
            decompResults.fxRisk[ccy] = decompResults.fxRisk[ccy] - fxRisk * fxRiskShiftSize(ccy);
        }
        
        return sensitivityRecords(decompResults.spotRisk, decompResults.fxRisk, indexCurrency, sr);
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

void DecomposedSensitivityStream::reset() {
    ss_->reset();
    decomposedRecords_.clear();
    itCurrent_ = decomposedRecords_.begin();
}

std::vector<SensitivityRecord>
DecomposedSensitivityStream::sensitivityRecords(const std::map<std::string, double>& eqDeltas,
                                                const std::map<std::string, double>& fxDeltas,
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
