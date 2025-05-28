/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/engine/saccrcalculator.hpp>
#include <orea/engine/saccrtradedata.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/equityposition.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/trs.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/instruments/commodityforward.hpp>
#include <qle/instruments/currencyswap.hpp>
#include <qle/instruments/fxforward.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/currency.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/tuple.hpp>
#include <ql/utilities/dataparsers.hpp>

#include <boost/regex.hpp>

#include <iomanip>

using namespace QuantLib;
using ore::analytics::Crif;
using ore::analytics::CrifRecord;
using ore::analytics::SaccrTradeData;
using QuantLib::Null;
using std::make_pair;
using std::map;
using std::max;
using std::pair;
using std::set;
using std::string;
using std::vector;

using RiskType = CrifRecord::RiskType;

namespace {

SaccrTradeData::AssetClass riskTypeToAssetClass(const RiskType& rt) {
    switch (rt) {
    case RiskType::CO:
        return SaccrTradeData::AssetClass::Commodity;
    case RiskType::FX:
        return SaccrTradeData::AssetClass::FX;
    case RiskType::EQ_IX:
    case RiskType::EQ_SN:
        return SaccrTradeData::AssetClass::Equity;
    case RiskType::CR_IX:
    case RiskType::CR_SN:
        return SaccrTradeData::AssetClass::Credit;
    case RiskType::IR:
        return SaccrTradeData::AssetClass::IR;
    default:
        return SaccrTradeData::AssetClass::None;
    }
}

} // namespace

