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

#include <orea/simm/crifrecord.hpp>
#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmcalculator.hpp>
#include <orea/simm/utilities.hpp>
#include <orea/simm/simmconfigurationbase.hpp>
#include <orea/app/structuredanalyticswarning.hpp>

#include <boost/math/distributions/normal.hpp>
#include <numeric>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quote.hpp>

using std::abs;
using std::accumulate;
using std::make_pair;
using std::make_tuple;
using std::map;
using std::max;
using std::min;
using std::pair;
using std::set;
using std::sqrt;
using std::string;
using std::tuple;
using std::vector;

using ore::data::checkCurrency;
using ore::data::NettingSetDetails;
using ore::data::Market;
using ore::data::to_string;
using ore::data::parseBool;
using QuantLib::close_enough;
using QuantLib::Null;
using QuantLib::Real;

namespace ore {
namespace analytics {

// Ease notation again
typedef CrifRecord::ProductClass ProductClass;
typedef SimmConfiguration::RiskClass RiskClass;
typedef SimmConfiguration::MarginType MarginType;
typedef CrifRecord::RiskType RiskType;
typedef CrifRecord::Regulation Regulation;
typedef SimmConfiguration::SimmSide SimmSide;

struct RegulationCompare {
    bool operator()(const pair<Size, set<Regulation>>& reg1, const pair<Size, set<Regulation>>& reg2) {
        return reg1.first < reg2.first;
    }
};

SimmCalculator::SimmCalculator(const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif,
                               const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration,
                               const string& calculationCcyCall, const string& calculationCcyPost,
                               const string& resultCcy, const QuantLib::ext::shared_ptr<Market> market,
                               const bool determineWinningRegulations, const bool enforceIMRegulations,
                               const bool quiet, const map<SimmSide, set<NettingSetDetails>>& hasSEC)
    : simmConfiguration_(simmConfiguration), calculationCcyCall_(calculationCcyCall),
      calculationCcyPost_(calculationCcyPost), resultCcy_(resultCcy.empty() ? calculationCcyCall_ : resultCcy),
      market_(market), quiet_(quiet), hasSEC_(hasSEC) {

    if (!crif) {
        WLOG("SimmCalculator(): CRIF input is null");
        return;
    }

    timer_.start("Total");

    QL_REQUIRE(checkCurrency(calculationCcyCall_), "SIMM Calculator: The Call side calculation currency ("
                                                   << calculationCcyCall_ << ") must be a valid ISO currency code");
    QL_REQUIRE(checkCurrency(calculationCcyPost_), "SIMM Calculator: The Post side calculation currency ("
                                                   << calculationCcyPost_ << ") must be a valid ISO currency code");
    QL_REQUIRE(checkCurrency(resultCcy_),
               "SIMM Calculator: The result currency (" << resultCcy_ << ") must be a valid ISO currency code");

    timer_.start("Cleaning up CRIF input");
    for (SlimCrifRecordContainer::iterator it = crif->begin(); it != crif->end(); it++) {
        // Remove empty
        if (it->riskType() == RiskType::Empty) {
            continue;
        }
        // Remove Schedule-only CRIF records
        const bool isSchedule = it->imModel() == CrifRecord::IMModel::Schedule;
        if (isSchedule) {
            if (!quiet_ && determineWinningRegulations) {
                ore::data::StructuredTradeWarningMessage(it->getTradeId(), it->getTradeType(), "SIMM calculator",
                                                         "Skipping over Schedule CRIF record")
                    .log();
            } 
            continue;
        }

        // Check for each netting set whether post/collect regs are populated at all
        bool collectRegsIsEmpty = it->collectRegulations().empty();
        const NettingSetDetails& nsd = it->getNettingSetDetails();
        if (collectRegsIsEmpty_.find(nsd) == collectRegsIsEmpty_.end()) {
            collectRegsIsEmpty_[nsd] = collectRegsIsEmpty;
        } else if (collectRegsIsEmpty_[nsd] && !collectRegsIsEmpty) {
            collectRegsIsEmpty_[nsd] = false;
        }
        bool postRegsIsEmpty = it->postRegulations().empty();
        if (postRegsIsEmpty_.find(nsd) == postRegsIsEmpty_.end()) {
            postRegsIsEmpty_[nsd] = postRegsIsEmpty;
        } else if (postRegsIsEmpty_[nsd] && !it->postRegulations().empty()) {
            postRegsIsEmpty_[nsd] = false;
        }

        // Make sure we have CRIF amount denominated in the result ccy
        if (it->requiresAmountUsd() && resultCcy_ == "USD" && it->hasAmountUsd()) {
            it->setAmountResultCurrency(it->amountUsd());
        } else if(it->requiresAmountUsd()) {
            // ProductClassMultiplier and AddOnNotionalFactor  don't have a currency and dont need to be converted,
            // we use the amount
            const Real fxSpot = fxRate(it->getCurrency() + resultCcy_);
            it->setAmountResultCurrency(fxSpot * it->amount());
        }
        it->setResultCurrency(resultCcy_);
    }
    timer_.stop("Cleaning up CRIF input");

    // Add CRIF records to each regulation under each netting set
    if (!quiet_) {
        LOG("SimmCalculator: Splitting up original CRIF records into their respective collect/post regulations");
    }
    
    splitCrifByRegulationsAndPortfolios(enforceIMRegulations, crif);

    cleanDuplicateRegulations();

    // If there are no CRIF records to process
    if (regSensitivities_.empty())
        return;

    // Some additional processing depending on the regulations applicable to each netting set
    for (auto& [side, nettingsSetCrifMap] : regSensitivities_) {
        for (auto& [nettingDetails, regulationCrifMap] : nettingsSetCrifMap) {

            // If netting set has Regulation::Unspecified plus other regulations, the Regulation::Unspecified sensis are to be excluded.
            // If netting set only has Regulation::Unspecified, then no regulations were ever specified, so all trades are
            // included.
            if (regulationCrifMap.count(set<Regulation>({Regulation::Unspecified})) > 0 && regulationCrifMap.size() > 1)
                regulationCrifMap.erase(set<Regulation>({Regulation::Unspecified}));

            set<set<Regulation>> crifsToErase;
            for (const auto& [regsKey, crif] : regulationCrifMap) {
                if (crif->empty())
                    crifsToErase.insert(regsKey);
            }
            for (const auto& regsKey : crifsToErase)
                regulationCrifMap.erase(regsKey);
        }
    }

    // Calculate SIMM call and post for each regulation under each netting set
    for (const auto& [side, nettingSetRegulationCrifMap] : regSensitivities_) {
        for (const auto& [nsd, regulationCrifMap] : nettingSetRegulationCrifMap) {
            // Calculate SIMM for particular side-nettingSet-regulation combination
            for (const auto& [regulation, crif] : regulationCrifMap) {
                bool hasFixedAddOn = false;
                for (const auto& sp : *crif) {
                    if (sp.riskType() == RiskType::AddOnFixedAmount) {
                        hasFixedAddOn = true;
                        break;
                    }
                }
                if (crif->hasCrifRecords() || hasFixedAddOn) {
                    calculateRegulationSimm(*crif, nsd, regulation, side);
                }
            }
        }
    }

    // Determine winning call and post regulations
    if (determineWinningRegulations) {
        timer_.start("Determining winning regulations");
        if (!quiet_) {
            LOG("SimmCalculator: Determining winning regulations");
        }

        for (auto sv : simmResults_) {
            const SimmSide side = sv.first;

            // Determine winning (call and post) regulation for each netting set
            for (const auto& kv : sv.second) {

                // Collect margin amounts and determine the highest margin amount
                Real winningMargin = std::numeric_limits<Real>::min();
                map<set<Regulation>, Real> nettingSetMargins;
                vector<Real> margins;
                for (const auto& regSimmResults : kv.second) {
                    const Real& im = regSimmResults.second.get(ProductClass::All, RiskClass::All, MarginType::All, "All");
                    nettingSetMargins[regSimmResults.first] = im;
                    if (im > winningMargin)
                        winningMargin = im;
                }

                // Determine winning regulations, i.e. regulations under which we find the highest margin amount
                std::set<Regulation> winningRegulations;
                for (const auto& kv : nettingSetMargins) {
                    if (close_enough(kv.second, winningMargin))
                        winningRegulations.insert(kv.first.begin(), kv.first.end());
                }

                // Populate internal list of winning regulators
                winningRegulations_[side][kv.first] = getWinningRegulation(winningRegulations);
            }
        }

        populateFinalResults();
        timer_.stop("Determining winning regulations");
    }
    timer_.stop("Total");
}

const void SimmCalculator::calculateRegulationSimm(const Crif& crif, const NettingSetDetails& nettingSetDetails,
                                                   const set<Regulation>& regulations, const SimmSide& side) {

    const string regTimerKey = "calculate " + ore::data::to_string(side) + " SIMM (" + regulationsToString(regulations) + ")";
    timer_.start(regTimerKey);

    if (!quiet_) {
        LOG("SimmCalculator: Calculating SIMM " << side << " for portfolio [" << nettingSetDetails << "], regulations "
                                                << regulations);
    }
    // Loop over portfolios and product classes
   for(const auto  productClass : crif.ProductClassesByNettingSetDetails(nettingSetDetails)){
        if (!quiet_) {
            LOG("SimmCalculator: Calculating SIMM for product class " << productClass);
        }

        // Delta margin components
        RiskClass rc = RiskClass::InterestRate;
        MarginType mt = MarginType::Delta;
        auto p = irDeltaMargin(nettingSetDetails, productClass, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::FX;
        p = margin(nettingSetDetails, productClass, RiskType::FX, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::CreditQualifying;
        p = margin(nettingSetDetails, productClass, RiskType::CreditQ, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::CreditNonQualifying;
        p = margin(nettingSetDetails, productClass, RiskType::CreditNonQ, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::Equity;
        p = margin(nettingSetDetails, productClass, RiskType::Equity, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::Commodity;
        p = margin(nettingSetDetails, productClass, RiskType::Commodity, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        // Vega margin components
        mt = MarginType::Vega;
        rc = RiskClass::InterestRate;
        p = irVegaMargin(nettingSetDetails, productClass, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::FX;
        p = margin(nettingSetDetails, productClass, RiskType::FXVol, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::CreditQualifying;
        p = margin(nettingSetDetails, productClass, RiskType::CreditVol, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::CreditNonQualifying;
        p = margin(nettingSetDetails, productClass, RiskType::CreditVolNonQ, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::Equity;
        p = margin(nettingSetDetails, productClass, RiskType::EquityVol, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::Commodity;
        p = margin(nettingSetDetails, productClass, RiskType::CommodityVol, crif, side);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        // Curvature margin components for sides call and post
        mt = MarginType::Curvature;
        rc = RiskClass::InterestRate;

        p = irCurvatureMargin(nettingSetDetails, productClass, side, crif);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::FX;
        p = curvatureMargin(nettingSetDetails, productClass, RiskType::FXVol, side, crif, false);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::CreditQualifying;
        p = curvatureMargin(nettingSetDetails, productClass, RiskType::CreditVol, side, crif);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::CreditNonQualifying;
        p = curvatureMargin(nettingSetDetails, productClass, RiskType::CreditVolNonQ, side, crif);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::Equity;
        p = curvatureMargin(nettingSetDetails, productClass, RiskType::EquityVol, side, crif, false);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        rc = RiskClass::Commodity;
        p = curvatureMargin(nettingSetDetails, productClass, RiskType::CommodityVol, side, crif, false);
        if (p.second)
            add(nettingSetDetails, regulations, productClass, rc, mt, p.first, side);

        // Base correlation margin components. This risk type came later so need to check
        // first if it is valid under the configuration
        if (simmConfiguration_->isValidRiskType(RiskType::BaseCorr)) {
            p = margin(nettingSetDetails, productClass, RiskType::BaseCorr, crif, side);
            if (p.second)
                add(nettingSetDetails, regulations, productClass, RiskClass::CreditQualifying, MarginType::BaseCorr,
                    p.first, side);
        }
    }

    // Calculate the higher level margins
    populateResults(side, nettingSetDetails, regulations);

    calcAddMargin(side, nettingSetDetails, regulations, crif);

    timer_.stop(regTimerKey);
}

const Regulation& SimmCalculator::winningRegulations(const SimmSide& side,
                                                                 const NettingSetDetails& nettingSetDetails) const {
    const auto& subWinningRegs = winningRegulations(side);
    QL_REQUIRE(subWinningRegs.find(nettingSetDetails) != subWinningRegs.end(),
               "SimmCalculator::winningRegulations(): Could not find netting set in the list of "
                   << side << " IM winning regulations: " << nettingSetDetails);
    return subWinningRegs.at(nettingSetDetails);
}

const map<NettingSetDetails, Regulation>& SimmCalculator::winningRegulations(const SimmSide& side) const {
    QL_REQUIRE(winningRegulations_.find(side) != winningRegulations_.end(),
               "SimmCalculator::winningRegulations(): Could not find list of" << side << " IM winning regulations");
    return winningRegulations_.at(side);
}

const map<SimmSide, map<NettingSetDetails, Regulation>>& SimmCalculator::winningRegulations() const {
    return winningRegulations_;
}

const SimmResults& SimmCalculator::simmResults(const SimmSide& side, const NettingSetDetails& nettingSetDetails,
                                               const set<Regulation>& regulation) const {
    const auto& subResults = simmResults(side, nettingSetDetails);
    QL_REQUIRE(subResults.find(regulation) != subResults.end(),
               "SimmCalculator::simmResults(): Could not find regulation in the SIMM "
                   << side << " results for netting set [" << nettingSetDetails << "]: " << regulation);
    return subResults.at(regulation);
}

const map<set<Regulation>, SimmResults>& SimmCalculator::simmResults(const SimmSide& side,
                                                            const NettingSetDetails& nettingSetDetails) const {
    const auto& subResults = simmResults(side);
    QL_REQUIRE(subResults.find(nettingSetDetails) != subResults.end(),
               "SimmCalculator::simmResults(): Could not find netting set in the SIMM "
                   << side << " results: " << nettingSetDetails);
    return subResults.at(nettingSetDetails);
}

const map<NettingSetDetails, map<set<Regulation>, SimmResults>>& SimmCalculator::simmResults(const SimmSide& side) const {
    QL_REQUIRE(simmResults_.find(side) != simmResults_.end(),
               "SimmCalculator::simmResults(): Could not find " << side << " IM in the SIMM results");
    return simmResults_.at(side);
}

const map<SimmSide, map<NettingSetDetails, map<set<Regulation>, SimmResults>>>& SimmCalculator::simmResults() const {
    return simmResults_;
}

const pair<Regulation, SimmResults>& SimmCalculator::finalSimmResults(const SimmSide& side,
                                                                  const NettingSetDetails& nettingSetDetails) const {
    const auto& subResults = finalSimmResults(side);
    QL_REQUIRE(subResults.find(nettingSetDetails) != subResults.end(),
               "SimmCalculator::finalSimmResults(): Could not find netting set in the final SIMM "
                   << side << " results: " << nettingSetDetails);
    return subResults.at(nettingSetDetails);
}

const map<NettingSetDetails, pair<Regulation, SimmResults>>& SimmCalculator::finalSimmResults(const SimmSide& side) const {
    QL_REQUIRE(finalSimmResults_.find(side) != finalSimmResults_.end(),
               "SimmCalculator::finalSimmResults(): Could not find " << side << " IM in the final SIMM results");
    return finalSimmResults_.at(side);
}

const map<SimmSide, map<NettingSetDetails, pair<Regulation, SimmResults>>>& SimmCalculator::finalSimmResults() const {
    return finalSimmResults_;
}

pair<map<string, Real>, bool> SimmCalculator::irDeltaMargin(const NettingSetDetails& nettingSetDetails,
                                                            const ProductClass& pc, const Crif& crif,
                                                            const SimmSide& side) const {
    timer_.start("irDeltaMargin()");

    const string& calcCcy = side == SimmSide::Call ? calculationCcyCall_ : calculationCcyPost_;

    // "Bucket" here referse to exposures under the CRIF qualifiers
    map<string, Real> bucketMargins;

    // Get alls IR qualifiers
    set<string> qualifiers =
        getQualifiers(crif, nettingSetDetails, pc, {RiskType::IRCurve, RiskType::XCcyBasis, RiskType::Inflation});

    // If there are no qualifiers, return early and set bool to false to indicate margin does not apply
    if (qualifiers.empty()) {
        bucketMargins["All"] = 0.0;
        timer_.stop("irDeltaMargin()");
        return make_pair(bucketMargins, false);
    }

    // Hold the concentration risk for each qualifier i.e. $CR_b$ from SIMM docs
    map<string, Real> concentrationRisk;
    // The delta margin for each currency i.e. $K_b$ from SIMM docs
    map<string, Real> deltaMargin;
    // The sum of the weighted sensitivities for each currency i.e. $\sum_{i,k} WS_{k,i}$ from SIMM docs
    map<string, Real> sumWeightedSensis;

    // Loop over the qualifiers i.e. currencies
    // Loop over the qualifiers i.e. currencies
    for (const auto& qualifier : qualifiers) {
        // Pair of iterators to start and end of IRCurve sensitivities with current qualifier
        auto pIrQualifier =
            crif.filterByQualifier(nettingSetDetails, pc, RiskType::IRCurve, qualifier);

        // Iterator to Xccy basis element with current qualifier (expect zero or one element)
        auto XccyCount = crif.countMatching(nettingSetDetails, pc, RiskType::XCcyBasis, qualifier);
        QL_REQUIRE(XccyCount < 2, "SIMM Calcuator: Expected either 0 or 1 elements for risk type "
                                      << RiskType::XCcyBasis << " and qualifier " << qualifier << " but got "
                                      << XccyCount);
        const auto& [itXccy, itXccyEnd] = crif.findBy(nettingSetDetails, pc, RiskType::XCcyBasis, qualifier);

        // Iterator to inflation element with current qualifier (expect zero or one element)
        auto inflationCount = crif.countMatching(nettingSetDetails, pc, RiskType::Inflation, qualifier);
        QL_REQUIRE(inflationCount < 2, "SIMM Calculator: Expected either 0 or 1 elements for risk type "
                                           << RiskType::Inflation << " and qualifier " << qualifier << " but got "
                                           << inflationCount);
        const auto& [itInflation, itInflationEnd] = crif.findBy(nettingSetDetails, pc, RiskType::Inflation, qualifier);

        // One pass to get the concentration risk for this qualifier
        // Note: XccyBasis is not included in the calculation of concentration risk and the XccyBasis sensitivity
        //       is not scaled by it
        for (auto it = pIrQualifier.first; it != pIrQualifier.second; ++it) {
            concentrationRisk[qualifier] += it->amountResultCurrency();
        }
        // Add inflation sensitivity to the concentration risk
        if (itInflation != itInflationEnd) {
            concentrationRisk[qualifier] += itInflation->amountResultCurrency();
        }
        // Divide by the concentration risk threshold
        Real concThreshold = simmConfiguration_->concentrationThreshold(RiskType::IRCurve, qualifier);
        if (resultCcy_ != "USD")
            concThreshold *= fxRate("USD" + resultCcy_);
        concentrationRisk[qualifier] /= concThreshold;
        // Final concentration risk amount
        concentrationRisk[qualifier] = std::max(1.0, std::sqrt(std::abs(concentrationRisk[qualifier])));

        // Calculate the delta margin piece for this qualifier i.e. $K_b$ from SIMM docs
        for (auto itOuter = pIrQualifier.first; itOuter != pIrQualifier.second; ++itOuter) {
            // Risk weight i.e. $RW_k$ from SIMM docs
            Real rwOuter = simmConfiguration_->weight(RiskType::IRCurve, qualifier, itOuter->getLabel1());
            // Weighted sensitivity i.e. $WS_{k,i}$ from SIMM docs
            Real wsOuter = rwOuter * itOuter->amountResultCurrency() * concentrationRisk[qualifier];
            // Update weighted sensitivity sum
            sumWeightedSensis[qualifier] += wsOuter;
            // Add diagonal element to delta margin
            deltaMargin[qualifier] += wsOuter * wsOuter;
            // Add the cross elements to the delta margin
            for (auto itInner = pIrQualifier.first; itInner != itOuter; ++itInner) {
                // Label2 level correlation i.e. $\phi_{i,j}$ from SIMM docs
                Real subCurveCorr = simmConfiguration_->correlation(RiskType::IRCurve, qualifier, "", itOuter->getLabel2(),
                                                                    RiskType::IRCurve, qualifier, "", itInner->getLabel2());
                // Label1 level correlation i.e. $\rho_{k,l}$ from SIMM docs
                Real tenorCorr = simmConfiguration_->correlation(RiskType::IRCurve, qualifier, itOuter->getLabel1(), "",
                                                                 RiskType::IRCurve, qualifier, itInner->getLabel1(), "");
                // Add cross element to delta margin
                Real rwInner = simmConfiguration_->weight(RiskType::IRCurve, qualifier, itInner->getLabel1());
                Real wsInner = rwInner * itInner->amountResultCurrency() * concentrationRisk[qualifier];
                deltaMargin[qualifier] += 2 * subCurveCorr * tenorCorr * wsOuter * wsInner;
            }
        }

        // Add the Inflation component, if any
        Real wsInflation = 0.0;
        if (itInflation != itInflationEnd) {
            // Risk weight
            Real rwInflation = simmConfiguration_->weight(RiskType::Inflation, qualifier, itInflation->getLabel1());
            // Weighted sensitivity
            wsInflation = rwInflation * itInflation->amountResultCurrency() * concentrationRisk[qualifier];
            // Update weighted sensitivity sum
            sumWeightedSensis[qualifier] += wsInflation;
            // Add diagonal element to delta margin
            deltaMargin[qualifier] += wsInflation * wsInflation;
            // Add the cross elements (Inflation with IRCurve tenors) to the delta margin
            // Correlation (know that Label1 and Label2 do not matter)
            Real corr = simmConfiguration_->correlation(RiskType::IRCurve, qualifier, "", "", RiskType::Inflation,
                                                        qualifier, "", "");
            for (auto it = pIrQualifier.first; it != pIrQualifier.second; ++it) {
                // Add cross element to delta margin
                Real rw = simmConfiguration_->weight(RiskType::IRCurve, qualifier, it->getLabel1());
                Real ws = rw * it->amountResultCurrency() * concentrationRisk[qualifier];
                deltaMargin[qualifier] += 2 * corr * ws * wsInflation;
            }
        }

        // Add the XccyBasis component, if any
        if (itXccy != itXccyEnd) {
            // Risk weight
            Real rwXccy = simmConfiguration_->weight(RiskType::XCcyBasis, qualifier, itXccy->getLabel1());
            // Weighted sensitivity (no concentration risk here)
            Real wsXccy = rwXccy * itXccy->amountResultCurrency();
            // Update weighted sensitivity sum
            sumWeightedSensis[qualifier] += wsXccy;
            // Add diagonal element to delta margin
            deltaMargin[qualifier] += wsXccy * wsXccy;
            // Add the cross elements (XccyBasis with IRCurve tenors) to the delta margin
            // Correlation (know that Label1 and Label2 do not matter)
            Real corr = simmConfiguration_->correlation(RiskType::IRCurve, qualifier, "", "", RiskType::XCcyBasis,
                                                        qualifier, "", "");
            for (auto it = pIrQualifier.first; it != pIrQualifier.second; ++it) {
                // Add cross element to delta margin
                Real rw = simmConfiguration_->weight(RiskType::IRCurve, qualifier, it->getLabel1(), calcCcy);
                Real ws = rw * it->amountResultCurrency() * concentrationRisk[qualifier];
                deltaMargin[qualifier] += 2 * corr * ws * wsXccy;
            }

            // Inflation vs. XccyBasis cross component if any
            if (itInflation != itInflationEnd) {
                // Correlation (know that Label1 and Label2 do not matter)
                Real corr = simmConfiguration_->correlation(RiskType::Inflation, qualifier, "", "", RiskType::XCcyBasis,
                                                            qualifier, "", "");
                deltaMargin[qualifier] += 2 * corr * wsInflation * wsXccy;
            }
        }

        // Finally have the value of $K_b$
        deltaMargin[qualifier] = std::sqrt(std::max(deltaMargin[qualifier], 0.0));
    }

    // Now calculate final IR delta margin by aggregating across currencies
    Real margin = 0.0;
    for (auto itOuter = qualifiers.begin(); itOuter != qualifiers.end(); ++itOuter) {
        // Diagonal term
        margin += deltaMargin.at(*itOuter) * deltaMargin.at(*itOuter);
        // Cross terms
        Real sOuter = std::max(std::min(sumWeightedSensis.at(*itOuter), deltaMargin.at(*itOuter)), -deltaMargin.at(*itOuter));
        for (auto itInner = qualifiers.begin(); itInner != itOuter; ++itInner) {
            Real sInner = std::max(std::min(sumWeightedSensis.at(*itInner), deltaMargin.at(*itInner)), -deltaMargin.at(*itInner));
            Real g = std::min(concentrationRisk.at(*itOuter), concentrationRisk.at(*itInner)) /
                     std::max(concentrationRisk.at(*itOuter), concentrationRisk.at(*itInner));
            Real corr = simmConfiguration_->correlation(RiskType::IRCurve, *itOuter, "", "", RiskType::IRCurve,
                                                        *itInner, "", "");
            margin += 2.0 * sOuter * sInner * corr * g;
        }
    }
    margin = std::sqrt(std::max(margin, 0.0));

    for (const auto& m : deltaMargin)
        bucketMargins[m.first] = m.second;
    bucketMargins["All"] = margin;

    timer_.stop("irDeltaMargin()");

    return make_pair(bucketMargins, true);
}

pair<map<string, Real>, bool> SimmCalculator::irVegaMargin(const NettingSetDetails& nettingSetDetails,
                                                           const ProductClass& pc, const Crif& crif,
                                                           const SimmSide& side) const {

    timer_.start("irVegaMargin()");

    const string& calcCcy = side == SimmSide::Call ? calculationCcyCall_ : calculationCcyPost_;

    // "Bucket" here refers to exposures under the CRIF qualifiers
    map<string, Real> bucketMargins;

    // Find the set of qualifiers, i.e. currencies, in the Simm sensitivities
    set<string> qualifiers = getQualifiers(crif, nettingSetDetails, pc, {RiskType::IRVol, RiskType::InflationVol});

    // If there are no qualifiers, return early and set bool to false to indicate margin does not apply
    if (qualifiers.empty()) {
        bucketMargins["All"] = 0.0;
        timer_.stop("irVegaMargin()");
        return make_pair(bucketMargins, false);
    }

    // Hold the concentration risk for each qualifier i.e. $VCR_b$ from SIMM docs
    map<string, Real> concentrationRisk;
    // The vega margin for each currency i.e. $K_b$ from SIMM docs
    map<string, Real> vegaMargin;
    // The sum of the weighted sensitivities for each currency i.e. $\sum_{k=1}^K VR_{k}$ from SIMM docs
    map<string, Real> sumWeightedSensis;

    // Loop over the qualifiers i.e. currencies
    for (const auto& qualifier : qualifiers) {
        // Pair of iterators to start and end of IRVol sensitivities with current qualifier
        auto pIrQualifier = crif.filterByQualifier(nettingSetDetails, pc, RiskType::IRVol, qualifier);

        // Pair of iterators to start and end of InflationVol sensitivities with current qualifier
        auto pInfQualifier = crif.filterByQualifier(nettingSetDetails, pc, RiskType::InflationVol, qualifier);

        // One pass to get the concentration risk for this qualifier
        for (auto it = pIrQualifier.first; it != pIrQualifier.second; ++it) {
            concentrationRisk[qualifier] += it->amountResultCurrency();
        }
        for (auto it = pInfQualifier.first; it != pInfQualifier.second; ++it) {
            concentrationRisk[qualifier] += it->amountResultCurrency();
        }
        // Divide by the concentration risk threshold
        Real concThreshold = simmConfiguration_->concentrationThreshold(RiskType::IRVol, qualifier);
        if (resultCcy_ != "USD")
            concThreshold *= fxRate("USD" + resultCcy_);
        concentrationRisk[qualifier] /= concThreshold;

        // Final concentration risk amount
        concentrationRisk[qualifier] = std::max(1.0, std::sqrt(std::abs(concentrationRisk[qualifier])));

        // Calculate the margin piece for this qualifier i.e. $K_b$ from SIMM docs
        // Start with IRVol vs. IRVol components
        for (auto itOuter = pIrQualifier.first; itOuter != pIrQualifier.second; ++itOuter) {
            // Risk weight i.e. $RW_k$ from SIMM docs
            Real rwOuter = simmConfiguration_->weight(RiskType::IRVol, qualifier, itOuter->getLabel1());
            // Weighted sensitivity i.e. $WS_{k,i}$ from SIMM docs
            Real wsOuter = rwOuter * itOuter->amountResultCurrency() * concentrationRisk[qualifier];
            // Update weighted sensitivity sum
            sumWeightedSensis[qualifier] += wsOuter;
            // Add diagonal element to vega margin
            vegaMargin[qualifier] += wsOuter * wsOuter;
            // Add the cross elements to the vega margin
            for (auto itInner = pIrQualifier.first; itInner != itOuter; ++itInner) {
                // Label1 level correlation i.e. $\rho_{k,l}$ from SIMM docs
                Real corr = simmConfiguration_->correlation(RiskType::IRVol, qualifier, itOuter->getLabel1(), "",
                                                            RiskType::IRVol, qualifier, itInner->getLabel1(), "");
                // Add cross element to vega margin
                Real rwInner = simmConfiguration_->weight(RiskType::IRVol, qualifier, itInner->getLabel1());
                Real wsInner = rwInner * itInner->amountResultCurrency() * concentrationRisk[qualifier];
                vegaMargin[qualifier] += 2 * corr * wsOuter * wsInner;
            }
        }

        // Now deal with inflation component
        // To be generic/future-proof, assume that we don't know correlation structure. The way SIMM is
        // currently, we could just sum over the InflationVol numbers within qualifier and use this.
        for (auto itOuter = pInfQualifier.first; itOuter != pInfQualifier.second; ++itOuter) {
            // Risk weight i.e. $RW_k$ from SIMM docs
            Real rwOuter = simmConfiguration_->weight(RiskType::InflationVol, qualifier, itOuter->getLabel1());
            // Weighted sensitivity i.e. $WS_{k,i}$ from SIMM docs
            Real wsOuter = rwOuter * itOuter->amountResultCurrency() * concentrationRisk[qualifier];
            // Update weighted sensitivity sum
            sumWeightedSensis[qualifier] += wsOuter;
            // Add diagonal element to vega margin
            vegaMargin[qualifier] += wsOuter * wsOuter;
            // Add the cross elements to the vega margin
            // Firstly, against all IRVol components
            for (auto itInner = pIrQualifier.first; itInner != pIrQualifier.second; ++itInner) {
                // Correlation i.e. $\rho_{k,l}$ from SIMM docs
                Real corr = simmConfiguration_->correlation(RiskType::InflationVol, qualifier, itOuter->getLabel1(), "",
                                                            RiskType::IRVol, qualifier, itInner->getLabel1(), "");
                // Add cross element to vega margin
                Real rwInner = simmConfiguration_->weight(RiskType::IRVol, qualifier, itInner->getLabel1());
                Real wsInner = rwInner * itInner->amountResultCurrency() * concentrationRisk[qualifier];
                vegaMargin[qualifier] += 2 * corr * wsOuter * wsInner;
            }
            // Secondly, against all previous InflationVol components
            for (auto itInner = pInfQualifier.first; itInner != itOuter; ++itInner) {
                // Correlation i.e. $\rho_{k,l}$ from SIMM docs
                Real corr = simmConfiguration_->correlation(RiskType::InflationVol, qualifier, itOuter->getLabel1(), "",
                                                            RiskType::InflationVol, qualifier, itInner->getLabel1(), "");
                // Add cross element to vega margin
                Real rwInner = simmConfiguration_->weight(RiskType::InflationVol, qualifier, itInner->getLabel1());
                Real wsInner = rwInner * itInner->amountResultCurrency() * concentrationRisk[qualifier];
                vegaMargin[qualifier] += 2 * corr * wsOuter * wsInner;
            }
        }

        // Finally have the value of $K_b$
        vegaMargin[qualifier] = std::sqrt(std::max(vegaMargin[qualifier], 0.0));
    }

    // Now calculate final vega margin by aggregating across currencies
    Real margin = 0.0;
    for (auto itOuter = qualifiers.begin(); itOuter != qualifiers.end(); ++itOuter) {
        // Diagonal term
        margin += vegaMargin.at(*itOuter) * vegaMargin.at(*itOuter);
        // Cross terms
        Real sOuter =
            std::max(std::min(sumWeightedSensis.at(*itOuter), vegaMargin.at(*itOuter)), -vegaMargin.at(*itOuter));
        for (auto itInner = qualifiers.begin(); itInner != itOuter; ++itInner) {
            Real sInner =
                std::max(std::min(sumWeightedSensis.at(*itInner), vegaMargin.at(*itInner)), -vegaMargin.at(*itInner));
            Real g = std::min(concentrationRisk.at(*itOuter), concentrationRisk.at(*itInner)) /
                     std::max(concentrationRisk.at(*itOuter), concentrationRisk.at(*itInner));
            Real corr = simmConfiguration_->correlation(RiskType::IRVol, *itOuter, "", "", RiskType::IRVol, *itInner,
                                                        "", "", calcCcy);
            margin += 2.0 * sOuter * sInner * corr * g;
        }
    }
    margin = std::sqrt(std::max(margin, 0.0));

    for (const auto& m : vegaMargin)
        bucketMargins[m.first] = m.second;
    bucketMargins["All"] = margin;

    timer_.stop("irVegaMargin()");

    return make_pair(bucketMargins, true);
}

pair<map<string, Real>, bool> SimmCalculator::irCurvatureMargin(const NettingSetDetails& nettingSetDetails,
                                                                const ProductClass& pc,
                                                                const SimmSide& side, const Crif& crif) const {
    timer_.start("irCurvatureMargin()");

    // "Bucket" here refers to exposures under the CRIF qualifiers
    map<string, Real> bucketMargins;

    // Multiplier for sensitivities, -1 if SIMM side is Post
    Real multiplier = side == SimmSide::Call ? 1.0 : -1.0;

    // Find the set of qualifiers, i.e. currencies, in the Simm sensitivities
    set<string> qualifiers = getQualifiers(crif, nettingSetDetails, pc, {RiskType::IRVol, RiskType::InflationVol});

    // If there are no qualifiers, return early and set bool to false to indicate margin does not apply
    if (qualifiers.empty()) {
        bucketMargins["All"] = 0.0;
        timer_.stop("irCurvatureMargin()");
        return make_pair(bucketMargins, false);
    }

    // The curvature margin for each currency i.e. $K_b$ from SIMM docs
    map<string, Real> curvatureMargin;
    // The sum of the weighted sensitivities for each currency i.e. $\sum_{k}^K CVR_{b,k}$ from SIMM docs
    map<string, Real> sumWeightedSensis;
    // The sum of all weighted sensitivities across currencies and risk factors
    Real sumWs = 0.0;
    // The sum of the absolute value of weighted sensitivities across currencies and risk factors
    Real sumAbsWs = 0.0;

    // Loop over the qualifiers i.e. currencies
    for (const auto& qualifier : qualifiers) {
        // Pair of iterators to start and end of IRVol sensitivities with current qualifier
        auto pIrQualifier = crif.filterByQualifier(nettingSetDetails, pc, RiskType::IRVol, qualifier);

        // Pair of iterators to start and end of InflationVol sensitivities with current qualifier
        auto pInfQualifier =
            crif.filterByQualifier(nettingSetDetails, pc, RiskType::InflationVol, qualifier);

        // Calculate the margin piece for this qualifier i.e. $K_b$ from SIMM docs
        // Start with IRVol vs. IRVol components
        for (auto itOuter = pIrQualifier.first; itOuter != pIrQualifier.second; ++itOuter) {
            // Curvature weight i.e. $SF(t_{kj})$ from SIMM docs
            Real sfOuter = simmConfiguration_->curvatureWeight(RiskType::IRVol, itOuter->getLabel1());
            // Curvature sensitivity i.e. $CVR_{ik}$ from SIMM docs
            Real wsOuter = sfOuter * (itOuter->amountResultCurrency() * multiplier);
            // Update weighted sensitivity sums
            sumWeightedSensis[qualifier] += wsOuter;
            sumWs += wsOuter;
            sumAbsWs += std::abs(wsOuter);
            // Add diagonal element to curvature margin
            curvatureMargin[qualifier] += wsOuter * wsOuter;
            // Add the cross elements to the curvature margin
            for (auto itInner = pIrQualifier.first; itInner != itOuter; ++itInner) {
                // Label1 level correlation i.e. $\rho_{k,l}$ from SIMM docs
                Real corr = simmConfiguration_->correlation(RiskType::IRVol, qualifier, itOuter->getLabel1(), "",
                                                            RiskType::IRVol, qualifier, itInner->getLabel1(), "");
                // Add cross element to curvature margin
                Real sfInner = simmConfiguration_->curvatureWeight(RiskType::IRVol, itInner->getLabel1());
                Real wsInner = sfInner * (itInner->amountResultCurrency() * multiplier);
                curvatureMargin[qualifier] += 2 * corr * corr * wsOuter * wsInner;
            }
        }

        // Now deal with inflation component
        const string simmVersion = simmConfiguration_->version();
        SimmVersion thresholdVersion = SimmVersion::V1_0;
        if (simmConfiguration_->isSimmConfigCalibration() || parseSimmVersion(simmVersion) > thresholdVersion) {
            // Weighted sensitivity i.e. $WS_{k,i}$ from SIMM docs
            Real infWs = 0.0;
            for (auto infIt = pInfQualifier.first; infIt != pInfQualifier.second; ++infIt) {
                // Curvature weight i.e. $SF(t_{kj})$ from SIMM docs
                Real infSf = simmConfiguration_->curvatureWeight(RiskType::InflationVol, infIt->getLabel1());
                infWs += infSf * (infIt->amountResultCurrency() * multiplier);
            }
            // Update weighted sensitivity sums
            sumWeightedSensis[qualifier] += infWs;
            sumWs += infWs;
            sumAbsWs += std::abs(infWs);

            // Add diagonal element to curvature margin - there is only one element for inflationVol
            curvatureMargin[qualifier] += infWs * infWs;

            // Add the cross elements to the curvature margin against IRVol components.
            // There are no cross elements against InflationVol since we only have one element.
            for (auto irIt = pIrQualifier.first; irIt != pIrQualifier.second; ++irIt) {
                // Correlation i.e. $\rho_{k,l}$ from SIMM docs
                Real corr = simmConfiguration_->correlation(RiskType::InflationVol, qualifier, "", "", RiskType::IRVol,
                                                            qualifier, irIt->getLabel1(), "");
                // Add cross element to curvature margin
                Real irSf = simmConfiguration_->curvatureWeight(RiskType::IRVol, irIt->getLabel1());
                Real irWs = irSf * (irIt->amountResultCurrency() * multiplier);
                curvatureMargin[qualifier] += 2 * corr * corr * infWs * irWs;
            }
        }

        // Finally have the value of $K_b$
        curvatureMargin[qualifier] = std::sqrt(std::max(curvatureMargin[qualifier], 0.0));
    }

    // If sum of absolute value of all individual curvature risks is zero, we can return 0.0
    if (close_enough(sumAbsWs, 0.0)) {
        bucketMargins["All"] = 0.0;
        timer_.stop("irCurvatureMargin()");
        return make_pair(bucketMargins, true);
    }

    // Now calculate final curvature margin by aggregating across currencies
    Real theta = std::min(sumWs / sumAbsWs, 0.0);

    Real margin = 0.0;
    for (auto itOuter = qualifiers.begin(); itOuter != qualifiers.end(); ++itOuter) {
        // Diagonal term
        margin += curvatureMargin.at(*itOuter) * curvatureMargin.at(*itOuter);
        // Cross terms
        Real sOuter = std::max(std::min(sumWeightedSensis.at(*itOuter), curvatureMargin.at(*itOuter)),
                               -curvatureMargin.at(*itOuter));
        for (auto itInner = qualifiers.begin(); itInner != itOuter; ++itInner) {
            Real sInner = std::max(std::min(sumWeightedSensis.at(*itInner), curvatureMargin.at(*itInner)),
                                   -curvatureMargin.at(*itInner));
            Real corr =
                simmConfiguration_->correlation(RiskType::IRVol, *itOuter, "", "", RiskType::IRVol, *itInner, "", "");
            margin += 2.0 * sOuter * sInner * corr * corr;
        }
    }
    margin = sumWs + lambda(theta) * std::sqrt(std::max(margin, 0.0));

    for (const auto& m : curvatureMargin)
        bucketMargins[m.first] = m.second;

    Real scaling = simmConfiguration_->curvatureMarginScaling();
    Real totalCurvatureMargin = scaling * std::max(margin, 0.0);
    // TODO: Review, should we return the pre-scaled value instead?
    bucketMargins["All"] = totalCurvatureMargin;

    timer_.stop("irCurvatureMargin()");

    return make_pair(bucketMargins, true);
}

pair<map<string, Real>, bool> SimmCalculator::margin(const NettingSetDetails& nettingSetDetails, const ProductClass& pc,
                                                     const RiskType& rt, const Crif& crif, const SimmSide& side) const {
    timer_.start("margin()");

    const string& calcCcy = side == SimmSide::Call ? calculationCcyCall_ : calculationCcyPost_;
    
    // "Bucket" here refers to exposures under the CRIF qualifiers for FX (and IR) risk class, and CRIF buckets for
    // every other risk class.
    // For FX Delta margin, this refers to WS_k in Section B. "Structure of the methodology", 8.(b).
    // For FX Vega margin, this refers to VR_k in Section B., 10.(d).
    // For other risk type, the bucket margin is K_b in the corresponding subsections.
    map<string, Real> bucketMargins;

    bool riskClassIsFX = rt == RiskType::FX || rt == RiskType::FXVol;

    // precomputed
    map<std::pair<std::string,std::string>, vector<CrifRecord>> crifByQualifierAndBucket;
    map<std::string, vector<CrifRecord>> crifByBucket;

    // Find the set of buckets and associated qualifiers for the netting set details, product class and risk type
    map<string, set<string>> buckets;
    auto pIt = crif.filterBy(nettingSetDetails, pc, rt);
    for (auto sit = pIt.first; sit != pIt.second; sit++) {
        CrifRecord it = sit->toCrifRecord();
        buckets[it.bucket].insert(it.qualifier);
        crifByQualifierAndBucket[std::make_pair(it.qualifier,it.bucket)].push_back(it);
        crifByBucket[it.bucket].push_back(it);
    }

    // If there are no buckets, return early and set bool to false to indicate margin does not apply
    if (buckets.empty()) {
        bucketMargins["All"] = 0.0;
        timer_.stop("margin()");
        return make_pair(bucketMargins, false);
    }

    // The margin for each bucket i.e. $K_b$ from SIMM docs
    map<string, Real> bucketMargin;
    // The sum of the weighted sensitivities for each bucket i.e. $\sum_{k=1}^{K} WS_{k}$ from SIMM docs
    map<string, Real> sumWeightedSensis;
    // The historical volatility ratio for the risk type - will be 1.0 if not applicable
    Real hvr = simmConfiguration_->historicalVolatilityRatio(rt);

    // Loop over the buckets
    for (const auto& kv : buckets) {
        string bucket = kv.first;

        // Initialise sumWeightedSensis here to ensure it is not empty in the later calculations
        sumWeightedSensis[bucket] = 0.0;

        // Get the concentration risk for each qualifier in current bucket i.e. $CR_k$ from SIMM docs
        map<string, Real> concentrationRisk;

        for (const auto& qualifier : kv.second) {

            // Do not include Risk_FX components in the calculation currency in the SIMM calculation
            if (rt == RiskType::FX && qualifier == calcCcy) {
                if (!quiet_) {
                    DLOG("Not calculating concentration risk for qualifier "
                         << qualifier << " of risk type " << rt
                         << " since the qualifier equals the SIMM calculation currency " << calcCcy);
                }
                continue;
            }

            // Pair of iterators to start and end of sensitivities with current qualifier
            auto pQualifier = crifByQualifierAndBucket[std::make_pair(qualifier,bucket)];

            // One pass to get the concentration risk for this qualifier
            for (auto it = pQualifier.begin(); it != pQualifier.end(); ++it) {
                // Get the sigma value if applicable - returns 1.0 if not applicable
                Real sigma = simmConfiguration_->sigma(rt, it->qualifier, it->label1, calcCcy);
                concentrationRisk[qualifier] += it->amountResultCcy * sigma * hvr;
            }
            // Divide by the concentration risk threshold
            Real concThreshold = simmConfiguration_->concentrationThreshold(rt, qualifier);
            if (resultCcy_ != "USD")
                concThreshold *= fxRate("USD" + resultCcy_);
            concentrationRisk[qualifier] /= concThreshold;
            // Final concentration risk amount
            concentrationRisk[qualifier] = std::max(1.0, std::sqrt(std::abs(concentrationRisk[qualifier])));
        }


        // Calculate the margin component for the current bucket
        // Pair of iterators to start and end of sensitivities within current bucket
        auto pBucket = crifByBucket[bucket];
        for (auto itOuter = pBucket.begin(); itOuter != pBucket.end(); ++itOuter) {
            // Do not include Risk_FX components in the calculation currency in the SIMM calculation
            if (rt == RiskType::FX && itOuter->qualifier == calcCcy) {
                if (!quiet_) {
                    DLOG("Skipping qualifier " << itOuter->qualifier << " of risk type " << rt
                                               << " since the qualifier equals the SIMM calculation currency "
                                               << calcCcy);
                }
                continue;
            }
            // Risk weight i.e. $RW_k$ from SIMM docs
            Real rwOuter = simmConfiguration_->weight(rt, itOuter->qualifier, itOuter->label1, calcCcy);
            // Get the sigma value if applicable - returns 1.0 if not applicable
            Real sigmaOuter = simmConfiguration_->sigma(rt, itOuter->qualifier, itOuter->label1, calcCcy);
            // Weighted sensitivity i.e. $WS_{k}$ from SIMM docs
            Real wsOuter =
                rwOuter * (itOuter->amountResultCcy * sigmaOuter * hvr) * concentrationRisk[itOuter->qualifier];
            // Get concentration risk for outer qualifier
            Real outerConcentrationRisk = concentrationRisk.at(itOuter->qualifier);
            // Update weighted sensitivity sum
            sumWeightedSensis[bucket] += wsOuter;
            // Add diagonal element to bucket margin
            bucketMargin[bucket] += wsOuter * wsOuter;
            // Add the cross elements to the bucket margin
            for (auto itInner = pBucket.begin(); itInner != itOuter; ++itInner) {
                // Do not include Risk_FX components in the calculation currency in the SIMM calculation
                if (rt == RiskType::FX && itInner->qualifier == calcCcy) {
                    if (!quiet_) {
                        DLOG("Skipping qualifier " << itInner->qualifier << " of risk type " << rt
                                                   << " since the qualifier equals the SIMM calculation currency "
                                                   << calcCcy);
                    }
                    continue;
                }
                // Correlation, $\rho_{k,l}$ in the SIMM docs
                Real corr =
                    simmConfiguration_->correlation(rt, itOuter->qualifier, itOuter->label1, itOuter->label2, rt,
                                                    itInner->qualifier, itInner->label1, itInner->label2, calcCcy);
                // $f_{k,l}$ from the SIMM docs
                Real f = std::min(outerConcentrationRisk, concentrationRisk.at(itInner->qualifier)) /
                         std::max(outerConcentrationRisk, concentrationRisk.at(itInner->qualifier));
                // Add cross element to delta margin
                Real sigmaInner = simmConfiguration_->sigma(rt, itInner->qualifier, itInner->label1, calcCcy);
                Real rwInner = simmConfiguration_->weight(rt, itInner->qualifier, itInner->label1, calcCcy);
                Real wsInner =
                    rwInner * (itInner->amountResultCcy * sigmaInner * hvr) * concentrationRisk[itInner->qualifier];
                bucketMargin[bucket] += 2 * corr * f * wsOuter * wsInner;
            }
            // For FX risk class, results are broken down by qualifier, i.e. currency, instead of bucket, which is not used for Risk_FX
            if (riskClassIsFX)
                bucketMargins[itOuter->qualifier] += wsOuter;
        }

        // Finally have the value of $K_b$
        bucketMargin[bucket] = std::sqrt(std::max(bucketMargin[bucket], 0.0));
    }

    // If there is a "Residual" bucket entry store it separately
    // This is $K_{residual}$ from SIMM docs
    Real residualMargin = 0.0;
    if (bucketMargin.count("Residual") > 0) {
        residualMargin = bucketMargin.at("Residual");
        bucketMargin.erase("Residual");
    }

    // Now calculate final margin by aggregating across non-residual buckets
    Real margin = 0.0;
    for (auto itOuter = bucketMargin.begin(); itOuter != bucketMargin.end(); ++itOuter) {
        string outerBucket = itOuter->first;
        // Diagonal term, $K_b^2$ from SIMM docs
        margin += itOuter->second * itOuter->second;
        // Cross terms
        // $S_b$ from SIMM docs
        Real sOuter = std::max(std::min(sumWeightedSensis.at(outerBucket), itOuter->second), -itOuter->second);
        for (auto itInner = bucketMargin.begin(); itInner != itOuter; ++itInner) {
            string innerBucket = itInner->first;
            // $S_c$ from SIMM docs
            Real sInner = std::max(std::min(sumWeightedSensis.at(innerBucket), itInner->second), -itInner->second);
            // $\gamma_{b,c}$ from SIMM docs
            // Interface to SimmConfiguration is on qualifiers => take any qualifier from each
            // of the respective (different) buckets to get the inter-bucket correlation
            string innerQualifier = *buckets.at(innerBucket).begin();
            string outerQualifier = *buckets.at(outerBucket).begin();
            Real corr = simmConfiguration_->correlation(rt, outerQualifier, "", "", rt, innerQualifier, "", "", calcCcy);
            margin += 2.0 * sOuter * sInner * corr;
        }
    }
    margin = std::sqrt(std::max(margin, 0.0));

    // Now add the residual component back in
    margin += residualMargin;
    if (!close_enough(residualMargin, 0.0))
        bucketMargins["Residual"] = residualMargin;

    // For non-FX risk class, results are broken down by buckets
    if (!riskClassIsFX)
        for (const auto& m : bucketMargin)
            bucketMargins[m.first] = m.second;
    else
        for (auto& m : bucketMargins)
            m.second = std::abs(m.second);

    bucketMargins["All"] = margin;
    timer_.stop("margin()");
    return make_pair(bucketMargins, true);
}

pair<map<string, Real>, bool> SimmCalculator::curvatureMargin(const NettingSetDetails& nettingSetDetails,
                                                              const ProductClass& pc, const RiskType& rt,
                                                              const SimmSide& side, const Crif& crif,
                                                              bool rfLabels) const {

    timer_.start("curvatureMargin()");

    const string& calcCcy = side == SimmSide::Call ? calculationCcyCall_ : calculationCcyPost_;

    // "Bucket" here refers to exposures under the CRIF qualifiers for FX (and IR) risk class, and CRIF buckets for
    // every other risk class
    // For FX Curvature marrgin, this refers to CVR_{b,k} in Section B. "Structure of the methodology", 11.(c).
    // For other risk types, the bucket margin is K_b in the corresponding subsection.
    map<string, Real> bucketMargins;

    bool riskClassIsFX = rt == RiskType::FX || rt == RiskType::FXVol;

    // Multiplier for sensitivities, -1 if SIMM side is Post
    Real multiplier = side == SimmSide::Call ? 1.0 : -1.0;

    // Find the set of buckets and associated qualifiers for the netting set details, product class and risk type
    map<string, set<string>> buckets;
    auto pIt = crif.filterBy(nettingSetDetails, pc, rt);
    for (auto sit = pIt.first; sit != pIt.second; sit++) {
        CrifRecord it = sit->toCrifRecord();
        buckets[it.bucket].insert(it.qualifier);
    }

    // If there are no buckets, return early and set bool to false to indicate margin does not apply
    if (buckets.empty()) {
        bucketMargins["All"] = 0.0;
        timer_.stop("curvatureMargin()");
        return make_pair(bucketMargins, false);
    }

    // The curvature margin for each bucket i.e. $K_b$ from SIMM docs
    map<string, Real> curvatureMargin;
    // The sum of the weighted (and absolute weighted) sensitivities for each bucket
    // i.e. $\sum_{k}^K CVR_{b,k}$ from SIMM docs
    map<string, Real> sumWeightedSensis;
    map<string, map<string, Real>> sumAbsTemp;
    map<string, Real> sumAbsWeightedSensis;

    // Loop over the buckets
    for (const auto& kv : buckets) {
        string bucket = kv.first;
        sumAbsTemp[bucket] = {};

        // Calculate the margin component for the current bucket
        // Pair of iterators to start and end of sensitivities within current bucket
        auto pBucket = crif.filterByBucket(nettingSetDetails, pc, rt, bucket);
        for (auto itOuter = pBucket.first; itOuter != pBucket.second; ++itOuter) {
            // Curvature weight i.e. $SF(t_{kj})$ from SIMM docs
            Real sfOuter = simmConfiguration_->curvatureWeight(rt, itOuter->getLabel1());
            // Get the sigma value if applicable - returns 1.0 if not applicable
            Real sigmaOuter = simmConfiguration_->sigma(rt, itOuter->getQualifier(), itOuter->getLabel1(), calcCcy);
            // Weighted curvature i.e. $CVR_{ik}$ from SIMM docs
            // WARNING: The order of multiplication here is important because unit tests fail if for
            //          example you use sfOuter * (itOuter->amountResultCurrency() * multiplier) * sigmaOuter;
            Real wsOuter = sfOuter * ((itOuter->amountResultCurrency() * multiplier) * sigmaOuter);
            // for ISDA SIMM 2.2 or higher, this $CVR_{ik}$ for EQ bucket 12 is zero
            const string simmVersion = simmConfiguration_->version();
            SimmVersion thresholdVersion = SimmVersion::V2_2;
            if ((simmConfiguration_->isSimmConfigCalibration() || parseSimmVersion(simmVersion) >= thresholdVersion) &&
                bucket == "12" && rt == RiskType::EquityVol) {
                wsOuter = 0.0;
            }
            // Update weighted sensitivity sum
            sumWeightedSensis[bucket] += wsOuter;
            sumAbsTemp[bucket][itOuter->getQualifier()] += rfLabels ? std::abs(wsOuter) : wsOuter;
            // Add diagonal element to curvature margin
            curvatureMargin[bucket] += wsOuter * wsOuter;
            // Add the cross elements to the curvature margin
            for (auto itInner = pBucket.first; itInner != itOuter; ++itInner) {
                // Correlation, $\rho_{k,l}$ in the SIMM docs
                Real corr =
                    simmConfiguration_->correlation(rt, itOuter->getQualifier(), itOuter->getLabel1(), itOuter->getLabel2(), rt,
                                                    itInner->getQualifier(), itInner->getLabel1(), itInner->getLabel2(), calcCcy);
                // Add cross element to delta margin
                Real sfInner = simmConfiguration_->curvatureWeight(rt, itInner->getLabel1());
                Real sigmaInner = simmConfiguration_->sigma(rt, itInner->getQualifier(), itInner->getLabel1(), calcCcy);
                Real wsInner = sfInner * ((itInner->amountResultCurrency() * multiplier) * sigmaInner);
                curvatureMargin[bucket] += 2 * corr * corr * wsOuter * wsInner;
            }
            // For FX risk class, results are broken down by qualifier, i.e. currency, instead of bucket, which is not
            // used for Risk_FX
            if (riskClassIsFX)
                bucketMargins[itOuter->getQualifier()] += wsOuter;
        }

        // Finally have the value of $K_b$
        Real bucketCurvatureMargin = std::sqrt(std::max(curvatureMargin[bucket], 0.0));
        curvatureMargin[bucket] = bucketCurvatureMargin;

        // Bucket level absolute sensitivity
        for (const auto& kv : sumAbsTemp[bucket]) {
            sumAbsWeightedSensis[bucket] += std::abs(kv.second);
        }
    }

    // If there is a "Residual" bucket entry store it separately
    // This is $K_{residual}$ from SIMM docs
    Real residualMargin = 0.0;
    Real residualSum = 0.0;
    Real residualAbsSum = 0.0;
    if (curvatureMargin.count("Residual") > 0) {
        residualMargin = curvatureMargin.at("Residual");
        residualSum = sumWeightedSensis.at("Residual");
        residualAbsSum = sumAbsWeightedSensis.at("Residual");
        // Remove the entries for "Residual" bucket
        curvatureMargin.erase("Residual");
        sumWeightedSensis.erase("Residual");
        sumAbsWeightedSensis.erase("Residual");
    }

    // Now calculate final margin
    Real margin = 0.0;

    // First, aggregating across non-residual buckets
    auto acc = [](const Real p, const pair<const string, Real>& kv) { return p + kv.second; };
    Real sumSensis = accumulate(sumWeightedSensis.begin(), sumWeightedSensis.end(), 0.0, acc);
    Real sumAbsSensis = accumulate(sumAbsWeightedSensis.begin(), sumAbsWeightedSensis.end(), 0.0, acc);

    if (!close_enough(sumAbsSensis, 0.0)) {
        Real theta = std::min(sumSensis / sumAbsSensis, 0.0);
        for (auto itOuter = curvatureMargin.begin(); itOuter != curvatureMargin.end(); ++itOuter) {
            string outerBucket = itOuter->first;
            // Diagonal term
            margin += itOuter->second * itOuter->second;
            // Cross terms
            // $S_b$ from SIMM docs
            Real sOuter = std::max(std::min(sumWeightedSensis.at(outerBucket), itOuter->second), -itOuter->second);
            for (auto itInner = curvatureMargin.begin(); itInner != itOuter; ++itInner) {
                string innerBucket = itInner->first;
                // $S_c$ from SIMM docs
                Real sInner = std::max(std::min(sumWeightedSensis.at(innerBucket), itInner->second), -itInner->second);
                // $\gamma_{b,c}$ from SIMM docs
                // Interface to SimmConfiguration is on qualifiers => take any qualifier from each
                // of the respective (different) buckets to get the inter-bucket correlation
                string innerQualifier = *buckets.at(innerBucket).begin();
                string outerQualifier = *buckets.at(outerBucket).begin();
                Real corr =
                    simmConfiguration_->correlation(rt, outerQualifier, "", "", rt, innerQualifier, "", "", calcCcy);
                margin += 2.0 * sOuter * sInner * corr * corr;
            }
        }
        margin = std::max(sumSensis + lambda(theta) * std::sqrt(std::max(margin, 0.0)), 0.0);
    }

    // Second, the residual bucket if necessary, and add "Residual" bucket back in to be added to the SIMM results
    if (!close_enough(residualAbsSum, 0.0)) {
        Real theta = std::min(residualSum / residualAbsSum, 0.0);
        curvatureMargin["Residual"] = std::max(residualSum + lambda(theta) * residualMargin, 0.0);
        margin += curvatureMargin["Residual"];
    }

    // For non-FX risk class, results are broken down by buckets
    if (!riskClassIsFX)
        for (const auto& m : curvatureMargin)
            bucketMargins[m.first] = m.second;
    else
        for (auto& m : bucketMargins)
            m.second = std::abs(m.second);

    bucketMargins["All"] = margin;
    timer_.stop("curvatureMargin()");
    return make_pair(bucketMargins, true);
}

void SimmCalculator::calcAddMargin(const SimmSide& side, const NettingSetDetails& nettingSetDetails,
                                   const set<Regulation>& regulations, const Crif& simmParameters) {
    timer_.start("calcAddMargin()");
    // Reference to SIMM results for this portfolio
    auto& results = simmResults_[side][nettingSetDetails][regulations];

    const bool overwrite = false;

    if (!quiet_) {
        DLOG("Calculating additional margin for portfolio [" << nettingSetDetails << "], regulation " << regulations
                                                             << " and SIMM side " << side);
    }

    // First, add scaled additional margin, using "ProductClassMultiplier"
    // risk type, for the portfolio
    auto pc = ProductClass::Empty;
    auto rt = RiskType::ProductClassMultiplier;
    auto pIt = simmParameters.filterBy(nettingSetDetails, pc, rt);

    for(auto sit = pIt.first; sit != pIt.second; sit++) {
        CrifRecord it = sit->toCrifRecord();
        // Qualifier should be a product class string
        auto qpc = parseProductClass(it.qualifier);
        if (results.has(qpc, RiskClass::All, MarginType::All, "All")) {
            Real im = results.get(qpc, RiskClass::All, MarginType::All, "All");
            Real factor = it.amount;
            QL_REQUIRE(factor >= 0.0, "SIMM Calculator: Amount for risk type "
                << rt << " must be greater than or equal to 0 but we got " << factor);
            Real pcmMargin = (factor - 1.0) * im;
            add(nettingSetDetails, regulations, qpc, RiskClass::All, MarginType::AdditionalIM, "All", pcmMargin, side,
                overwrite);

            // Add to aggregation at margin type level
            add(nettingSetDetails, regulations, qpc, RiskClass::All, MarginType::All, "All", pcmMargin, side, overwrite);
            // Add to aggregation at product class level
            add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::AdditionalIM, "All", pcmMargin,
                side, overwrite);
            // Add to aggregation at portfolio level
            add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::All, "All", pcmMargin, side,
                overwrite);
            CrifRecord spRecord = it;
            if (side == SimmSide::Call)
                spRecord.collectRegulations = regulations;
            else
                spRecord.postRegulations = regulations;
            if (!simmParameters_)
                simmParameters_ = QuantLib::ext::make_shared<Crif>();
            simmParameters_->addRecord(spRecord);
        }
    }

    // Second, add fixed amounts IM, using "AddOnFixedAmount" risk type, for the portfolio
    pIt = simmParameters.filterBy(nettingSetDetails, pc, RiskType::AddOnFixedAmount);
    for (auto sit = pIt.first; sit != pIt.second; sit++) {
        CrifRecord it = sit->toCrifRecord();
        Real fixedMargin = it.amountResultCcy;
        add(nettingSetDetails, regulations, ProductClass::AddOnFixedAmount, RiskClass::All, MarginType::AdditionalIM,
            "All", fixedMargin, side, overwrite);

        // Add to aggregation at margin type level
        add(nettingSetDetails, regulations, ProductClass::AddOnFixedAmount, RiskClass::All, MarginType::All, "All",
            fixedMargin,
            side, overwrite);
        // Add to aggregation at product class level
        add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::AdditionalIM, "All",
            fixedMargin, side, overwrite);
        // Add to aggregation at portfolio level
        add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::All, "All", fixedMargin, side,
            overwrite);
        CrifRecord spRecord = it;
        if (side == SimmSide::Call)
            spRecord.collectRegulations = regulations;
        else
            spRecord.postRegulations = regulations;
        if (!simmParameters_)
            simmParameters_ = QuantLib::ext::make_shared<Crif>();
        simmParameters_->addRecord(spRecord);
    }

    // Third, add percentage of notional amounts IM, using "AddOnNotionalFactor"
    // and "Notional" risk types, for the portfolio.
    pIt = simmParameters.filterBy(nettingSetDetails, pc, RiskType::AddOnNotionalFactor);
    for (auto sit = pIt.first; sit != pIt.second; sit++) {
        CrifRecord it = sit->toCrifRecord();

        // We should have a single corresponding CrifRecord with risk type
        // "Notional" and the same qualifier. Search for it.
        auto pQualifierIt = simmParameters.filterByQualifier(nettingSetDetails, pc, RiskType::Notional, it.qualifier);
        const auto count = std::distance(pQualifierIt.first, pQualifierIt.second);
        QL_REQUIRE(count < 2, "Expected either 0 or 1 elements for risk type "
                                                << RiskType::Notional << " and qualifier " << it.qualifier
                                                << " but got " << count);

        // If we have found a corresponding notional, update the additional margin
        if (count == 1) {
            Real notional = pQualifierIt.first->amountResultCurrency();
            Real factor = it.amount;
            Real notionalFactorMargin = notional * factor / 100.0;

            add(nettingSetDetails, regulations, ProductClass::AddOnNotionalFactor, RiskClass::All,
                MarginType::AdditionalIM, "All", notionalFactorMargin, side, overwrite);

            // Add to aggregation at margin type level
            add(nettingSetDetails, regulations, ProductClass::AddOnNotionalFactor, RiskClass::All, MarginType::All,
                "All",
                notionalFactorMargin, side, overwrite);
            // Add to aggregation at product class level
            add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::AdditionalIM, "All",
                notionalFactorMargin, side, overwrite);
            // Add to aggregation at portfolio level
            add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::All, "All",
                notionalFactorMargin,
                side, overwrite);
            CrifRecord spRecord = it;
            if (side == SimmSide::Call)
                spRecord.collectRegulations = regulations;
            else
                spRecord.postRegulations = regulations;
            if (!simmParameters_)
                simmParameters_ = QuantLib::ext::make_shared<Crif>();
            simmParameters_->addRecord(spRecord);
        }
    }
    timer_.stop("calcAddMargin()");
}

void SimmCalculator::populateResults(const SimmSide& side, const NettingSetDetails& nettingSetDetails,
                                     const set<Regulation>& regulations) {

    if (!quiet_) {
        LOG("SimmCalculator: Populating higher level results")
    }

    // Sets of classes (excluding 'All')
    auto pcs = simmConfiguration_->productClasses(false);
    auto rcs = simmConfiguration_->riskClasses(false);
    auto mts = simmConfiguration_->marginTypes(false);

    // Populate netting set level results for each portfolio

    // Reference to SIMM results for this portfolio
    auto& results = simmResults_[side][nettingSetDetails][regulations];

    // Fill in the margin within each (product class, risk class) combination
    for (const auto& pc : pcs) {
        for (const auto& rc : rcs) {
            // Margin for a risk class is just sum over margin for each margin type
            // within that risk class
            Real riskClassMargin = 0.0;
            bool hasRiskClass = false;
            for (const auto& mt : mts) {
                if (results.has(pc, rc, mt, "All")) {
                    riskClassMargin += results.get(pc, rc, mt, "All");
                    if (!hasRiskClass)
                        hasRiskClass = true;
                }
            }

            // Add the margin to the results if it was calculated
            if (hasRiskClass) {
                add(nettingSetDetails, regulations, pc, rc, MarginType::All, "All", riskClassMargin, side);
            }
        }
    }

    // Fill in the margin within each product class by aggregating across risk classes
    for (const auto& pc : pcs) {
        Real productClassMargin = 0.0;
        bool hasProductClass = false;

        // IM within product class across risk classes requires correlation
        // o suffix => outer and i suffix => inner here
        for (auto ito = rcs.begin(); ito != rcs.end(); ++ito) {
            // Skip to next if no results for current risk class
            if (!results.has(pc, *ito, MarginType::All, "All"))
                continue;
            // If we get to here, we have the current product class
            if (!hasProductClass)
                hasProductClass = true;

            Real imo = results.get(pc, *ito, MarginType::All, "All");
            // Diagonal term
            productClassMargin += imo * imo;

            // Now, cross terms
            for (auto iti = rcs.begin(); iti != ito; ++iti) {
                if (!results.has(pc, *iti, MarginType::All, "All"))
                    continue;
                Real imi = results.get(pc, *iti, MarginType::All, "All");
                // Get the correlation between risk classes
                Real corr = simmConfiguration_->correlationRiskClasses(*ito, *iti);
                productClassMargin += 2.0 * corr * imo * imi;
            }
        }

        // Add the margin to the results if it was calculated
        if (hasProductClass) {
            productClassMargin = std::sqrt(std::max(productClassMargin, 0.0));
            add(nettingSetDetails, regulations, pc, RiskClass::All, MarginType::All, "All", productClassMargin, side);
        }
    }

    // Overall initial margin for the portfolio is the sum of the initial margin in
    // each of the product classes. Could have done it in the last loop but cleaner here.
    Real im = 0.0;
    for (const auto& pc : pcs) {
        if (results.has(pc, RiskClass::All, MarginType::All, "All")) {
            im += results.get(pc, RiskClass::All, MarginType::All, "All");
        }
    }
    add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, MarginType::All, "All", im, side);

    // Combinations outside of the natural SIMM hierarchy

    // Across risk class, for each product class and margin type
    for (const auto& pc : pcs) {
        for (const auto& mt : mts) {
            Real margin = 0.0;
            bool hasPcMt = false;

            // IM within product class and margin type across risk classes
            // requires correlation. o suffix => outer and i suffix => inner here
            for (auto ito = rcs.begin(); ito != rcs.end(); ++ito) {
                // Skip to next if no results for current risk class
                if (!results.has(pc, *ito, mt, "All"))
                    continue;
                // If we get to here, we have the current product class & margin type
                if (!hasPcMt)
                    hasPcMt = true;

                Real imo = results.get(pc, *ito, mt, "All");
                // Diagonal term
                margin += imo * imo;

                // Now, cross terms
                for (auto iti = rcs.begin(); iti != ito; ++iti) {
                    if (!results.has(pc, *iti, mt, "All"))
                        continue;
                    Real imi = results.get(pc, *iti, mt, "All");
                    // Get the correlation between risk classes
                    Real corr = simmConfiguration_->correlationRiskClasses(*ito, *iti);
                    margin += 2.0 * corr * imo * imi;
                }
            }

            // Add the margin to the results if it was calculated
            if (hasPcMt) {
                margin = std::sqrt(std::max(margin, 0.0));
                add(nettingSetDetails, regulations, pc, RiskClass::All, mt, "All", margin, side);
            }
        }
    }

    // Across product class, for each risk class and margin type
    for (const auto& rc : rcs) {
        for (const auto& mt : mts) {
            Real margin = 0.0;
            bool hasRcMt = false;

            // Can just sum across product class
            for (const auto& pc : pcs) {
                // Skip to next if no results for current product class
                if (!results.has(pc, rc, mt, "All"))
                    continue;
                // If we get to here, we have the current risk class & margin type
                if (!hasRcMt)
                    hasRcMt = true;

                margin += results.get(pc, rc, mt, "All");
            }

            // Add the margin to the results if it was calculated
            if (hasRcMt) {
                add(nettingSetDetails, regulations, ProductClass::All, rc, mt, "All", margin, side);
            }
        }
    }

    // Across product class and margin type for each risk class
    // Have already computed MarginType::All results above so just need
    // to sum over product class for each risk class here
    for (const auto& rc : rcs) {
        Real margin = 0.0;
        bool hasRc = false;

        for (const auto& pc : pcs) {
            // Skip to next if no results for current product class
            if (!results.has(pc, rc, MarginType::All, "All"))
                continue;
            // If we get to here, we have the current risk class
            if (!hasRc)
                hasRc = true;

            margin += results.get(pc, rc, MarginType::All, "All");
        }

        // Add the margin to the results if it was calculated
        if (hasRc) {
            add(nettingSetDetails, regulations, ProductClass::All, rc, MarginType::All, "All", margin, side);
        }
    }

    // Across product class and risk class for each margin type
    // Have already computed RiskClass::All results above so just need
    // to sum over product class for each margin type here
    for (const auto& mt : mts) {
        Real margin = 0.0;
        bool hasMt = false;

        for (const auto& pc : pcs) {
            // Skip to next if no results for current product class
            if (!results.has(pc, RiskClass::All, mt, "All"))
                continue;
            // If we get to here, we have the current risk class
            if (!hasMt)
                hasMt = true;

            margin += results.get(pc, RiskClass::All, mt, "All");
        }

        // Add the margin to the results if it was calculated
        if (hasMt) {
            add(nettingSetDetails, regulations, ProductClass::All, RiskClass::All, mt, "All", margin, side);
        }
    }
}

void SimmCalculator::populateFinalResults(const map<SimmSide, map<NettingSetDetails, Regulation>>& winningRegs) {

    timer_.start("populateFinalResults()");

    if (!quiet_) {
        LOG("SimmCalculator: Populating final winning regulators' IM");
    }
    winningRegulations_ = winningRegs;

    // Populate list of trade IDs of final trades used for SIMM winning reg
    for (auto& tids : finalTradeIds_)
        tids.second.clear();
    for (const auto& sv : winningRegs) {
        const SimmSide& side = sv.first;
        finalTradeIds_.insert(std::make_pair(side, std::set<string>()));
        
        for (const auto& nv : sv.second) {
            const NettingSetDetails& nsd = nv.first;
            const Regulation& winningReg = nv.second;

            if (tradeIds_.count(side) > 0)
                if (tradeIds_.at(side).count(nsd) > 0)
                    if (tradeIds_.at(side).at(nsd).count(winningReg) > 0)
                        for (const string& tid : tradeIds_.at(side).at(nsd).at(winningReg))
                            finalTradeIds_.at(side).insert(tid);
        }
    }

    // Populate final SIMM results
    for (const auto& sv : simmResults_) {
        const SimmSide side = sv.first;

        for (const auto& nv : sv.second) {
            const NettingSetDetails& nsd = nv.first;

            const Regulation& winningReg = winningRegulations(side, nsd);
            // If no results found for winning regulator, i.e IM is Schedule IM only, use empty SIMM results
            SimmResults simmResults = SimmResults(resultCcy_);
            for (const auto& [regs, results] : nv.second) {
                if (regs.find(winningReg) != regs.end()) {
                    simmResults = results;
                    break;
                }
            }

            finalSimmResults_[side][nsd] = make_pair(winningReg, simmResults);
        }
    }
    timer_.stop("populateFinalResults()");
}

void SimmCalculator::populateFinalResults() {
    populateFinalResults(winningRegulations_);
}

void SimmCalculator::add(const NettingSetDetails& nettingSetDetails, const set<Regulation>& regulations, const ProductClass& pc,
                         const RiskClass& rc, const MarginType& mt, const string& b, Real margin, SimmSide side,
                         const bool overwrite) {
    if (!quiet_) {
        DLOG("Calculated " << side << " margin for [netting set details, product class, risk class, margin type] = ["
                           << "[" << NettingSetDetails(nettingSetDetails) << "]"
                           << ", " << pc << ", " << rc << ", " << mt << "] of " << margin);
    }

    const string& calculationCcy = side == SimmSide::Call ? calculationCcyCall_ : calculationCcyPost_;
    simmResults_[side][nettingSetDetails][regulations].add(pc, rc, mt, b, margin, resultCcy_, calculationCcy, overwrite);
}

void SimmCalculator::add(const NettingSetDetails& nettingSetDetails, const set<Regulation>& regulations, const ProductClass& pc,
                         const RiskClass& rc, const MarginType& mt, const map<string, Real>& margins, SimmSide side,
                         const bool overwrite) {

    for (const auto& kv : margins)
        add(nettingSetDetails, regulations, pc, rc, mt, kv.first, kv.second, side, overwrite);
}

void SimmCalculator::splitCrifByRegulationsAndPortfolios(const bool enforceIMRegulations, const QuantLib::ext::shared_ptr<Crif>& crif) {

    MEM_LOG_USING_LEVEL(ORE_WARNING, "Before splitting CRIF records in SIMM calculator");
    timer_.start("Splitting CRIF by regs");
    for (const auto& slimCrifRecord : *crif) {

        // Remove empty
        if (slimCrifRecord.riskType() == RiskType::Empty)
            continue;

        if (slimCrifRecord.imModel() == CrifRecord::IMModel::Schedule)
            continue;

        for (const auto& side : {SimmSide::Call, SimmSide::Post}) {
            const NettingSetDetails& nettingSetDetails = slimCrifRecord.getNettingSetDetails();

            bool nettingSetCollectRegsIsEmpty = false;
            bool nettingSetPostRegsIsEmpty = false;
            if (collectRegsIsEmpty_.find(nettingSetDetails) != collectRegsIsEmpty_.end())
                nettingSetCollectRegsIsEmpty = collectRegsIsEmpty_.at(nettingSetDetails);
            if (postRegsIsEmpty_.find(nettingSetDetails) != postRegsIsEmpty_.end())
                nettingSetPostRegsIsEmpty = postRegsIsEmpty_.at(nettingSetDetails);

            set<Regulation> regs;
            if (enforceIMRegulations) {
                regs = side == SimmSide::Call ? slimCrifRecord.collectRegulations() : slimCrifRecord.postRegulations();
            }
            if (regs.empty())
                regs.insert(Regulation::Unspecified);

            if (regs.find(Regulation::Excluded) != regs.end())
                continue;
            if ((regs.size() == 1 && regs.find(Regulation::Unspecified) != regs.end()) && enforceIMRegulations &&
                !(nettingSetCollectRegsIsEmpty && nettingSetPostRegsIsEmpty)) {
                continue;
            }

            CrifRecord crifRecord = slimCrifRecord.toCrifRecord();
            crifRecord.collectRegulations.clear();
            crifRecord.postRegulations.clear();

            // Keep a record of trade IDs for each regulation
            for (const auto& r : regs) {
                if (!crifRecord.isSimmParameter())
                    tradeIds_[side][nettingSetDetails][r].insert(crifRecord.tradeId);
            }

            set<Regulation> newRegs, cftcSecRegs;
            for (const auto& reg : regs) {
                if (reg == Regulation::SEC || reg == Regulation::CFTC)
                    cftcSecRegs.insert(reg);
                else
                    newRegs.insert(reg);
            }

            // We make sure to ignore amountCcy when aggregating the records, since we will only be using
            // amountResultCcy, and we may have CRIF records that are equal everywhere except for the amountCcy,
            // and this will fail in the case of Risk_XCcyBasis and Risk_Inflation.
            auto& regCrifMap = regSensitivities_[side][nettingSetDetails];
            for (auto& regsSet : {newRegs, cftcSecRegs}) {
                if (regsSet.empty())
                    continue;

                if (regCrifMap.find(regsSet) == regCrifMap.end()) {
                    auto crif = QuantLib::ext::make_shared<Crif>();
                    crif->setAggregate(true);
                    regCrifMap[regsSet] = crif;
                }

                // Simple case, for non-SEC/non-CFTC records, we just add to their respective Crifs
                // and handle later when cleaning up duplicate regs
                regCrifMap[regsSet]->addRecord(crifRecord, true);
            }
        }
    }
    timer_.stop("Splitting CRIF by regs");

    // Handle specific case for SEC/CFTC - CFTC records are to be added to SEC
    timer_.start("Handling SEC and CFTC special case");
    for (auto& [side, nettingSetRegulationCrifMap] : regSensitivities_) {
        for (auto& [nettingSetDetails, regulationsCrifMap] : nettingSetRegulationCrifMap) {
            const bool hasSecGlobal = hasSEC_[side].find(nettingSetDetails) != hasSEC_[side].end();
            auto itCrifCftc = regulationsCrifMap.find({Regulation::CFTC});
            auto itCrifSec = regulationsCrifMap.find({Regulation::SEC});
            auto itCrifSecCftc = regulationsCrifMap.find({Regulation::SEC, Regulation::CFTC});
            const bool hasCftc = itCrifCftc != regulationsCrifMap.end();
            const bool hasSec = itCrifSec != regulationsCrifMap.end();
            const bool hasSecCftc = itCrifSecCftc != regulationsCrifMap.end();

            if (!hasSecGlobal)
                QL_REQUIRE(
                    !hasSec,
                    "Mismatch in internal records for SEC. There should be no SEC CRIF records, but one was found.");

            /* hasSecGlobal hasCftc  hasSec hasSecCftc  Logic
                    Y          Y       Y        Y       Move {CFTC} to {SEC} -> Move {CFTC,SEC} into {CFTC} and {SEC}, delete {CFTC,SEC}
                    Y          Y       Y        N       Move {CFTC} to {SEC}
                    Y          Y       N        Y       Move {CFTC} to {CFTC,SEC}, delete {CFTC}
                    Y          Y       N        N       Relabel {CFTC} to {CFTC,SEC}
                    Y          N       Y        Y       Move {CFTC,SEC} to {SEC}, relabel {CFTC,SEC} to {CFTC}
            */

            if (hasSecGlobal) {
                if (hasCftc) {
                    if (hasSec) {
                        // Move {CFTC} to {SEC}
                        itCrifSec->second->addRecords(*itCrifCftc->second, true);

                        if (hasSecCftc) {
                            // Move {CFTC,SEC} into {CFTC} and {SEC}, delete {CFTC,SEC}
                            itCrifCftc->second->addRecords(*itCrifSecCftc->second, true);
                            itCrifSec->second->addRecords(*itCrifSecCftc->second, true);
                            regulationsCrifMap.erase({Regulation::SEC, Regulation::CFTC});
                        }
                    } else {
                        // Move {CFTC} to {CFTC,SEC}, delete {CFTC} / Relabel {CFTC} to {CFTC,SEC}
                        if (hasSecCftc) {
                            itCrifSecCftc->second->addRecords(*itCrifCftc->second, true);
                        } else {
                            regulationsCrifMap[{Regulation::SEC, Regulation::CFTC}] = itCrifCftc->second;
                        }
                        regulationsCrifMap.erase({Regulation::CFTC});
                    }
                } else {
                    if (hasSec && hasSecCftc) {
                        // Move {CFTC,SEC} to {SEC}
                        itCrifSec->second->addRecords(*itCrifSecCftc->second, true);
                        // Relabel {CFTC,SEC} to {CFTC}
                        regulationsCrifMap[{Regulation::CFTC}] = itCrifSecCftc->second;
                        regulationsCrifMap.erase({Regulation::SEC, Regulation::CFTC});
                    }
                }
                
            }
        }
    }
    timer_.stop("Handling SEC and CFTC special case");
    MEM_LOG_USING_LEVEL(ORE_WARNING, "After splitting CRIF records in SIMM calculator");
}

void SimmCalculator::cleanDuplicateRegulations() {

    MEM_LOG_USING_LEVEL(ORE_WARNING, "Before cleaning up duplicate regulations in SIMM calculator");

    // Now we clear up duplicate instances of some regs,
    // Example: If we have {ESA,USPR} and {ESA}, then move {ESA,USPR} records to {ESA}, and relabel {ESA,USPR}->{USPR}.
    // 1. Count occurrences of each regulation
    // 2. Iteratively split up the set with the highest number of (duplicate) regulations until there are no more duplicates
    timer_.start("Clean up duplicate regs");

    std::function<vector<set<Regulation>>(const set<Regulation>&, Size)> generateCombinations =
        [&](const set<Regulation>& regulations, Size combinationSize) {
            std::function<void(const vector<Regulation>&, Size, Size, vector<Regulation>&, vector<set<Regulation>>&)>
                generateCurrentCombination = [&](const vector<Regulation>& regulations, Size combinationSize,
                                                 Size startIdx, vector<Regulation>& currentCombination,
                                                 vector<set<Regulation>>& regulationCombinations) {
                    if (currentCombination.size() == combinationSize) {
                        set<Regulation> currCombination(currentCombination.begin(), currentCombination.end());
                        regulationCombinations.push_back(currCombination);
                        return;
                    }

                    for (Size i = startIdx; i < regulations.size(); i++) {
                        currentCombination.push_back(regulations[i]);
                        generateCurrentCombination(regulations, combinationSize, i + 1, currentCombination,
                                                   regulationCombinations);
                        currentCombination.pop_back();
                    }
                };

            vector<set<Regulation>> regulationCombinations;
            vector<Regulation> currentCombination;
            vector<Regulation> regs(regulations.begin(), regulations.end());

            generateCurrentCombination(regs, combinationSize, 0, currentCombination, regulationCombinations);

            return regulationCombinations;
        };

    for (auto& [side, nettingSetRegulationCrifMap] : regSensitivities_) {
        for (auto& [nsd, regulationsCrifMap] : nettingSetRegulationCrifMap) {

            auto createQueue = [&regulationsCrifMap]() {
                map<Regulation, Size> regCounts;
                std::priority_queue<pair<Size, set<Regulation>>, vector<pair<Size, set<Regulation>>>, RegulationCompare>
                    queue;

                // Count occurrences of each value in all containers - we want only one for each, i.e. no duplicates
                regCounts.clear();
                for (const auto& [regs, crif] : regulationsCrifMap) {
                    for (const auto& reg : regs) {
                        if (regCounts.find(reg) == regCounts.end())
                            regCounts[reg] = 0;
                        regCounts[reg]++;
                    }
                }

                // For each set of regulations, count no. of duplicated regs, and order them based on that
                for (const auto& [regs, crif] : regulationsCrifMap) {
                    Size duplicateCount = std::count_if(regs.begin(), regs.end(),
                                                        [&](const Regulation& reg) { return regCounts[reg] > 1; });
                    if (duplicateCount > 0 && regs.size() > 1)
                        queue.push({duplicateCount, regs});
                }

                return queue;
            };

            Size currIter = 0;
            auto queue = createQueue();
            while (!queue.empty()) {
                // Limit to avoid infinite loop
                if (currIter++ == 1000)
                    QL_FAIL("SimmCalculator: Cleaning up duplicate regulations - Iteration limit exceeded.");

                const auto& [numDuplicates, regs] = queue.top();

                // Look for largest set of duplicates already existing that we can move duplicates into
                for (Size iterSize = numDuplicates; iterSize > 0; iterSize--) {
                    if (iterSize == regs.size())
                        continue;

                    auto regCombinations = generateCombinations(regs, iterSize);
                    bool duplicateFound = false;
                    for (const auto& comb : regCombinations) {
                        // Move CRIF records from duplicate regulations
                        if (regulationsCrifMap.find(comb) != regulationsCrifMap.end()) {
                            duplicateFound = true;

                            regulationsCrifMap[comb]->addRecords(*regulationsCrifMap[regs]);

                            // For the remaining regulations that were not transferred, check if already in the map.
                            // If already exists, move the records there, otherwise we relabel/update the key.
                            set<Regulation> newRegs;
                            for (const auto& reg : regs) {
                                if (comb.find(reg) == comb.end())
                                    newRegs.insert(reg);
                            }

                            if (regulationsCrifMap.find(newRegs) != regulationsCrifMap.end()) {
                                regulationsCrifMap[newRegs]->addRecords(*regulationsCrifMap[regs]);
                            } else {
                                regulationsCrifMap[newRegs] = regulationsCrifMap[regs];
                            }
                            regulationsCrifMap.erase(regs);

                            break;
                        }
                    }
                    if (duplicateFound)
                        break;
                }
                // Update/recreate queue after changes
                queue = createQueue();
            }
        }
    }
    timer_.stop("Clean up duplicate regs");
    MEM_LOG_USING_LEVEL(ORE_WARNING, "After cleaning up duplicate regulations in SIMM calculator");
}

Real SimmCalculator::lambda(Real theta) const {
    // Use boost inverse normal here as opposed to QL. Using QL inverse normal
    // will cause the ISDA SIMM unit tests to fail
    static Real q = boost::math::quantile(boost::math::normal(), 0.995);

    return (q * q - 1.0) * (1.0 + theta) - theta;
}

std::set<std::string> SimmCalculator::getQualifiers(const Crif& crif,
                                                    const ore::data::NettingSetDetails& nettingSetDetails,
                                                    const ProductClass& pc,
                                                    const vector<RiskType>& riskTypes) const {
    std::set<std::string> qualifiers;
    for (auto& rt : riskTypes) {
        auto qualis = crif.qualifiersBy(nettingSetDetails, pc, rt);
        qualifiers.insert(qualis.begin(), qualis.end());
    }
    return qualifiers;
}

Real SimmCalculator::fxRate(const string& ccyPair) const {
    QL_REQUIRE(market_, "SimmCalculator::fxRate(): Market is required but is null.");
    return market_->fxRate(ccyPair)->value();
}

} // namespace analytics
} // namespace ore
