/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <boost/regex.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/engine/saccrtradedata.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/utilities.hpp>
#include <ored/portfolio/asianoption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/bondrepo.hpp>
#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/cashposition.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ored/portfolio/commoditydigitaloption.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/commodityposition.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/commodityswaption.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <ored/portfolio/equitybarrieroption.hpp>
#include <ored/portfolio/equitydigitaloption.hpp>
#include <ored/portfolio/equitydoublebarrieroption.hpp>
#include <ored/portfolio/equitydoubletouchoption.hpp>
#include <ored/portfolio/equityeuropeanbarrieroption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/equityposition.hpp>
#include <ored/portfolio/equitytouchoption.hpp>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/portfolio/fxdoublebarrieroption.hpp>
#include <ored/portfolio/fxdoubletouchoption.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/nettingsetdefinition.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/trs.hpp>
#include <ored/portfolio/varianceswap.hpp>
#include <ored/portfolio/windowbarrieroption.hpp>
#include <ored/scripting/scriptedinstrument.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/errors.hpp>
#include <ql/instruments/doublebarrieroption.hpp>
#include <ql/position.hpp>
#include <ql/settings.hpp>
#include <ql/time/daycounter.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

namespace {
#define TRY_AND_CATCH(expr, msg)                                                                                       \
    try {                                                                                                              \
        expr;                                                                                                          \
    } catch (const std::exception& e) {                                                                                \
        QL_FAIL("SA-CCR DEBUG: " << msg << ": " << e.what());                                                          \
    }

using QuantLib::close_enough;
using QuantLib::Real;
using QuantLib::Size;

Real phi(QuantLib::ext::optional<Real> P, QuantLib::ext::optional<Real> K, QuantLib::ext::optional<Real> T,
         QuantLib::ext::optional<Real> sigma, Real callPut) {
    QL_REQUIRE(P, "phi(): P cannot be null");
    QL_REQUIRE(!close_enough(*P, 0), "phi(): P cannot be zero");
    QL_REQUIRE(K, "phi(): K cannot be null");
    QL_REQUIRE(!close_enough(*K, 0), "phi(): K cannot be zero");
    QL_REQUIRE(T, "phi(): T cannot be null");
    QL_REQUIRE(!close_enough(*T, 0), "phi(): cannot divide by zero (T)");
    QL_REQUIRE(sigma, "phi(): sigma cannot be null");
    QL_REQUIRE(!close_enough(*sigma, 0), "phi(): sigma cannot be zero");

    if (QuantLib::close_enough(*T, 0)) {
        const Real x = callPut * std::log(*P / *K);
        return x > 0 ? 1 : -1;
    } else {
        const Real x =
            callPut * (std::log(*P / *K) + 0.5 * sigma.get() * sigma.get() * T.get()) / (*sigma * std::sqrt(*T));
        const QuantLib::CumulativeNormalDistribution N;
        return N(x);
    }
}

Real getOptionPrice(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {

    // Get additional results
    map<string, QuantLib::ext::any> addResults;
    if (auto vanillaOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::VanillaOptionTrade>(trade)) {
        addResults = vanillaOpt->instrument()->additionalResults();
    } else if (auto equityOpPosition = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOptionPosition>(trade)) {
        addResults = equityOpPosition->options().front()->additionalResults();
    } else {
        addResults = trade->instrument()->additionalResults();
    }

    // Get forward price
    if (auto it = addResults.find("forwardPrice"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);
    else if (auto it = addResults.find("atmForward"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);
    else if (auto it = addResults.find("forward"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);
    else if (auto it = addResults.find("Forward"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);
    else if (auto it = addResults.find("1_forward"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);
    else if (auto it = addResults.find("spot"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);

    QL_FAIL("getOptionPrice() Could not get option price");
}

Real getStrike(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {

    // Get additional results
    map<string, QuantLib::ext::any> addResults;
    if (true) {
        addResults = trade->instrument()->additionalResults();
    }

    // Get forward price
    if (auto it = addResults.find("Strike"); it != addResults.end())
        return QuantLib::ext::any_cast<Real>(it->second);

    QL_FAIL("getStrike() Could not get option strike");
}

bool isFixedLeg(const ore::data::LegData& legData) {
    return (legData.legType() == "Fixed" || legData.legType() == "Cashflow" || legData.legType() == "CommodityFixed");
}

QuantLib::Period getCMSIndexPeriod(const std::string& index) {
    std::vector<std::string> tokens;
    boost::split(tokens, index, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 3,
               "getCMSIndexEndDate() Expected 3 tokens. Got " << tokens.size() << ": '" << index << "'");
    return ore::data::parsePeriod(tokens[2]);
}

} // namespace

namespace ore {
namespace analytics {

// using ore::analytics::CrifRecord;
// using ore::analytics::StructuredAnalyticsErrorMessage;
// using ore::analytics::StructuredAnalyticsWarningMessage;
using ore::data::CollateralBalance;
using ore::data::CounterpartyCreditQuality;
using ore::data::CounterpartyInformation;
using ore::data::LegData;
using ore::data::NettingSetDefinition;
using ore::data::OptionData;
using ore::data::parseBarrierType;
using ore::data::parseCurrency;
using ore::data::parseCurrencyWithMinors;
using ore::data::parseDate;
using ore::data::parseDoubleBarrierType;
using ore::data::parseIndex;
using ore::data::parseOptionType;
using ore::data::parsePositionType;
using ore::data::Portfolio;
using ore::data::StructuredConfigurationErrorMessage;
using ore::data::StructuredConfigurationWarningMessage;
using ore::data::StructuredTradeErrorMessage;
using ore::data::StructuredTradeWarningMessage;
using ore::data::Trade;
using QuantLib::Barrier;
using QuantLib::Currency;
using QuantLib::DoubleBarrier;
using QuantLib::Leg;
using QuantLib::Null;
using QuantLib::Option;
using QuantLib::Period;
using QuantLib::Position;
using QuantLib::Real;
using QuantLib::ext::any;
using QuantLib::ext::any_cast;
using QuantLib::ext::dynamic_pointer_cast;
using QuantLib::ext::make_shared;
using QuantLib::ext::optional;
using QuantLib::ext::shared_ptr;
using std::make_pair;
using std::make_tuple;
using std::pair;
using std::set;
using std::string;
using std::tuple;
using std::vector;
using ProductClass = CrifRecord::ProductClass;
using RiskType = CrifRecord::RiskType;
using AssetClass = SaccrTradeData::AssetClass;
using AdjustedNotional = SaccrTradeData::AdjustedNotional;
using UnderlyingData = SaccrTradeData::UnderlyingData;
using Dates = SaccrTradeData::Dates;
using Contribution = SaccrTradeData::Contribution;
using HedgingData = SaccrTradeData::HedgingData;
using CommodityHedgingSet = SaccrTradeData::CommodityHedgingSet;
using FxAmounts = SaccrTradeData::FxAmounts;

// Ease the notation below
using OREAssetClass = ore::data::AssetClass;

// clang-format off
std::map<AssetClass, set<OREAssetClass>> saccrToOreAssetClassMap({
    {AssetClass::IR, {OREAssetClass::IR, OREAssetClass::INF}},
    {AssetClass::FX, {OREAssetClass::FX}},
    {AssetClass::Equity, {OREAssetClass::EQ}},
    {AssetClass::Commodity, {OREAssetClass::COM}},
    {AssetClass::Credit, {OREAssetClass::CR, OREAssetClass::BOND, OREAssetClass::BOND_INDEX}}, // BOND, BOND_INDEX ?
});

std::map<OREAssetClass, AssetClass> oreToSaccrAssetClassMap({
    {OREAssetClass::IR, AssetClass::IR},
    {OREAssetClass::INF, AssetClass::IR},
    {OREAssetClass::FX, AssetClass::FX},
    {OREAssetClass::EQ, AssetClass::Equity},
    {OREAssetClass::COM, AssetClass::Commodity},
    {OREAssetClass::CR, AssetClass::Credit},
    {OREAssetClass::BOND, AssetClass::Credit},
    {OREAssetClass::BOND_INDEX, AssetClass::Credit}
});
// clang-format on

void SaccrTradeData::initialise(const shared_ptr<Portfolio>& portfolio) {

    portfolio_ = portfolio;

    for (const auto& [id, trade] : portfolio_->trades()) {
        QL_REQUIRE(data_.find(id) == data_.end(),
                   "SaccrTradeData::buildImpl() TradeImpl already assigned for trade ID " << id);

        // Collect trade data
        try {
            data_[id] = getImpl(trade);
        } catch (const std::exception& e) {
            map<string, string> addFields({{"tradeId", id}, {"tradeType", trade->tradeType()}});
            StructuredAnalyticsErrorMessage("SA-CCR", "Could not get trade data impl", e.what(), addFields).log();
            continue;
        }

        // FIXME: Before calling `tradeImpl->calculate()`, we need to have trade counts and NPV first, because these
        // need to be known first before certain operations.
        //          e.g. trade count can affect the maturity factor, and NPV in a very small no. of cases
        //          (Cash/Commodity/Equity/EquityOption positions) is used to determine delta.

        // Store trade counts
        const auto& nsd = trade->envelope().nettingSetDetails();
        if (tradeCount_.find(nsd) == tradeCount_.end())
            tradeCount_[nsd] = 0;
        tradeCount_[nsd]++;

        // Collect NPV
        if (NPV_.find(nsd) == NPV_.end())
            NPV_[nsd] = 0.0;

        Real npv = data_[id]->NPV();
        Real npvBase;
        npvBase = npv * getFxRate(trade->npvCurrency() + baseCurrency_);

        NPV_[nsd] += npvBase;
    }

    validate();
    vector<string> implsToRemove;
    for (const auto& [tid, tradeImpl] : data_) {
        try {
            tradeImpl->calculate();
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage(
                "SA-CCR", "Getting SA-CCR trade impl",
                "Could not calculate contributions " + tradeImpl->name() + ": " + ore::data::to_string(e.what()),
                {{"tradeId", tradeImpl->trade()->id()}, {"tradeType", tradeImpl->trade()->tradeType()}})
                .log();
            implsToRemove.push_back(tid);
        }
    }

    for (const string& id : implsToRemove)
        data_.erase(id);

    // If we removed some impls - should we also update the trade count and NPV? Or maybe just fail the entire calc if
    // even one of the impls fails.
}

string SaccrTradeData::getUnderlyingName(const string& index, const OREAssetClass& assetClass, bool withPrefix) const {
    string name = index;

    // Remove prefix
    if (!withPrefix) {
        string prefix;
        switch (assetClass) {
        case OREAssetClass::COM:
            prefix = "COMM-";
            break;
        case OREAssetClass::EQ:
            prefix = "EQ-";
            break;
        case OREAssetClass::IR:
            prefix = "IR-";
            break;
        case OREAssetClass::FX:
            prefix = "FX-";
            break;
        default:
            break;
        }
        if (!prefix.empty() && name.substr(0, prefix.size()) == prefix)
            name.erase(0, prefix.size());
    }

    if (assetClass == OREAssetClass::COM) {
        // Remove expiry of form NAME-YYYY-MM-DD
        Date expiry;
        if (name.size() > 10) {
            string test = name.substr(name.size() - 10);
            if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}-\\d{2}"))) {
                expiry = parseDate(test);
                name = name.substr(0, name.size() - test.size() - 1);
            }
        }

        // Remove expiry of form NAME-YYYY-MM if NAME-YYYY-MM-DD failed
        if (expiry == Date() && name.size() > 7) {
            string test = name.substr(name.size() - 7);
            if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}"))) {
                expiry = parseDate(test + "-01");
                name = name.substr(0, name.size() - test.size() - 1);
            }
        }
    }
    return name;
}

const map<string, SaccrTradeData::CommodityHedgingSet> commodityBucketMapping = {
    {"1", CommodityHedgingSet::Energy},       {"2", CommodityHedgingSet::Energy},
    {"3", CommodityHedgingSet::Energy},       {"4", CommodityHedgingSet::Energy},
    {"5", CommodityHedgingSet::Energy},       {"6", CommodityHedgingSet::Energy},
    {"7", CommodityHedgingSet::Energy},       {"8", CommodityHedgingSet::Energy},
    {"9", CommodityHedgingSet::Energy},       {"11", CommodityHedgingSet::Metal},
    {"12", CommodityHedgingSet::Metal},       {"13", CommodityHedgingSet::Agriculture},
    {"14", CommodityHedgingSet::Agriculture}, {"15", CommodityHedgingSet::Agriculture},
    {"16", CommodityHedgingSet::Other},       {"10", CommodityHedgingSet::Other}};

string SaccrTradeData::getSimmQualifier(const string& name) const {

    string underlyingName = name;
    for (auto oreAssetClass : {OREAssetClass::COM, OREAssetClass::EQ, OREAssetClass::FX, OREAssetClass::IR,
                               OREAssetClass::CR, OREAssetClass::BOND, OREAssetClass::BOND_INDEX}) {

        underlyingName = getUnderlyingName(name, oreAssetClass);
        if (nameMapper_->hasQualifier(underlyingName))
            underlyingName = nameMapper_->qualifier(underlyingName);
        if (underlyingName != name)
            break;
    }

    return underlyingName;
}

string SaccrTradeData::getCommodityHedgingSet(const string& comm) const {
    auto qualifier = getSimmQualifier(comm);
    QL_REQUIRE(bucketMapper_, "no bucket name mapper provided");
    auto bucket = bucketMapper_->bucket(RiskType::Commodity, qualifier);
    const auto it = commodityBucketMapping.find(bucket);

    if (it == commodityBucketMapping.end())
        QL_FAIL("no hedging set found for " << qualifier);
    return ore::data::to_string(it->second);
}

const map<string, string> commodityQualifierMapping = {{"Coal Americas", "Coal"},
                                                       {"Coal Europe", "Coal"},
                                                       {"Coal Africa", "Coal"},
                                                       {"Coal Australia", "Coal"},
                                                       {"Crude oil Americas", "Crude oil"},
                                                       {"Crude oil Europe", "Crude oil"},
                                                       {"Crude oil Asia/Middle East", "Crude oil"},
                                                       {"Light Ends Americas", "Light Ends"},
                                                       {"Light Ends Europe", "Light Ends"},
                                                       {"Light Ends Asia", "Light Ends"},
                                                       {"Middle Distillates Americas", "Middle Distillates"},
                                                       {"Middle Distillates Europe", "Middle Distillates"},
                                                       {"Middle Distillates Asia", "Middle Distillates"},
                                                       {"Heavy Distillates Americas", "Heavy Distillates"},
                                                       {"Heavy Distillates Europe", "Heavy Distillates"},
                                                       {"Heavy Distillates Asia", "Heavy Distillates"},
                                                       {"NA Natural Gas Gulf Coast", "Natural Gas"},
                                                       {"NA Natural Gas North East", "Natural Gas"},
                                                       {"NA Natural Gas West", "Natural Gas"},
                                                       {"EU Natural Gas Europe", "Natural Gas"},
                                                       {"NA Power Eastern Interconnect", "Power"},
                                                       {"NA Power ERCOT", "Power"},
                                                       {"NA Power Western Interconnect", "Power"},
                                                       {"EU Power Germany", "Power"},
                                                       {"EU Power UK", "Power"}};

// const map<string, string> commodityQualifierMapping = {{"Coal Americas", "Coal"},
//                                                        {"Coal Europe", "Coal"},
//                                                        {"Coal Africa", "Coal"},
//                                                        {"Coal Australia", "Coal"},
//                                                        {"Crude oil Americas", "Oil"},
//                                                        {"Crude oil Europe", "Oil"},
//                                                        {"Crude oil Asia/Middle East", "Oil"},
//                                                        {"Light Ends Americas", "Light Ends"},
//                                                        {"Light Ends Europe", "Light Ends"},
//                                                        {"Light Ends Asia", "Light Ends"},
//                                                        {"Middle Distillates Americas", "Middle Distillates"},
//                                                        {"Middle Distillates Europe", "Middle Distillates"},
//                                                        {"Middle Distillates Asia", "Middle Distillates"},
//                                                        {"Heavy Distillates Americas", "Heavy Distillates"},
//                                                        {"Heavy Distillates Europe", "Heavy Distillates"},
//                                                        {"Heavy Distillates Asia", "Heavy Distillates"},
//                                                        {"NA Natural Gas Gulf Coast", "Gas"},
//                                                        {"NA Natural Gas North East", "Gas"},
//                                                        {"NA Natural Gas West", "Gas"},
//                                                        {"EU Natural Gas Europe", "Gas"},
//                                                        {"NA Power Eastern Interconnect", "Electricity"},
//                                                        {"NA Power ERCOT", "Electricity"},
//                                                        {"NA Power Western Interconnect", "Electricity"},
//                                                        {"EU Power Germany", "Electricity"},
//                                                        {"EU Power UK", "Electricity"}};

// TODO: shouldn't need mapQualifier arg here anymore
string SaccrTradeData::getCommodityHedgingSubset(const string& comm, bool mapQualifier) const {
    auto qualifier = getSimmQualifier(comm);

    // some qualifiers are grouped together, check if this is one
    if (mapQualifier)
        return getQualifierCommodityMapping(qualifier);

    return qualifier;
}

string SaccrTradeData::getQualifierCommodityMapping(const string& qualifier) const {
    if (auto it = commodityQualifierMapping.find(qualifier); it != commodityQualifierMapping.end())
        return it->second;

    return qualifier;
}

Real SaccrTradeData::getFxRate(const string& ccyPair) const {
    try {
        return market_->fxRate(ccyPair)->value();
    } catch (const std::exception& e) {
        QL_FAIL("SaccrTradeData::getFxRate() Could not get FX rate for ccy pair '" << ccyPair << "': " << e.what());
    }
}

set<OREAssetClass> SaccrTradeData::saccrAssetClassToOre(const AssetClass& saccrAssetClass) {
    QL_REQUIRE(saccrToOreAssetClassMap.find(saccrAssetClass) != saccrToOreAssetClassMap.end(),
               "saccrAssetClassToOre() : Invalid SA-CCR asset class " << saccrAssetClass);
    return saccrToOreAssetClassMap.at(saccrAssetClass);
}

AssetClass SaccrTradeData::oreAssetClassToSaccr(const OREAssetClass& oreAssetClass) {
    QL_REQUIRE(oreToSaccrAssetClassMap.find(oreAssetClass) != oreToSaccrAssetClassMap.end(),
               "oreAssetClassToSaccr() : Invalid ORE asset class " << oreAssetClass);
    return oreToSaccrAssetClassMap.at(oreAssetClass);
}

const std::string& SaccrTradeData::counterparty(const NettingSetDetails& nsd) const {
    QL_REQUIRE(nettingSetToCpty_.find(nsd) != nettingSetToCpty_.end(),
               "SaccrTradeData::counterparty() : Could not find netting set [" << ore::data::to_string(nsd) << "]");
    return *nettingSetToCpty_.at(nsd).begin();
}

const Real SaccrTradeData::NPV(const NettingSetDetails& nsd) const {
    QL_REQUIRE(NPV_.find(nsd) != NPV_.end(),
               "SaccrTradeData::NPV() : Could not find netting set [" << ore::data::to_string(nsd) << "]");
    return NPV_.at(nsd);
}

void SaccrTradeData::validate() {
    DLOG("SA-CCR: Validating configurations");

    const bool emptyNettingSetManager = nettingSetManager_->empty();
    const bool emptyCollateralBalances = collateralBalances_->empty();
    const bool emptyCounterpartyManager = counterpartyManager_->empty();

    // Check #1: Log a top-level warning message for configs that were not provided at all
    auto analyticSubField = map<string, string>({{"analyticType", "SA-CCR"}});
    if (emptyNettingSetManager) {
        StructuredConfigurationWarningMessage("Netting set definitions", "", "Validating input configurations",
                                              "Input configuration was not provided. The default values will be "
                                              "used for all netting sets in the portfolio",
                                              analyticSubField)
            .log();
    }
    if (emptyCollateralBalances) {
        StructuredConfigurationWarningMessage("Collateral balances", "", "Validating input configurations",
                                              "Input configuration was not provided. The default values will be "
                                              "used for all netting sets in the portfolio",
                                              analyticSubField)
            .log();
    }
    if (emptyCounterpartyManager) {
        StructuredConfigurationWarningMessage("Counterparty information", "", "Validating input configurations",
                                              "Input configuration was not provided. The default values will be "
                                              "used for all netting sets in the portfolio",
                                              analyticSubField)
            .log();
    }

    // Check #2: Collect list of netting sets from netting set definitions
    const vector<NettingSetDetails>& nettingSets = nettingSetManager_->uniqueKeys();
    nettingSets_ = set<NettingSetDetails>(nettingSets.begin(), nettingSets.end());

    DLOG("SA-CCR: Validating netting set definitions");

    // Check #3: Ensure that each trade has an existing entry in the netting set definitions
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        if (!nettingSetManager_->has(tradeNettingSetDetails)) {
            if (!emptyNettingSetManager) {
                StructuredConfigurationWarningMessage("Netting set definitions",
                                                      ore::data::to_string(tradeNettingSetDetails),
                                                      "Validating input configurations",
                                                      "Failed to find an entry for the given netting set "
                                                      "details, so the default configuration will be "
                                                      "assumed for this missing netting set definition.",
                                                      analyticSubField)
                    .log();
            }

            // Add default netting set definition entry in place of missing netting set
            NettingSetDefinition nsd = NettingSetDefinition(
                tradeNettingSetDetails, "Bilateral", baseCurrency_, "", 0.0, saCcrDefaults_.nettingSetDef.thresholdRcv,
                0.0, saCcrDefaults_.nettingSetDef.mtaRcv, saCcrDefaults_.nettingSetDef.iaHeld, "FIXED", "1D", "1D",
                ore::data::to_string(saCcrDefaults_.nettingSetDef.mpor), 0.0, 0.0, vector<string>(), false, "Bilateral",
                saCcrDefaults_.nettingSetDef.calculateIMAmount, saCcrDefaults_.nettingSetDef.calculateVMAmount);
            nettingSetManager_->add(QuantLib::ext::make_shared<NettingSetDefinition>(nsd));
            nettingSets_.insert(nsd.nettingSetDetails());
        }
    }

    DLOG("SA-CCR: Validating collateral balances");

    // Check #4: Check if there are balances that override the calculateIMAmount and caculateVMAmount in netting set
    // definitions
    set<NettingSetDetails> checkedNettingSets;
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        // To avoid duplicated warnings for the same netting set details
        if (checkedNettingSets.find(tradeNettingSetDetails) != checkedNettingSets.end())
            continue;

        checkedNettingSets.insert(tradeNettingSetDetails);

        const auto& nsd = nettingSetManager_->nettingSetDefinitions().at(tradeNettingSetDetails);
        if (!nsd->activeCsaFlag())
            continue;

        const bool calculateIM = nsd->csaDetails()->calculateIMAmount();
        const bool calculateVM = nsd->csaDetails()->calculateVMAmount();

        if (collateralBalances_->has(tradeNettingSetDetails)) {
            const auto& cb = collateralBalances_->get(tradeNettingSetDetails);
            if (calculateIM && cb->initialMargin() != Null<Real>()) {
                StructuredConfigurationWarningMessage(
                    "Collateral balances and netting set definitions", ore::data::to_string(tradeNettingSetDetails),
                    "Validating input configurations",
                    "CalculateIMAmount=True, but an initial margin amount was still provided. This overriding "
                    "initial margin balance will be used.",
                    analyticSubField)
                    .log();
            }
            if (calculateVM && cb->variationMargin() != Null<Real>()) {
                StructuredConfigurationWarningMessage(
                    "Collateral balances and netting set definitions", ore::data::to_string(tradeNettingSetDetails),
                    "Validating input configurations",
                    "CalculateVMAmount=True, but a variation margin amount was still provided. This overriding "
                    "variation margin balance will be used.",
                    analyticSubField)
                    .log();
            }
        }
    }

    // Check #5: Ensure that collateral balances are unique
    set<NettingSetDetails> netSetsToProcess;
    for (const auto& [tradeId, tradeImpl] : data())
        netSetsToProcess.insert(tradeImpl->trade()->envelope().nettingSetDetails());

    map<NettingSetDetails, int> collateralBalanceCounts;
    for (const auto& cb : collateralBalances_->collateralBalances()) {
        const NettingSetDetails& nettingSetDetails = cb.first;
        if (netSetsToProcess.count(nettingSetDetails) == 0)
            continue;

        if (collateralBalanceCounts.find(nettingSetDetails) == collateralBalanceCounts.end()) {
            collateralBalanceCounts.insert(make_pair(nettingSetDetails, 0));
        } else {
            collateralBalanceCounts[nettingSetDetails] += 1;
        }
    }
    for (const auto& c : collateralBalanceCounts) {
        const NettingSetDetails& nettingSetDetails = c.first;
        const int n = c.second;
        if (n > 1) {
            StructuredConfigurationWarningMessage(
                "Collateral balances", ore::data::to_string(nettingSetDetails), "Validating input configurations",
                "Multiple entries found (" + std::to_string(n) + ").", analyticSubField)
                .log();
        }
    }

    // Check #6: Ensure that each trade has an existing entry in the collateral balances
    checkedNettingSets.clear();
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        // To avoid duplicated warnings for the same netting set details
        if (checkedNettingSets.find(tradeNettingSetDetails) != checkedNettingSets.end())
            continue;

        // We require a collateral balance if there is none found in the collateral balances input file or in the
        // SIMM-generated collateral balances
        const auto& nsd = nettingSetManager_->nettingSetDefinitions().at(tradeNettingSetDetails);
        bool requiresCollateralBalance = nsd->activeCsaFlag() && !collateralBalances_->has(tradeNettingSetDetails);
        if (requiresCollateralBalance) {
            // If there is already a collateral balance from calculated IM and VM is to be calculated
            if (nsd->csaDetails()->calculateIMAmount() && calculatedCollateralBalances_->has(tradeNettingSetDetails) &&
                nsd->csaDetails()->calculateVMAmount())
                requiresCollateralBalance = false;
        }

        // if (requiresCollateralBalance && !collateralBalances_->has(tradeNettingSetDetails)) {
        if (requiresCollateralBalance) {
            if (!emptyCollateralBalances) {
                StructuredConfigurationWarningMessage("Collateral balances",
                                                      ore::data::to_string(tradeNettingSetDetails),
                                                      "Validating input configurations",
                                                      "Failed to find an entry for the given netting set "
                                                      "details, so the default configuration will be "
                                                      "assumed for this missing collateral balance.",
                                                      analyticSubField)
                    .log();
            }

            // Add default collateral balances entry in place of missing netting set
            CollateralBalance cb = CollateralBalance(tradeNettingSetDetails, saCcrDefaults_.collBalances.ccy,
                                                     saCcrDefaults_.collBalances.im, saCcrDefaults_.collBalances.vm);
            collateralBalances_->add(QuantLib::ext::make_shared<CollateralBalance>(cb));
            defaultIMBalances_.insert(tradeNettingSetDetails);
            defaultVMBalances_.insert(tradeNettingSetDetails);
        }

        checkedNettingSets.insert(tradeNettingSetDetails);
    }

    // Check #7: Ensure that each netting set has an entry in the collateral balances (even if it does not have a trade)
    for (const NettingSetDetails& nettingSetDetails : nettingSets_) {
        // We require a collateral balance if there is none found in the collateral balances input file
        const auto& nsd = nettingSetManager_->nettingSetDefinitions().at(nettingSetDetails);
        bool requiresCollateralBalance = nsd->activeCsaFlag();

        if (requiresCollateralBalance) {
            if (!collateralBalances_->has(nettingSetDetails) &&
                !calculatedCollateralBalances_->has(nettingSetDetails)) {
                // Add default collateral balances entry in place of missing netting set
                CollateralBalance cb =
                    CollateralBalance(nettingSetDetails, saCcrDefaults_.collBalances.ccy,
                                      saCcrDefaults_.collBalances.im, saCcrDefaults_.collBalances.vm);
                collateralBalances_->add(QuantLib::ext::make_shared<CollateralBalance>(cb));
                defaultIMBalances_.insert(nettingSetDetails);
                defaultVMBalances_.insert(nettingSetDetails);
            } else if (collateralBalances_->has(nettingSetDetails)) {
                auto& cb = collateralBalances_->get(nettingSetDetails);
                if (cb->variationMargin() == Null<Real>() &&
                    !nettingSetManager_->get(nettingSetDetails)->csaDetails()->calculateVMAmount()) {
                    cb->variationMargin() = saCcrDefaults_.collBalances.vm;
                    defaultVMBalances_.insert(nettingSetDetails);

                    StructuredConfigurationWarningMessage(
                        "Collateral balances", ore::data::to_string(nettingSetDetails),
                        "Validating input configurations",
                        "CalculateVMAmount was set to \'false\' in the netting "
                        "set definition, but no VariationMargin "
                        "was provided in the collateral balance. The default value of " +
                            std::to_string(saCcrDefaults_.collBalances.vm) + " will be assumed.",
                        analyticSubField)
                        .log();
                }
            }
        }
    }

    DLOG("SA-CCR: Validating counterparty information");

    // Check #8: Ensure that each trade has an existing entry in the counterparty information
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const string tradeCpty = trade->envelope().counterparty();
        if (!counterpartyManager_->has(tradeCpty)) {
            if (!emptyCounterpartyManager) {
                StructuredConfigurationWarningMessage(
                    "Counterparty information", tradeCpty, "Validating input configurations",
                    "Failed to find an entry for the given counterparty, so the default configuration will be "
                    "assumed for this counterparty",
                    analyticSubField)
                    .log();
            }

            // Add default counterparty entry in place of missing counterparty
            CounterpartyInformation cptyInfo =
                CounterpartyInformation(tradeCpty, saCcrDefaults_.cptyInfo.ccp, CounterpartyCreditQuality::NR,
                                        Null<Real>(), saCcrDefaults_.cptyInfo.saccrRW, "");
            counterpartyManager_->add(QuantLib::ext::make_shared<CounterpartyInformation>(cptyInfo));
        }
    }

    // Check #9: Create default counterparty information for netting sets that do not have trades
    // (since we create nettingSet-counterparty mappings via trades)
    if (!counterpartyManager_->has(saCcrDefaults_.cptyInfo.counterpartyId)) {
        // Add default counterparty entry in place of missing counterparty
        CounterpartyInformation cptyInfo =
            CounterpartyInformation(saCcrDefaults_.cptyInfo.counterpartyId, saCcrDefaults_.cptyInfo.ccp,
                                    CounterpartyCreditQuality::NR, Null<Real>(), saCcrDefaults_.cptyInfo.saccrRW, "");
        counterpartyManager_->add(QuantLib::ext::make_shared<CounterpartyInformation>(cptyInfo));
    }

    // Check #10: Check that each counterparty SA-CCR risk weight is between 0 and 1.5
    set<string> checkedCounterparties;
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const string& tradeCpty = trade->envelope().counterparty();

        // To avoid duplicated warnings for the same netting set details
        if (checkedCounterparties.find(tradeCpty) != checkedCounterparties.end())
            continue;

        const auto& c = counterpartyManager_->counterpartyInformation().at(tradeCpty);
        if (c->saCcrRiskWeight() < 0.0 || c->saCcrRiskWeight() > 1.5) {
            StructuredConfigurationWarningMessage(
                "Counterparty Information", tradeCpty, "Validating input configurations",
                "Unexpected SA-CCR risk weight (should be between 0.0 and 1.5, inclusive). The provided amount of " +
                    std::to_string(c->saCcrRiskWeight()) + " will still be used in subsequent calculations.",
                analyticSubField)
                .log();
        }
        checkedCounterparties.insert(tradeCpty);
    }
    checkedCounterparties.clear();

    // Check #11: For netting sets with clearing counterparty, ensure that initial margin is zero
    set<NettingSetDetails> clearingNettingSets;
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const NettingSetDetails& nettingSetDetails = trade->envelope().nettingSetDetails();
        const string& cpty = trade->envelope().counterparty();
        const bool isClearingCp = counterpartyManager_->get(cpty)->isClearingCP();
        if (isClearingCp)
            clearingNettingSets.insert(nettingSetDetails);
    }