namespace ore {
namespace analytics {
using namespace ore::data;

using AssetClass = SaccrTradeData::AssetClass;

void SaccrCalculator::clear() {
    totalNPV_ = 0.0;
    totalCC_ = 0.0;
    NPV_.clear();
    EAD_.clear();
    RC_.clear();
    PFE_.clear();
    multiplier_.clear();
    addOn_.clear();
    addOnAssetClass_.clear();
    addOnHedgingSet_.clear();
    nettingSets_.clear();
    isIndex_.clear();
    basisHedgingSets_.clear();
    volatilityHedgingSets_.clear();
}

SaccrCalculator::SaccrCalculator(const QuantLib::ext::shared_ptr<Crif>& capitalCrif,
                                 const QuantLib::ext::shared_ptr<SaccrTradeData>& saccrTradeData,
                                 const string& baseCurrency,
                                 const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager,
                                 const QuantLib::ext::shared_ptr<CounterpartyManager>& counterpartyManager,
                                 const QuantLib::ext::shared_ptr<Market>& market,
                                 const map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>>& outReports)
    : reports_(outReports), crif_(capitalCrif), saccrTradeData_(saccrTradeData), nettingSetManager_(nettingSetManager),
      counterpartyManager_(counterpartyManager), market_(market), baseCurrency_(baseCurrency),
      hasNettingSetDetails_(false) {

    QL_REQUIRE(crif_, "SaccrCalculator: Crif cannot be null");

    clear();

    for (const auto& scr : *crif_) {
        try {
            CrifRecord cr = scr.toCrifRecord();
            processCrifRecord(cr);
        } catch (const std::exception& e) {
            StructuredAnalyticsWarningMessage("SA-CCR", "Processing Capital CRIF", e.what()).log();
        }
    }

    hasNettingSetDetails_ = std::any_of(nettingSets_.begin(), nettingSets_.end(),
                                        [](const NettingSetDetails& nsd) { return !nsd.emptyOptionalFields(); });

    aggregate();
}

void SaccrCalculator::processCrifRecord(const CrifRecord& record) {
    nettingSetToCpty_[record.nettingSetDetails].insert(record.counterpartyId);

    if (nettingSets_.find(record.nettingSetDetails) == nettingSets_.end()) {
        nettingSets_.insert(record.nettingSetDetails);
        NPV_[record.nettingSetDetails] = 0.0;
        RC_[record.nettingSetDetails] = 0.0;
        addOn_[record.nettingSetDetails] = 0.0;
        PFE_[record.nettingSetDetails] = 0.0;
        multiplier_[record.nettingSetDetails] = 0.0;

        amountsBase_[record.nettingSetDetails].im = 0.0;
        amountsBase_[record.nettingSetDetails].vm = 0.0;
        amountsBase_[record.nettingSetDetails].mta = 0.0;
        amountsBase_[record.nettingSetDetails].tha = 0.0;
        amountsBase_[record.nettingSetDetails].iah = 0.0;
    }

    Real usdBaseRate = getFxRate("USD");
    if (record.riskType == RiskType::PV) {
        Real npv = 0.0;
        if (record.amountUsd != Null<Real>())
            npv = record.amountUsd;
        else if (record.amount != Null<Real>() && !record.amountCurrency.empty())
            npv = record.amount * getFxRate(record.amountCurrency);
        else
            QL_FAIL("Could not get valid PV amount from CRIF record");
        totalNPV_ += npv;
        NPV_[record.nettingSetDetails] += npv;
        tradeNPV_[record.tradeId] += npv;

    } else if (record.riskType == RiskType::COLL) {
        if (record.hedgingSet == "SettlementType") {
            auto saccrLabel1 = boost::get<string>(record.saccrLabel1);
            QL_REQUIRE(saccrLabel1 == "STM" || saccrLabel1 == "NOM",
                       "SaccrCalculator::processCrifRecord() : Only SettlementType 'STM' is supported");
        } else if (record.hedgingSet == "Direction") {
            QL_REQUIRE(boost::get<string>(record.saccrLabel1) == "Mutual",
                       "SaccrCalculator::processCrifRecord() : Only Direction 'Mutual' is supported");
        } else if (record.hedgingSet == "MPOR") {
            MPOR_[record.nettingSetDetails] = boost::get<Size>(record.saccrLabel1);
        } else if (record.hedgingSet == "IM") {
            QL_REQUIRE(boost::get<string>(record.saccrLabel2) == "Cash",
                       "SaccrCalculator::processCrifRecord() : Only 'Cash' IM is currently supported");
            amountsBase_[record.nettingSetDetails].im += record.amountUsd * usdBaseRate;
        } else if (record.hedgingSet == "VM") {
            QL_REQUIRE(boost::get<string>(record.saccrLabel2) == "Cash",
                       "SaccrCalculator::processCrifRecord() : Only 'Cash' VM is currently supported");
            amountsBase_[record.nettingSetDetails].vm += record.amountUsd * usdBaseRate;
        } else if (record.hedgingSet == "IA") {
            amountsBase_[record.nettingSetDetails].iah += record.amountUsd * usdBaseRate;
        } else if (record.hedgingSet == "MTA") {
            amountsBase_[record.nettingSetDetails].mta += record.amountUsd * usdBaseRate;
        } else if (record.hedgingSet == "TA") {
            amountsBase_[record.nettingSetDetails].tha += record.amountUsd * usdBaseRate;
        } else {
            QL_FAIL("SaccrCalculator::processCrifRecord() : Invalid hedging set " << record.hedgingSet);
        }
    } else if (record.riskType == RiskType::FX || record.riskType == RiskType::CO || record.riskType == RiskType::IR ||
               record.riskType == RiskType::EQ_IX || record.riskType == RiskType::EQ_SN ||
               record.riskType == RiskType::CR_IX || record.riskType == RiskType::CR_SN) {
        auto assetClass = riskTypeToAssetClass(record.riskType);
        auto effectiveNotionalBase = record.amountUsd * usdBaseRate;

        // TODO: This should be updated once we've added the "_BASIS" convention prescribed by the Capital CRIF
        // docs/unit test
        if (record.hedgingSet.find("_BASIS") != string::npos)
            basisHedgingSets_.insert(record.hedgingSet);

        if (record.hedgingSet.find("_VOL") != string::npos)
            volatilityHedgingSets_.insert(record.hedgingSet);

        if (record.riskType == RiskType::EQ_IX || record.riskType == RiskType::CR_IX)
            isIndex_[record.qualifier] = true;
        else if (record.riskType == RiskType::EQ_SN || record.riskType == RiskType::CR_SN)
            isIndex_[record.qualifier] = false;

        tradeAssetClasses_[record.tradeId] = assetClass;
        assetClasses_[record.nettingSetDetails].insert(assetClass);

        // if (addOnHedgingSet_.find(hedgingSetKey) == addOnHedgingSet_.end())
        //     addOnHedgingSet_[hedgingSetKey] = 0.0;
        string hedgingSubset = assetClass == AssetClass::Commodity ? record.bucket : record.qualifier;
        HedgingSetKey hedgingSetKey(record.nettingSetDetails, assetClass, record.hedgingSet);
        if (effectiveNotional_.find(hedgingSetKey) == effectiveNotional_.end()) {
            effectiveNotional_[hedgingSetKey] = 0.0;
            addOnHedgingSet_[hedgingSetKey] = 0.0;
        }
        AssetClassKey assetClassKey(record.nettingSetDetails, assetClass);
        if (addOnAssetClass_.find(assetClassKey) == addOnAssetClass_.end())
            addOnAssetClass_[assetClassKey] = 0.0;
        effectiveNotional_[hedgingSetKey] += effectiveNotionalBase;

        HedgingSubsetKey hedgingSubsetKey(record.nettingSetDetails, assetClass, record.hedgingSet, hedgingSubset);
        if (subsetEffectiveNotional_.find(hedgingSubsetKey) == subsetEffectiveNotional_.end())
            subsetEffectiveNotional_[hedgingSubsetKey] = 0.0;
        subsetEffectiveNotional_[hedgingSubsetKey] += effectiveNotionalBase;
    } else {
        QL_FAIL("SaccrCalculator::processCrifRecord() : Unexpected risk type " << record.riskType);
    }
}

Real SaccrCalculator::getFxRate(const string& ccy) {
    Real fx = 1.0;
    if (ccy != baseCurrency_) {
        Handle<Quote> fxQuote = market_->fxRate(ccy + baseCurrency_);
        fx = fxQuote->value();
    }
    return fx;
}

void SaccrCalculator::aggregate() {
    LOG("SA-CCR aggregation");
    // RC
    for (const auto& nsd : nettingSets_) {
        Real initialMargin = amountsBase_[nsd].im;
        Real variationMargin = amountsBase_[nsd].vm;
        Real mta = amountsBase_[nsd].mta;
        Real th = amountsBase_[nsd].tha;
        Real independentAmountHeld = amountsBase_[nsd].iah;
        Real nica = independentAmountHeld + initialMargin;
        Real C = variationMargin + nica;
        RC_[nsd] = std::max(NPV_[nsd] - C, std::max(th + mta - nica, 0.0));
    }

    // Hedging set AddOn calculation
    DLOG("SA-CCR: Hedging set AddOn calculation");
    for (map<HedgingSetKey, Real>::iterator it = addOnHedgingSet_.begin(); it != addOnHedgingSet_.end(); it++) {
        // Add-ons
        const NettingSetDetails& nettingSetDetails = QuantLib::ext::get<0>(it->first);
        const AssetClass& assetClass = QuantLib::ext::get<1>(it->first);
        const string& hedgingSet = QuantLib::ext::get<2>(it->first);
        if (assetClass == AssetClass::IR) {
            // effectiveNotional_[it->first] =
            //     sqrt(D1 * D1 + D2 * D2 + D3 * D3 + 1.4 * (D1 * D2 + D2 * D3) + 0.6 * D1 * D3);
            Real supervisoryFactor = 0.005; // 0.5%
            addOnHedgingSet_[it->first] = supervisoryFactor * effectiveNotional_[it->first];
            DLOG("AddOn for [" << nettingSetDetails << "]/" << assetClass << "/" << hedgingSet << ": "
                               << addOnHedgingSet_[it->first]);
        } else if (assetClass == AssetClass::FX) {
            Real supervisoryFactor = 0.04; // 4%
            addOnHedgingSet_[it->first] = supervisoryFactor * fabs(effectiveNotional_[it->first]);
            DLOG("AddOn for [" << nettingSetDetails << "]/" << assetClass << "/" << hedgingSet << ": "
                               << addOnHedgingSet_[it->first]);

        } else if (assetClass == AssetClass::Commodity) {
            Real addonType = 0;
            Real addonTypeSquared = 0;
            for (const auto& [hedgingSubsetKey, effectiveNotional] : subsetEffectiveNotional_) {
                HedgingSetKey hedgingSetKey(QuantLib::ext::get<0>(hedgingSubsetKey),
                                            QuantLib::ext::get<1>(hedgingSubsetKey),
                                            QuantLib::ext::get<2>(hedgingSubsetKey));
                if (hedgingSetKey != it->first)
                    continue;
                const string& hedgingSubset = QuantLib::ext::get<3>(hedgingSubsetKey);
                vector<string> tokens;
                boost::split(tokens, hedgingSubset, boost::is_any_of("_"));
                QL_REQUIRE(tokens.size() == 1 || tokens.size() == 2,
                           "Could not split hedging subset. Expected 1 or 2 tokens. Got " << tokens.size());
                bool isPower = true;
                for (const string& t : tokens) {
                    string hedgingSubset = saccrTradeData_->getCommodityHedgingSubset(t);
                    if (hedgingSubset != "Power")
                        isPower = false;
                }
                Real supervisoryFactor = isPower ? 0.4 : 0.18;
                const Real tmp = supervisoryFactor * effectiveNotional;
                addonType += tmp;
                addonTypeSquared += tmp * tmp;
            }
            const constexpr Real corr = 0.4;
            addOnHedgingSet_[it->first] =
                std::sqrt((corr * addonType) * (corr * addonType) + (1 - corr * corr) * addonTypeSquared);
        } else if (assetClass == AssetClass::Equity) {
            // Close duplicate of commodity logic
            Real addonType = 0;
            Real addonTypeSquared = 0;
            for (const auto& [hedgingSubsetKey, effectiveNotional] : subsetEffectiveNotional_) {
                string hedgingSubset = QuantLib::ext::get<3>(hedgingSubsetKey);
                HedgingSetKey hedgingSetKey(QuantLib::ext::get<0>(hedgingSubsetKey),
                                            QuantLib::ext::get<1>(hedgingSubsetKey),
                                            QuantLib::ext::get<2>(hedgingSubsetKey));
                if (hedgingSetKey != it->first)
                    continue;
                bool isEquityIndex = isIndex_[hedgingSubset];
                Real supervisoryFactor = isEquityIndex ? 0.2 : 0.32;
                Real corr = isEquityIndex ? 0.8 : 0.5;
                Real tmp = supervisoryFactor * effectiveNotional;

                addonType += corr * tmp;
                addonTypeSquared += (1 - corr * corr) * tmp * tmp;
            }
            addOnHedgingSet_[it->first] = std::sqrt(addonType * addonType + addonTypeSquared);
        } else if (assetClass == AssetClass::Credit) {
            // TODO
        } else
            QL_FAIL("asset class " << assetClass << " not covered");

        // For hedging sets consisting of basis transactions,
        // the supervisory factor applicable to a given asset class must be multiplied by one-half.
        if (basisHedgingSets_.find(hedgingSet) != basisHedgingSets_.end())
            addOnHedgingSet_[it->first] *= 0.5;

        if (volatilityHedgingSets_.find(hedgingSet) != volatilityHedgingSets_.end())
            addOnHedgingSet_[it->first] *= 5;
    }

    // Asset class AddOn calculation, pure aggregation across matching hedging sets
    DLOG("SA-CCR: Asset Class AddOn calculation");
    for (map<AssetClassKey, Real>::iterator it = addOnAssetClass_.begin(); it != addOnAssetClass_.end(); it++) {
        for (map<HedgingSetKey, Real>::iterator ith = addOnHedgingSet_.begin(); ith != addOnHedgingSet_.end(); ith++) {
            NettingSetDetails nettingSetDetails = QuantLib::ext::get<0>(ith->first);
            AssetClass assetClass = QuantLib::ext::get<1>(ith->first);
            // string hedgingSet = QuantLib::ext::get<2>(it->first);
            AssetClassKey key(nettingSetDetails, assetClass);
            if (it->first != key)
                continue;
            it->second += ith->second;
        }
    }

    // Netting set AddOn calculation, pure aggregation across asset classes
    // Multiplier
    // PFE
    // EAD
    DLOG("SA-CCR: Aggregate AddOn and EAD calculation");
    for (map<NettingSetDetails, Real>::iterator it = addOn_.begin(); it != addOn_.end(); it++) {
        NettingSetDetails nettingSetDetails = it->first;

        for (map<AssetClassKey, Real>::iterator ita = addOnAssetClass_.begin(); ita != addOnAssetClass_.end(); ita++) {
            NettingSetDetails aNettingSetDetails = ita->first.first;
            if (nettingSetDetails != aNettingSetDetails)
                continue;
            it->second += ita->second;
        }

        QuantLib::ext::shared_ptr<NettingSetDefinition> ndef = nettingSetManager_->get(nettingSetDetails);

        const Real independentAmountHeld = amountsBase_[nettingSetDetails].iah;
        const Real initialMargin = amountsBase_[nettingSetDetails].im;
        const Real variationMargin = amountsBase_[nettingSetDetails].vm;

        const Real nica = independentAmountHeld + initialMargin;
        const Real C = variationMargin + nica;

        const Real V = NPV_[nettingSetDetails];
        const Real A = addOn_[nettingSetDetails];
        multiplier_[nettingSetDetails] = std::min(1.0, 0.05 + 0.95 * std::exp((V - C) / (2.0 * 0.95 * A)));
        PFE_[nettingSetDetails] = multiplier_[nettingSetDetails] * addOn_[nettingSetDetails];
        const constexpr Real alpha = 1.4;
        Real ead = alpha * (RC_[nettingSetDetails] + PFE_[nettingSetDetails]);
        EAD_[nettingSetDetails] = ead;

        // Get the counterparty
        string cpStr = *nettingSetToCpty_[nettingSetDetails].begin();
        QL_REQUIRE(!cpStr.empty(), "Netting set does not contain valid counterparty");
        QuantLib::ext::shared_ptr<CounterpartyInformation> cp = counterpartyManager_->get(cpStr);

        Real rw = cp->saCcrRiskWeight();
        RW_[nettingSetDetails] = rw;

        Real cc = ead * rw;
        CC_[nettingSetDetails] = cc;
        totalCC_ += cc;
    }

    nettingSetDetails_.clear();
    for (map<NettingSetDetails, Real>::iterator it = addOn_.begin(); it != addOn_.end(); it++)
        nettingSetDetails_.push_back(it->first);

    assetClasses_.clear();
    for (map<AssetClassKey, Real>::iterator it = addOnAssetClass_.begin(); it != addOnAssetClass_.end(); it++) {
        NettingSetDetails nettingSetDetails = it->first.first;
        if (assetClasses_.find(nettingSetDetails) == assetClasses_.end())
            assetClasses_[nettingSetDetails] = set<AssetClass>();
        assetClasses_[nettingSetDetails].insert(it->first.second);
    }

    hedgingSets_.clear();
    for (map<HedgingSetKey, Real>::iterator it = addOnHedgingSet_.begin(); it != addOnHedgingSet_.end(); it++) {
        NettingSetDetails nettingSetDetails = QuantLib::ext::get<0>(it->first);
        AssetClass ac = QuantLib::ext::get<1>(it->first);
        pair<NettingSetDetails, AssetClass> key(nettingSetDetails, ac);
        if (hedgingSets_.find(key) == hedgingSets_.end())
            hedgingSets_[key] = vector<string>();
        hedgingSets_[key].push_back(QuantLib::ext::get<2>(it->first));
    }

    if (reports_.size() > 0)
        writeReports();

    DLOG("SA-CCR: Aggregation done");
}

const set<AssetClass>& SaccrCalculator::assetClasses(NettingSetDetails nettingSetDetails) const {
    auto assetClassesIt = assetClasses_.find(nettingSetDetails);
    QL_REQUIRE(assetClassesIt != assetClasses_.end(), "netting set not found in asset class map");
    return assetClassesIt->second;
}

const vector<string>& SaccrCalculator::hedgingSets(NettingSetDetails nettingSetDetails, AssetClass assetClass) const {
    pair<NettingSetDetails, AssetClass> key(nettingSetDetails, assetClass);
    auto hedgingSetsIt = hedgingSets_.find(key);
    QL_REQUIRE(hedgingSetsIt != hedgingSets_.end(), "netting set and asset class not found in hedging set map");
    return hedgingSetsIt->second;
}

Real SaccrCalculator::EAD(NettingSetDetails nettingSetDetails) const {
    auto eadIt = EAD_.find(nettingSetDetails);
    QL_REQUIRE(eadIt != EAD_.end(), "netting set " << nettingSetDetails << " not found in EAD");
    return eadIt->second;
}

Real SaccrCalculator::EAD(string nettingSet) const {
    NettingSetDetails nettingSetDetails(nettingSet);
    return EAD(nettingSetDetails);
}

Real SaccrCalculator::RW(NettingSetDetails nettingSetDetails) const {
    auto rwIt = RW_.find(nettingSetDetails);
    QL_REQUIRE(rwIt != RW_.end(), "netting set " << nettingSetDetails << " not found in RW");
    return rwIt->second;
}

Real SaccrCalculator::CC(NettingSetDetails nettingSetDetails) const {
    auto ccIt = CC_.find(nettingSetDetails);
    QL_REQUIRE(ccIt != CC_.end(), "netting set " << nettingSetDetails << " not found in CC");
    return ccIt->second;
}

Real SaccrCalculator::RC(NettingSetDetails nettingSetDetails) const {
    auto rcIt = RC_.find(nettingSetDetails);
    QL_REQUIRE(rcIt != RC_.end(), "netting set " << nettingSetDetails << " not found in RC");
    return rcIt->second;
}

Real SaccrCalculator::PFE(NettingSetDetails nettingSetDetails) const {
    auto pfeIt = PFE_.find(nettingSetDetails);
    QL_REQUIRE(pfeIt != PFE_.end(), "netting set " << nettingSetDetails << " not found in PFE");
    return pfeIt->second;
}

Real SaccrCalculator::multiplier(NettingSetDetails nettingSetDetails) const {
    auto multiplierIt = multiplier_.find(nettingSetDetails);
    QL_REQUIRE(multiplierIt != multiplier_.end(), "netting set " << nettingSetDetails << " not found in multiplier");
    return multiplierIt->second;
}

Real SaccrCalculator::addOn(NettingSetDetails nettingSetDetails) const {
    auto addOnIt = addOn_.find(nettingSetDetails);
    QL_REQUIRE(addOnIt != addOn_.end(), "netting set " << nettingSetDetails << " not found in addOn");
    return addOnIt->second;
}

Real SaccrCalculator::NPV(NettingSetDetails nettingSetDetails) const {
    auto npvIt = NPV_.find(nettingSetDetails);
    QL_REQUIRE(npvIt != NPV_.end(), "netting set " << nettingSetDetails << " not found in npv");
    return npvIt->second;
}

Real SaccrCalculator::addOn(NettingSetDetails nettingSetDetails, AssetClass assetClass) const {
    AssetClassKey key(nettingSetDetails, assetClass);
    auto addOnIt = addOnAssetClass_.find(key);
    QL_REQUIRE(addOnIt != addOnAssetClass_.end(),
               "netting set " << nettingSetDetails << " and " << assetClass << " not found in addOnAssetClass");
    return addOnIt->second;
}

Real SaccrCalculator::addOn(NettingSetDetails nettingSetDetails, AssetClass assetClass, string hedgingSet) const {
    HedgingSetKey key(nettingSetDetails, assetClass, hedgingSet);
    auto addOnHedgingSetIt = addOnHedgingSet_.find(key);
    QL_REQUIRE(addOnHedgingSetIt != addOnHedgingSet_.end(),
               "netting set " << nettingSetDetails << ", asset class " << assetClass << ", hedging set " << hedgingSet
                              << " not found in addOnAssetClass");
    return addOnHedgingSetIt->second;
}

void SaccrCalculator::writeReports() {

    LOG("writing reports");

    auto summaryReportIt = reports_.find(ReportType::Summary);
    if (summaryReportIt != reports_.end()) {
        summaryReportIt->second->addColumn("NettingSet", string());

        if (hasNettingSetDetails_) {
            for (const auto& nettingSetField : NettingSetDetails::optionalFieldNames())
                summaryReportIt->second->addColumn(nettingSetField, string());
        }

        summaryReportIt->second->addColumn("AssetClass", string())
            .addColumn("HedgingSet", string())
            .addColumn("AddOn", string())
            .addColumn("NPV", string())
            .addColumn("IndependentAmountHeld", string())
            .addColumn("InitialMargin", string())
            .addColumn("VariationMargin", string())
            .addColumn("ThresholdAmount", string())
            .addColumn("MinimumTransferAmount", string())
            .addColumn("RC", string())
            .addColumn("Multiplier", string())
            .addColumn("PFE", string())
            .addColumn("EAD", string())
            .addColumn("RW", string())
            .addColumn("CC", string());

        summaryReportIt->second->next();

        Size numNettingSetFields = NettingSetDetails::fieldNames(hasNettingSetDetails_).size();
        for (Size t = 0; t < numNettingSetFields; t++)
            summaryReportIt->second->add("All");

        // Portfolio level
        summaryReportIt->second->add("All")
            .add("All")
            .add("")
            .add(std::to_string(totalNPV_))
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add(std::to_string(CC()));

        for (const NettingSetDetails& nettingSetDetails : nettingSetDetails_) {
            const set<AssetClass>& assetClasses = assetClasses_[nettingSetDetails];

            // Netting set level
            summaryReportIt->second->next();

            map<string, string> nettingSetMap = nettingSetDetails.mapRepresentation();
            for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                summaryReportIt->second->add(nettingSetMap[fieldName]);

            summaryReportIt->second->add("All")
                .add("All")
                .add(std::to_string(addOn(nettingSetDetails)))
                .add(std::to_string(NPV(nettingSetDetails)))
                .add(std::to_string(amountsBase_[nettingSetDetails].iah))
                .add(std::to_string(amountsBase_[nettingSetDetails].im))
                .add(std::to_string(amountsBase_[nettingSetDetails].vm))
                .add(std::to_string(amountsBase_[nettingSetDetails].tha))
                .add(std::to_string(amountsBase_[nettingSetDetails].mta))
                .add(std::to_string(RC(nettingSetDetails)))
                .add(std::to_string(multiplier(nettingSetDetails)))
                .add(std::to_string(PFE(nettingSetDetails)))
                .add(std::to_string(EAD(nettingSetDetails)))
                .add(std::to_string(RW(nettingSetDetails)))
                .add(std::to_string(CC(nettingSetDetails)));

            for (const AssetClass& assetClass : assetClasses) {
                // Asset class level
                summaryReportIt->second->next();

                for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                    summaryReportIt->second->add(nettingSetMap[fieldName]);

                summaryReportIt->second->add(to_string(assetClass))
                    .add("All")
                    .add(std::to_string(addOn(nettingSetDetails, assetClass)))
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("");

                const vector<string>& hedgingSets = this->hedgingSets(nettingSetDetails, assetClass);
                for (const string& hedgingSet : hedgingSets) {
                    // Hedging set level
                    summaryReportIt->second->next();

                    for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                        summaryReportIt->second->add(nettingSetMap[fieldName]);

                    summaryReportIt->second->add(to_string(assetClass))
                        .add(hedgingSet)
                        .add(std::to_string(addOn(nettingSetDetails, assetClass, hedgingSet)))
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("");
                }
            }
        }
        summaryReportIt->second->end();
    }
}

} // namespace analytics
} // namespace ore
