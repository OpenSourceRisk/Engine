/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/simm/portfoliomodifier.hpp>

#include <boost/range/join.hpp>

#include <orea/simm/simmconfiguration.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/simm/simmtradedata.hpp>
#include <orea/simm/utilities.hpp>

#include <ored/portfolio/callableswap.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/crosscurrencyswap.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

using ore::data::CashflowData;
using ore::data::EngineFactory;
using ore::data::FxForward;
using ore::data::FxSwap;
using ore::data::LegAdditionalData;
using ore::data::LegData;
using ore::data::LegType;
using ore::data::Portfolio;
using ore::data::to_string;
using ore::data::parseBool;
using ore::data::Trade;
using ore::analytics::CrifRecord;
using QuantExt::FXLinkedCashFlow;
using namespace QuantLib;
using std::string;
using std::vector;

namespace {

// Utility method to copy LegData
LegData copyLegData(const LegData& ld, bool initialExchange = false, bool finalExchange = false) {
    return LegData(ld.concreteLegData(), ld.isPayer(), ld.currency(), ld.schedule(), ld.dayCounter(), ld.notionals(),
                   ld.notionalDates(), ld.paymentConvention(), initialExchange, finalExchange, false,
                   ld.isNotResetXCCY(), ld.foreignCurrency(), ld.foreignAmount(), ld.resetStartDate(), ld.fxIndex(),
                   ld.amortizationData(), ld.paymentLag());
}

} // namespace