    for (const NettingSetDetails& nsd : clearingNettingSets) {
        if (collateralBalances_->has(nsd)) {
            const auto& cb = collateralBalances_->get(nsd);
            if (cb->initialMargin() != Null<Real>())
                cb->initialMargin() = 0.0;
        }
        if (calculatedCollateralBalances_ && calculatedCollateralBalances_->has(nsd)) {
            const auto& cb = calculatedCollateralBalances_->get(nsd);
            if (cb->initialMargin() != Null<Real>())
                cb->initialMargin() = 0.0;
        }
    }

    // Check #12: Ensuring each netting set has a counterparty ID associated to it
    for (const auto& [id, tradeImpl] : data()) {
        // build up nettingSet -> counterparty map for the aggregation step
        nettingSetToCpty_[tradeImpl->nettingSetDetails()].insert(tradeImpl->counterparty());
    }
    for (const NettingSetDetails& nsd : nettingSets_) {
        if (nettingSetToCpty_.find(nsd) == nettingSetToCpty_.end())
            nettingSetToCpty_[nsd] = {saCcrDefaults_.cptyInfo.counterpartyId};
    }
    // Validate nettingSet-to-counterParty map. Only allow 1-to-1 and many-to-1 mappings.
    for (const auto& n : nettingSetToCpty_) {
        if (n.second.size() > 1) {
            StructuredAnalyticsWarningMessage(
                "SA-CCR", "Invalid netting set and counterparty pair",
                ore::data::to_string(n.first) +
                    ": Found more than one counterparty associated with this netting set."
                    "The first SA-CCR risk weight found will be used for this netting set."
                    " Please check the netting set and counterparty details in the portfolio.")
                .log();
        }
    }

