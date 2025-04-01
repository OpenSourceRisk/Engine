/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orea/simm/simmtradedata.hpp>
#include <orea/simm/utilities.hpp>

#include <ored/portfolio/trs.hpp>
#include <ored/utilities/parsers.hpp>

//#include <orepcredit/ored/portfolio/assetbackedcreditdefaultswap.hpp>
//#include <orepproxy/ored/portfolio/imscheduletrade.hpp>
//#include <orepproxy/ored/portfolio/sensitrade.hpp>

#include <orea/simm/simmbucketmapper.hpp>
#include <ored/portfolio/ascot.hpp>
#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/bondrepo.hpp>
#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/cdo.hpp>
#include <ored/portfolio/commodityapo.hpp>
#include <ored/portfolio/commodityoptionstrip.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/commodityswaption.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/compositetrade.hpp>
#include <ored/portfolio/convertiblebond.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswapoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/equityswap.hpp>
#include <ored/portfolio/forwardbond.hpp>
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
#include <ored/portfolio/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

using ore::data::LegType;
using ore::data::BondTRS;
// using oreplus::data::SensiTrade;
using std::multimap;

namespace ore {
namespace analytics {

using namespace QuantLib;

namespace {

string getTradeCurrency(const QuantLib::ext::shared_ptr<ore::data::Trade>& t,
                        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refData) {

    // for composite trades we try to get the trade currency from the underlying trades

    if (t->tradeType() == "CompositeTrade") {
        std::set<string> underlyingCurrencies;
        auto c = QuantLib::ext::dynamic_pointer_cast<ore::data::CompositeTrade>(t);
        QL_REQUIRE(c, "internal error: could not cast CompositeTrade to trade class");
        for (auto const& u : c->trades()) {
            string ccy = getTradeCurrency(u, refData);
            if (!ccy.empty())
                underlyingCurrencies.insert(ccy);
        }
        if (underlyingCurrencies.empty()) {
            return "";
        } else if (underlyingCurrencies.size() == 1) {
            return *underlyingCurrencies.cbegin();
        } else {
            string ccys;
            for (auto const& c : underlyingCurrencies)
                ccys += c + ",";
            if (!ccys.empty())
                ccys.pop_back();
            ore::data::StructuredTradeErrorMessage(
                t->id(), t->tradeType(), "Ambiguous SIMM CreditQ trade currencies for composite trade",
                "Found currencies " + ccys + ", will use " + (*underlyingCurrencies.cbegin()))
                .log();
            return *underlyingCurrencies.cbegin();
        }
    }

    // handle non-composite trade types

    if (t->tradeType() == "CreditDefaultSwap" || t->tradeType() == "Bond" || t->tradeType() == "ConvertibleBond" ||
        t->tradeType() == "ForwardBond" || t->tradeType() == "IndexCreditDefaultSwap" ||
        t->tradeType() == "IndexCreditDefaultSwapOption" || t->tradeType() == "BondOption" ||
        t->tradeType() == "BondRepo" || t->tradeType() == "RiskParticipationAgreement" || t->tradeType() == "Ascot" ||
        t->tradeType() == "AssetBackedCreditDefaultSwap" || t->tradeType() == "CreditLinkedSwap" ||
        t->tradeType() == "CrossCurrencySwap" || t->tradeType() == "InflationSwap") {
        QL_REQUIRE(!t->legCurrencies().empty(),
                   "Expected legCurrencies to be populated for trade type " << t->tradeType());
        return t->legCurrencies()[0];
    } else if (t->tradeType() == "BondTRS") {
        auto bondTrs = QuantLib::ext::dynamic_pointer_cast<BondTRS>(t);
        QL_REQUIRE(bondTrs, "internal error: could not cast BondTRS to trade class");
        return bondTrs->bondData().currency();
    } else if (t->tradeType() == "TotalReturnSwap" || t->tradeType() == "ContractForDifference") {
        auto trsT = QuantLib::ext::dynamic_pointer_cast<ore::data::TRS>(t);
        QL_REQUIRE(trsT, "internal error: could not cast TRS to trade class");
        return trsT->creditRiskCurrency();
    } else if (t->tradeType() == "SyntheticCDO") {
        QL_REQUIRE(!t->legCurrencies().empty(),
                   "Expected legCurrencies to be populated for trade type " << t->tradeType());
        return t->legCurrencies()[0];
    } else if (t->tradeType() == "Swap") {
        auto tmp = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(t);
        QL_REQUIRE(tmp, "getTradeCurrency: internal error, could not cast to Swap");
        for (const auto& leg : tmp->legData()) {
            if (leg.legType() == LegType::CMB) {
                auto cmbData = QuantLib::ext::dynamic_pointer_cast<ore::data::CMBLegData>(leg.concreteLegData());
                QL_REQUIRE(cmbData, "getTradeCurrency: internal error, could to cast to CMBLegData.");
                return getCmbLegCreditRiskCurrency(*cmbData, refData);
            }
        }
        QL_REQUIRE(!t->legCurrencies().empty(),
                   "Expected legCurrencies to be populated for trade type " << t->tradeType());
        return t->legCurrencies()[0];
    } else {
        return "";
    }
}

} // namespace

void SimmTradeData::TradeAttributes::setExtendedAttributes(
    const QuantLib::ext::shared_ptr<ore::data::Trade> trade, const QuantLib::ext::shared_ptr<ore::data::Market>& market,
    const QuantLib::ext::shared_ptr<SimmBucketMapper>& bucketMapper, const bool emitStructuredError) {

    // mtm value and currency
    setPresentValue(QuantLib::Null<Real>());
    setPresentValueCurrency("");
    TRY_AND_CATCH(setPresentValue(trade->instrument()->NPV()), trade->id(), getTradeType(), "setPresentValue",
                  emitStructuredError);
    TRY_AND_CATCH(setPresentValueCurrency(trade->npvCurrency()), trade->id(), getTradeType(), "setPresentValueCurrency",
                  emitStructuredError);

    // trade date
    const map<string, string>& additionalFields = trade->envelope().additionalFields();
    auto itTradeDate = additionalFields.find("trade_date");
    setTradeDate(itTradeDate != additionalFields.end() ? itTradeDate->second : "");

    // end date
    setEndDate(ore::data::to_string(trade->maturity()));

    // notional and trade currency
    TRY_AND_CATCH(setNotional(trade->notional()), trade->id(), trade->tradeType(), "setNotional", emitStructuredError);
    TRY_AND_CATCH(setNotionalCurrency(trade->notionalCurrency()), trade->id(), trade->tradeType(),
                  "setNotionalCurrency", emitStructuredError);

    // notional2 and currency, underlying, settlement type, strike are taken from trade details

    QuantLib::Real notional = getNotional();
    string notionalCcy = getNotionalCurrency();
    if (notional != Null<Real>() && !notionalCcy.empty())
        TRY_AND_CATCH(
            setNotionalUSD(notionalCcy == "USD" ? notional : market->fxRate(notionalCcy + "USD")->value() * notional),
            trade->id(), trade->tradeType(), "setNotionalUSD", emitStructuredError);

}

SimmTradeData::SimmTradeData(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                             const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                             const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
                             const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& bucketMapper)
    : referenceData_(referenceData), bucketMapper_(bucketMapper) {

    hasNettingSetDetails_ = portfolio->hasNettingSetDetails();

    processPortfolio(portfolio, market);
}

void SimmTradeData::TradeAttributes::setTradeType(const string tradeType) { tradeType_ = tradeType; }

void SimmTradeData::TradeAttributes::setOriginalTradeType(const string tradeType) { originalTradeType_ = tradeType; }

void SimmTradeData::TradeAttributes::setTradeCurrency(const string& tradeCurrency) { tradeCurrency_ = tradeCurrency; }

void SimmTradeData::TradeAttributes::setSimmProductClass(const CrifRecord::ProductClass& pc) {
    simmProductClass_ = pc;
}

void SimmTradeData::TradeAttributes::setScheduleProductClass(const CrifRecord::ProductClass& pc) {
    scheduleProductClass_ = pc;
}

void SimmTradeData::TradeAttributes::setNotional(double d) { notional_ = d; }

void SimmTradeData::TradeAttributes::setNotionalCurrency(const string& s) { notionalCurrency_ = s; }

void SimmTradeData::TradeAttributes::setPresentValue(double d) { presentValue_ = d; }

void SimmTradeData::TradeAttributes::setPresentValueCurrency(const string& s) { presentValueCurrency_ = s; }

void SimmTradeData::TradeAttributes::setEndDate(const string& s) { endDate_ = s; }

void SimmTradeData::TradeAttributes::setPresentValueUSD(double d) { presentValueUSD_ = d; }

void SimmTradeData::TradeAttributes::setNotionalUSD(double d) { notionalUSD_ = d; }

void SimmTradeData::processPortfolio(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                                     const QuantLib::ext::shared_ptr<ore::data::Market>& market,
				     const QuantLib::ext::shared_ptr<Portfolio>& auxiliaryPortfolio) {
    for (auto [tradeId, t] : portfolio->trades()) {
        bool emitStructuredError = true;
        try {
            // add trade to container
            const map<string, string>& additionalFields = t->envelope().additionalFields();

	    auto tmp = QuantLib::ext::make_shared<TradeAttributes>();

	    auto simmPC = CrifRecord::ProductClass::Empty;
            auto schedulePC = CrifRecord::ProductClass::Empty;

	    TRY_AND_CATCH(simmPC = simmProductClassFromOreTrade(t), tradeId, t->tradeType(), "simmPc",
			  emitStructuredError);
	    simmTradeIds_.insert(tradeId);
            
	    TRY_AND_CATCH(schedulePC = scheduleProductClassFromOreTrade(t), tradeId, t->tradeType(), "schedulePC",
			  emitStructuredError);

            if (!has(tradeId)) {
                const NettingSetDetails nettingSetDetails = t->envelope().nettingSetDetails();
                add(tradeId, nettingSetDetails, t->envelope().counterparty());
            }

            // set trade attributes

            if (hasAttributes(tradeId)) {

                tmp = getAttributes(tradeId);

            } else {

                if (tmp->getTradeType().empty()) {
                    if (auto it = additionalFields.find("external_trade_type"); it != additionalFields.end())
                        tmp->setTradeType(it->second);
                    else
                        tmp->setTradeType(t->tradeType());
                }

                tmp->setExtendedAttributes(t, market, bucketMapper_, emitStructuredError);

                TRY_AND_CATCH(tmp->setTradeCurrency(getTradeCurrency(t, referenceData_)), tradeId, t->tradeType(),
                              "setTradeCurrency", emitStructuredError);
                tmp->setSimmProductClass(simmPC);
                tmp->setScheduleProductClass(schedulePC);
                ISDATradeTaxonomy tradeTaxonomy;
                TRY_AND_CATCH(tradeTaxonomy = deriveISDATradeTaxonomy(t), tradeId, t->tradeType(), "tradeTaxonomy",
                              emitStructuredError);
                tmp->setAssetClass(tradeTaxonomy.assetClass);
                tmp->setBaseProduct(tradeTaxonomy.baseProduct);
                tmp->setSubProduct(tradeTaxonomy.subProduct);
                std::string xccyResetting;
                TRY_AND_CATCH(xccyResetting = tradeIsXccyResetting(t), tradeId, t->tradeType(), "xccyResetting",
                              emitStructuredError);
                tmp->setXccyResettable(xccyResetting);
            }

            // set final trade attributes

            setAttributes(tradeId, tmp);
        } catch (const std::exception& e) {
            if (emitStructuredError)
                ore::data::StructuredTradeErrorMessage(tradeId, t->tradeType(),
                                                       "Internal error while processing simm trade data", e.what())
                    .log();
        }
    } // loop over portfolio trades

} // processPortfolio()

void SimmTradeData::add(const string& tradeId, const NettingSetDetails& nettingSetDetails,
                        const string& counterpartyId) {
    QL_REQUIRE(!has(tradeId), "The tradeId is already in the SimmTradeData container");

    nettingSetDetails_[tradeId] = nettingSetDetails;
    counterpartyIds_[tradeId] = counterpartyId;
}

void SimmTradeData::add(const string& tradeId, const string& portfolioId, const string& counterpartyId) {
    add(tradeId, NettingSetDetails(portfolioId), counterpartyId);
}

void SimmTradeData::add(const string& tradeId) { add(tradeId, defaultPortfolioId_, defaultCounterpartyId_); }

set<pair<string, QuantLib::Size>> SimmTradeData::get() const {
    set<pair<string, QuantLib::Size>> tradeIds;
    Size i = 0;
    for (auto const& t : simmTradeIds_) {
        tradeIds.insert(std::make_pair(t, i++));
    }
    return tradeIds;
}

set<pair<string, QuantLib::Size>> SimmTradeData::get(const NettingSetDetails& nettingSetDetails) const {
    set<pair<string, QuantLib::Size>> tradeIds;
    Size i = 0;
    for (auto const& t : simmTradeIds_) {
        if (nettingSetDetails_.at(t) == nettingSetDetails)
            tradeIds.insert(std::make_pair(t, i));
        ++i;
    }
    return tradeIds;
}

set<string> SimmTradeData::portfolioIds() const {
    set<string> portfolioIds;
    for (const auto& kv : nettingSetDetails_) {
        if (portfolioIds.count(kv.second.nettingSetId()) == 0) {
            portfolioIds.insert(kv.second.nettingSetId());
        }
    }
    return portfolioIds;
}

set<NettingSetDetails> SimmTradeData::nettingSetDetails() const {
    set<NettingSetDetails> nettingSetDetails;
    for (const auto& kv : nettingSetDetails_) {
        if (nettingSetDetails.count(kv.second) == 0) {
            nettingSetDetails.insert(kv.second);
        }
    }
    return nettingSetDetails;
}

const string& SimmTradeData::portfolioId(const string& tradeId) const {
    return nettingSetDetails(tradeId).nettingSetId();
}

const NettingSetDetails& SimmTradeData::nettingSetDetails(const string& tradeId) const {
    auto n = nettingSetDetails_.find(tradeId);
    QL_REQUIRE(n != nettingSetDetails_.end(),
               "The tradeId " << tradeId
                              << " is not found in the SimmTradeData container (netting set details lookup)");
    return n->second;
}

set<string> SimmTradeData::counterpartyIds() const {
    set<string> counterpartyIds;
    for (const auto& kv : counterpartyIds_) {
        if (counterpartyIds.count(kv.second) == 0) {
            counterpartyIds.insert(kv.second);
        }
    }
    return counterpartyIds;
}

const string& SimmTradeData::counterpartyId(const string& tradeId) const {
    QL_REQUIRE(has(tradeId), "The tradeId " << tradeId << " is not found in the SimmTradeData container");
    return counterpartyIds_.at(tradeId);
}

bool SimmTradeData::has(const string& tradeId) const { return nettingSetDetails_.count(tradeId) > 0; }

bool SimmTradeData::empty() const { return nettingSetDetails_.empty(); }

void SimmTradeData::clear() {
    nettingSetDetails_.clear();
    counterpartyIds_.clear();
    simmTradeIds_.clear();
    tradeAttributes_.clear();
}

bool SimmTradeData::hasAttributes(const string& tradeId) const { return tradeAttributes_.count(tradeId) > 0; }

void SimmTradeData::setAttributes(const string& tradeId, const QuantLib::ext::shared_ptr<TradeAttributes>& attributes) {
    tradeAttributes_[tradeId] = attributes;
}

//void SimmTradeData::setOriginalTrade(const string& tradeId, const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {
//    originalTrades_[tradeId] = trade;
//}

const QuantLib::ext::shared_ptr<SimmTradeData::TradeAttributes>& SimmTradeData::getAttributes(const string& tradeId) const {
    auto it = tradeAttributes_.find(tradeId);
    QL_REQUIRE(it != tradeAttributes_.end(), "There are no additional trade attributes for trade " << tradeId);
    return it->second;
}

SimmTradeData::ISDATradeTaxonomy
SimmTradeData::deriveISDATradeTaxonomy(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {
    ISDATradeTaxonomy taxonomy;
    taxonomy.assetClass = "";
    taxonomy.baseProduct = "";
    taxonomy.subProduct = "";

    auto itac = trade->additionalData().find("isdaAssetClass");
    if (itac != trade->additionalData().end())
        taxonomy.assetClass = boost::any_cast<std::string>(itac->second);
    auto itbp = trade->additionalData().find("isdaBaseProduct");
    if (itbp != trade->additionalData().end())
        taxonomy.baseProduct = boost::any_cast<std::string>(itbp->second);
    auto itsp = trade->additionalData().find("isdaSubProduct");
    if (itsp != trade->additionalData().end())
        taxonomy.subProduct = boost::any_cast<std::string>(itsp->second);
    
    return taxonomy;
}

std::string SimmTradeData::tradeIsXccyResetting(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {
    // leave the field empty if the trade is not a x-ccy swap
    std::string resetting = "";
    if (trade) {
        auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
        if (swap) {
            // Beside the specific tradetype for x-ccy swaps it is also possible to represent a x-ccy swap as a normal
            // swap therefore check if we have at least two legs with two different currencies
            std::set<std::string> uniqueCurrencies(swap->legCurrencies().begin(), swap->legCurrencies().end());
            if (uniqueCurrencies.size() > 1) {
                resetting = "false";
                // We have a x-ccy swap now check if there is a resetting leg
                for (const auto& leg : swap->legData()) {
                    if (!leg.isNotResetXCCY()) {
                        resetting = "true";
                        break;
                    }
                }
            }
        }
    }
    return resetting;
}

} // namespace analytics
} // namespace ore
