/*
  Copyright (C) 2023 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file orepsimm/ored/portfolio/equityindexdecomposition.hpp
    \brief Helper function used for the index decompositon
 */
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/currencyhedgedequityindexdecomposition.hpp>

namespace ore {
namespace data {

boost::shared_ptr<CurrencyHedgedEquityIndexDecomposition>
loadCurrencyHedgedIndexDecomposition(const std::string& name, const boost::shared_ptr<ReferenceDataManager>& refDataMgr,
                                     const boost::shared_ptr<CurveConfigurations>& curveConfigs) {
    boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum> indexRefData;
    boost::shared_ptr<EquityIndexReferenceDatum> underlyingRefData;
    std::map<std::string, std::pair<double, std::string>> currencyWeightsAndFxIndexNames;
    
    if (refDataMgr) {
        try {
            indexRefData = boost::dynamic_pointer_cast<CurrencyHedgedEquityIndexReferenceDatum>(
                refDataMgr->getData("CurrencyHedgedEquityIndex", name));
        } catch (...) {
            // Index not a CurrencyHedgedEquityIndex or referencedata is missing, don't throw here
        }

        if (indexRefData != nullptr) {
            try {
                underlyingRefData = boost::dynamic_pointer_cast<EquityIndexReferenceDatum>(
                    refDataMgr->getData("EquityIndex", indexRefData->underlyingIndexName()));
            } catch (...) {
                // referencedata is missing, but don't throw here, return null ptr
            }
        }
    }

    if (indexRefData == nullptr || underlyingRefData == nullptr) {
        return nullptr;
    }

    // Load currency Weights
    std::map<std::string, double> currencyWeights;
    std::string indexCurrency;
    std::string underlyingIndexCurrency;
    std::string fxIndexName;
    
    // Get currency hedged index currency
    if (curveConfigs && curveConfigs->hasEquityCurveConfig(indexRefData->id())) {
        indexCurrency = curveConfigs->equityCurveConfig(indexRefData->id())->currency();
    } else {
        WLOG("Can not find curveConfig for " << indexRefData->id() << " and can not determine the index currecy");
        return nullptr;
    }

    
    if (curveConfigs && curveConfigs->hasEquityCurveConfig(indexRefData->underlyingIndexName())) {
        
        // Get Fx Index to convert UnderlyingIndexCCY into HedgedIndexCurrency
        std::string underlyingIndexName = indexRefData->underlyingIndexName();
        underlyingIndexCurrency =
            curveConfigs->equityCurveConfig(indexRefData->underlyingIndexName())->currency();
        
        auto fxIndexIt = indexRefData->fxIndexes().find(underlyingIndexCurrency);
        if (fxIndexIt != indexRefData->fxIndexes().end()) {
            fxIndexName = fxIndexIt->second;
        } else {
            fxIndexName = "FX-GENERIC-" + indexCurrency + "-" + underlyingIndexCurrency;
        }

        // Load the currency weights at referenceDate for hedge notionals 
        QuantLib::Date refDate =
            CurrencyHedgedEquityIndexDecomposition::referenceDate(indexRefData, Settings::instance().evaluationDate());
        
        std::vector<std::pair<std::string, double>> underlyingIndexWeightsAtRebalancing;

        if (indexRefData->currencyWeights().empty()) {
            boost::shared_ptr<ReferenceDatum> undIndexRefDataAtRefDate;
            try {
                undIndexRefDataAtRefDate = refDataMgr->getData("EquityIndex", underlyingIndexName, refDate);
            } catch (...) {
                // Try to load ref data, but don't throw on error
            }
            if (undIndexRefDataAtRefDate) {
                underlyingIndexWeightsAtRebalancing =
                    boost::dynamic_pointer_cast<EquityIndexReferenceDatum>(undIndexRefDataAtRefDate)->underlyings();
            }
        } else {
            underlyingIndexWeightsAtRebalancing = indexRefData->currencyWeights();
        }
        
        if (underlyingIndexWeightsAtRebalancing.empty()) {
            currencyWeights[underlyingIndexCurrency] = 1.0;
        } else {
            for (const auto& [name, weight] : underlyingIndexWeightsAtRebalancing) {
                // try look up currency in reference data and add if FX delta risk if necessary
                if (curveConfigs->hasEquityCurveConfig(name)) {
                    auto ecc = curveConfigs->equityCurveConfig(name);
                    auto eqCcy = ecc->currency();
                    currencyWeights[eqCcy] += weight;
                } else {
                    // Assume Index currency
                    currencyWeights[underlyingIndexCurrency] += weight;
                }
            }
        }
    }

    for (const auto& [currency, weight] : currencyWeights) {
        if (currency != indexCurrency) {
            auto defaultIndexIt = indexRefData->fxIndexes().find(currency);
            if (defaultIndexIt != indexRefData->fxIndexes().end()) {
                currencyWeightsAndFxIndexNames[currency] = std::make_pair(weight, defaultIndexIt->second);
            } else {
                currencyWeightsAndFxIndexNames[currency] =
                    std::make_pair(weight, "FX-GENERIC-" + indexCurrency + "-" + currency);
            }
        }
    }
    return boost::make_shared<CurrencyHedgedEquityIndexDecomposition>(name, indexRefData, underlyingRefData,
                                                                      indexCurrency, underlyingIndexCurrency,
                                                                      fxIndexName, currencyWeightsAndFxIndexNames);
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::referenceDate(
    const boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
                                                      const QuantLib::Date& asof) {
    QuantLib::Date hedgingDate = CurrencyHedgedEquityIndexDecomposition::rebalancingDate(refData, asof);
    if (hedgingDate == QuantLib::Date()) {
        return QuantLib::Date();
    } else {
        return refData->hedgeCalendar().advance(hedgingDate, -refData->referenceDateOffset() * QuantLib::Days,
                                                QuantLib::Preceding);
    }
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::rebalancingDate(
    const boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
                                                        const QuantLib::Date& asof) {
    if (refData->rebalancingStrategy() ==
        ore::data::CurrencyHedgedEquityIndexReferenceDatum::RebalancingDate::EndOfMonth) {
        QuantLib::Date lastBusinessDayOfCurrentMonth = QuantLib::Date::endOfMonth(asof);
        lastBusinessDayOfCurrentMonth =
            refData->hedgeCalendar().adjust(lastBusinessDayOfCurrentMonth, QuantLib::Preceding);
        if (asof == lastBusinessDayOfCurrentMonth) {
            return asof;
        } else {
            return refData->hedgeCalendar().advance(QuantLib::Date(1, asof.month(), asof.year()), -1 * QuantLib::Days,
                                                    QuantLib::Preceding);
        }
    }
    return QuantLib::Date();
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::referenceDate(const QuantLib::Date& asof) const {
    return CurrencyHedgedEquityIndexDecomposition::referenceDate(this->indexRefData(), asof);
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::rebalancingDate(const QuantLib::Date& asof) const {
    return CurrencyHedgedEquityIndexDecomposition::rebalancingDate(this->indexRefData(), asof);
}

std::map<std::string, double> CurrencyHedgedEquityIndexDecomposition::fxSpotRiskFromForwards(
    const double quantity, const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& todaysMarket) const {

    std::map<std::string, double> fxRisks;
    auto indexCurve = todaysMarket->equityCurve(indexName());
    auto underlyingCurve = todaysMarket->equityCurve(underlyingIndexName());
    QuantLib::Date refDate = referenceDate(asof);
    double adjustmentFactor = 1.0;
    // If adjustement is daily, the fxForward notional will be adjusted by the relative return of the underlying index
    if (indexRefData_->hedgeAdjustmentRule() == CurrencyHedgedEquityIndexReferenceDatum::HedgeAdjustment::Daily) {
        adjustmentFactor = underlyingCurve->fixing(asof) / underlyingCurve->fixing(rebalancingDate(asof));
    }
    // Compute notionals and fxSpotRisks
    for (const auto& [ccy, weightAndIndex] : currencyWeightsAndFxIndexNames()) {
        double weight = weightAndIndex.first;
        auto fxIndexFamily = parseFxIndex(fxIndexName())->familyName();
        auto fxIndex = todaysMarket->fxIndex("FX-" + fxIndexFamily + "-" + underlyingIndexCurrency_ + "-" + indexCurrency_);
        double forwardNotional =
            quantity * adjustmentFactor * weight * indexCurve->fixing(refDate) / fxIndex->fixing(refDate);
        fxRisks[ccy] = 0.01 * forwardNotional * fxIndex->fixing(asof);
    }
    return fxRisks;
}

double
CurrencyHedgedEquityIndexDecomposition::unhedgedDelta(double hedgedDelta, const double quantity,
                                                      const QuantLib::Date& asof,
                                                      const boost::shared_ptr<ore::data::Market>& todaysMarket) const {
    auto indexCurve = todaysMarket->equityCurve(indexName());
    auto underlyingCurve = todaysMarket->equityCurve(underlyingIndexName());
    QuantLib::Date rebalanceDt = rebalancingDate(asof);
    auto fxIndexFamily = parseFxIndex(fxIndexName())->familyName();
    auto fxIndex = todaysMarket->fxIndex("FX-" + fxIndexFamily + "-" + underlyingIndexCurrency_ + "-" + indexCurrency_);
    // In case we have a option unit delta isnt one
    double scaling = (hedgedDelta * 100 / quantity) / indexCurve->fixing(asof); 
    double fxReturn = fxIndex->fixing(asof) / fxIndex->fixing(rebalanceDt);
    double underlyingIndexReturn = underlyingCurve->equitySpot()->value() / underlyingCurve->fixing(rebalanceDt);
    double unhedgedDelta= indexCurve->fixing(rebalanceDt) * underlyingIndexReturn * fxReturn;
    return scaling * 0.01 * quantity * unhedgedDelta;
}

void CurrencyHedgedEquityIndexDecomposition::addAdditionalFixingsForEquityIndexDecomposition(
    const QuantLib::Date& asof, std::map<std::string, std::set<QuantLib::Date>>& fixings) const {
    if (isValid()) {
        QuantLib::Date rebalancingDt = rebalancingDate(asof);
        QuantLib::Date referenceDt = referenceDate(asof);
        fixings[IndexNameTranslator::instance().oreName(indexName())].insert(rebalancingDt);
        fixings[IndexNameTranslator::instance().oreName(indexName())].insert(referenceDt);

        IndexNameTranslator::instance().add(underlyingIndexName(), "EQ-" + underlyingIndexName());
        fixings[IndexNameTranslator::instance().oreName(underlyingIndexName())].insert(rebalancingDt);
        fixings[IndexNameTranslator::instance().oreName(underlyingIndexName())].insert(referenceDt);

        fixings[fxIndexName()].insert(referenceDt);
        fixings[fxIndexName()].insert(rebalancingDt);

        for (const auto& [currency, name] : currencyWeightsAndFxIndexNames()) {
            fixings[name.second].insert(referenceDt);
            fixings[name.second].insert(rebalancingDt);
        }
    }
}

} // namespace data
} // namespace ore