    // Check #13: Final confirmation/validation, which itself is a validation of the previous checks:
    // For each trade, check that there is a collateral balance, netting set definition and counterparty info
    for (const auto& [tradeId, tradeImpl] : data()) {
        const auto& trade = tradeImpl->trade();
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();
        const string& cpty = trade->envelope().counterparty();

        QL_REQUIRE(nettingSetManager_->has(tradeNettingSetDetails),
                   "Trade id \'" << tradeId << "\': Could not find corresponding entry for [" << tradeNettingSetDetails
                                 << "] in the netting set definitions.");

        if (nettingSetManager_->get(tradeNettingSetDetails)->activeCsaFlag()) {
            QL_REQUIRE(collateralBalances_->has(tradeNettingSetDetails) ||
                           calculatedCollateralBalances_->has(tradeNettingSetDetails),
                       "Trade id \'" << tradeId << "\': Could not find corresponding entry for ["
                                     << tradeNettingSetDetails << "] in the collateral balances.");
            QL_REQUIRE(counterpartyManager_->has(cpty),
                       "Trade id \'" << tradeId << "\': Could not find corresponding counerparty entry for " << cpty
                                     << " in the counterparty information.");
        }
    }

    // Check #14
    // Set default NPV for any additional netting sets added in validate() step
    for (const auto& nsd : nettingSets_) {
        if (NPV_.find(nsd) == NPV_.end())
            NPV_[nsd] = 0.0;
    }
}

set<shared_ptr<SaccrTradeData::Impl>> impls = {make_shared<ScriptedTradeSaccrImpl>(),
                                               make_shared<VarianceSwapSaccrImpl>(),
                                               make_shared<BondRepoSaccrImpl>(),
                                               make_shared<BondTRSSaccrImpl>(),
                                               make_shared<CommodityForwardSaccrImpl>(),
                                               make_shared<CommodityDigitalOptionSaccrImpl>(),
                                               make_shared<CommoditySpreadOptionSaccrImpl>(),
                                               make_shared<CommoditySwaptionSaccrImpl>(),
                                               make_shared<CommodityPositionSaccrImpl>(),
                                               make_shared<EquityForwardSaccrImpl>(),
                                               make_shared<EquityDigitalOptionSaccrImpl>(),
                                               make_shared<EquityTouchOptionSaccrImpl>(),
                                               make_shared<EquityDoubleTouchOptionSaccrImpl>(),
                                               make_shared<EquityBarrierOptionSaccrImpl>(),
                                               make_shared<EquityDoubleBarrierOptionSaccrImpl>(),
                                               make_shared<EquityPositionSaccrImpl>(),
                                               make_shared<EquityOptionPositionSaccrImpl>(),
                                               make_shared<CashPositionSaccrImpl>(),
                                               make_shared<FRASaccrImpl>(),
                                               make_shared<CapFloorSaccrImpl>(),
                                               make_shared<TotalReturnSwapSaccrImpl>(),
                                               make_shared<SwapSaccrImpl>(),
                                               make_shared<SwaptionSaccrImpl>(),
                                               make_shared<VanillaOptionSaccrImpl>(),
                                               make_shared<FxSaccrImpl>()};

shared_ptr<SaccrTradeData::Impl> SaccrTradeData::getImpl(const shared_ptr<Trade>& trade) {

    shared_ptr<Impl> tradeImpl;

    set<string> skipTradeTypes = {"UseCounterparty", "Failed"};
    if (skipTradeTypes.find(trade->tradeType()) != skipTradeTypes.end())
        QL_FAIL("Skipping " + trade->tradeType() + " trade.");

    for (const auto& impl : impls) {
        auto implTradeTypes = impl->getTradeTypes();
        if (implTradeTypes.find(trade->tradeType()) != implTradeTypes.end()) {
            tradeImpl = impl->copy();
            break;
        }
    }

    QL_REQUIRE(tradeImpl, "SA-CCR trade data not yet implemented for trade type " << trade->tradeType() << ".");

    tradeImpl->setTradeData(QuantLib::ext::weak_ptr<SaccrTradeData>(shared_from_this()));
    tradeImpl->setTrade(trade);

    return tradeImpl;
}

std::ostream& operator<<(std::ostream& os, SaccrTradeData::AssetClass ac) {
    switch (ac) {
    case SaccrTradeData::IR:
        return os << "IR";
    case SaccrTradeData::FX:
        return os << "FX";
    case SaccrTradeData::Credit:
        return os << "Credit";
    case SaccrTradeData::Equity:
        return os << "Equity";
    case SaccrTradeData::Commodity:
        return os << "Commodity";
    case SaccrTradeData::None:
        return os << "AssetClass::None";
    default:
        QL_FAIL("Unknown SaccrTradeData::AssetClass");
    }
}

std::ostream& operator<<(std::ostream& os, SaccrTradeData::CommodityHedgingSet hs) {
    switch (hs) {
    case SaccrTradeData::CommodityHedgingSet::Energy:
        return os << "Energy";
    case SaccrTradeData::CommodityHedgingSet::Agriculture:
        return os << "Agriculture";
    case SaccrTradeData::CommodityHedgingSet::Metal:
        return os << "Metal";
    case SaccrTradeData::CommodityHedgingSet::Other:
        return os << "Other";
    default:
        QL_FAIL("Unknown SaccrTradeData::AssetClass");
    }
}