namespace ore {
namespace analytics {

std::pair<std::set<std::string>, std::set<std::string>>
applySimmExemptions(Portfolio& portfolio, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                    const set<CrifRecord::Regulation>& simmExemptionOverrides) {
    LOG("Start applying SIMM exemptions to the portfolio");

    // Collect portfolio regulation details 
    map<NettingSetDetails, set<CrifRecord::Regulation>> regs;
    map<string, set<CrifRecord::Regulation>> regCache;
    for (auto& [tradeId, trade] : portfolio.trades()) {
        const auto& addFields = trade->envelope().additionalFields();
        const auto& nsd = trade->envelope().nettingSetDetails();
        if (addFields.find("collect_regulations") != addFields.end()) {
            const string& collectRegsStr = addFields.at("collect_regulations");
            set<CrifRecord::Regulation> collectRegs;
            if (regCache.find(collectRegsStr) == regCache.end())
                regCache[collectRegsStr] = parseRegulationString(collectRegsStr);
            collectRegs = regCache.at(collectRegsStr);
            regs[nsd].insert(collectRegs.begin(), collectRegs.end());
        }
        if (addFields.find("post_regulations") != addFields.end()) {
            const string& postRegsStr = addFields.at("post_regulations");
            set<CrifRecord::Regulation> postRegs;
            if (regCache.find(postRegsStr) == regCache.end())
                regCache[postRegsStr] = parseRegulationString(postRegsStr);
            postRegs = regCache.at(postRegsStr);
            regs[nsd].insert(postRegs.begin(), postRegs.end());
        }
    }
    
    std::set<std::string> removedTrades, modifiedTrades;
    vector<QuantLib::ext::shared_ptr<Trade>> newTrades;

    const bool hasSimmExemptionOverrides = simmExemptionOverrides.size() > 0;

    for (auto& [tradeId,trade] : portfolio.trades()) {

        auto& additionalFields = trade->envelope().additionalFields();
        set<CrifRecord::Regulation> collectRegs, postRegs;
        if (additionalFields.find("collect_regulations") != additionalFields.end())
            collectRegs = parseRegulationString(additionalFields.at("collect_regulations"));
        if (additionalFields.find("post_regulations") != additionalFields.end())
            postRegs = parseRegulationString(additionalFields.at("post_regulations"));

        if (trade->tradeType() == "FxForward") {
            // Mark physically settled FX forwards for removal
            QuantLib::ext::shared_ptr<FxForward> fxForward = QuantLib::ext::dynamic_pointer_cast<FxForward>(trade);
            if (fxForward->settlement() == "Physical") {
                if (!ore::data::isPseudoCurrency(fxForward->boughtCurrency()) &&
                    !ore::data::isPseudoCurrency(fxForward->soldCurrency())) {
                    removedTrades.insert(fxForward->id());
                    string msg = "Physically settled FX Forward will be removed";

                    // Raise SIMM exemption warnings only for regulations that are not overriden.
                    // If all regs are overriden so that no exemption is applied, then no need for a structured message
                    bool isFullyOverriden = false;
                    if (hasSimmExemptionOverrides) {
                        set<CrifRecord::Regulation> exemptCollectRegs = removeRegulations(collectRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptPostRegs = removeRegulations(postRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptRegs(exemptCollectRegs);
                        exemptRegs.insert(exemptPostRegs.begin(), exemptPostRegs.end());
                        exemptRegs.erase(CrifRecord::Regulation::Excluded);

                        // If no more regulations are considered exempt, then they are all SIMM exemption overrides
                        if (exemptRegs.empty())
                            isFullyOverriden = true;
                        else
                            msg += " for the following regulations: " + regulationsToString(exemptRegs);
                    }
                    const auto& nsdRegs = regs[trade->envelope().nettingSetDetails()];
                    if (!isFullyOverriden || nsdRegs.empty()) {
                        auto subFields = std::map<string, string>({{"tradeId", fxForward->id()}});
                        ore::analytics::StructuredAnalyticsWarningMessage("SIMM", "SIMM exemptions", msg, subFields)
                            .log();
                    }
                }
            }
        } else if (trade->tradeType() == "FxSwap") {
            // Mark physically settled FX swap for removal
            QuantLib::ext::shared_ptr<FxSwap> fxSwap = QuantLib::ext::dynamic_pointer_cast<FxSwap>(trade);
            if (fxSwap->settlement() == "Physical") {
                if (!ore::data::isPseudoCurrency(fxSwap->nearBoughtCurrency()) &&
                    !ore::data::isPseudoCurrency(fxSwap->nearSoldCurrency())) {
                    removedTrades.insert(fxSwap->id());
                    string msg = "Physically settled FX Swap will be removed";

                    // Raise SIMM exemption warnings only for regulations that are not overriden.
                    // If all regs are overriden so that no exemption is applied, then no need for a structured message
                    bool isFullyOverriden = false;
                    if (hasSimmExemptionOverrides) {
                        set<CrifRecord::Regulation> exemptCollectRegs = removeRegulations(collectRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptPostRegs = removeRegulations(postRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptRegs(exemptCollectRegs);
                        exemptRegs.insert(exemptPostRegs.begin(), exemptPostRegs.end());
                        exemptRegs.erase(CrifRecord::Regulation::Excluded);

                        // If no more regulations are considered exempt, then they are all SIMM exemption overrides
                        if (exemptRegs.empty())
                            isFullyOverriden = true;
                        else
                            msg += " for the following regulations: " + regulationsToString(exemptRegs);
                    }
                    const auto& nsdRegs = regs[trade->envelope().nettingSetDetails()];
                    if (!isFullyOverriden || nsdRegs.empty()) {
                        auto subFields = std::map<string, string>({{"tradeId", fxSwap->id()}});
                        ore::analytics::StructuredAnalyticsWarningMessage("SIMM", "SIMM exemptions", msg, subFields)
                            .log();
                    }
                }
            }

        } else if (trade->tradeType() == "Swap" || trade->tradeType() == "CrossCurrencySwap") {
            QuantLib::ext::shared_ptr<ore::data::Swap> swap;
            vector<LegData> legData;
            if (trade->tradeType() == "Swap") {
                // Deal with cross currency swaps
                swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
                legData = swap->legData();
            } else {
                swap = QuantLib::ext::dynamic_pointer_cast<ore::data::CrossCurrencySwap>(trade);
                legData = swap->legData();
            }

            // Check if the swap has multiple currencies
            bool hasNonVanillaLeg = false;
            map<string, vector<Size>> legCcys;
            for (Size i = 0; i < legData.size(); i++) {
                const LegData& ld = legData[i];
                const string& ccy = ld.currency();

                if (ld.legType() != LegType::Cashflow && ld.legType() != LegType::Fixed &&
                    ld.legType() != LegType::Floating) {
                    hasNonVanillaLeg = true;
                    break;
                }

                if (ld.legType() == LegType::Fixed || ld.legType() == LegType::Floating)
                    legCcys[ccy].push_back(i);
            }

            // Inflation, CMS, etc. - non-vanilla IR coupon types do not qualify for SIMM exemptions
            if (hasNonVanillaLeg)
                continue;

            // If not cross currency or there are fewer than 2 legs
            if (legCcys.size() != 2)
                continue;

            // If non-deliverable (i.e. cash settlement), we can continue to the next trade
            if (swap->settlement() != "Physical")
                continue;

            // Check that all legs in a given ccy are in the same direction (payer, receiver)
            bool legsSameDirection = true;
            for (const auto& [ccy, legIdxs] : legCcys) {
                std::size_t idx = legIdxs[0];
                if (!std::all_of(legIdxs.begin(), legIdxs.end(),
                                 [&legData, idx](Size i) { return legData[i].isPayer() == legData[idx].isPayer(); })) {
                    legsSameDirection = false;
                }
            }
            if (!legsSameDirection)
                continue;

            // If cross currency, but after converting "unidade" to standard ccys reduce to one ccy, do not apply
            // exemptions, see ISDA FAQ E2 (e.g. CLF / CLP xccy swaps do not qualify for exemptions)
            std::set<std::string> stdCcys;
            for (auto const& d : legData)
                stdCcys.insert(isUnidadeCurrency(d.currency()) ? simmStandardCurrency(d.currency()) : d.currency());
            if (stdCcys.size() <= 1)
                continue;

            // Get list of legs with notional exchanges
            map<string, vector<Size>> legNotionalIdx;
            bool hasResettingLeg = false;
            for (const auto& [ccy, legIdxs] : legCcys) {
                for (const Size legIdx : legIdxs) {
                    if (legData[legIdx].notionalInitialExchange() || legData[legIdx].notionalAmortizingExchange() ||
                        legData[legIdx].notionalFinalExchange())
                        legNotionalIdx[ccy].push_back(legIdx);

                    if (!legData[legIdx].isNotResetXCCY())
                        hasResettingLeg = true;
                }
            }
            bool hasNotionalFlows = legNotionalIdx.size() == 2 && (legNotionalIdx.begin()->second.size() == 1 &&
                                                                   std::prev(legNotionalIdx.end())->second.size() == 1);

            // SIMM exemptions do not apply if the notional flows come from the same ccy
            // (and hence going in the same direction) or if only one ccy has notional flows
            // unless there is at least one resetting leg
            if (!hasNotionalFlows && !hasResettingLeg)
                continue;

            // Populate idx vector for easier access to the 2 legs
            vector<Size> legDataIdx;
            if (hasNotionalFlows) {
                // We assume a maximum of 1 notional leg per ccy (hence a maximum of 2 notional legs in the swap)
                bool hasMultipleNotionalLegs = false;
                for (const auto& [ccy, legIdxs] : legNotionalIdx) {
                    if (legIdxs.size() > 1)
                        hasMultipleNotionalLegs = true;
                }
                if (hasMultipleNotionalLegs)
                    continue;

                for (const auto& [ccy, legIdxs] : legNotionalIdx)
                    legDataIdx.push_back(*legIdxs.begin());
            } else if (hasResettingLeg) {
                for (const auto& [ccy, legIdxs] : legCcys) {
                    for (Size legIdx : legIdxs) {
                        // We want to choose the resetting leg for that ccy
                        if (!legData[legIdx].isNotResetXCCY()) {
                            legDataIdx.push_back(legIdx);
                            break;
                        }
                        // If none of the legs are resetting, we just use the first leg for that ccy
                        if (legIdx == legIdxs.back())
                            legDataIdx.push_back(legIdxs.front());
                    }
                }
            }
            std::sort(legDataIdx.begin(), legDataIdx.end());

            if (legData[legDataIdx[0]].isNotResetXCCY() && legData[legDataIdx[1]].isNotResetXCCY()) {
                // If neither leg is resetting cross currency, we just need to remove principal exchanges
                if ((legData[legDataIdx[0]].notionalInitialExchange() && legData[legDataIdx[1]].notionalInitialExchange()) ||
                    (legData[legDataIdx[0]].notionalFinalExchange() && legData[legDataIdx[1]].notionalFinalExchange()) ||
                    (legData[legDataIdx[0]].notionalAmortizingExchange() && legData[legDataIdx[1]].notionalAmortizingExchange())) {
                    // Only need to replace the swap if there are some principal exchanges

                    vector<LegData> newLegData;
                    for (Size i = 0; i < legData.size(); i++)
                        if (legData[i].legType() == LegType::Cashflow || i == legDataIdx[0] || i == legDataIdx[1])
                            newLegData.push_back(copyLegData(legData[i]));

                    auto newTrade = swap->tradeType() == "CrossCurrencySwap"
                                        ? QuantLib::ext::make_shared<ore::data::CrossCurrencySwap>(swap->envelope(), newLegData)
                                        : QuantLib::ext::make_shared<ore::data::Swap>(swap->envelope(), newLegData);
                    newTrades.push_back(newTrade);
                    newTrades.back()->id() = swap->id();
                    modifiedTrades.insert(swap->id());
                    string msg = "Cross currency swap with ID " + swap->id() + " will be modified";

                    // Raise SIMM exemption warnings only for regulations that are not overriden.
                    // If all regs are overriden so that no exemption is applied, then no need for a structured message
                    bool isFullyOverriden = false;
                    if (hasSimmExemptionOverrides) {
                        set<CrifRecord::Regulation> exemptCollectRegs = removeRegulations(collectRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptPostRegs = removeRegulations(postRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptRegs(exemptCollectRegs);
                        exemptRegs.insert(exemptPostRegs.begin(), exemptPostRegs.end());

                        // If no more regulations are considered exempt, then they are all SIMM exemption overrides
                        if (exemptRegs.empty())
                            isFullyOverriden = true;
                        else
                            msg += " for the following regulations: " + regulationsToString(exemptRegs);
                    }
                    if (!isFullyOverriden) {
                        WLOG(msg);
                    }
                }
            } else {
                // One of the legs is resetting cross currency (trivial check that both are not flagged as resetting)
                if (legData[legDataIdx[0]].isNotResetXCCY() == legData[legDataIdx[1]].isNotResetXCCY())
                    continue;

                Size rsLegDataIdx = legData[legDataIdx[0]].isNotResetXCCY() ? legDataIdx[1] : legDataIdx[0];
                Size nrsLegDataIdx = legData[legDataIdx[0]].isNotResetXCCY() ? legDataIdx[0] : legDataIdx[1];

                // Need the trade to have been built for the logic here to work
                swap->build(engineFactory);

                // Some checks
                if (legData[legDataIdx[0]].notionalInitialExchange() != legData[legDataIdx[1]].notionalInitialExchange())
                    continue;
                if (legData[legDataIdx[0]].notionalFinalExchange() != legData[legDataIdx[1]].notionalFinalExchange())
                    continue;

                // Find the leg index for the resetting notional leg
                Size rsLegIdx = rsLegDataIdx == legDataIdx[0]
                                    ? legData.size()
                                    : (swap->legs().size() >= legData.size() + 2 ? legData.size() + 1 : legData.size());

                // Get the non-resetting leg notional
                QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(swap->legs()[nrsLegDataIdx][0]);
                QL_REQUIRE(coupon, "The non-resetting coupon leg appears to have no coupons");
                Real nrsNotional = coupon->nominal();

                // Get cashflows needed to modify the cross currency resetting swap
                vector<Real> rsAmounts;
                vector<string> rsDates;
                vector<Real> nrsAmounts;
                vector<string> nrsDates;
                bool allFxLinkedFlowsAreKnown = true;
                if (rsLegIdx < swap->legs().size()) {
                    for (Size i = 0; i < swap->legs()[rsLegIdx].size(); i++) {
                        // The initial exchange is handled via the initial notional exchange flag in the modified trade
                        if (i == 0 && legData[rsLegDataIdx].notionalInitialExchange())
                            continue;

                        const QuantLib::ext::shared_ptr<CashFlow>& cf = swap->legs()[rsLegIdx][i];

                        // Simple cashflow and payment is considered not to have occured
                        QuantLib::ext::shared_ptr<SimpleCashFlow> scf = QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(cf);
                        if (scf && !scf->hasOccurred()) {
                            if (i < swap->legs()[rsLegIdx].size() - 1) {
                                rsAmounts.push_back(-scf->amount());
                                rsDates.push_back(to_string(io::iso_date(scf->date())));
                                nrsAmounts.push_back(rsAmounts.back() > 0.0 ? nrsNotional : -nrsNotional);
                                nrsDates.push_back(to_string(io::iso_date(scf->date())));
                            }
                        }

                        // FX linked cashflow, FX fixing date in the past and payment is today or in the future
                        QuantLib::ext::shared_ptr<FXLinkedCashFlow> fxcf = QuantLib::ext::dynamic_pointer_cast<FXLinkedCashFlow>(cf);
                        if (fxcf && fxcf->fxFixingDate() <= Settings::instance().evaluationDate() &&
                            !fxcf->hasOccurred()) {
                            if (i < swap->legs()[rsLegIdx].size() - 1) {
                                rsAmounts.push_back(-fxcf->amount());
                                rsDates.push_back(to_string(io::iso_date(fxcf->date())));
                                nrsAmounts.push_back(rsAmounts.back() > 0.0 ? nrsNotional : -nrsNotional);
                                nrsDates.push_back(to_string(io::iso_date(fxcf->date())));
                            }
                        }

                        // Update flag indicating that all fx linked cashflows are known
                        if (fxcf && fxcf->fxFixingDate() > Settings::instance().evaluationDate())
                            allFxLinkedFlowsAreKnown = false;
                    }
                }

                // Create the new trade
                bool newInitialExchange =
                    legData[legDataIdx[0]].notionalInitialExchange() && legData[rsLegDataIdx].notionals().empty();
                bool newFinalExchange = !allFxLinkedFlowsAreKnown && legData[legDataIdx[0]].notionalFinalExchange();
                if (rsAmounts.size() > 0 || newInitialExchange != legData[legDataIdx[0]].notionalInitialExchange() ||
                    newInitialExchange != legData[legDataIdx[1]].notionalInitialExchange() ||
                    newFinalExchange != legData[legDataIdx[0]].notionalFinalExchange() ||
                    newFinalExchange != legData[legDataIdx[1]].notionalFinalExchange()) {
                    vector<LegData> newLegData;
                    for (Size i = 0; i < legData.size(); i++)
                        if (legData[i].legType() == LegType::Cashflow)
                            newLegData.push_back(copyLegData(legData[i]));
                        else if (i == legDataIdx[0] || i == legDataIdx[1])
                            newLegData.push_back(copyLegData(legData[i], newInitialExchange, newFinalExchange));

                    // Add the additional legs to offset the resetting leg known amounts
                    if (rsAmounts.size() > 0) {
                        newLegData.push_back(LegData(QuantLib::ext::make_shared<CashflowData>(rsAmounts, rsDates),
                                                     legData[rsLegDataIdx].isPayer(),
                                                     legData[rsLegDataIdx].currency()));
                    }

                    if (nrsAmounts.size() > 0) {
                        newLegData.push_back(LegData(QuantLib::ext::make_shared<CashflowData>(nrsAmounts, nrsDates),
                                                     legData[nrsLegDataIdx].isPayer(),
                                                     legData[nrsLegDataIdx].currency()));
                    }

                    // Set up the resettable swap for amendment
                    auto newTrade = swap->tradeType() == "CrossCurrencySwap"
                                        ? QuantLib::ext::make_shared<ore::data::CrossCurrencySwap>(swap->envelope(), newLegData)
                                        : QuantLib::ext::make_shared<ore::data::Swap>(swap->envelope(), newLegData);
                    newTrades.push_back(newTrade);
                    newTrades.back()->id() = swap->id();
                    modifiedTrades.insert(swap->id());
                    string msg = "Cross currency resettable swap with ID " + swap->id() + " will be modified";
                    if (hasSimmExemptionOverrides) {
                        set<CrifRecord::Regulation> exemptCollectRegs = removeRegulations(collectRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptPostRegs = removeRegulations(postRegs, simmExemptionOverrides);
                        set<CrifRecord::Regulation> exemptRegs(exemptCollectRegs);
                        exemptRegs.insert(exemptPostRegs.begin(), exemptPostRegs.end());
                        msg += " for the following regulations: " + ore::data::to_string(exemptRegs);
                    }
                    WLOG(msg);
                } else {
                    DLOG("No offsetting amounts needed and no change in exchange flags, so cross currency resettable "
                         "swap with ID "
                         << swap->id() << " will not be modified.");
                }
            }
        }
    }

    // Remove trades that need removal
    for (const auto& id : removedTrades) {
        DLOG("Removing trade with ID " << id);
        portfolio.remove(id);
    }

    // Add replacement trades
    for (auto& trade : newTrades) {
        DLOG("Adding replacement trade with ID " << trade->id());
        portfolio.remove(trade->id());
        auto [ft, success] = buildTrade(trade, engineFactory, "portfolioModifier/SIMM exemptions",
                                        portfolio.ignoreTradeBuildFail(), portfolio.buildFailedTrades(), true);
        if (success)
            portfolio.add(trade);
        else if (ft)
            portfolio.add(ft);
    }

    LOG("Finished applying SIMM exemptions to the portfolio");

    return std::make_pair(removedTrades, modifiedTrades);
}

}
}
