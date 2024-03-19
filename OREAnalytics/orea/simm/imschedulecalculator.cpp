/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/simm/imschedulecalculator.hpp>
#include <orea/simm/imscheduleresults.hpp>

#include <orea/simm/utilities.hpp>
#include <orea/simm/crifrecord.hpp>

#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>

#include <orea/app/structuredanalyticswarning.hpp>
#include <ql/math/comparison.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using std::map;
using std::pair;
using std::max;
using std::min;
using std::string;
using std::vector;
using QuantLib::close_enough;

using ore::data::checkCurrency;
using ore::data::NettingSetDetails;
using ore::data::parseDate;
using ore::data::parseBool;
using ore::data::to_string;
using QuantLib::Real;
using QuantLib::Null;

namespace ore {
namespace analytics {

IMScheduleCalculator::IMScheduleCalculator(const Crif& crif, const string& calculationCcy,
                                           const QuantLib::ext::shared_ptr<ore::data::Market> market,
                                           const bool determineWinningRegulations, const bool enforceIMRegulations,
                                           const bool quiet,
                                           const map<SimmSide, set<NettingSetDetails>>& hasSEC,
                                           const map<SimmSide, set<NettingSetDetails>>& hasCFTC)
    : crif_(crif), calculationCcy_(calculationCcy), market_(market), quiet_(quiet),
      hasSEC_(hasSEC), hasCFTC_(hasCFTC) {

    QL_REQUIRE(checkCurrency(calculationCcy_),
               "The calculation currency (" << calculationCcy_ << ") must be a valid ISO currency code");

    QuantLib::Date today = QuantLib::Settings::instance().evaluationDate();
    QuantLib::DayCounter dayCounter = QuantLib::ActualActual(QuantLib::ActualActual::ISDA);

    // Collect Schedule CRIF records
    Crif tmp;
    for (const CrifRecord& cr : crif_) {
        const bool isSchedule = cr.imModel == "Schedule";

        if (!isSchedule) {
            if (determineWinningRegulations) {
                ore::data::StructuredTradeWarningMessage(
                    cr.tradeId, cr.tradeType, "IM Schedule calculator",
                    "Skipping over CRIF record without im_model=Schedule for portfolio [ " +
                        to_string(cr.nettingSetDetails) + "]")
                    .log();
            }
            continue;
        }

        // Check for each netting set whether post/collect regs are populated at all
        if (collectRegsIsEmpty_.find(cr.nettingSetDetails) == collectRegsIsEmpty_.end()) {
            collectRegsIsEmpty_[cr.nettingSetDetails] = cr.collectRegulations.empty();
        } else if (collectRegsIsEmpty_.at(cr.nettingSetDetails) && !cr.collectRegulations.empty()) {
            collectRegsIsEmpty_.at(cr.nettingSetDetails) = false;
        }
        if (postRegsIsEmpty_.find(cr.nettingSetDetails) == postRegsIsEmpty_.end()) {
            postRegsIsEmpty_[cr.nettingSetDetails] = cr.postRegulations.empty();
        } else if (postRegsIsEmpty_.at(cr.nettingSetDetails) && !cr.postRegulations.empty()) {
            postRegsIsEmpty_.at(cr.nettingSetDetails) = false;
        }

        tmp.addRecord(cr);
    }
    crif_ = tmp;


    // Separate out CRIF records by regulations and collect per-trade data
    LOG("IMScheduleCalculator: Collecting CRIF trade data");
    for (const auto& crifRecord : crif_)
        collectTradeData(crifRecord, enforceIMRegulations);

    // Remove (or modify) trades with incomplete data
    for (auto& sv : nettingSetRegTradeData_) {
        const SimmSide& side = sv.first;

        for (auto& nv : sv.second) {
            const NettingSetDetails& nsd = nv.first;

            for (auto& rv : nv.second) {
                const string& regulation = rv.first;
                auto& tradeDataMap = rv.second;

                // Remove (or modify) trades with incomplete Schedule data
                set<string> tradesToRemove;
                for (auto& td : tradeDataMap) {
                    if (td.second.incomplete()) {
                        auto subFields = map<string, string>({{"tradeId", td.first}});
                        // If missing PV, assume PV = 0
                        if (td.second.missingPVData()) {
                            td.second.presentValue = 0.0;
                            td.second.presentValueUsd = 0.0;
                            td.second.presentValueCcy = td.second.notionalCcy;
                        }
                        // If missing Notional, do not process the trade
                        if (td.second.missingNotionalData()) {
                            ore::analytics::StructuredAnalyticsWarningMessage(
                                "IMSchedule", "Incomplete CRIF trade data",
                                "Missing Notional data. The trade will not be processed.", subFields)
                                .log();
                            tradesToRemove.insert(td.first);
                        }
                    }
                }

                for (const string& tid : tradesToRemove) {
                    tradeDataMap.erase(tid);
                    tradeIds_.at(side).at(nsd).at(regulation).erase(tid);
                }

                // Calculate Schedule data for each trade data obj
                for (auto& td : tradeDataMap) {
                    IMScheduleTradeData& tradeData = td.second;

                    // Calculate gross IM for each IM Schedule trade
                    tradeData.maturity = dayCounter.yearFraction(today, tradeData.endDate);
                    tradeData.label = label(tradeData.productClass, tradeData.maturity);
                    tradeData.labelString = labelString(tradeData.label);
                    tradeData.multiplier = multiplier(tradeData.label);
                    tradeData.grossMarginUsd = tradeData.multiplier * tradeData.notionalUsd;

                    // Convert some trade data values into calculation currency
                    const Real usdSpot = calculationCcy_ != "USD" ? market_->fxRate(calculationCcy_ + "USD")->value() : 1.0;
                    tradeData.notionalCalc = tradeData.notionalUsd / usdSpot;
                    tradeData.presentValueCalc = tradeData.presentValueUsd / usdSpot;
                    tradeData.grossMarginCalc = tradeData.grossMarginUsd / usdSpot;
                    if (side == SimmSide::Call)
                        tradeData.collectRegulations = regulation;
                    if (side == SimmSide::Post)
                        tradeData.postRegulations = regulation;
                }
            }
        }
    }

    // Some additional processing depending on the regulations applicable to each netting set
    for (auto& sv : nettingSetRegTradeData_) {
        const SimmSide& side = sv.first;
    
        for (auto& s : sv.second) {
            const NettingSetDetails& nettingDetails = s.first;

            // Where there is SEC and CFTC in the portfolio, we add the CFTC trades to SEC,
            // but still continue with CFTC calculations
            const bool hasCFTCGlobal = hasCFTC_.at(side).find(nettingDetails) != hasCFTC_.at(side).end();
            const bool hasSECGlobal = hasSEC_.at(side).find(nettingDetails) != hasSEC_.at(side).end();
            const bool hasCFTCLocal = s.second.count("CFTC") > 0;
            const bool hasSECLocal = s.second.count("SEC") > 0;

            if ((hasSECLocal && hasCFTCLocal) || (hasCFTCGlobal && hasSECGlobal)) {
                if (!hasSECLocal && !hasCFTCLocal)
                    continue;

                if (hasCFTCLocal) {
                    // At this point, we expect to have CFTC trade data at least for the netting set
                    const map<string, IMScheduleTradeData>& tradeDataMapCFTC = s.second.at("CFTC");
                    map<string, IMScheduleTradeData>& tradeDataMapSEC = s.second["SEC"];
                    for (const auto& kv : tradeDataMapCFTC) {
                        // Only add CFTC records to SEC if the record was not already in SEC,
                        // i.e. we skip over CRIF records with regulations specified as e.g. "..., CFTC, SEC, ..."
                        if (tradeDataMapSEC.find(kv.first) == tradeDataMapSEC.end()) {
                            tradeDataMapSEC[kv.first] = kv.second;
                        }
                    }
                }
            }

            // If netting set has "Unspecified" plus other regulations, the "Unspecified" sensis are to be excluded.
            // If netting set only has "Unspecified", then no regulations were ever specified, so all trades are
            // included.
            if (s.second.count("Unspecified") > 0 && s.second.size() > 1)
                s.second.erase("Unspecified");
        }
    }

    // Calculate the higher level margins
    LOG("IMScheduleCalculator: Populating higher level results")
    for (const auto& sv : nettingSetRegTradeData_) {
        const SimmSide side = sv.first;

        for (const auto& nv : sv.second) {
            const NettingSetDetails& nsd = nv.first;

            for (const auto& rv : nv.second) {
                const string& regulation = rv.first;
                populateResults(nsd, regulation, side);
            }
        }
    }

    if (determineWinningRegulations) {
        LOG("IMScheduleCalculator: Determining winning regulations");

        // Determine winning call and post regulations
        for (const auto& sv : imScheduleResults_) {
            const SimmSide side = sv.first;

            // Determine winning regulation for each netting set
            // Collect margin amounts and determine the highest margin amount
            for (const auto& nv : sv.second) {
                const NettingSetDetails& nsd = nv.first;
                Real winningMargin = std::numeric_limits<Real>::min();
                map<string, Real> nettingSetMargins;
                vector<Real> margins;

                for (const auto& rv : nv.second) {
                    const string& regulation = rv.first;
                    const IMScheduleResults& schResult = rv.second;
                    const Real& im = schResult.get(ProductClass::All).scheduleIM;

                    nettingSetMargins[regulation] = im;
                    if (im > winningMargin)
                        winningMargin = im;
                }

                // Determine winning regulations, i.e. regulations under which we find the highest margin amount
                vector<string> winningRegulations;
                for (const auto& kv : nettingSetMargins) {
                    if (close_enough(kv.second, winningMargin))
                        winningRegulations.push_back(kv.first);
                }

                // In the case of multiple winning regulations, pick one based on the priority in the list
                //const Regulation winningRegulation = getWinningRegulation(winningRegulations);
                string winningRegulation = winningRegulations.size() > 1
                                               ? to_string(getWinningRegulation(winningRegulations))
                                               : winningRegulations.at(0);

                // Populate internal list of winning regulators
                winningRegulations_[side][nsd] = ore::data::to_string(winningRegulation);
            }
        }

        populateFinalResults();
    }
}

const string& IMScheduleCalculator::winningRegulations(const SimmSide& side,
                                                       const NettingSetDetails& nettingSetDetails) const {
    const auto& subWinningRegs = winningRegulations(side);
    QL_REQUIRE(subWinningRegs.find(nettingSetDetails) != subWinningRegs.end(),
               "IMScheduleCalculator::winningRegulations(): Could not find netting set in the list of "
                   << side << " schedule IM winning regulations: " << nettingSetDetails);
    return subWinningRegs.at(nettingSetDetails);
}

const map<NettingSetDetails, string>& IMScheduleCalculator::winningRegulations(const SimmSide& side) const {
    QL_REQUIRE(winningRegulations_.find(side) != winningRegulations_.end(),
               "IMScheduleCalculator::winningRegulations(): Could not find list of"
                   << side << " schedule IM winning regulations");
    return winningRegulations_.at(side);
}

const map<SimmConfiguration::SimmSide, map<NettingSetDetails, string>>&
IMScheduleCalculator::winningRegulations() const {
    return winningRegulations_;
}

const map<string, IMScheduleResults>& IMScheduleCalculator::imScheduleSummaryResults(const SimmSide& side, const NettingSetDetails& nsd) const {
    const auto& subResults = imScheduleSummaryResults(side);
    QL_REQUIRE(subResults.find(nsd) != subResults.end(),
               "IMScheduleCalculator::imScheduleSummaryResults(): Could not find netting set in the "
                   << side << " IM schedule results: " << nsd);
    return subResults.at(nsd);
}

const map<NettingSetDetails, map<string, IMScheduleResults>>& IMScheduleCalculator::imScheduleSummaryResults(const SimmSide& side) const {
    QL_REQUIRE(imScheduleResults_.find(side) != imScheduleResults_.end(),
               "IMScheduleCalculator::imScheduleSummaryResults(): Could not find " << side
                                                                                   << " IM in the IM Schedule results");
    return imScheduleResults_.at(side);
}

const map<SimmConfiguration::SimmSide, map<NettingSetDetails, map<string, IMScheduleResults>>>&
IMScheduleCalculator::imScheduleSummaryResults() const {
    return imScheduleResults_;
};

const pair<string, IMScheduleResults>& IMScheduleCalculator::finalImScheduleSummaryResults(const SimmSide& side,
                                                                                           const NettingSetDetails& nsd) const {
    const auto& subResults = finalImScheduleSummaryResults(side);
    QL_REQUIRE(subResults.find(nsd) != subResults.end(),
               "IMScheduleCalculator::finalImScheduleSummaryResults(): Could not find netting set in the final IM Schedule "
                   << side << " results: " << nsd);
    return subResults.at(nsd);
}

const map<NettingSetDetails, pair<string, IMScheduleResults>>& IMScheduleCalculator::finalImScheduleSummaryResults(const SimmSide& side) const {
    QL_REQUIRE(finalImScheduleResults_.find(side) != finalImScheduleResults_.end(),
               "IMScheduleCalculator::finalImScheduleSummaryResults(): Could not find "
                   << side << " IM in the final IM Schedule results");
    return finalImScheduleResults_.at(side);
}

const vector<IMScheduleCalculator::IMScheduleTradeData>& IMScheduleCalculator::imScheduleTradeResults(const string& tradeId) const {
    QL_REQUIRE(finalTradeData_.find(tradeId) != finalTradeData_.end(),
               "IMScheduleCalculator::imScheduleTradeResults(): Could not find results for trade: " << tradeId);
    return finalTradeData_.at(tradeId);
}

const map<string, vector<IMScheduleCalculator::IMScheduleTradeData>>& IMScheduleCalculator::imScheduleTradeResults() const {
    return finalTradeData_;
}

const IMScheduleLabel IMScheduleCalculator::label(const ProductClass& pc, const Real& maturity) {
    if (pc == ProductClass::Credit) {
        if (maturity >= 0.0 && maturity < 2.0) {
            return IMScheduleLabel::Credit2;
        } else if (maturity < 5.0) {
            return IMScheduleLabel::Credit5;
        } else {
            return IMScheduleLabel::Credit100;
        }

    } else if (pc == ProductClass::Commodity) {
        return IMScheduleLabel::Commodity;

    } else if (pc == ProductClass::Equity) {
        return IMScheduleLabel::Equity;

    } else if (pc == ProductClass::FX) {
        return IMScheduleLabel::FX;

    } else if (pc == ProductClass::Rates) {
        if (maturity >= 0.0 && maturity < 2.0) {
            return IMScheduleLabel::Rates2;
        } else if (maturity < 5.0) {
            return IMScheduleLabel::Rates5;
        } else {
            return IMScheduleLabel::Rates100;
        }

    } else if (pc == ProductClass::Other) {
        return IMScheduleLabel::Other;
    } else {
        QL_FAIL("IMSchedule::label() Invalid product class " << pc);
    }
}

const string IMScheduleCalculator::labelString(const IMScheduleLabel& label) {
    const map<IMScheduleLabel, string> labelStringMap_ = map<IMScheduleLabel, string>({
        {IMScheduleLabel::Credit2, "Credit 0-2 years"},
        {IMScheduleLabel::Credit5, "Credit 2-5 years"},
        {IMScheduleLabel::Credit100, "Credit 5+ years"},
        {IMScheduleLabel::Commodity, "Commodity"},
        {IMScheduleLabel::Equity, "Equity"},
        {IMScheduleLabel::FX, "FX"},
        {IMScheduleLabel::Rates2, "Interest Rate 0-2 years"},
        {IMScheduleLabel::Rates5, "Interest Rate 2-5 years"},
        {IMScheduleLabel::Rates100, "Interest Rate 5+ years"},
        {IMScheduleLabel::Other, "Other"},
    });

    return labelStringMap_.at(label);
}

void IMScheduleCalculator::collectTradeData(const CrifRecord& cr, const bool enforceIMRegulations) {

    DLOG("Processing CRIF record for IMSchedule calculation: trade ID \'"
         << cr.tradeId << "\', portfolio [" << cr.nettingSetDetails << "], product class " << cr.productClass
         << ", risk type " << cr.riskType << ", end date " << cr.endDate);
    QL_REQUIRE(cr.riskType == RiskType::PV || cr.riskType == RiskType::Notional,
               "Unexpected risk type found in CRIF " << cr.riskType << " for trade ID " << cr.tradeId);

    for (const auto& side : {SimmSide::Call, SimmSide::Post}) {
        const NettingSetDetails& nettingSetDetails = cr.nettingSetDetails;

        bool collectRegsIsEmpty = false;
        bool postRegsIsEmpty = false;
        if (collectRegsIsEmpty_.find(cr.nettingSetDetails) != collectRegsIsEmpty_.end())
            collectRegsIsEmpty = collectRegsIsEmpty_.at(cr.nettingSetDetails);
        if (postRegsIsEmpty_.find(cr.nettingSetDetails) != postRegsIsEmpty_.end())
            postRegsIsEmpty = postRegsIsEmpty_.at(cr.nettingSetDetails);

        string regsString;
        if (enforceIMRegulations)
            regsString = side == SimmSide::Call ? cr.collectRegulations : cr.postRegulations;
        set<string> regs = parseRegulationString(regsString);

        for (const string& r : regs) {
            if (r == "Unspecified" && enforceIMRegulations && !(collectRegsIsEmpty && postRegsIsEmpty)) {
                continue;
            } else if (r != "Excluded") {
                // Keep a record of trade IDs for each regulation
                tradeIds_[side][nettingSetDetails][r].insert(cr.tradeId);

                auto& tradeDataMap = nettingSetRegTradeData_[side][nettingSetDetails][r];
                auto it = tradeDataMap.find(cr.tradeId);
                if (it != tradeDataMap.end()) {
                    IMScheduleTradeData& tradeData = it->second;

                    QL_REQUIRE(cr.productClass == tradeData.productClass, "Product class is not matching for trade ID "
                                                                              << cr.tradeId << ": " << cr.productClass
                                                                              << " and " << tradeData.productClass);
                    QuantLib::Date incomingDate = parseDate(cr.endDate);
                    QL_REQUIRE(incomingDate == tradeData.endDate, "End date is not matching for trade ID "
                                                                      << cr.tradeId << ": " << incomingDate << " and "
                                                                      << tradeData.endDate);
                    if (cr.riskType == RiskType::PV) {
                        QL_REQUIRE(tradeData.missingPVData(), "Adding PV data for trade that already has PV data, i.e. "
                                                              "multiple PV records found for the same trade: "
                                                                  << tradeData.tradeId);
                        tradeData.presentValue = cr.amount;
                        tradeData.presentValueUsd = cr.amountUsd;
                        tradeData.presentValueCcy = cr.amountCurrency;
                    } else {
                        QL_REQUIRE(tradeData.missingNotionalData(),
                                   "Adding Notional data for trade that already has PV data, i.e. "
                                   "multiple Notional records found for the same trade: "
                                       << tradeData.tradeId);
                        tradeData.notional = cr.amount;
                        tradeData.notionalUsd = cr.amountUsd;
                        tradeData.notionalCcy = cr.amountCurrency;
                    }
                } else {
                    const string collectRegs = side == SimmSide::Call ? cr.collectRegulations : "";
                    const string postRegs = side == SimmSide::Post ? cr.postRegulations : "";
                    tradeDataMap.insert(
                        {cr.tradeId, IMScheduleTradeData(cr.tradeId, cr.nettingSetDetails, cr.riskType, cr.productClass,
                                                         cr.amount, cr.amountCurrency, cr.amountUsd,
                                                         parseDate(cr.endDate), calculationCcy_, collectRegs, postRegs)});
                }
            }
        }
    }
}

void IMScheduleCalculator::populateResults(const NettingSetDetails& nettingSetDetails, const string& regulation,
                                           const SimmSide& side) {

    LOG("IMScheduleCalculator: Populating " << side << " IM for netting set [" << nettingSetDetails
                                            << "] under regulation " << regulation);


    const auto& regTradeData = nettingSetRegTradeData_.at(side).at(nettingSetDetails).at(regulation);

    Real grossMarginCalc = 0;
    Real grossRCCalc = 0;
    Real presentValueCalc = 0;

    // Populate results at the product class level
    for (const auto& td : regTradeData) {
        const IMScheduleTradeData& tradeData = td.second;
        add(side, nettingSetDetails, regulation, tradeData.productClass, calculationCcy_, tradeData.grossMarginCalc);
        
        // Sum up trade details to obtain netting set level values:
        // - Gross margin, gross RC, net RC, PVs
        grossMarginCalc += tradeData.grossMarginCalc;
        grossRCCalc +=
            side == SimmSide::Call ? max(0.0, tradeData.presentValueCalc) : min(0.0, tradeData.presentValueCalc);

        presentValueCalc += tradeData.presentValueCalc;
    }

    // Calculate other amounts at the nettingSet-regulator level

    // Net replacement cost
    Real netRCCalc = side == SimmSide::Call ? max(0.0, presentValueCalc) : min(0.0, presentValueCalc);

    // Net-to-gross ratio
    Real netToGrossCalc = close_enough(grossRCCalc, 0.0) ? 1.0 : netRCCalc / grossRCCalc;

    // Schedule IM
    Real scheduleMarginCalc = grossMarginCalc * (0.4 + 0.6 * netToGrossCalc);

    // Populate higher level results
    imScheduleResults_.at(side)
        .at(nettingSetDetails)
        .at(regulation)
        .add(ProductClass::All, calculationCcy_, grossMarginCalc, grossRCCalc, netRCCalc, netToGrossCalc,
             scheduleMarginCalc);
}

void IMScheduleCalculator::populateFinalResults(const map<SimmSide, map<NettingSetDetails, string>>& winningRegs) {

    LOG("IMScheduleCalculator: Populating final winning regulators' IM");

    winningRegulations_ = winningRegs;

    // Populate final IMSchedule results
    for (const auto& sv : imScheduleResults_) {
        const SimmSide side = sv.first;
        
        for (const auto& nv : sv.second) {
            const NettingSetDetails& nsd = nv.first;
            
            const string& reg = winningRegulations(side, nsd);
            // If no results found for winning regulator, i.e IM was calculated from SIMM only, use empty results
            const IMScheduleResults& results =
                nv.second.find(reg) == nv.second.end() ? IMScheduleResults(calculationCcy_) : nv.second.at(reg);
            finalImScheduleResults_[side][nsd] = make_pair(reg, results);
            winningRegulations_[side][nsd] = reg;
        }
    }

    // Recombine trade data to form a single list of trade-level Schedule IM data
    for (const auto& sv : nettingSetRegTradeData_) {
        const SimmSide& side = sv.first;
        for (const auto& nv : sv.second) {
            for (const auto& rv : nv.second) {
                const string& regulation = rv.first;
                for (const auto& td : rv.second) {
                    // For every trade data obj, try to add this back to the finalTradeData_ map
                    const string& tradeId = td.first;
                    IMScheduleTradeData tradeData = td.second;
                    
                    // We use vectors instead of trade data since the same trade ID could have multiple
                    // Schedule CRIF records, e.g. because of SIMM exemption overrides.
                    if (finalTradeData_.find(tradeId) == finalTradeData_.end())
                        finalTradeData_[tradeId] = std::vector<IMScheduleTradeData>();
                    
                    if (finalTradeData_.at(tradeId).empty()) {
                        finalTradeData_.at(tradeId).push_back(tradeData);
                    } else {
                        bool foundMatch = false;
                        for (IMScheduleTradeData& tdd : finalTradeData_.at(tradeId)) {
                            // If the two trade data objs have the same data, then just combine regulations
                            if (std::tie(tdd.presentValueCcy, tdd.presentValueUsd, tdd.notionalCcy, tdd.notionalUsd) ==
                                std::tie(tradeData.presentValueCcy, tradeData.presentValueUsd, tradeData.notionalCcy,
                                            tradeData.notionalUsd)) {
                                if (side == SimmSide::Call) {
                                    tdd.collectRegulations = combineRegulations(tdd.collectRegulations, regulation);
                                } else {
                                    tdd.postRegulations = combineRegulations(tdd.postRegulations, regulation);
                                }
                                foundMatch = true;
                                break;
                            }
                        }
                        if (!foundMatch)
                            finalTradeData_.at(tradeId).push_back(tradeData);
                    }
                }
            }
        }
    }
}

void IMScheduleCalculator::populateFinalResults() {
    populateFinalResults(winningRegulations_);
}

void IMScheduleCalculator::add(const SimmSide& side, const NettingSetDetails& nsd, const string& regulation,
                               const ProductClass& pc, const string& calcCcy, const Real& grossIM, const Real& grossRC,
                               const Real& netRC, const Real& ngr, const Real& scheduleIM) {
    
    QuantLib::Real netToGrossRatio = ngr != Null<Real>() && close_enough(ngr, 0.0) ? 0.0 : ngr;
    imScheduleResults_[side][nsd][regulation].add(pc, calcCcy, grossIM, grossRC, netRC, netToGrossRatio, scheduleIM);
}

} // namespace analytics
} // namespace ore