UnderlyingData SaccrTradeData::Impl::getUnderlyingData(const string& originalName,
                                                       const optional<OREAssetClass>& oreAssetClass) const {

    OREAssetClass assetClass = OREAssetClass::EQ;
    if (oreAssetClass.has_value()) {
        assetClass = oreAssetClass.get();
    } else {
        try {
            auto index = parseIndex(originalName);
            if (dynamic_pointer_cast<QuantExt::EquityIndex2>(index)) {
                assetClass = OREAssetClass::EQ;
            } else if (dynamic_pointer_cast<QuantExt::FxIndex>(index)) {
                assetClass = OREAssetClass::FX;
            } else if (dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                assetClass = OREAssetClass::COM;
            } else if (dynamic_pointer_cast<QuantLib::IborIndex>(index) ||
                       dynamic_pointer_cast<QuantLib::SwapIndex>(index)) {
                assetClass = OREAssetClass::IR;
            } else if (dynamic_pointer_cast<QuantLib::ZeroInflationIndex>(index)) {
                assetClass = OREAssetClass::INF;
            } else if (dynamic_pointer_cast<QuantExt::BondIndex>(index) ||
                       dynamic_pointer_cast<QuantExt::ConstantMaturityBondIndex>(index)) {
                assetClass = OREAssetClass::BOND_INDEX;
            }
        } catch (...) {
            QL_FAIL(name() << "::getUnderlyingData() Could not derive underlying data asset class from name '"
                           << originalName << "'.");
        }
    }

    string qualifier;
    if (assetClass == OREAssetClass::FX) {
        vector<string> tokens;
        // e.g. FX-ECB-EUR-USD or EUR-USD
        boost::split(tokens, originalName, boost::is_any_of(" -/"));
        QL_REQUIRE(
            tokens.size() >= 2,
            "SaccrTradeData::getUnderlyings() Cannot get currencies. Expected at least 2 tokens: " << originalName);
        vector<string> ccys;
        ccys.push_back(tokens[tokens.size() - 1]);
        ccys.push_back(tokens[tokens.size() - 2]);
        std::sort(ccys.begin(), ccys.end());
        qualifier = boost::join(ccys, "");
    } else if (assetClass == OREAssetClass::EQ || assetClass == OREAssetClass::CR ||
               assetClass == OREAssetClass::BOND || assetClass == OREAssetClass::BOND_INDEX) {
        qualifier = tradeData_.lock()->getSimmQualifier(originalName);
    } else {
        // At least IR and COMM
        qualifier = tradeData_.lock()->getUnderlyingName(originalName, assetClass);
    }

    bool isIndex = false;
    if (assetClass == OREAssetClass::BOND_INDEX) {
        isIndex = true;
    } else if (assetClass == OREAssetClass::EQ || assetClass == OREAssetClass::CR) {
        // TODO: Check if isIndex = true from reference data
    }

    AssetClass saccrAssetClass = oreAssetClassToSaccr(assetClass);

    return UnderlyingData(originalName, qualifier, saccrAssetClass, assetClass, isIndex);
}

UnderlyingData SaccrTradeData::Impl::getUnderlyingData(const string& boughtCurrency, const string& soldCurrency) const {
    return getUnderlyingData(soldCurrency + "-" + boughtCurrency, OREAssetClass::FX);
}

void SaccrTradeData::Impl::calculate() {

    contributions_ = calculateContributions();
    calculated_ = true;
}

vector<Contribution> SaccrTradeData::Impl::calculateContributions() const {
    vector<Contribution> contributions = calculateImplContributions();

    addHedgingData(contributions);

    // Default trade start/end date if start/end date was not populated
    // Start date (S) / End date (E)
    Date today = QuantLib::Settings::instance().evaluationDate();
    Date startDate = Date::maxDate();
    Date endDate = Date::minDate();

    for (const auto& l : trade_->legs()) {
        if (!l.empty()) {
            startDate = std::min(startDate, l.front()->date());
            endDate = std::max(endDate, l.back()->date());
        }
    }

    for (auto& c : contributions) {
        auto assetClass = c.underlyingData.saccrAssetClass;
        if (assetClass == AssetClass::IR || assetClass == AssetClass::Credit) {
            if (!c.startDate.has_value())
                c.startDate = startDate > today ? dc().yearFraction(today, startDate) : 0;

            // Trade should be matured if this condition is not true, but we include it here just in case
            if (!c.endDate.has_value())
                c.endDate = endDate > today ? dc().yearFraction(today, endDate) : 0;
        }

        c.adjustedNotional *=
            getSupervisoryDuration(c.underlyingData.saccrAssetClass, c.startDate, c.endDate).get_value_or(1.0);
        if (c.maturity == Null<Real>())
            c.maturity = getMaturity();
        if (c.maturityFactor == Null<Real>())
            c.maturityFactor = getMaturityFactor(c.maturity);
        if (c.bucket.empty())
            c.bucket = getBucket(c);
    }
    return contributions;
}

AdjustedNotional SaccrTradeData::Impl::getFxAdjustedNotional(const FxAmounts& fxAmounts) const {

    const string& baseCcy = tradeData_.lock()->baseCurrency();

    Real soldFx = getFxRate(fxAmounts.soldCurrency + baseCcy);
    Real boughtFx = getFxRate(fxAmounts.boughtCurrency + baseCcy);

    Real soldNotional = fxAmounts.soldAmount * soldFx;
    Real boughtNotional = fxAmounts.boughtAmount * boughtFx;

    Real notional = std::max(boughtNotional, soldNotional);

    return AdjustedNotional(notional, baseCcy);
}

FxAmounts SaccrTradeData::Impl::getSingleFxAmounts(const shared_ptr<Trade>& trade) const {
    const auto& tradePtr = trade ? trade : trade_;

    if (auto fxBarrierOption = dynamic_pointer_cast<ore::data::FxOptionWithBarrier>(tradePtr)) {
        // FxBarrierOption, FxDoubleBarrierOption
        return FxAmounts(fxBarrierOption->boughtAmount(), fxBarrierOption->boughtCurrency(),
                         fxBarrierOption->soldAmount(), fxBarrierOption->soldCurrency());
    } else if (auto fxDBarrierOption = dynamic_pointer_cast<ore::data::FxEuropeanBarrierOption>(tradePtr)) {
        return FxAmounts(fxDBarrierOption->boughtAmount(), fxDBarrierOption->boughtCurrency(),
                         fxDBarrierOption->soldAmount(), fxDBarrierOption->soldCurrency());
    } else if (auto fxFwd = dynamic_pointer_cast<ore::data::FxForward>(tradePtr)) {
        return FxAmounts(fxFwd->boughtAmount(), fxFwd->boughtCurrency(), fxFwd->soldAmount(), fxFwd->soldCurrency());
    } else if (auto fxSwap = dynamic_pointer_cast<ore::data::FxSwap>(tradePtr)) {
        return FxAmounts(fxSwap->nearBoughtAmount(), fxSwap->nearBoughtCurrency(), fxSwap->nearSoldAmount(),
                         fxSwap->nearSoldCurrency());
    } else if (auto fxOpt = dynamic_pointer_cast<ore::data::FxOption>(tradePtr)) {
        return FxAmounts(fxOpt->boughtAmount(), fxOpt->boughtCurrency(), fxOpt->soldAmount(), fxOpt->soldCurrency());
    } else if (auto fxDigitalOption = dynamic_pointer_cast<ore::data::FxDigitalOption>(tradePtr)) {
        Real boughtAmount = fxDigitalOption->payoffAmount();
        string boughtCcy = fxDigitalOption->payoffCurrency();
        Real soldAmount = 0;
        string soldCcy = tradeData_.lock()->baseCurrency();
        return FxAmounts(boughtAmount, boughtCcy, soldAmount, soldCcy);
    } else if (auto fxDigitalBarOption = dynamic_pointer_cast<ore::data::FxDigitalBarrierOption>(tradePtr)) {
        Real boughtAmount = fxDigitalBarOption->payoffAmount();
        string boughtCcy = fxDigitalBarOption->notionalCurrency();
        Real soldAmount = 0;
        string soldCcy = tradeData_.lock()->baseCurrency();
        return FxAmounts(boughtAmount, boughtCcy, soldAmount, soldCcy);
    } else if (auto fxDer = dynamic_pointer_cast<ore::data::FxSingleAssetDerivative>(tradePtr)) {
        // TODO: nulls
        string boughtCcy = fxDer->boughtCurrency();
        string soldCcy = fxDer->soldCurrency();

        Real boughtAmount = Null<Real>(), soldAmount = Null<Real>();
        if (auto fxKikoBarrierOption = dynamic_pointer_cast<ore::data::FxKIKOBarrierOption>(fxDer)) {
            boughtAmount = fxKikoBarrierOption->boughtAmount();
            soldAmount = fxKikoBarrierOption->soldAmount();
        } else if (auto fxTouchOption = dynamic_pointer_cast<ore::data::FxTouchOption>(fxDer)) {
            boughtAmount = fxTouchOption->payoffAmount();
            soldAmount = 0;
        } else if (auto fxDTouchOption = dynamic_pointer_cast<ore::data::FxDoubleTouchOption>(fxDer)) {
            boughtAmount = fxDTouchOption->payoffAmount();
            soldAmount = 0;
        } else {
            QL_FAIL(name() << "::getSingleFxAmounts() Could not get bought/sold amount");
        }

        return FxAmounts(boughtAmount, boughtCcy, soldAmount, soldCcy);
    } else if (auto swap = dynamic_pointer_cast<ore::data::Swap>(tradePtr)) {
        vector<tuple<Real, string, bool>> ccyAmounts;
        for (const auto& ld : swap->legData()) {
            if (isFixedLeg(ld))
                continue;

            // TODO: Handle varying notionals
            ccyAmounts.push_back(make_tuple(ld.notionals().front(), ld.currency(), ld.isPayer()));
        }
        QL_REQUIRE(ccyAmounts.size() == 2,
                   name() << "::getSingleFxAmounts() Swap type must have exactly 2 currencies. Found "
                          << ccyAmounts.size());
        string boughtCurrency, soldCurrency;
        Real soldAmount = Null<Real>(), boughtAmount = Null<Real>();
        for (const auto& [amount, ccy, isPayer] : ccyAmounts) {
            if (isPayer) {
                soldCurrency = ccy;
                soldAmount = amount;
            } else {
                boughtCurrency = ccy;
                boughtAmount = amount;
            }
        }
        QL_REQUIRE(!boughtCurrency.empty() && !soldCurrency.empty(),
                   "Swap type must have exactly 1 payer currency and 1 sold currency.");
        return FxAmounts(boughtAmount, boughtCurrency, soldAmount, soldCurrency);
    } else {
        QL_FAIL("getSingleFxAmounts() unsupported trade type " << tradePtr->tradeType());
    }
}

const vector<Contribution>& SaccrTradeData::Impl::getContributions() const {

    if (!calculated_)
        QL_FAIL(name() << "::getContributions() calculate() method must be called first");

    return contributions_;
}

void SaccrTradeData::Impl::addHedgingData(vector<Contribution>& contributions) const {
    // Populate hedging sets/subsets
    for (auto& c : contributions) {
        if (!c.hedgingData.empty())
            continue;

        HedgingData& hedgingData = c.hedgingData;

        if (c.underlyingData.saccrAssetClass == AssetClass::FX) {
            QL_REQUIRE(c.underlyingData.qualifier.size() == 6,
                       name() << "::getHedgingData() Expected FX underlying name in the form CCY1CCY2. Got "
                              << c.underlyingData.qualifier << ".");
            vector<string> undCurrencies(
                {c.underlyingData.qualifier.substr(0, 3), c.underlyingData.qualifier.substr(3, 6)});
            std::sort(undCurrencies.begin(), undCurrencies.end());
            string hedgingSet = boost::join(undCurrencies, "");

            hedgingData.hedgingSet = hedgingSet;
        } else if (c.underlyingData.saccrAssetClass == AssetClass::IR) {
            string ccy;
            if (c.underlyingData.oreAssetClass == OREAssetClass::IR) {
                try {
                    ccy = parseCurrency(c.underlyingData.qualifier.substr(0, 3)).code();
                } catch (...) {
                    try {
                        Size n = c.underlyingData.qualifier.size();
                        ccy = parseCurrency(c.underlyingData.qualifier.substr(n - 3, n)).code();
                    } catch (...) {
                        QL_FAIL(name() << "::addHedgingData() Could not get currency from IR index '"
                                       << c.underlyingData.qualifier << "'");
                    }
                }
            }
            if (c.underlyingData.oreAssetClass == OREAssetClass::INF && ccy.empty()) {
                // TODO: May not always be right, but good enough for most INF based products
                ccy = trade_->legCurrencies().front();
            }

            string hedgingSet = ccy;
            if (c.underlyingData.oreAssetClass == OREAssetClass::INF)
                hedgingSet = ccy + "_INFL";

            hedgingData.hedgingSet = hedgingSet;

            // TODO: get IR hedging subset (maturity buckets)

        } else if (c.underlyingData.saccrAssetClass == AssetClass::Commodity) {
            hedgingData.hedgingSet = tradeData_.lock()->getCommodityHedgingSet(c.underlyingData.qualifier);
            hedgingData.hedgingSubset = tradeData_.lock()->getCommodityHedgingSubset(c.underlyingData.qualifier);
        } else if (c.underlyingData.saccrAssetClass == AssetClass::Equity ||
                   c.underlyingData.saccrAssetClass == AssetClass::Credit) {
            hedgingData.hedgingSet = ore::data::to_string(c.underlyingData.saccrAssetClass);
            hedgingData.hedgingSubset = c.underlyingData.qualifier;
        }

        // Volatility transactions
        if (c.isVol)
            hedgingData.hedgingSet += "_VOL";
    }

    // FIXME:
    // There may be cases where we have > 2 contributions, but still 2 underlyings (as defined by
    // Contribution.underlyingData), e.g. 2 contributions from Underlying1, 1 contribution from Underlying2.
    bool isBasis =
        // Exactly 2 contributions
        contributions.size() == 2 &&
        // Same asset class & hedging set
        contributions[0].underlyingData.saccrAssetClass == contributions[1].underlyingData.saccrAssetClass &&
        contributions[0].hedgingData.hedgingSet == contributions[1].hedgingData.hedgingSet &&
        contributions[0].underlyingData.oreAssetClass != OREAssetClass::FX &&
        contributions[1].underlyingData.oreAssetClass != OREAssetClass::FX &&
        // Same ccy
        contributions[0].currency == contributions[1].currency &&
        // Different underlyings
        contributions[0].underlyingData != contributions[1].underlyingData &&
        // One is long w.r.t. the underlying, the other is short
        contributions[0].delta * contributions[1].delta < 0.0;

    if (isBasis) {
        auto& c1 = contributions[0];
        auto& c2 = contributions[1];
        // Update hedging set
        string newHedgingSet = c1.hedgingData.hedgingSet + "_BASIS";
        c1.hedgingData.hedgingSet = newHedgingSet;
        c2.hedgingData.hedgingSet = newHedgingSet;

        // Update hedging subset
        vector<string> qualifiers({c1.underlyingData.qualifier, c2.underlyingData.qualifier});
        std::sort(qualifiers.begin(), qualifiers.end());

        if (c1.underlyingData.qualifier == qualifiers[0] && c1.delta < 0) {
            c1.delta *= -1.0;
            c2.delta *= -1.0;
        }

        string newHedgingSubset = boost::join(qualifiers, "_");
        c1.hedgingData.hedgingSubset = newHedgingSubset;
        c2.hedgingData.hedgingSubset = newHedgingSubset;
    }
}

