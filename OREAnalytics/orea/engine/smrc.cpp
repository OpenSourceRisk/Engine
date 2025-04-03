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

#include <ored/marketdata/market.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/convertiblebond.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/equityposition.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/trs.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <orea/engine/smrc.hpp>

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/instruments/currencyswap.hpp>
#include <qle/instruments/fxforward.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/currency.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/utilities/dataparsers.hpp>

#include <iomanip>

using namespace std;

namespace ore {
namespace analytics {
using namespace ore::data;

SMRC::SMRC(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<Market>& market,
           const std::string& baseCcyCode, QuantLib::ext::shared_ptr<ore::data::Report> smrcReportDetail,
           QuantLib::ext::shared_ptr<ore::data::Report> smrcReportAggr)
    : portfolio_(portfolio), market_(market), baseCcyCode_(baseCcyCode), smrcReportDetail_(smrcReportDetail),
      smrcReportAggr_(smrcReportAggr) {
    tradeDetails();
    writeReports();
}

DayCounter dc = ActualActual(ActualActual::ISDA);
void SMRC::tradeDetails() {

    DLOG("SMRC: Collecting trade contributions");
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        DLOG("SMRC: Processing trade: " << tradeId);
        try {
            const bool isSupportedTradeType = find(supportedTypes_.begin(), supportedTypes_.end(),
                                                   trade->tradeType()) != supportedTypes_.end();
            if (!isSupportedTradeType) {
                StructuredTradeWarningMessage(tradeId, trade->tradeType(), "Trade will not be processed",
                                              "SMRC: Trade type is not supported")
                    .log();
                continue;
            }
            TradeData tradeDataBase;
            tradeDataBase.id = tradeId;
            tradeDataBase.type = trade->tradeType();
            tradeDataBase.nettingSet = trade->envelope().nettingSetId();
            Real npvFxRate = trade->npvCurrency().empty() ? 1.0 : getFxRate(trade->npvCurrency());
            tradeDataBase.npv = trade->instrument()->NPV() * npvFxRate;
            tradeDataBase.riskWeight = getRiskWeight(trade);
            tradeDataBase.maturityDate = trade->maturity();
            Real notionalFxRate = trade->notionalCurrency().empty() ? 1.0 : getFxRate(trade->notionalCurrency());
            Real tradeNotional = trade->notional() * notionalFxRate;

            // asset, signedNotional and id2 (if applicable)

            if (tradeDataBase.type == "FxForward") {
                auto tradeFxForward = QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
                QL_REQUIRE(tradeFxForward != nullptr, "internal error: FxForward null pointer in SMRC aggregation");

                // Bought currency
                const string& boughtCcy = tradeFxForward->boughtCurrency();
                const Real boughtNotional = tradeFxForward->boughtAmount() * getFxRate(tradeFxForward->boughtCurrency());
                if (FxForwardCcyBuckets_.find(boughtCcy) == FxForwardCcyBuckets_.end())
                    FxForwardCcyBuckets_[boughtCcy] = 0;
                FxForwardCcyBuckets_[boughtCcy] += boughtNotional;
                
                TradeData boughtCcyTradeData = tradeDataBase;
                boughtCcyTradeData.asset = boughtCcy;
                boughtCcyTradeData.signedNotional = boughtNotional;
                tradeData_.push_back(boughtCcyTradeData);

                // Sold currency
                const string& soldCcy = tradeFxForward->soldCurrency();
                const Real soldNotional = tradeFxForward->soldAmount() * getFxRate(tradeFxForward->soldCurrency());
                if (FxForwardCcyBuckets_.find(soldCcy) == FxForwardCcyBuckets_.end())
                    FxForwardCcyBuckets_[soldCcy] = 0;
                FxForwardCcyBuckets_[soldCcy] -= soldNotional;
                
                TradeData soldCcyTradeData = tradeDataBase;
                soldCcyTradeData.asset = soldCcy;
                soldCcyTradeData.signedNotional = -soldNotional;
                tradeData_.push_back(soldCcyTradeData);
            } else if (tradeDataBase.type == "FxOption") {
                auto tradeFxOption = QuantLib::ext::dynamic_pointer_cast<FxOption>(trade);
                QL_REQUIRE(tradeFxOption != nullptr, "internal error: FxOption null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;
                const string& boughtCcy = tradeFxOption->boughtCurrency();
                const string& soldCcy = tradeFxOption->soldCurrency();
                tradeData.asset = boughtCcy + soldCcy;
                tradeData.signedNotional = getFxOptionSign(boughtCcy, soldCcy, tradeFxOption->option().callPut(),
                                                           tradeFxOption->option().longShort()) *
                                           tradeNotional;

                std::set<string> ccyPair = {boughtCcy, soldCcy};
                if (FxOptionCcyPairs_.find(ccyPair) == FxOptionCcyPairs_.end())
                    FxOptionCcyPairs_[ccyPair] = 0;
                FxOptionCcyPairs_[ccyPair] += tradeData.signedNotional;
                tradeData_.push_back(tradeData);
            } else if (tradeDataBase.type == "EquityPosition") {
                auto tradeEquityPosition = QuantLib::ext::dynamic_pointer_cast<EquityPosition>(trade);
                QL_REQUIRE(tradeEquityPosition != nullptr, "internal error: EquityPosition null pointer in SMRC aggregation");
                
                std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>> indices = tradeEquityPosition->indices();

                auto addFields = tradeEquityPosition->envelope().additionalFields();
                auto it = addFields.find("smrc_notional");
                QL_REQUIRE(it != addFields.end(), "EquityPosition requires additional field smrc_notional");
                tradeNotional = parseReal(it->second);
                
                for (const auto& underlying : tradeEquityPosition->data().underlyings()) {
                    TradeData tradeData = tradeDataBase;
                    const string& indexName = underlying.name();

                    if (EquityBuckets_.find(indexName) == EquityBuckets_.end()) // include the weights
                        EquityBuckets_[indexName] = 0;

                    Real underlyingNotional = tradeNotional * underlying.weight();
                    EquityBuckets_[indexName] += underlyingNotional;

                    tradeData.asset = indexName;
                    tradeData.signedNotional = underlyingNotional;
                    tradeData_.push_back(tradeData);
                }

            } else if (tradeDataBase.type == "EquityOption") {
                auto tradeEquityOption = QuantLib::ext::dynamic_pointer_cast<EquityOption>(trade);
                QL_REQUIRE(tradeEquityOption != nullptr, "internal error: EquityOption null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;
                
                const string& equity = tradeEquityOption->equityName();
                tradeData.asset = equity;
                
                const string& longShort = tradeEquityOption->option().longShort();
                const string& putCall = tradeEquityOption->option().callPut();
                tradeData.signedNotional = getOptionSign(longShort, putCall) * tradeNotional;

                if (EquityBuckets_.find(equity) == EquityBuckets_.end())
                    EquityBuckets_[equity] = 0;
                EquityBuckets_[equity] += tradeData.signedNotional;
                
                tradeData_.push_back(tradeData);
            } else if (tradeDataBase.type == "EquityOptionPosition") {
                auto tradeEquityOptionPosition = QuantLib::ext::dynamic_pointer_cast<EquityOptionPosition>(trade);
                QL_REQUIRE(tradeEquityOptionPosition != nullptr, "internal error: EquityOptionPosition null pointer in SMRC aggregation");

                auto OptionPositionData = tradeEquityOptionPosition->data().underlyings();
                for (auto const& indices : OptionPositionData) {
                    TradeData tradeData = tradeDataBase;

                    const string& indexName = indices.underlying().name();
                    tradeData.asset = indexName;

                    const string& longShort = indices.optionData().longShort();
                    const string& putCall = indices.optionData().callPut();
                    tradeData.signedNotional = getOptionSign(longShort, putCall) * tradeNotional;

                    if (EquityBuckets_.find(indexName) == EquityBuckets_.end())
                        EquityBuckets_[indexName] = 0;
                    EquityBuckets_[indexName] += tradeData.signedNotional;

                    tradeData_.push_back(tradeData);
                }
            } else if (tradeDataBase.type == "TotalReturnSwap" || tradeDataBase.type == "ContractForDifference") {
                auto tradeTRS = QuantLib::ext::dynamic_pointer_cast<TRS>(trade);
                QL_REQUIRE(tradeTRS != nullptr, "internal error: TRS null pointer in SMRC aggregation");
                auto underlying = tradeTRS->underlying().front(); // FIXME
                int multiplier = (tradeTRS->returnData().payer()) ? 1 : -1;
                const string& underlyingTradeType = underlying->tradeType();
                if (underlyingTradeType == "EquityPosition") {
                    auto tradeEquityPosition = QuantLib::ext::dynamic_pointer_cast<EquityPosition>(underlying);
                    std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>> indices = tradeEquityPosition->indices();
                    for (auto const& index : indices) {
                        TradeData tradeData = tradeDataBase;
                        
                        const string& indexName = index->name();
                        tradeData.asset = indexName;
                        tradeData.signedNotional = multiplier * tradeNotional;
                        tradeData.id2 = "EquityPosition";
                        tradeData_.push_back(tradeData);

                        if (EquityBuckets_.find(indexName) == EquityBuckets_.end())
                            EquityBuckets_[indexName] = 0;
                        EquityBuckets_[indexName] += tradeData.signedNotional;
                    }
                } else if (underlyingTradeType == "EquityOptionPosition") {
                    auto tradeEquityOptionPosition = QuantLib::ext::dynamic_pointer_cast<EquityOptionPosition>(underlying);
                    QL_REQUIRE(tradeEquityOptionPosition != nullptr, "internal error: EquityOptionPosition null pointer in SMRC aggregation");
                    auto OptionPositionData = tradeEquityOptionPosition->data().underlyings();
                    for (auto const& indices : OptionPositionData) {
                        TradeData tradeData = tradeDataBase;

                        const string& indexName = indices.underlying().name();
                        tradeData.asset = indexName;
                        
                        const string& longShort = indices.optionData().longShort();
                        const string& putCall = indices.optionData().callPut();
                        tradeData.signedNotional = multiplier * getOptionSign(longShort, putCall) * tradeNotional;
                        tradeData.id2 = "EquityOptionPosition";
                        tradeData_.push_back(tradeData);
                        
                        if (EquityBuckets_.find(indexName) == EquityBuckets_.end())
                            EquityBuckets_[indexName] = 0;
                        EquityBuckets_[indexName] += tradeData.signedNotional;
                    }
                } else if (underlyingTradeType == "ConvertibleBond") {
                    auto tradeConvertibleBond = QuantLib::ext::dynamic_pointer_cast<ConvertibleBond>(underlying);
                    QL_REQUIRE(tradeConvertibleBond != nullptr, "internal error: ConvertibleBond null pointer in SMRC aggregation");
                    
                    TradeData tradeData = tradeDataBase;
                    
                    const string& securityId = tradeConvertibleBond->data().bondData().securityId();
                    const string& riskweight = std::to_string(tradeDataBase.riskWeight);
                    
                    tradeData.asset = securityId;
                    tradeData.signedNotional = multiplier * tradeNotional;
                    tradeData_.push_back(tradeData);

                    std::vector<string> bondMaturity = {securityId, riskweight};
                    if (BondBuckets_.find(bondMaturity) == BondBuckets_.end())
                        BondBuckets_[bondMaturity] = 0;
                    BondBuckets_[bondMaturity] += tradeData.signedNotional;
                } else if (underlyingTradeType == "Bond") {
                    auto tradeBond = QuantLib::ext::dynamic_pointer_cast<ore::data::Bond>(underlying);
                    QL_REQUIRE(tradeBond != nullptr, "internal error: Bond null pointer in SMRC aggregation");
                    TradeData tradeData = tradeDataBase;
                    
                    auto bondData = tradeBond->bondData();
                    const string& securityId = bondData.securityId();
                    const string& riskweight = std::to_string(getBondWeight(securityId, tradeData.maturityDate));

                    tradeData.asset = securityId;
                    tradeData.signedNotional = multiplier * tradeNotional;
                    tradeData.id2 = riskweight;
                    tradeData_.push_back(tradeData);

                    std::vector<string> bondMaturity = {securityId, riskweight};
                    if (BondBuckets_.find(bondMaturity) == BondBuckets_.end())
                        BondBuckets_[bondMaturity] = 0;
                    BondBuckets_[bondMaturity] += tradeData.signedNotional;
                }
            } else if (tradeDataBase.type == "CommodityForward") {
                auto tradeCommodityFwd = QuantLib::ext::dynamic_pointer_cast<CommodityForward>(trade);
                QL_REQUIRE(tradeCommodityFwd != nullptr, "internal error: CommodityFwd null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;
                const string& commodity = tradeCommodityFwd->commodityName();
                const int multiplier = (tradeCommodityFwd->position() == "Long") ? 1 : -1;

                tradeData.asset = commodity;
                tradeData.signedNotional = multiplier * tradeCommodityFwd->currentNotional() *
                                           getFxRate(tradeCommodityFwd->notionalCurrency());
                tradeData_.push_back(tradeData);

                if (CommodityBuckets_.find(commodity) == CommodityBuckets_.end())
                    CommodityBuckets_[commodity] = 0;
                CommodityBuckets_[commodity] += tradeData.signedNotional;    
            } else if (tradeDataBase.type == "CommoditySwap") {
                auto tradeCommoditySwap = QuantLib::ext::dynamic_pointer_cast<CommoditySwap>(trade);
                QL_REQUIRE(tradeCommoditySwap != nullptr, "internal error: CommoditySwap null pointer in SMRC aggregation");

                for (Size t = 0; t < tradeCommoditySwap->legData().size(); t++) {
                    const auto& l = tradeCommoditySwap->legData()[t];
                    QuantLib::ext::shared_ptr<LegAdditionalData> coml = (dynamic_pointer_cast<LegAdditionalData>(l.concreteLegData()));
                    QL_REQUIRE(coml != nullptr, "internal error: CommoditySwap Leg null pointer in SMRC aggregation");
                    if (coml->legType() == LegType::CommodityFloating) {
                        QuantLib::ext::shared_ptr<CommodityFloatingLegData> comfl = (dynamic_pointer_cast<CommodityFloatingLegData>(coml));
                        QL_REQUIRE(comfl != nullptr, "internal error: CommoditySwap FloatingLeg null pointer in SMRC aggregation");

                        TradeData tradeData = tradeDataBase;
                        const string& commodityName = comfl->name();
                        tradeData.asset = commodityName;
                        
                        Real notional = getLegAverageNotional(tradeCommoditySwap, t) * getFxRate(l.currency());
                        if (notional == Null<Real>())
                            notional = tradeNotional;
                        const Real multiplier = l.isPayer() ? 1.0 : -1.0;
                        tradeData.signedNotional = multiplier * notional;
                        tradeData_.push_back(tradeData);

                        if (CommodityBuckets_.find(commodityName) == CommodityBuckets_.end())
                            CommodityBuckets_[commodityName] = 0;
                        CommodityBuckets_[commodityName] += tradeData.signedNotional;
                    }
                }
            } else if (tradeDataBase.type == "CommodityOption") {
                auto tradeCommodityOption = QuantLib::ext::dynamic_pointer_cast<CommodityOption>(trade);
                QL_REQUIRE(tradeCommodityOption != nullptr, "internal error: CommodityOption null pointer in SMRC aggregation");
                
                const string& putCall = tradeCommodityOption->option().callPut();
                const string& longShort = tradeCommodityOption->option().longShort();
                auto underlyingIndices = tradeCommodityOption->underlyingIndices();
                set<string> assetNames;
                for (auto const& index : underlyingIndices) {
                    for (auto const& commodity : index.second) {
                        TradeData tradeData = tradeDataBase;
                        
                        const string& commodityName = commodity;
                        tradeData.asset = commodityName;
                        tradeData.signedNotional = getOptionSign(longShort, putCall) * tradeNotional;

                        if (CommodityBuckets_.find(commodityName) == CommodityBuckets_.end())
                            CommodityBuckets_[commodityName] = 0;
                        CommodityBuckets_[commodityName] += tradeData.signedNotional; 

                        tradeData_.push_back(tradeData);
                    }
                }
            } else if (tradeDataBase.type == "ConvertibleBond") {
                auto tradeConvertibleBond = QuantLib::ext::dynamic_pointer_cast<ConvertibleBond>(trade);
                QL_REQUIRE(tradeConvertibleBond != nullptr, "internal error: ConvertibleBond null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;
                
                const string& securityId = tradeConvertibleBond->data().bondData().securityId();
                const string& riskweight = std::to_string(tradeData.riskWeight);
                
                tradeData.asset = securityId;
                tradeData.signedNotional = tradeNotional;
                tradeData.id2 = riskweight;// CHECK
                tradeData_.push_back(tradeData);
                
                const std::vector<string>& bondMaturity = {securityId, riskweight};
                if (BondBuckets_.find(bondMaturity) == BondBuckets_.end())
                    BondBuckets_[bondMaturity] = 0;
                BondBuckets_[bondMaturity] += tradeData.signedNotional;
            } else if (tradeDataBase.type == "Bond") {
                auto tradeBond = QuantLib::ext::dynamic_pointer_cast<ore::data::Bond>(trade);
                QL_REQUIRE(tradeBond != nullptr, "internal error: Bond null pointer in SMRC aggregation");
                
                TradeData tradeData = tradeDataBase;

                const string& securityId = tradeBond->bondData().securityId();
                const string& riskweight = std::to_string(getBondWeight(securityId, tradeDataBase.maturityDate));
                
                tradeData.asset = securityId;
                tradeData.signedNotional = tradeNotional;
                tradeData.id2 = riskweight;// CHECK
                tradeData_.push_back(tradeData);

                const std::vector<string>& bondMaturity = {securityId, riskweight};
                if (BondBuckets_.find(bondMaturity) == BondBuckets_.end())
                    BondBuckets_[bondMaturity] = 0;
                BondBuckets_[bondMaturity] += tradeData.signedNotional;
            } else if (tradeDataBase.type == "ForwardBond") {
                auto tradeBondForward = QuantLib::ext::dynamic_pointer_cast<ForwardBond>(trade);
                QL_REQUIRE(tradeBondForward != nullptr, "internal error: ForwardBond null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;
                auto underlyingBondData = tradeBondForward->bondData();
                const string& securityId = underlyingBondData.securityId();
                const string& riskweight = std::to_string(getBondWeight(securityId, tradeData.maturityDate));
                int multiplier = (tradeBondForward->longInForward() == "true") ? 1 : -1;
                
                tradeData.asset = securityId;
                tradeData.signedNotional = multiplier * tradeNotional;
                tradeData.id2 = riskweight; // CHECK
                tradeData_.push_back(tradeData);

                const std::vector<string>& bondMaturity = {securityId, riskweight};
                if (BondBuckets_.find(bondMaturity) == BondBuckets_.end())
                    BondBuckets_[bondMaturity] = 0;
                BondBuckets_[bondMaturity] += tradeData.signedNotional;
            } else if (tradeDataBase.type == "BondOption") {
                auto tradeBondOption = QuantLib::ext::dynamic_pointer_cast<BondOption>(trade);
                QL_REQUIRE(tradeBondOption != nullptr, "internal error: BondOption null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;

                const string& putCall = tradeBondOption->optionData().callPut();
                const string& longShort = tradeBondOption->optionData().longShort();
                auto bondData = tradeBondOption->bondData();
                const string& securityId = bondData.securityId();
                const string& riskweight = std::to_string(getBondWeight(securityId, tradeData.maturityDate));

                tradeData.asset = securityId;
                tradeData.signedNotional = getOptionSign(longShort, putCall) * tradeNotional;
                tradeData.id2 = riskweight; // CHECK
                tradeData_.push_back(tradeData);

                const std::vector<string>& bondMaturity = {securityId, riskweight};
                if (BondBuckets_.find(bondMaturity) == BondBuckets_.end())
                    BondBuckets_[bondMaturity] = 0;
                BondBuckets_[bondMaturity] += tradeData.signedNotional;
            } else if (tradeDataBase.type == "Swap") {
                auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
                QL_REQUIRE(swap != nullptr, "internal error: Swap null pointer in SMRC aggregation");
                const string& riskweight = std::to_string(getSwapWeight(tradeDataBase.maturityDate));
                for (auto l : swap->legData()) {
                    if (l.legType() == LegType::Floating) {
                        QuantLib::ext::shared_ptr<FloatingLegData> fl = (dynamic_pointer_cast<FloatingLegData>(l.concreteLegData()));
                        QL_REQUIRE(fl != nullptr, "internal error: Swap FloatingLeg null pointer in SMRC aggregation");
                        
                        TradeData tradeData = tradeDataBase;
                        const string& underlyingIndex = fl->index();
                        const Real multiplier = l.isPayer() ? 1.0 : -1.0;
                        
                        tradeData.asset = underlyingIndex;
                        tradeData.signedNotional = multiplier * tradeNotional;
                        tradeData.id2 = riskweight; // CHECK
                        tradeData_.push_back(tradeData);
                        
                        const std::vector<string>& indexMaturity = {underlyingIndex, riskweight};
                        if (SwapIndexMaturity_.find(indexMaturity) == SwapIndexMaturity_.end())
                            SwapIndexMaturity_[indexMaturity] = 0;
                        SwapIndexMaturity_[indexMaturity] += tradeData.signedNotional;
                    }
                }
            } else if (tradeDataBase.type == "Swaption") {
                QuantLib::ext::shared_ptr<ore::data::Swaption> swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade);
                QL_REQUIRE(swaption != nullptr, "internal error: Swaption null pointer in SMRC aggregation");
                const Date& maturity = swaption->maturity();
                const string& riskweight = std::to_string(getSwapWeight(maturity));
                auto OptionData = swaption->optionData();
                const string& longShort = OptionData.longShort();
                for (Size t = 0; t < swaption->legData().size(); t++) {
                    const auto& l = swaption->legData()[t];
                    if (l.legType() != LegType::Floating) {
                        break;
                    } else if (l.legType() == LegType::Floating) {
                        QuantLib::ext::shared_ptr<FloatingLegData> fl = (dynamic_pointer_cast<FloatingLegData>(l.concreteLegData()));
                        QL_REQUIRE(fl != nullptr, "internal error: Swaption FloatingLeg null pointer in SMRC aggregation");

                        TradeData tradeData = tradeDataBase;
                        const string& underlyingIndex = fl->index();

                        tradeData.asset = underlyingIndex;
                        tradeData.signedNotional = getSwaptionSign(longShort, l.isPayer()) * tradeNotional;
                        tradeData.id2 = riskweight; // Check
                        tradeData_.push_back(tradeData);

                        const std::vector<string>& indexMaturity = {underlyingIndex, riskweight};
                        if (SwapIndexMaturity_.find(indexMaturity) == SwapIndexMaturity_.end())
                            SwapIndexMaturity_[indexMaturity] = 0;
                        SwapIndexMaturity_[indexMaturity] += tradeData.signedNotional;
                    }
                }
            } else if (tradeDataBase.type == "ForwardRateAgreement") {
                auto FRA = QuantLib::ext::dynamic_pointer_cast<ForwardRateAgreement>(trade);
                QL_REQUIRE(FRA != nullptr, "internal error: FRA null pointer in SMRC aggregation");

                TradeData tradeData = tradeDataBase;
                const string& riskweight = std::to_string(getSwapWeight(tradeData.maturityDate));
                auto underlyingIndex = FRA->index();

                tradeData.asset = underlyingIndex;
                tradeData.signedNotional = tradeNotional;
                tradeData_.push_back(tradeData);

                const std::vector<string>& indexMaturity = {underlyingIndex, riskweight};
                if (SwapIndexMaturity_.find(indexMaturity) == SwapIndexMaturity_.end())
                    SwapIndexMaturity_[indexMaturity] = 0;
                SwapIndexMaturity_[indexMaturity] += tradeData.signedNotional;
            } else if (tradeDataBase.type == "CapFloor") {
                auto CapFloor = QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloor>(trade);
                QL_REQUIRE(CapFloor != nullptr, "internal error: CapFloor null pointer in SMRC aggregation");
                const string& riskweight = std::to_string(getSwapWeight(tradeDataBase.maturityDate));
                const string& longShort = CapFloor->longShort();
                auto legData = CapFloor->leg();
                auto underlyingIndices = legData.indices();
                for (auto const& index : underlyingIndices) {
                    TradeData tradeData = tradeDataBase;
                    const Real multiplier = longShort == "Long" ? 1.0 : -1.0;
                    
                    tradeData.asset = index;
                    tradeData.signedNotional = multiplier * tradeNotional;
                    tradeData_.push_back(tradeData);

                    const std::vector<string>& indexMaturity = {index, riskweight};
                    if (SwapIndexMaturity_.find(indexMaturity) == SwapIndexMaturity_.end())
                        SwapIndexMaturity_[indexMaturity] = 0;
                    SwapIndexMaturity_[indexMaturity] += tradeData.signedNotional;
                }
            }
            LOG("SMRC: Trade details processed for trade " << tradeDataBase.id << " " << tradeDataBase.nettingSet);

        } catch (const std::exception& e) {
            ALOG("SMRC: trade " << tradeId << " failed to process: " << e.what());
        }
    }
    LOG("SMRC: Collecting trade contributions done");
}


Real SMRC::getLegAverageNotional(const QuantLib::ext::shared_ptr<Trade>& trade, const Size legIdx) const {
    const Date& today = Settings::instance().evaluationDate();

    if (trade->tradeType() == "CommoditySwap") {
        Real avgNotional = Null<Real>();
        // Get maximum current cash flow amount (quantity * strike, quantity * spot/forward price) across legs
        // include gearings and spreads; note that the swap is in a single currency.
        const auto& commoditySwap = QuantLib::ext::dynamic_pointer_cast<ore::data::CommoditySwap>(trade);
        if (commoditySwap->legData()[legIdx].legType() == LegType::CommodityFloating) {
            Real currentPrice = Null<Real>();
            Real totalQuantity = 0.0;
            Real countTimes = 0.0;
            for (const auto& flow : commoditySwap->legs()[legIdx]) {
                if (flow->hasOccurred(today))
                    continue;

                // pick flow with earliest payment date on this leg
                Real yearFrac = 1.0;
                auto commCashflow = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(flow);
                QL_REQUIRE(commCashflow, "Could not get CommoditySwap cashflow fixing");


                if (currentPrice == Null<Real>())
                    currentPrice = commCashflow->fixing();

                if (auto commAvgCashflow =
                        QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndexedAverageCashFlow>(flow)) {
                    const Date& startDate = commAvgCashflow->startDate();
                    const Date& endDate = commAvgCashflow->endDate();
                    yearFrac = dc.yearFraction(std::max(startDate, today), endDate);
                }

                Real gearing = commCashflow->gearing();
                Real quantity = commCashflow->quantity();
                Real spread = commCashflow->spread();

                totalQuantity += gearing * quantity * (currentPrice + spread) * yearFrac;
                countTimes += yearFrac;
            }

            avgNotional = totalQuantity;
            if (totalQuantity > 0.0 && countTimes > 0.0)
                avgNotional /= countTimes;
        }
        return avgNotional;
    } else {
        QL_FAIL("SMRC::getLegMaxNotional() invalid trade type '" << trade->tradeType() << "'");
    }
}

Real SMRC::getFxRate(const string& ccy) const {
    Real fx = 1.0;
    if (ccy != baseCcyCode_) {
        Handle<Quote> fxQuote = market_->fxRate(ccy + baseCcyCode_);
        fx = fxQuote->value();
    }
    return fx;
}

Real SMRC::getRiskWeight(const QuantLib::ext::shared_ptr<Trade>& trade) const {
    const string tradeType = trade->tradeType();
    if (tradeType == "FxForward") {
        auto tradeFxForward = QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
        QL_REQUIRE(tradeFxForward != nullptr, "internal error: FxForward null pointer in SMRC getRiskWeight");      
        if (find(majorCcys_.begin(), majorCcys_.end(), tradeFxForward->boughtCurrency()) == majorCcys_.end())
            return 0.2;
        else if (find(majorCcys_.begin(), majorCcys_.end(), tradeFxForward->soldCurrency()) == majorCcys_.end())
            return 0.2;
        else
            return 0.06;
    } else if (tradeType == "FxOption") {
        auto tradeFxOption = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade);
        QL_REQUIRE(tradeFxOption != nullptr, "internal error: FxOption null pointer in SMRC getRiskWeight");  
        if (find(majorCcys_.begin(), majorCcys_.end(), tradeFxOption->boughtCurrency()) == majorCcys_.end())
            return 0.2;
        else if (find(majorCcys_.begin(), majorCcys_.end(), tradeFxOption->soldCurrency()) == majorCcys_.end())
            return 0.2;
        else
            return 0.06;
    } else if (tradeType == "CommodityForward" || tradeType == "CommoditySwap" || tradeType == "CommodityOption")
        return 0.2;
    else if (tradeType == "EquityOption" || tradeType == "EquityPosition" || tradeType == "EquityOptionPosition")
        return 0.25;
    else if (tradeType == "TotalReturnSwap" || tradeType == "ContractForDifference") {
        auto tradeTRS = QuantLib::ext::dynamic_pointer_cast<ore::data::TRS>(trade);
        QL_REQUIRE(tradeTRS != nullptr, "internal error: TRS null pointer in SMRC getRiskWeight");  
        auto underlying = tradeTRS->underlying().front(); // FIXME
        string underlyingTradeType = underlying->tradeType();
        if (underlyingTradeType == "EquityPosition" || underlyingTradeType == "EquityOptionPosition")
            return 0.25;
        else if (underlyingTradeType == "ConvertibleBond")
            return 0.15;
        else if (underlyingTradeType == "Bond") {
            auto tradeBond = QuantLib::ext::dynamic_pointer_cast<ore::data::Bond>(underlying);
            QL_REQUIRE(tradeBond != nullptr, "internal error: Bond null pointer in SMRC getRiskWeight");  
            auto bondData = tradeBond->bondData();
            Date maturityDate = underlying->maturity();
            string securityId = bondData.securityId();
            return getBondWeight(securityId, maturityDate);
        } else
            return 0;
    } else if (tradeType == "Bond") {
        auto tradeBond = QuantLib::ext::dynamic_pointer_cast<ore::data::Bond>(trade);
        QL_REQUIRE(tradeBond != nullptr, "internal error: Bond null pointer in SMRC getRiskWeight");  
        auto bondData = tradeBond->bondData();
        Date maturityDate = trade->maturity();
        string securityId = bondData.securityId();
        return getBondWeight(securityId, maturityDate);
    } else if (tradeType == "ForwardBond") {
        auto tradeBond = QuantLib::ext::dynamic_pointer_cast<ore::data::ForwardBond>(trade);
        QL_REQUIRE(tradeBond != nullptr, "internal error: ForwardBond null pointer in SMRC getRiskWeight");  
        auto bondData = tradeBond->bondData();
        Date maturityDate = trade->maturity();
        string securityId = bondData.securityId();
        return getBondWeight(securityId, maturityDate);
    } else if (tradeType == "BondOption") {
        auto tradeBondOption = QuantLib::ext::dynamic_pointer_cast<ore::data::BondOption>(trade);
        QL_REQUIRE(tradeBondOption != nullptr, "internal error: BondOption null pointer in SMRC getRiskWeight");  
        Date maturityDate = trade->maturity();
        auto bondData = tradeBondOption->bondData();
        string securityId = bondData.securityId();
        return getBondWeight(securityId, maturityDate);
    } else if (tradeType == "ConvertibleBond")
        return 0.15;
    else if (tradeType == "Swap") {
        auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
        QL_REQUIRE(swap != nullptr, "internal error: FxForward null pointer in SMRC getRiskWeight");  
        Date maturityDate = trade->maturity();
        for (auto l : swap->legData()) {
            if (l.legType() == LegType::Floating)
                return getSwapWeight(maturityDate);
        }
        return 0;
    } else if (tradeType == "ForwardRateAgreement" || tradeType == "CapFloor" || tradeType == "Swaption") {
        Date maturityDate = trade->maturity();
        return getSwapWeight(maturityDate);
    }
    return 0;
}

Real SMRC::getFxOptionSign(const string& ccyA, const string& ccyB, const string& putCall, const string& longShort) const {
    string ccy1, ccy2;
    string key;
    const string& ccyAUpper = boost::to_upper_copy(ccyA);
    const string& ccyBUpper = boost::to_upper_copy(ccyB);
    const string& putCallUpper = boost::to_upper_copy(putCall);
    const string& longShortUpper = boost::to_upper_copy(longShort);
    if (ccyAUpper <= ccyBUpper) {
        ccy1 = ccyAUpper;
        ccy2 = ccyBUpper;
    } else {
        ccy1 = ccyBUpper;
        ccy2 = ccyAUpper;
    }
    key = ccy1 + '_' + ccy2 + '_' + putCallUpper + '_' + longShortUpper;
    map<string, QuantLib::Real> signMap = {
        {ccy1 + '_' + ccy2 + "_CALL_LONG", +1}, {ccy1 + '_' + ccy2 + "_CALL_SHORT", -1},
        {ccy2 + '_' + ccy1 + "_CALL_LONG", -1}, {ccy2 + '_' + ccy1 + "_CALL_SHORT", +1},
        {ccy1 + '_' + ccy2 + "_PUT_LONG", -1},  {ccy1 + '_' + ccy2 + "_PUT_SHORT", +1},
        {ccy2 + '_' + ccy1 + "_PUT_LONG", +1},  {ccy2 + '_' + ccy1 + "_PUT_SHORT", -1},
    };
    return signMap[key];
}

Real SMRC::getSwaptionSign(const string& longShort, const bool FloatingPayer) const {
    const string& longShortLower = boost::to_upper_copy(longShort);
    string FloatingPayerReceiver;
    string key;
    if (FloatingPayer) {
        FloatingPayerReceiver = "PAYER";
    } else {
        FloatingPayerReceiver = "RECEIVER";
    }
    key = FloatingPayerReceiver + '_' + longShortLower;
    map<string, QuantLib::Real> signMap = {
        {"PAYER_LONG", +1}, {"PAYER_SHORT", -1},
        {"RECEIVER_LONG", -1}, {"RECEIVER_SHORT", +1},
    };
    return signMap[key];
}

Real SMRC::getOptionSign(const string& longShort, const string& putCall) const {
    const string& longShortUpper = boost::to_upper_copy(longShort);
    const string& putCallUpper = boost::to_upper_copy(putCall);
    string key;
    key = putCallUpper + '_' + longShortUpper;
    map<string, QuantLib::Real> signMap = {
        {"CALL_LONG", +1},
        {"CALL_SHORT", -1},
        {"PUT_LONG", -1},
        {"PUT_SHORT", +1},
    };
    return signMap[key];
}

Real SMRC::getBondWeight(const string& underlyingISIN, const Date& maturityDate) const { 
    Time timeUntilMaturity = dc.yearFraction(Settings::instance().evaluationDate(), maturityDate); 
    if (boost::algorithm::contains(underlyingISIN, "ISIN:US9128")) {
        if (timeUntilMaturity < 5)
            return 0.015;
        else if (timeUntilMaturity < 10)
            return 0.025;
        else if (timeUntilMaturity < 15)
            return 0.0275;
        else
            return 0.03;
    } 
    else {
        if (timeUntilMaturity < 1)
            return 0.02;
        else if (timeUntilMaturity < 2)
            return 0.03;
        else if (timeUntilMaturity < 3)
            return 0.05;
        else if (timeUntilMaturity < 5)
            return 0.06;
        else if (timeUntilMaturity < 10)
            return 0.07;
        else if (timeUntilMaturity < 15)
            return 0.075;
        else if (timeUntilMaturity < 20)
            return 0.08;
        else if (timeUntilMaturity < 25)
            return 0.085;
        else
            return 0.09;
    }
    return 0;
}

Real SMRC::getSwapWeight(const Date& maturityDate) const {
    Time timeUntilMaturity = dc.yearFraction(Settings::instance().evaluationDate(), maturityDate);
    if (timeUntilMaturity < 0.25)
        return 0;
    else if (timeUntilMaturity < 0.5)
        return 0.005;
    else if (timeUntilMaturity < 0.75)
        return 0.0075;
    else if (timeUntilMaturity < 1)
        return 0.01;
    else if (timeUntilMaturity < 2)
        return 0.015;
    else if (timeUntilMaturity < 3)
        return 0.02;
    else if (timeUntilMaturity < 5)
        return 0.03;
    else if (timeUntilMaturity < 10)
        return 0.04;
    else if (timeUntilMaturity < 15)
        return 0.045;
    else if (timeUntilMaturity < 20)
        return 0.05;
    else if (timeUntilMaturity < 25)
        return 0.055;
    else
        return 0.06;
}

void SMRC::writeReports() {

    LOG("SMRC: writing detail report");
    smrcReportDetail_->addColumn("TradeId", string())
        .addColumn("TradeType", string())
        .addColumn("NettingSet", string())
        .addColumn("Asset", string())
        .addColumn("MaturityDate", string())
        .addColumn("Maturity", double(), 6)
        .addColumn("NPV", double(), 2)
        .addColumn("SignedNotional", double(), 2)
        .addColumn("RiskWeight", double(), 4);

    for (Size i = 0; i < tradeData_.size(); i++)
        smrcReportDetail_->next()
            .add(tradeData_[i].id)
            .add(tradeData_[i].type)
            .add(tradeData_[i].nettingSet)
            .add(tradeData_[i].asset)
            .add(ore::data::to_string(tradeData_[i].maturityDate))
            .add(dc.yearFraction(Settings::instance().evaluationDate(), tradeData_[i].maturityDate))
            .add(tradeData_[i].npv)
            .add(tradeData_[i].signedNotional)
            .add(tradeData_[i].riskWeight);

    smrcReportDetail_->end();

    LOG("SMRC: writing aggregated report");
    smrcReportAggr_->addColumn("TradeType", string())
        .addColumn("Asset", string())
        .addColumn("RiskWeight", double(), 4)
        .addColumn("SignedNotional", double(), 2)
        .addColumn("SMRC", double(), 2);

    // Process FxForward
    for (auto const& ccy : FxForwardCcyBuckets_) {
        double riskWeight;
        if (find(majorCcys_.begin(), majorCcys_.end(), ccy.first) == majorCcys_.end())
            riskWeight = 0.2;
        else
            riskWeight = 0.06;
        smrcReportAggr_->next()
            .add("FxForward")
            .add(ccy.first)
            .add(riskWeight)
            .add(ccy.second)
            .add(riskWeight * fabs(ccy.second));
    }

    // Process FxOption
    for (auto const& ccyPair : FxOptionCcyPairs_) {
        double riskWeight = 0.06;
        for (auto const& ccy : ccyPair.first) {
            if (find(majorCcys_.begin(), majorCcys_.end(), ccy) == majorCcys_.end()) {
                riskWeight = 0.2;
                break;
            }
        }
        string ccyPairLabel = "";
        for (auto const& ccy : ccyPair.first)
            ccyPairLabel += ccy;
        smrcReportAggr_->next()
            .add("FxOption")
            .add(ccyPairLabel)
            .add(riskWeight)
            .add(ccyPair.second)
            .add(riskWeight * fabs(ccyPair.second));
    }
    // Process EquityUnderlyings
    for (auto const& Equity : EquityBuckets_) {
        double riskWeight = 0.25;
        smrcReportAggr_->next()
            .add("Equity")
            .add(Equity.first)
            .add(riskWeight)
            .add(Equity.second)
            .add(riskWeight * fabs(Equity.second));
    }
    // Process CommodityUnderlyings
    for (auto const& Commodity : CommodityBuckets_) {
        double riskWeight = 0.2;
        smrcReportAggr_->next()
            .add("Commodity")
            .add(Commodity.first)
            .add(riskWeight)
            .add(Commodity.second)
            .add(riskWeight * fabs(Commodity.second));
    }
    // Process BondUnderlyings
    for (auto const& BondMaturity : BondBuckets_) {
        auto bondMaturityValue = BondMaturity.first;
        string index = bondMaturityValue.front();
        float riskWeight = std::stof(bondMaturityValue.back());
        smrcReportAggr_->next()
            .add("Bond")
            .add(index)
            .add(riskWeight)
            .add(BondMaturity.second)
            .add(riskWeight * fabs(BondMaturity.second));
    }
    // Process SwapUnderlyings
    for (auto const& indexMaturity : SwapIndexMaturity_) {
        auto indexMaturityValue = indexMaturity.first;
        string index = indexMaturityValue.front();
        float riskWeight = std::stof(indexMaturityValue.back());
        smrcReportAggr_->next()
            .add("Swap")
            .add(index)
            .add(riskWeight)
            .add(indexMaturity.second)
            .add(riskWeight * fabs(indexMaturity.second));
    }
    smrcReportAggr_->end();
}

// namespace analytics

} // namespace analytics

} // namespace oreplus
