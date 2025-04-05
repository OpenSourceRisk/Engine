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

#include <orea/simm/simmtradedata.hpp>
#include <orea/simm/utilities.hpp>

#include <ored/portfolio/trs.hpp>
#include <ored/utilities/parsers.hpp>

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

#define TRY_AND_CATCH(expr, tradeId, tradeType, context, emitStructuredError)                                          \
    try {                                                                                                              \
        expr;                                                                                                          \
    } catch (const std::exception& e) {                                                                                \
            ore::data::StructuredTradeErrorMessage(                                                                    \
                tradeId, tradeType, "Error while setting simm trade data (" + std::string(context) + ")", e.what())    \
                .log();                                                                                                \
    }

using ore::data::LegType;
using ore::data::BondTRS;
using std::multimap;

namespace ore {
namespace analytics {

using namespace QuantLib;

void SimmTradeData::init() {
    if (!initialised_) {
        hasNettingSetDetails_ = portfolio_->hasNettingSetDetails();
        processPortfolio(portfolio_, market_, auxiliaryPortfolio_);
        initialised_ = true;
    }
}

void SimmTradeData::TradeAttributes::setTradeType(const string tradeType) { tradeType_ = tradeType; }

void SimmTradeData::TradeAttributes::setSimmProductClass(const CrifRecord::ProductClass& pc) { simmProductClass_ = pc; }

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

    // end date
    setEndDate(ore::data::to_string(trade->maturity()));

    // notional and trade currency
    TRY_AND_CATCH(setNotional(trade->notional()), trade->id(), trade->tradeType(), "setNotional", emitStructuredError);
    TRY_AND_CATCH(setNotionalCurrency(trade->notionalCurrency()), trade->id(), trade->tradeType(),
                  "setNotionalCurrency", emitStructuredError);

    QuantLib::Real notional = getNotional();
    string notionalCcy = getNotionalCurrency();
    if (notional != Null<Real>() && !notionalCcy.empty())
        TRY_AND_CATCH(
            setNotionalUSD(notionalCcy == "USD" ? notional : market->fxRate(notionalCcy + "USD")->value() * notional),
            trade->id(), trade->tradeType(), "setNotionalUSD", emitStructuredError);
}

void SimmTradeData::processPortfolio(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                                     const QuantLib::ext::shared_ptr<ore::data::Market>& market,
				     const QuantLib::ext::shared_ptr<Portfolio>& auxiliaryPortfolio) {
    DLOG("SimmTradeData::processPortfolio called");
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

                tmp->setSimmProductClass(simmPC);
                tmp->setScheduleProductClass(schedulePC);
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
    initialised_ = false;
}

bool SimmTradeData::hasAttributes(const string& tradeId) const { return tradeAttributes_.count(tradeId) > 0; }

void SimmTradeData::setAttributes(const string& tradeId, const QuantLib::ext::shared_ptr<TradeAttributes>& attributes) {
    tradeAttributes_[tradeId] = attributes;
}

const QuantLib::ext::shared_ptr<SimmTradeData::TradeAttributes>& SimmTradeData::getAttributes(const string& tradeId) const {
    QL_REQUIRE(initialised_, "SimmTradeData not initalised yet");
    auto it = tradeAttributes_.find(tradeId);
    QL_REQUIRE(it != tradeAttributes_.end(), "There are no additional trade attributes for trade " << tradeId);
    QL_REQUIRE(it->second, "TradeAttributes pointer is null");
    return it->second;
}

} // namespace analytics
} // namespace ore