Real SaccrTradeData::Impl::getMaturity(const shared_ptr<Trade>& trade) const {
    auto& tradePtr = trade ? trade : trade_;

    Date today = QuantLib::Settings::instance().evaluationDate();
    Date matDate = tradePtr->maturity();
    Real M = matDate <= today ? 0.0 : dc().yearFraction(today, matDate);

    return M;
}

optional<Real> SaccrTradeData::Impl::getSupervisoryDuration(const AssetClass& assetClass,
                                                            const optional<Real>& startDate,
                                                            const optional<Real>& endDate) const {

    optional<Real> supervisoryDuration = boost::none;

    if (assetClass == AssetClass::IR || assetClass == AssetClass::Credit) {
        QL_REQUIRE(startDate.has_value() && endDate.has_value(),
                   "SaccrTradeData::Impl::getSupervisoryDuration() : start and end date cannot be null");
        return (exp(-0.05 * startDate.get()) - exp(-0.05 * endDate.get())) / 0.05;
    }

    return supervisoryDuration;
}

Real SaccrTradeData::Impl::getSupervisoryOptionVolatility(const UnderlyingData& underlyingData) const {

    Real sigma = Null<Real>();

    const auto& assetClass = underlyingData.saccrAssetClass;
    if (assetClass == AssetClass::Equity) {
        if (underlyingData.isIndex)
            sigma = 0.75;
        else
            sigma = 1.2;
    } else if (assetClass == AssetClass::Credit) {
        if (underlyingData.isIndex)
            sigma = 0.8;
        else
            sigma = 1.0;
    } else if (assetClass == AssetClass::IR) {
        sigma = 0.5;
    } else if (assetClass == AssetClass::FX) {
        sigma = 0.15;
    } else if (assetClass == AssetClass::Commodity) {
        string hedgingSet = tradeData_.lock()->getCommodityHedgingSet(underlyingData.qualifier);
        string hedgingSubset = tradeData_.lock()->getCommodityHedgingSubset(underlyingData.qualifier);
        if (hedgingSet == ore::data::to_string(CommodityHedgingSet::Energy)) {
            string hssLower = boost::to_lower_copy(hedgingSubset);
            if (hssLower.find("oil") != string::npos || hssLower.find("gas") != string::npos)
                sigma = 0.7;
            else
                sigma = 1.5;
        } else {
            sigma = 0.7;
        }
    } else {
        QL_FAIL(name() << "::getSupervisoryOptionVolatility() Unknown asset class: "
                       << ore::data::to_string(assetClass));
    }

    return sigma;
}

vector<Contribution> SaccrTradeData::Impl::calculateSingleOptionContribution(const shared_ptr<Trade>& trade) const {
    auto& tradePtr = trade ? trade : trade_;

    // Get underlying data, option data and notional-related things (handle delta after hedging data has been added)
    OptionData optionData;
    Contribution contrib;
    Real callPut = Null<Real>();
    Real price = getOptionPrice(tradePtr);
    // TODO: Must set also strike, optionDeltaPrice
    if (auto eqBarrierOption = dynamic_pointer_cast<ore::data::EquityBarrierOption>(tradePtr)) {
        auto underlyingData = getUnderlyingData(eqBarrierOption->equityName(), OREAssetClass::EQ);
        optionData = eqBarrierOption->option();

        const string& currency = eqBarrierOption->tradeCurrency().code();
        Real quantity = eqBarrierOption->quantity();
        Real adjNotional = quantity * price;

        contrib = Contribution(underlyingData, currency, adjNotional);
        contrib.strike = eqBarrierOption->strike();
    } else if (auto eqDBarrierOpt = dynamic_pointer_cast<ore::data::EquityDoubleBarrierOption>(tradePtr)) {
        auto underlyingData = getUnderlyingData(eqDBarrierOpt->equityName(), OREAssetClass::EQ);
        optionData = eqDBarrierOpt->option();

        const string& currency = eqDBarrierOpt->tradeCurrency().code();
        Real quantity = eqDBarrierOpt->quantity();
        Real adjNotional = quantity * price;

        contrib = Contribution(underlyingData, currency, adjNotional);
        contrib.strike = eqDBarrierOpt->strike();
    } else if (auto commSwaption = dynamic_pointer_cast<ore::data::CommoditySwaption>(tradePtr)) {
        auto underlyingData = getUnderlyingData(commSwaption->name(), OREAssetClass::COM);
        optionData = commSwaption->option();
        QL_REQUIRE(optionData.style() != "Bermudan",
                   name() << "::calculateSingleOptionContribution() Bermudan swaption not supported.");

        // Call/Put
        for (Size i = 0; i < commSwaption->legData().size(); ++i) {
            if (auto leg = QuantLib::ext::dynamic_pointer_cast<ore::data::CommodityFixedLegData>(
                    commSwaption->legData()[i].concreteLegData())) {
                // If callPut is already defined, then it was already set on a previous leg
                if (callPut != Null<Real>())
                    QL_FAIL(name() << "::calculateSingleOptionContribution(): Could not get option type. Found more "
                                      "than one CommodityFixed leg.");

                callPut = commSwaption->legData()[i].isPayer() ? -1 : 1;
            }
        }

        const string& currency = commSwaption->notionalCurrency();
        Real strike = getStrike(commSwaption);
        Real adjNotional = commSwaption->notional();

        contrib = Contribution(underlyingData, currency, adjNotional);
        contrib.strike = strike;
    } else if (auto fxBarrierOption = dynamic_pointer_cast<ore::data::FxBarrierOption>(tradePtr)) {
        // TODO
        auto fxAmounts = getSingleFxAmounts(fxBarrierOption);
        auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
        optionData = fxBarrierOption->option();

        AdjustedNotional adjNotional = getFxAdjustedNotional(fxAmounts);

        contrib = Contribution(underlyingData, adjNotional.currency, adjNotional.notional);
        contrib.strike = fxBarrierOption->strike();
    } else if (auto fxEBarrierOption = dynamic_pointer_cast<ore::data::FxEuropeanBarrierOption>(tradePtr)) {
        auto fxAmounts = getSingleFxAmounts(fxEBarrierOption);
        auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
        optionData = fxEBarrierOption->option();

        AdjustedNotional adjNotional = getFxAdjustedNotional(fxAmounts);

        contrib = Contribution(underlyingData, adjNotional.currency, adjNotional.notional);
        contrib.strike = fxEBarrierOption->strike();
    } else if (auto fxKikoBarrierOption = dynamic_pointer_cast<ore::data::FxKIKOBarrierOption>(tradePtr)) {
        auto fxAmounts = getSingleFxAmounts(fxKikoBarrierOption);
        auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
        optionData = fxKikoBarrierOption->option();

        AdjustedNotional adjNotional = getFxAdjustedNotional(fxAmounts);

        contrib = Contribution(underlyingData, adjNotional.currency, adjNotional.notional);
        contrib.strike = fxKikoBarrierOption->strike();
    } else if (auto fxDoubleBarrierOpt = dynamic_pointer_cast<ore::data::FxDoubleBarrierOption>(tradePtr)) {
        auto fxAmounts = getSingleFxAmounts(fxDoubleBarrierOpt);
        auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
        optionData = fxDoubleBarrierOpt->option();

        AdjustedNotional adjNotional = getFxAdjustedNotional(fxAmounts);

        contrib = Contribution(underlyingData, adjNotional.currency, adjNotional.notional);
        contrib.strike = fxDoubleBarrierOpt->strike();
        //} else if (auto equityOpPosition = dynamic_pointer_cast<ore::data::EquityOptionPosition>(tradePtr)) {
        //    QL_REQUIRE(equityOpPosition->options().size() == 1);
        //    equityOpPosition->
        //    contrib = Contribution(underlyingData, , );
        //    //strike = equityOpPosition->data().underlyings().front().strike();
    } else if (auto eqDigitalOption = dynamic_pointer_cast<ore::data::EquityDigitalOption>(tradePtr)) {
        auto underlyingData = getUnderlyingData(eqDigitalOption->equityName(), OREAssetClass::EQ);
        optionData = eqDigitalOption->option();

        Real notional = eqDigitalOption->notional();
        contrib = Contribution(underlyingData, eqDigitalOption->notionalCurrency(), notional);
        contrib.currentPrice = notional / eqDigitalOption->quantity();
        contrib.strike = eqDigitalOption->strike();
    } else if (auto commDigitalOption = dynamic_pointer_cast<ore::data::CommodityDigitalOption>(tradePtr)) {
        auto underlyingData = getUnderlyingData(commDigitalOption->commodityName(), OREAssetClass::COM);
        optionData = commDigitalOption->option();

        Real notional = commDigitalOption->notional();
        contrib = Contribution(underlyingData, commDigitalOption->notionalCurrency(), notional);
        contrib.strike = commDigitalOption->strike();
    } else if (auto fxDigitalOption = dynamic_pointer_cast<ore::data::FxDigitalOption>(tradePtr)) {
        auto fxAmounts = getSingleFxAmounts(fxDigitalOption);
        auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
        optionData = fxDigitalOption->option();

        AdjustedNotional adjNotional = getFxAdjustedNotional(fxAmounts);

        contrib = Contribution(underlyingData, adjNotional.currency, adjNotional.notional);
        contrib.strike = fxDigitalOption->strike();
    } else if (auto fxDigitalBarrierOption = dynamic_pointer_cast<ore::data::FxDigitalBarrierOption>(tradePtr)) {
        auto fxAmounts = getSingleFxAmounts(fxDigitalBarrierOption);
        auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
        optionData = fxDigitalBarrierOption->option();

        AdjustedNotional adjNotional = getFxAdjustedNotional(fxAmounts);

        contrib = Contribution(underlyingData, adjNotional.currency, adjNotional.notional);
        contrib.strike = fxDigitalBarrierOption->strike();
    } else if (auto swaption = dynamic_pointer_cast<ore::data::Swaption>(tradePtr)) {
        UnderlyingData underlyingData;
        optionData = swaption->optionData();

        const auto& legData = swaption->legData();
        Real strike = Null<Real>();
        for (const auto& ld : legData) {
            if (ld.legType() == "Floating") {
                auto floatingLeg = dynamic_pointer_cast<ore::data::FloatingLegData>(ld.concreteLegData());
                underlyingData = getUnderlyingData(floatingLeg->index(), OREAssetClass::IR);
            } else if (ld.legType() == "Fixed") {
                auto fixedLeg = dynamic_pointer_cast<ore::data::FixedLegData>(ld.concreteLegData());
                auto rates = fixedLeg->rates();
                QL_REQUIRE(rates.size() == 1,
                           name() << "::calculateSingleOptionContribution() Only 1 fixed leg strike/rate supported");
                strike = rates.front();
                callPut = ld.isPayer() ? -1.0 : 1.0;
            }
        }
        contrib = Contribution(underlyingData, swaption->notionalCurrency(), swaption->notional());
        contrib.strike = strike;
    } else if (auto vanillaOpt = dynamic_pointer_cast<ore::data::VanillaOptionTrade>(tradePtr)) {
        UnderlyingData underlyingData;
        optionData = vanillaOpt->option();

        string currency = vanillaOpt->notionalCurrency();
        Real notional = Null<Real>();
        auto eqOption = dynamic_pointer_cast<ore::data::EquityOption>(vanillaOpt);
        auto eqFutOption = dynamic_pointer_cast<ore::data::EquityFutureOption>(vanillaOpt);
        auto commOption = dynamic_pointer_cast<ore::data::CommodityOption>(vanillaOpt);
        if (eqOption || eqFutOption || commOption) {
            auto assetClass = commOption ? OREAssetClass::COM : OREAssetClass::EQ;
            underlyingData = getUnderlyingData(vanillaOpt->asset(), assetClass);
            notional = vanillaOpt->quantity() * price;
        } else if (auto fxOption = dynamic_pointer_cast<ore::data::FxOption>(vanillaOpt)) {
            auto fxAmounts = getSingleFxAmounts();
            underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
            auto adjNotional = getFxAdjustedNotional(fxAmounts);
            notional = adjNotional.notional;
            currency = adjNotional.currency;
        } else {
            QL_FAIL(name() << "::calculateSingleOptionContribution() Unsupported vanilla option trade type: "
                           << trade_->tradeType());
        }
        contrib = Contribution(underlyingData, currency, notional);
        if (auto ar = vanillaOpt->instrument()->additionalResults(); ar.find("strike") != ar.end())
            contrib.strike = any_cast<Real>(ar.at("strike"));
        else
            contrib.strike = vanillaOpt->strike().value();
    } else {
        QL_FAIL(name() << "::calculateSingleOptionContribution() Unsupported trade type " << tradePtr->tradeType());
    }

    contrib.lastExerciseDate = getLastExerciseDate(optionData);
    contrib.optionDeltaPrice = price;
    contrib.currentPrice = price;
    contrib.isOption = true;
    // TODO: longShort and callPut might not necessarily be right here, depending on the trade
    Real volatility = getSupervisoryOptionVolatility(contrib.underlyingData);
    if (callPut == Null<Real>())
        callPut = parseOptionType(optionData.callPut()) == Option::Call ? 1.0 : -1.0;

    Real boughtSold = parsePositionType(optionData.longShort()) == Position::Long ? 1.0 : -1.0;

    // TODO: Barrier options should be scaled down based on Alive probability
    Real delta = boughtSold * callPut *
                 phi(contrib.optionDeltaPrice, contrib.strike, volatility, contrib.lastExerciseDate, callPut);
    contrib.delta = delta;

    vector<Contribution> contributions({contrib});
    return contributions;
}

