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

QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexDecomposition>
loadCurrencyHedgedIndexDecomposition(const std::string& name, const QuantLib::ext::shared_ptr<ReferenceDataManager>& refDataMgr,
                                     const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs) {
    QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum> indexRefData;
    QuantLib::ext::shared_ptr<EquityIndexReferenceDatum> underlyingRefData;
    std::map<std::string, std::pair<double, std::string>> currencyWeightsAndFxIndexNames;
    
    if (refDataMgr) {
        if (refDataMgr->hasData("CurrencyHedgedEquityIndex", name))
            indexRefData = QuantLib::ext::dynamic_pointer_cast<CurrencyHedgedEquityIndexReferenceDatum>(
                refDataMgr->getData("CurrencyHedgedEquityIndex", name));

        if (indexRefData != nullptr && refDataMgr->hasData("EquityIndex", indexRefData->underlyingIndexName()))
            underlyingRefData = QuantLib::ext::dynamic_pointer_cast<EquityIndexReferenceDatum>(
                refDataMgr->getData("EquityIndex", indexRefData->underlyingIndexName()));
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
        
        std::map<std::string, double> underlyingIndexWeightsAtRebalancing;

        if (indexRefData->currencyWeights().empty()) {
            QuantLib::ext::shared_ptr<ReferenceDatum> undIndexRefDataAtRefDate;
            try {
                undIndexRefDataAtRefDate = refDataMgr->getData("EquityIndex", underlyingIndexName, refDate);
            } catch (...) {
                // Try to load ref data, but don't throw on error
            }
            if (undIndexRefDataAtRefDate) {
                underlyingIndexWeightsAtRebalancing =
                    QuantLib::ext::dynamic_pointer_cast<EquityIndexReferenceDatum>(undIndexRefDataAtRefDate)->underlyings();
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
    return QuantLib::ext::make_shared<CurrencyHedgedEquityIndexDecomposition>(name, indexRefData, underlyingRefData,
                                                                      indexCurrency, underlyingIndexCurrency,
                                                                      fxIndexName, currencyWeightsAndFxIndexNames);
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::referenceDate(
    const QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
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
    const QuantLib::ext::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
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
    const double quantity, const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket, const double shiftsize) const {

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
        fxRisks[ccy] = shiftsize * forwardNotional * fxIndex->fixing(asof);
    }
    return fxRisks;
}

double
CurrencyHedgedEquityIndexDecomposition::unhedgedSpotExposure(double hedgedExposure, const double quantity,
                                                      const QuantLib::Date& asof,
                                                      const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket) const {
    auto indexCurve = todaysMarket->equityCurve(indexName());
    auto underlyingCurve = todaysMarket->equityCurve(underlyingIndexName());
    QuantLib::Date rebalanceDt = rebalancingDate(asof);
    auto fxIndexFamily = parseFxIndex(fxIndexName())->familyName();
    auto fxIndex = todaysMarket->fxIndex("FX-" + fxIndexFamily + "-" + underlyingIndexCurrency_ + "-" + indexCurrency_);
    double hedgedUnitPrice = (hedgedExposure / quantity);
    // In case we have a option and the unit delta isnt one
    double scaling =  hedgedUnitPrice / indexCurve->fixing(asof); 
    // Change in the fx since the last rebalacing
    double fxReturn = fxIndex->fixing(asof) / fxIndex->fixing(rebalanceDt);
    // Return of the underlying since last rebalacning
    double underlyingIndexReturn = underlyingCurve->equitySpot()->value() / underlyingCurve->fixing(rebalanceDt);
    // Unhedged price of the index 
    double unhedgedUnitPrice= indexCurve->fixing(rebalanceDt) * underlyingIndexReturn * fxReturn;
    // Unhedged exposure
    return scaling * quantity * unhedgedUnitPrice;
}

void CurrencyHedgedEquityIndexDecomposition::addAdditionalFixingsForEquityIndexDecomposition(
    const QuantLib::Date& asof, std::map<std::string, RequiredFixings::FixingDates>& fixings) const {
    if (isValid()) {
        QuantLib::Date rebalancingDt = rebalancingDate(asof);
        QuantLib::Date referenceDt = referenceDate(asof);
        fixings[IndexNameTranslator::instance().oreName(indexName())].addDate(rebalancingDt, false);
        fixings[IndexNameTranslator::instance().oreName(indexName())].addDate(referenceDt, false);

        IndexNameTranslator::instance().add(underlyingIndexName(), "EQ-" + underlyingIndexName());
        fixings[IndexNameTranslator::instance().oreName(underlyingIndexName())].addDate(rebalancingDt, false);
        fixings[IndexNameTranslator::instance().oreName(underlyingIndexName())].addDate(referenceDt, false);

        fixings[fxIndexName()].addDate(referenceDt, false);
        fixings[fxIndexName()].addDate(rebalancingDt, false);

        for (const auto& [currency, name] : currencyWeightsAndFxIndexNames()) {
            fixings[name.second].addDate(referenceDt, false);
            fixings[name.second].addDate(rebalancingDt, false);
        }
    }
}

} // namespace data
} // namespace ore