Real SaccrTradeData::Impl::getLastExerciseDate(const OptionData& optionData) const {
    // Exercise date
    Date latestExerciseDate = Date::minDate();
    for (const string& d : optionData.exerciseDates()) {
        latestExerciseDate = std::max(latestExerciseDate, parseDate(d));
    }
    const QuantLib::Date& today = QuantLib::Settings::instance().evaluationDate();
    Real T = dc().yearFraction(today, latestExerciseDate);
    T = std::max(0.0, T);

    return T;
}

tuple<Real, string, optional<Real>> SaccrTradeData::Impl::getLegAverageNotional(Size legIdx,
                                                                                const string& legType) const {
    Real avgNotional = 0.0;
    Real countTimes = 0.0;
    const Leg& leg = trade_->legs()[legIdx];
    bool useCurrentPrice = legType.find("Equity") != string::npos || legType.find("Commodity") != string::npos;
    Real avgWeightedQuantity = 0.0;
    const QuantLib::Date& today = QuantLib::Settings::instance().evaluationDate();
    optional<Real> currentPrice;

    for (auto l : leg) {
        if (l->hasOccurred(today))
            continue;

        Real yearFrac = 1.0;

        if (auto commCoupon = dynamic_pointer_cast<QuantExt::CommodityIndexedCashFlow>(l)) {
            Real gearing = commCoupon->gearing();
            Real quantity = commCoupon->periodQuantity();
            Real spread = commCoupon->spread();

            if (!currentPrice)
                currentPrice = commCoupon->fixing();
            Real effPrice = *currentPrice + spread;
            Real periodNotional = gearing * quantity * effPrice * yearFrac;

            avgNotional += periodNotional;
            avgWeightedQuantity += periodNotional * quantity;

        } else if (auto commAvgCoupon = dynamic_pointer_cast<QuantExt::CommodityIndexedAverageCashFlow>(l)) {
            Real gearing = commAvgCoupon->gearing();
            Real quantity = commAvgCoupon->periodQuantity();
            Real spread = commAvgCoupon->spread();

            if (!currentPrice)
                currentPrice = commAvgCoupon->fixing();
            Real effPrice = *currentPrice + spread;

            const Date& startDate = commAvgCoupon->startDate();
            const Date& endDate = commAvgCoupon->endDate();
            yearFrac = dc().yearFraction(std::max(startDate, today), endDate);
            Real periodNotional = gearing * quantity * effPrice * yearFrac;

            avgNotional += periodNotional;
            avgWeightedQuantity += periodNotional * quantity;
        } else if (auto eqCoupon = dynamic_pointer_cast<QuantExt::EquityCoupon>(l)) {
            Real notional = eqCoupon->nominal();
            if (!currentPrice)
                currentPrice = eqCoupon->initialPrice();

            Real quantity = eqCoupon->quantity();
            if (quantity == Null<Real>())
                quantity = notional / *currentPrice;

            const Date& startDate = eqCoupon->accrualStartDate();
            const Date& endDate = eqCoupon->accrualEndDate();
            yearFrac = dc().yearFraction(std::max(startDate, today), endDate);

            Real periodNotional = notional * yearFrac;
            avgNotional += periodNotional;
            avgWeightedQuantity += periodNotional * quantity;
        } else if (auto coupon = dynamic_pointer_cast<QuantLib::Coupon>(l)) {
            const Date& startDate = coupon->accrualStartDate();
            const Date& endDate = coupon->accrualEndDate();
            yearFrac = dc().yearFraction(std::max(startDate, today), endDate);
            Real notional = coupon->nominal();

            avgNotional += notional * yearFrac;
        } else if (auto fxLinkedCashflow = dynamic_pointer_cast<QuantExt::FXLinkedCashFlow>(l)) {
            continue;
        } else if (auto cashflow = dynamic_pointer_cast<QuantExt::SimpleCashFlow>(l)) {
            continue;
        } else {
            QL_FAIL("unsupported coupon type");
        }

        countTimes += yearFrac;
    }

    if (countTimes > 0.0) {
        avgNotional /= countTimes;
        avgWeightedQuantity /= avgNotional * countTimes;
    }

    optional<Real> avgCurrentPrice;
    if (useCurrentPrice)
        avgCurrentPrice = avgNotional / avgWeightedQuantity;

    return make_tuple(avgNotional, trade_->legCurrencies()[legIdx], avgCurrentPrice);
}

string SaccrTradeData::Impl::getBucket(const Contribution& contribution) const {
    string bucket;

    auto assetClass = contribution.underlyingData.saccrAssetClass;
    auto endDate = contribution.maturity;

    if (assetClass == AssetClass::IR) {
        if (endDate < 1.0)
            bucket = "1";
        else if (endDate <= 5.0)
            bucket = "2";
        else
            bucket = "3";
    } else if (assetClass == AssetClass::FX) {
        // Do nothing
    } else if (assetClass == AssetClass::Equity) {
        if (contribution.underlyingData.isIndex)
            bucket = "Index";
        else
            bucket = "";

    } else if (assetClass == AssetClass::Credit) {
        // For single name: SubAsset Class for Credit Single Name, e.g., AAA, AA...IG
        // For index: Concatenation of “Index -“ and SubAsset Class, e.g., Index - IG, Index - SG
        // TODO
        /// SNRFOR -> IG, AAA-A
        // PREFT1 -> IG, A, BBB
        // SECDOM -> SG, BB, B
        // SUBL2 -> SG, B-CCC
        if (contribution.underlyingData.isIndex) {
            bucket = "Index-IG"; // "Index-SG"
        } else {
            bucket = "IG";
        }
    } else if (assetClass == AssetClass::Commodity) {
        // TODO: SubAsset Class for Commodity - same as hedgingSet in many cases, but not always, e.g. HS=Energy,
        // B=OIL/GAS We should probably also change the bucket mapping to make it configurable like the SIMM bucket
        // mapper
        bucket = contribution.hedgingData.hedgingSubset.get_value_or("");
    }

    return bucket;
}

Real SaccrTradeData::Impl::getMaturityFactor(Real maturity) const {
    Real maturityFactor = Null<Real>();
    shared_ptr<ore::data::NettingSetDefinition> ndef = tradeData_.lock()->nettingSetManager()->get(nettingSetDetails());
    if (ndef->activeCsaFlag()) {
        shared_ptr<CounterpartyInformation> cp = tradeData_.lock()->counterpartyManager()->get(counterparty());
        QL_REQUIRE(ndef->csaDetails()->marginPeriodOfRisk().units() == QuantLib::Weeks, "MPOR is expected in weeks");
        Size MPORinWeeks = (Size)weeks(ndef->csaDetails()->marginPeriodOfRisk());
        Size tradeCount = tradeData_.lock()->tradeCount(nettingSetDetails());
        if (tradeCount > 5000 && !cp->isClearingCP())
            MPORinWeeks = 4.0;
        maturityFactor = 1.5 * sqrt(MPORinWeeks / 52.0);
    } else {
        const Real m = std::max(maturity, 2.0 / 52.0);
        maturityFactor = std::sqrt(std::min(m, 1.0));
    }
    return maturityFactor;
}

set<string> CommodityForwardSaccrImpl::getTradeTypes() const { return {"CommodityForward"}; }

vector<Contribution> CommodityForwardSaccrImpl::calculateImplContributions() const {
    // For commodity derivatives, the adjusted notional is defined as the product of the
    // current price of one unit of the stock or commodity (eg a share of equity or barrel of oil) and
    // the number of units referenced by the trade.

    auto commFwd = dynamic_pointer_cast<ore::data::CommodityForward>(trade_);

    Real currentNotional = commFwd->currentNotional();
    string notionalCurrency = commFwd->notionalCurrency();
    Real currentPrice = currentNotional / commFwd->quantity();

    Real delta = parsePositionType(commFwd->position()) == Position::Long ? 1 : -1;
    string underlyingName = commFwd->commodityName();
    UnderlyingData underlyingData = getUnderlyingData(underlyingName, OREAssetClass::COM);

    Contribution contrib(underlyingData, notionalCurrency, currentNotional, delta);
    contrib.currentPrice = currentPrice;

    return vector<Contribution>({contrib});
}

set<string> CommodityDigitalOptionSaccrImpl::getTradeTypes() const { return {"CommodityDigitalOption"}; }

vector<Contribution> CommodityDigitalOptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

set<string> CommoditySpreadOptionSaccrImpl::getTradeTypes() const { return {"CommoditySpreadOption"}; }

vector<Contribution> CommoditySpreadOptionSaccrImpl::calculateImplContributions() const {
    auto commSpreadOption = QuantLib::ext::dynamic_pointer_cast<ore::data::CommoditySpreadOption>(trade_);

    vector<Contribution> contributions;
    for (Size i = 0; i < commSpreadOption->legs().size(); i++) {
        string legType = commSpreadOption->csoData().legData()[i].legType();

        // Get CommodityFloating leg underlying name
        const auto& legData = commSpreadOption->csoData().legData().at(i).concreteLegData();
        auto commLeg = QuantLib::ext::dynamic_pointer_cast<ore::data::CommodityFloatingLegData>(legData);
        string legUnderlyingName = commLeg->name();
        auto underlyingData = getUnderlyingData(legUnderlyingName, OREAssetClass::COM);

        Real delta = commSpreadOption->legPayers().at(i) ? -1.0 : 1.0;

        const auto& [legNotional, legCurrency, legCurrentPrice] = getLegAverageNotional(i, legType);

        Contribution contrib(underlyingData, legCurrency, legNotional, delta);
        contrib.currentPrice = legCurrentPrice;
        contributions.push_back(contrib);
    }

    return contributions;
}

set<string> CommoditySwaptionSaccrImpl::getTradeTypes() const { return {"CommoditySwaption"}; }

vector<Contribution> CommoditySwaptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

set<string> CommodityPositionSaccrImpl::getTradeTypes() const { return {"CommodityPosition"}; }

vector<Contribution> CommodityPositionSaccrImpl::calculateImplContributions() const {
    auto commPosition = dynamic_pointer_cast<ore::data::CommodityPosition>(trade_);

    Real npv = trade_->instrument()->NPV();
    Real delta = npv > 0.0 ? 1.0 : -1.0;

    vector<Contribution> contributions;
    for (const auto& und : commPosition->data().underlyings()) {
        auto underlyingData = getUnderlyingData(und.name(), OREAssetClass::COM);
        Contribution contrib(underlyingData, trade_->npvCurrency(), 0.0, delta, 0.0, 0.0);
        contributions.push_back(contrib);
    }

    return contributions;
}

set<string> EquityForwardSaccrImpl::getTradeTypes() const { return {"EquityForward"}; }

vector<Contribution> EquityForwardSaccrImpl::calculateImplContributions() const {
    auto eqForward = dynamic_pointer_cast<ore::data::EquityForward>(trade_);

    auto underlyingData = getUnderlyingData(eqForward->eqName(), OREAssetClass::EQ);
    Real fwdPrice = getOptionPrice(eqForward);
    Real notional = eqForward->quantity() * fwdPrice;
    Real delta = parsePositionType(eqForward->longShort()) == Position::Long ? 1.0 : -1.0;

    Contribution contrib(underlyingData, eqForward->currency(), notional, delta);
    contrib.currentPrice = fwdPrice;
    return vector<Contribution>({contrib});
}

set<string> EquityPositionSaccrImpl::getTradeTypes() const { return {"EquityPosition"}; }

vector<Contribution> EquityPositionSaccrImpl::calculateImplContributions() const {
    auto eqPosition = dynamic_pointer_cast<ore::data::EquityPosition>(trade_);

    Real npv = trade_->instrument()->NPV();
    Real delta = npv > 0.0 ? 1.0 : -1.0;

    vector<Contribution> contributions;
    for (const auto& und : eqPosition->data().underlyings()) {
        auto underlyingData = getUnderlyingData(und.name(), OREAssetClass::EQ);
        Contribution contrib(underlyingData, eqPosition->notionalCurrency(), 0.0, delta, 0.0, 0.0);
        contributions.push_back(contrib);
    }

    return contributions;
}

set<string> EquityOptionPositionSaccrImpl::getTradeTypes() const { return {"EquityOptionPosition"}; }

vector<Contribution> EquityOptionPositionSaccrImpl::calculateImplContributions() const {
    auto eqOpPosition = dynamic_pointer_cast<ore::data::EquityOptionPosition>(trade_);

    Real npv = trade_->instrument()->NPV();
    Real delta = npv > 0.0 ? 1.0 : -1.0;

    vector<Contribution> contributions;
    for (const auto& und : eqOpPosition->data().underlyings()) {
        auto underlyingData = getUnderlyingData(und.underlying().name(), OREAssetClass::EQ);
        Contribution contrib(underlyingData, eqOpPosition->notionalCurrency(), 0.0, delta, 0.0, 0.0);
        contributions.push_back(contrib);
    }

    return contributions;
}

set<string> EquityDigitalOptionSaccrImpl::getTradeTypes() const { return {"EquityDigitalOption"}; }

vector<Contribution> EquityDigitalOptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

// bool FxSaccrImpl::isFlipped() const {
//     const auto& [origBoughtCcy, origSoldCcy] = getSingleFxAmounts();
//     return getFirstRiskFactor() != origBoughtCcy;
// }

set<string> EquityTouchOptionSaccrImpl::getTradeTypes() const { return {"EquityTouchOption"}; }

vector<Contribution> EquityTouchOptionSaccrImpl::calculateImplContributions() const {
    auto eqTouchOption = dynamic_pointer_cast<ore::data::EquityTouchOption>(trade_);

    string currency = eqTouchOption->payoffCurrency();
    // TODO: Should scale down notional based on Alive probability
    Real notional = eqTouchOption->payoffAmount();
    Real delta = parsePositionType(eqTouchOption->option().longShort()) == Position::Long ? 1.0 : -1.0;
    auto underlyingData = getUnderlyingData(eqTouchOption->equityName(), OREAssetClass::EQ);
    Contribution contrib(underlyingData, currency, notional, delta);
    Real fwdPrice = getOptionPrice(eqTouchOption);
    contrib.currentPrice = fwdPrice;

    return vector<Contribution>({contrib});
}

set<string> EquityDoubleTouchOptionSaccrImpl::getTradeTypes() const { return {"EquityDoubleTouchOption"}; }

vector<Contribution> EquityDoubleTouchOptionSaccrImpl::calculateImplContributions() const {
    auto eqDTouchOption = dynamic_pointer_cast<ore::data::EquityDoubleTouchOption>(trade_);

    string currency = eqDTouchOption->payoffCurrency();
    // TODO: Should scale down notional based on Alive probability
    Real notional = eqDTouchOption->payoffAmount();
    Real delta = parsePositionType(eqDTouchOption->option().longShort()) == Position::Long ? 1.0 : -1.0;
    auto underlyingData = getUnderlyingData(eqDTouchOption->equityName(), OREAssetClass::EQ);
    Contribution contrib(underlyingData, currency, notional, delta);
    Real fwdPrice = getOptionPrice(eqDTouchOption);
    contrib.currentPrice = fwdPrice;

    return vector<Contribution>({contrib});
}

set<string> EquityBarrierOptionSaccrImpl::getTradeTypes() const { return {"EquityBarrierOption"}; }

vector<Contribution> EquityBarrierOptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

set<string> EquityDoubleBarrierOptionSaccrImpl::getTradeTypes() const { return {"EquityDoubleBarrierOption"}; }

vector<Contribution> EquityDoubleBarrierOptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

set<string> FxSaccrImpl::getTradeTypes() const {
    // clang-format off
    return {"FxOption",
            "FxBarrierOption",
            "FxEuropeanBarrierOption",
            "FxKIKOBarrierOption",
            "FxDoubleBarrierOption",
            "FxDigitalOption",
            "FxDigitalBarrierOption",

            "FxTouchOption",
            "FxDoubleTouchOption",
            "FxForward",
            "FxSwap"};
    // clang-format on
}

vector<Contribution> FxSaccrImpl::calculateImplContributions() const {

    // Option type trades
    set<string> singleOptionTradeTypes({"FxBarrierOption", "FxEuropeanBarrierOption", "FxKIKOBarrierOption",
                                        "FxDoubleBarrierOption", "FxDigitalOption", "FxDigitalBarrierOption",
                                        "FxOption"});
    if (singleOptionTradeTypes.find(trade_->tradeType()) != singleOptionTradeTypes.end()) {
        return calculateSingleOptionContribution();
    }

    // Non-option trades
    auto fxAmounts = getSingleFxAmounts();
    auto underlyingData = getUnderlyingData(fxAmounts.boughtCurrency, fxAmounts.soldCurrency);
    Real delta = fxAmounts.boughtCurrency == underlyingData.qualifier.substr(0, 3) ? 1.0 : -1.0;
    auto fxAdjNotional = getFxAdjustedNotional(fxAmounts);
    Contribution contrib(underlyingData, fxAdjNotional.currency, fxAdjNotional.notional, delta);

    vector<Contribution> contributions({contrib});
    return contributions;
}

set<string> CashPositionSaccrImpl::getTradeTypes() const { return {"CashPosition"}; }

vector<Contribution> CashPositionSaccrImpl::calculateImplContributions() const {
    auto cashPosition = dynamic_pointer_cast<ore::data::CashPosition>(trade_);
    Real npv = trade_->instrument()->NPV();
    Real delta = npv > 0.0 ? 1.0 : -1.0;

    vector<Contribution> contributions;
    auto underlyingData =
        getUnderlyingData(parseCurrencyWithMinors(cashPosition->currency()).code(), OREAssetClass::IR);
    Contribution contrib(underlyingData, trade_->npvCurrency(), 0.0, delta);
    contributions.push_back(contrib);

    return contributions;
}

set<string> FRASaccrImpl::getTradeTypes() const { return {"ForwardRateAgreement"}; }

vector<Contribution> FRASaccrImpl::calculateImplContributions() const {
    auto fra = dynamic_pointer_cast<ore::data::ForwardRateAgreement>(trade_);

    Real delta = parsePositionType(fra->longShort()) == Position::Long ? 1.0 : -1.0;
    Contribution contrib(getUnderlyingData(fra->index(), OREAssetClass::IR), fra->notionalCurrency(), fra->notional(),
                         delta);

    vector<Contribution> contributions({contrib});
    return contributions;
}

set<string> CapFloorSaccrImpl::getTradeTypes() const { return {"CapFloor"}; }

vector<Contribution> CapFloorSaccrImpl::calculateImplContributions() const {
    auto capFloor = dynamic_pointer_cast<ore::data::CapFloor>(trade_);
    string legType = capFloor->leg().legType();
    QL_REQUIRE(legType == "Floating", name() << "::calculateImplContribution() Only Floating legs supported for now.");

    const auto& [notional, notionalCcy, currentPrice] = getLegAverageNotional(0, legType);

    auto floatingLeg = dynamic_pointer_cast<ore::data::FloatingLegData>(capFloor->leg().concreteLegData());
    auto underlyingData = getUnderlyingData(floatingLeg->index(), OREAssetClass::IR);

    Real delta = trade_->instrument()->multiplier();
    Contribution contrib(underlyingData, notionalCcy, notional, delta);

    vector<Contribution> contributions({contrib});
    return contributions;
}

set<string> BondTRSSaccrImpl::getTradeTypes() const { return {"BondTRS"}; }

vector<Contribution> BondTRSSaccrImpl::calculateImplContributions() const {
    auto bondTRS = dynamic_pointer_cast<ore::data::BondTRS>(trade_);
    auto underlyingData = getUnderlyingData(bondTRS->bondData().securityId(), OREAssetClass::CR);
    Real delta = bondTRS->payTotalReturnLeg() ? -1 : 1;
    Contribution contrib(underlyingData, bondTRS->notionalCurrency(), bondTRS->notional(), delta);
    const auto& today = QuantLib::Settings::instance().evaluationDate();
    contrib.startDate = 0.0;
    Date maturityDate = Date::minDate();
    string bondMaturityDate = bondTRS->bondData().maturityDate();
    if (!bondMaturityDate.empty()) {
        maturityDate = parseDate(bondMaturityDate);
        contrib.endDate = std::max(0.0, dc().yearFraction(today, maturityDate));
    } else {
        contrib.endDate = getMaturity();
    }
    return vector<Contribution>({contrib});
}

set<string> BondRepoSaccrImpl::getTradeTypes() const { return {"BondRepo"}; }

vector<Contribution> BondRepoSaccrImpl::calculateImplContributions() const {
    auto bondRepo = dynamic_pointer_cast<ore::data::BondRepo>(trade_);
    auto underlyingData = getUnderlyingData(bondRepo->bondData().securityId(), OREAssetClass::CR);
    Real delta = bondRepo->legPayers()[0] ? -1 : 1;
    Contribution contrib(underlyingData, bondRepo->notionalCurrency(), bondRepo->notional(), delta, false, false);
    const auto& today = QuantLib::Settings::instance().evaluationDate();
    contrib.startDate = 0.0;
    Date maturityDate = Date::minDate();
    string bondMaturityDate = bondRepo->bondData().maturityDate();
    if (!bondMaturityDate.empty()) {
        maturityDate = parseDate(bondMaturityDate);
        contrib.endDate = std::max(0.0, dc().yearFraction(today, maturityDate));
    } else {
        contrib.endDate = getMaturity();
    }
    return vector<Contribution>({contrib});
}

set<string> ScriptedTradeSaccrImpl::getTradeTypes() const {
    return {"ScriptedTrade", "FxAsianOption", "CommodityAsianOption", "EquityAsianOption"};
}

vector<Contribution> ScriptedTradeSaccrImpl::calculateImplContributions() const {
    auto scriptedTrade = dynamic_pointer_cast<ore::data::ScriptedTrade>(trade_);
    auto stInstr = scriptedTrade->instrument()->qlInstrument(true);
    const auto& ar = stInstr->additionalResults();

    auto getStringValues = [this, &ar](const string& key) {
        vector<string> values;
        auto it = ar.find(key);
        if (it != ar.end()) {
            if (it->second.type() == typeid(vector<string>)) {
                auto arVector = any_cast<vector<string>>(it->second);
                values.insert(values.end(), arVector.begin(), arVector.end());
            } else if (it->second.type() == typeid(string)) {
                values.push_back(any_cast<string>(it->second));
            } else {
                QL_FAIL(name() << "::calculateImplContributions() Could not get additional result '" << key
                               << "'. Expected string or vector<string>.");
            }
        } else {
            QL_FAIL(name() << "::calculateImplContributions() Additional result '" << key << "' does not exist");
        }
        return values;
    };

    auto getRealValues = [this, &ar](const string& key) {
        vector<Real> values;
        auto it = ar.find(key);
        if (it != ar.end()) {
            if (it->second.type() == typeid(vector<Real>)) {
                auto arVector = any_cast<vector<Real>>(it->second);
                values.insert(values.end(), arVector.begin(), arVector.end());
            } else if (it->second.type() == typeid(Real)) {
                values.push_back(any_cast<Real>(it->second));
            } else {
                QL_FAIL(name() << "::calculateImplContributions() Could not get additional result '" << key
                               << "'. Expected Real or vector<Real>.");
            }
        } else {
            QL_FAIL(name() << "::calculateImplContributions() Additional result '" << key << "' does not exist");
        }
        return values;
    };

    auto getDateValues = [this, &ar](const string& key) {
        vector<Date> values;
        auto it = ar.find(key);
        if (it != ar.end()) {
            if (it->second.type() == typeid(vector<Date>)) {
                auto arVector = any_cast<vector<Date>>(it->second);
                values.insert(values.end(), arVector.begin(), arVector.end());
            } else if (it->second.type() == typeid(Date)) {
                values.push_back(any_cast<Date>(it->second));
            } else {
                QL_FAIL(name() << "::calculateImplContributions() Could not get additional result '" << key
                               << "'. Expected Date or vector<Date>.");
            }
        } else {
            QL_FAIL(name() << "::calculateImplContributions() Additional result '" << key << "' does not exist");
        }
        return values;
    };

    auto getFlag = [this, &ar](const string& key, bool defaultValue = false) {
        auto it = ar.find(key);
        if (it != ar.end()) {
            if (it->second.type() == typeid(Real)) {
                auto flag = any_cast<Real>(it->second);
                QL_REQUIRE(flag == 0 || flag == 1,
                           name() << "::calculateImplContributions() Could not get additional result '" << key
                                  << "'. Expected 0 or 1.");
                return flag == 1;
            } else {
                QL_FAIL(name() << "::calculateImplContributions() Could not get additional result '" << key
                               << "'. Expected a Real number.");
            }
        } else {
            return defaultValue;
        }
    };

    vector<Contribution> contributions;

    // Create base contribution objs for each underlying
    vector<string> underlyings = getStringValues("underlyingName");
    Size contribSize = underlyings.size();
    for (const string& und : underlyings)
        contributions.push_back(Contribution(getUnderlyingData(und), scriptedTrade->notionalCurrency()));

    vector<Real> notionals = getRealValues("saccrNotional");
    QL_REQUIRE(notionals.size() == 1 || notionals.size() == underlyings.size(),
               name() << "::calculateImplContributions() Size mismatch between underlyings and notionals");
    if (notionals.size() == 1)
        notionals.resize(contribSize, notionals[0]);
    for (Size i = 0; i < contribSize; i++)
        contributions[i].adjustedNotional = notionals[i];

    vector<Real> currentPrice = getRealValues("currentPrice");
    QL_REQUIRE(currentPrice.size() == 1 || currentPrice.size() == underlyings.size(),
               name() << "::calculateImplContributions() Size mismatch between underlyings and notionals");
    if (currentPrice.size() == 1)
        currentPrice.resize(contribSize, currentPrice[0]);
    for (Size i = 0; i < contribSize; i++) {
        const auto& saccrAssetClass = contributions[i].underlyingData.saccrAssetClass;
        if (saccrAssetClass == AssetClass::IR || saccrAssetClass == AssetClass::Credit)
            contributions[i].currentPrice = currentPrice[i];
    }

    vector<Real> longShort = getRealValues("longShort");
    QL_REQUIRE(longShort.size() == 1 || longShort.size() == underlyings.size(),
               name() << "::calculateImplContributions() Size mismatch between longShort and notionals");
    if (longShort.size() == 1)
        longShort.resize(contribSize, longShort[0]);

    // Delta
    vector<Real> deltas;
    bool isOption = getFlag("isOption", false);
    if (isOption) {
        vector<Real> optionStrikes = getRealValues("optionStrike");
        vector<Real> optionPrices = getRealValues("optionPrice");
        vector<Real> putCall = getRealValues("putCall");
        vector<Date> lastExerciseDates = getDateValues("lastExerciseDate");

        // Some special cases where there are multiple exercise dates but we only need the last date
        for (const string& sn : {"VarianceOption"}) {
            if (scriptedTrade->scriptName() == sn) {
                QL_REQUIRE(!lastExerciseDates.empty(),
                           name() << "::calculateImplContributions() " << sn << " exercise dates cannot be empty");
                lastExerciseDates.resize(contribSize, lastExerciseDates.back());
            }
        }

        QL_REQUIRE(optionStrikes.size() == 1 || optionStrikes.size() == underlyings.size(),
                   name() << "::calculateImplContributions() Size mismatch of option strikes");
        QL_REQUIRE(optionPrices.size() == 1 || optionPrices.size() == underlyings.size(),
                   name() << "::calculateImplContributions() Size mismatch of current prices");
        QL_REQUIRE(putCall.size() == 1 || putCall.size() == underlyings.size(),
                   name() << "::calculateImplContributions() Size mismatch of put call");
        QL_REQUIRE(lastExerciseDates.size() == 1 || lastExerciseDates.size() == underlyings.size(),
                   name() << "::calculateImplContributions() Size mismatch of last exercise dates");

        if (optionStrikes.size() == 1)
            optionStrikes.resize(contribSize, optionStrikes[0]);
        if (optionPrices.size() == 1)
            optionPrices.resize(contribSize, optionPrices[0]);
        if (putCall.size() == 1)
            putCall.resize(contribSize, putCall[0]);
        if (lastExerciseDates.size() == 1)
            lastExerciseDates.resize(contribSize, lastExerciseDates[0]);

        const auto& today = QuantLib::Settings::instance().evaluationDate();
        for (Size i = 0; i < contribSize; i++) {
            Real volatility = getSupervisoryOptionVolatility(contributions[i].underlyingData);
            contributions[i].isOption = true;
            contributions[i].lastExerciseDate = std::max(0.0, dc().yearFraction(today, lastExerciseDates[i]));
            contributions[i].optionDeltaPrice = optionPrices[i];
            contributions[i].strike = optionStrikes[i];

            Real delta = putCall[i] * longShort[i] *
                         phi(contributions[i].optionDeltaPrice, contributions[i].strike,
                             contributions[i].lastExerciseDate, volatility, putCall[i]);
            contributions[i].delta = delta;
        }
    } else {
        for (Size i = 0; i < contribSize; i++)
            contributions[i].delta = longShort[i];
    }

    for (auto& c : contributions) {
        auto assetClass = c.underlyingData.saccrAssetClass;
        if (assetClass == AssetClass::IR || assetClass == AssetClass::Credit) {
            c.startDate = 0.0;
            Period cmsPeriod = getCMSIndexPeriod(c.underlyingData.originalName);
            c.endDate = years(cmsPeriod);
        }
    }

    return contributions;
}

set<string> VanillaOptionSaccrImpl::getTradeTypes() const {
    return {"EquityOption", "EquityEuropeanBarrierOption", "EquityFutureOption", "CommodityOption"};
}

vector<Contribution> VanillaOptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

set<string> TotalReturnSwapSaccrImpl::getTradeTypes() const { return {"TotalReturnSwap"}; }

vector<Contribution> TotalReturnSwapSaccrImpl::calculateImplContributions() const {
    auto trs = dynamic_pointer_cast<ore::data::TRS>(trade_);

    string trsCcy = trs->returnData().currency();

    QL_REQUIRE(trs->underlying().size() == 1,
               name() << "::calculateImplContributions() Only single-underlying TRS is supported. Found "
                      << trs->underlying().size());
    const auto& underlying = trs->underlying().front();
    Real totalWeight = 0.0;
    vector<pair<Contribution, Real>> contributionWeights;
    if (auto eqPos = dynamic_pointer_cast<ore::data::EquityPosition>(underlying)) {
        for (const auto& und : eqPos->data().underlyings()) {
            auto underlyingData = getUnderlyingData(und.name(), OREAssetClass::EQ);
            Real delta = und.weight() > 0 ? 1.0 : -1.0;
            // Notional will be populated later
            Contribution contrib(underlyingData, trsCcy, Null<Real>(), delta);
            Real weight = und.weight();
            totalWeight += weight;
            contributionWeights.push_back({contrib, weight});
        }
    } else if (auto eqOpPos = dynamic_pointer_cast<ore::data::EquityOptionPosition>(underlying)) {
        set<string> eqUnderlyings;
        auto optUnderlyings = eqOpPos->data().underlyings();
        for (const auto& und : optUnderlyings)
            eqUnderlyings.insert(und.underlying().name());
        // For multiple option positions, we want to get the forward price for each equity option position.
        QL_REQUIRE(eqUnderlyings.size() == 1, name() << "::calculateImplContributions() Only 1 underlying currently "
                                                        "supported for TRS EquityOptionPosition. Found "
                                                     << eqUnderlyings.size());

        for (const auto& und : optUnderlyings) {
            auto underlyingData = getUnderlyingData(und.underlying().name(), OREAssetClass::EQ);
            // Notional will be populated later
            Contribution contrib(underlyingData, trsCcy, Null<Real>());
            contrib.isOption = true;
            contrib.lastExerciseDate = getLastExerciseDate(und.optionData());

            Real price = getOptionPrice(underlying);
            contrib.currentPrice = price;
            contrib.optionDeltaPrice = price;

            Real callPut = parseOptionType(und.optionData().callPut()) == Option::Call ? 1.0 : -1.0;
            Real optLongShort = parsePositionType(und.optionData().longShort()) == Position::Long ? 1.0 : -1.0;
            Real volatility = getSupervisoryOptionVolatility(underlyingData);
            contrib.strike = und.strike();
            Real delta = callPut * optLongShort *
                         phi(contrib.optionDeltaPrice, contrib.strike, contrib.lastExerciseDate, volatility, callPut);
            contrib.delta = delta;

            Real weight = und.underlying().weight();
            totalWeight += weight;

            contributionWeights.push_back({contrib, weight});
        }
    } else {
        QL_FAIL(name() << "::calculateImplContributions() Underlying trade type " << underlying->tradeType()
                       << " not yet supported.");
    }

    // FIXME: Divide total trade notional across the underlyings based on quantity - not completely accurate
    Real currentNotional = trs->notional();
    Real trsLongShort = trs->returnData().payer() ? -1.0 : 1.0;
    vector<Contribution> contributions;
    for (auto& [contrib, weight] : contributionWeights) {
        contrib.adjustedNotional = currentNotional * (weight / totalWeight);
        contrib.delta *= trsLongShort;
        contributions.push_back(contrib);
    }

    return contributions;
}

set<string> SwapSaccrImpl::getTradeTypes() const {
    return {"Swap", "EquitySwap", "CrossCurrencySwap", "CommoditySwap", "InflationSwap"};
}

vector<Contribution> SwapSaccrImpl::calculateImplContributions() const {

    vector<LegData> legs;
    if (auto swap = dynamic_pointer_cast<ore::data::Swap>(trade_))
        legs = swap->legData();
    else if (auto commSwap = dynamic_pointer_cast<ore::data::CommoditySwap>(trade_))
        legs = commSwap->legData();
    else
        QL_FAIL(name() << "::calculateImplContributions() Could not cast underlying trade");

    // Each non-fixed leg should map to a Contribution
    vector<Contribution> contributions;
    for (Size i = 0; i < legs.size(); i++) {
        const auto& legData = legs[i];

        if (isFixedLeg(legData))
            continue;

        // Get underlying name
        string legUnderlyingName;
        OREAssetClass legAssetClass = OREAssetClass::PORTFOLIO_DETAILS;
        if (auto equityLeg = dynamic_pointer_cast<ore::data::EquityLegData>(legData.concreteLegData())) {
            legUnderlyingName = equityLeg->eqName();
            legAssetClass = OREAssetClass::EQ;
        } else if (auto commFloatingLeg =
                       dynamic_pointer_cast<ore::data::CommodityFloatingLegData>(legData.concreteLegData())) {
            legUnderlyingName = commFloatingLeg->name();
            legAssetClass = OREAssetClass::COM;
        } else if (auto yoyLeg = dynamic_pointer_cast<ore::data::YoYLegData>(legData.concreteLegData())) {
            legUnderlyingName = yoyLeg->index();
            legAssetClass = OREAssetClass::INF;
        } else if (auto cpiLeg = dynamic_pointer_cast<ore::data::CPILegData>(legData.concreteLegData())) {
            legUnderlyingName = cpiLeg->index();
            legAssetClass = OREAssetClass::INF;
        } else if (auto floatingLeg = dynamic_pointer_cast<ore::data::FloatingLegData>(legData.concreteLegData())) {
            legUnderlyingName = floatingLeg->index();
            legAssetClass = OREAssetClass::IR;
        } else {
            QL_FAIL(name() << "::calculateImplContributions() Could not cast concrete leg data for leg type "
                           << legData.legType());
        }

        UnderlyingData underlyingData = getUnderlyingData(legUnderlyingName, legAssetClass);

        Real legMultiplier = legData.isPayer() ? -1 : 1;
        const auto& [legCurrentNotional, legCcy, currentPrice] = getLegAverageNotional(i, legData.legType());
        Real legNotional = legCurrentNotional * legMultiplier;
        Real delta = legNotional > 0.0 ? 1.0 : -1.0;
        legNotional = abs(legNotional);

        Contribution contrib(underlyingData, legCcy, legNotional, delta);

        // Current price
        if (legAssetClass == OREAssetClass::EQ || legAssetClass == OREAssetClass::COM)
            contrib.currentPrice = currentPrice;

        contributions.push_back(contrib);
    }

    return contributions;
}

set<string> SwaptionSaccrImpl::getTradeTypes() const { return {"Swaption"}; }

vector<Contribution> SwaptionSaccrImpl::calculateImplContributions() const {
    return calculateSingleOptionContribution();
}

set<string> VarianceSwapSaccrImpl::getTradeTypes() const {
    return {"FxVarianceSwap", "CommodityVarianceSwap", "EquityVarianceSwap"};
}

vector<Contribution> VarianceSwapSaccrImpl::calculateImplContributions() const {
    auto varSwap = dynamic_pointer_cast<ore::data::VarSwap>(trade_);
    auto underlyingData = getUnderlyingData(varSwap->underlying()->name(), varSwap->assetClassUnderlying());

    Real currentPrice = 0.0;
    auto ar = trade_->instrument()->additionalResults();
    auto it = ar.find("accruedVariance");
    if (it != ar.end()) {
        if (it->second.type() == typeid(Real))
            currentPrice = any_cast<Real>(it->second);
        else if (it->second.type() == typeid(int))
            currentPrice = any_cast<int>(it->second);
    }
    Real adjustedNotional = varSwap->notional() * currentPrice;

    Real boughtSold = parsePositionType(varSwap->longShort()) == Position::Long ? 1.0 : -1.0;
    Contribution contrib(underlyingData, varSwap->currency(), adjustedNotional, boughtSold, false, true);

    return vector<Contribution>({contrib});
}
} // namespace analytics
} // namespace ore