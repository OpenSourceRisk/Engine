/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/time/calendar.hpp>
#include <qle/instruments/fxforward.hpp>

using namespace QuantExt;

namespace ore {
namespace data {

void FxForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    const boost::shared_ptr<Market>& market = engineFactory->market();

    // If you Buy EURUSD forward, then you buy EUR and sell USD.
    // EUR = foreign, USD = Domestic.
    // You pay in USD, so the Domestic / Sold ccy is the "payer" currency
    Currency boughtCcy = parseCurrency(boughtCurrency_);
    Currency soldCcy = parseCurrency(soldCurrency_);
    Date maturityDate = parseDate(maturityDate_);

    // Derive pay date from payment data parameters
    Date payDate;
    if (payDate_.empty()) {
        LOG("Attempting paydate advance");
        payDate = payCalendar_.advance(maturityDate, payLag_, payConvention_);
    } else {
        payDate = parseDate(payDate_);
        QL_REQUIRE(payDate >= maturityDate, "FX Forward settlement date should equal or exceed the maturity date.");
    }

    // Add required FX fixing for Cash settlement. If fixingDate == payDate, no fixing is required.
    // Currently, fxFixingCalendar is NullCalendar() and fxFixingDays is zero, so fixingDate = maturityDate
    Date fixingDate;
    Calendar fxFixingCalendar = parseCalendar(fxFixingCalendar_);
    fixingDate = fxFixingCalendar.advance(maturityDate, -static_cast<Integer>(fxFixingDays_), Days);
    bool fixingRequired = (settlement_ == "Cash") && (payDate > fixingDate);

    // If Cash settlement, check that we have a settlement currency. If not, set to domestic currency.
	Currency payCcy;
	if (settlement_ == "Cash") {
		if (payCurrency_.empty()) {
            LOG("Settlement currency was not specified, defaulting to " << soldCcy.code());
			payCcy = soldCcy;
		} else {
			payCcy = parseCurrency(payCurrency_);
            QL_REQUIRE(payCcy == boughtCcy || payCcy == soldCcy, "Settlement currency must be either " <<
																 boughtCcy.code() << " or " << soldCcy.code());
		}	
	}
    
    boost::shared_ptr<QuantExt::FxIndex> fxIndex;
    if (fixingRequired) {
        QL_REQUIRE(!fxIndex_.empty(), "FX settlement index must be specified for non-deliverable forwards.");
		
        Currency nonPayCcy = payCcy == boughtCcy ? soldCcy : boughtCcy;
        fxIndex = buildFxIndex(fxIndex_, payCcy.code(), nonPayCcy.code(), market,
                               engineFactory->configuration(MarketContext::pricing), fxFixingCalendar_, fxFixingDays_);
        
		requiredFixings_.addFixingDate(fixingDate, fxIndex_, payDate);
    }

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxForward");

    try {
        DLOG("Build FxForward with maturity date " << QuantLib::io::iso_date(maturityDate) << "and pay date "
                                                   << QuantLib::io::iso_date(payDate));

        boost::shared_ptr<QuantLib::Instrument> instrument =
            boost::make_shared<QuantExt::FxForward>(boughtAmount_, boughtCcy, soldAmount_, soldCcy, maturityDate, false,
                                                    settlement_ == "Physical", payDate, payCcy, fxIndex, fixingDate);
        instrument_.reset(new VanillaInstrument(instrument));

        npvCurrency_ = (settlement_ == "Physical") ? soldCcy.code() : payCcy.code();
        notional_ = Null<Real>(); // soldAmount_;
        notionalCurrency_ = "";   // soldCurrency_;
        maturity_ = maturityDate;

    } catch (std::exception&) {
        instrument_.reset();
        throw;
    }

    // Set up Legs
    legs_ = {{boost::make_shared<SimpleCashFlow>(boughtAmount_, payDate)},
             {boost::make_shared<SimpleCashFlow>(soldAmount_, payDate)}};
    legCurrencies_ = {boughtCurrency_, soldCurrency_};
    legPayers_ = {false, true};

    // set Pricing engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<FxForwardEngineBuilderBase> fxBuilder =
        boost::dynamic_pointer_cast<FxForwardEngineBuilderBase>(builder);

    instrument_->qlInstrument()->setPricingEngine(fxBuilder->engine(boughtCcy, soldCcy));

    DLOG("FxForward leg 0: " << legs_[0][0]->date() << " " << legs_[0][0]->amount());
    DLOG("FxForward leg 1: " << legs_[1][0]->date() << " " << legs_[1][0]->amount());
}

QuantLib::Real FxForward::notional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument()->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "currentNotional not provided"))
            ALOG("error when retrieving notional: " << e.what());
    }
    // if not provided, return null
    return Null<Real>();
}

std::string FxForward::notionalCurrency() const {
    // try to get the notional ccy from the additional results of the instrument
    try {
        return instrument_->qlInstrument()->result<std::string>("notionalCurrency");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "notionalCurrency not provided"))
            ALOG("error when retrieving notional ccy: " << e.what());
    }
    // if not provided, return an empty string
    return "";
}

void FxForward::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxForwardData");
    QL_REQUIRE(fxNode, "No FxForwardData Node");
    maturityDate_ = XMLUtils::getChildValue(fxNode, "ValueDate", true);
    boughtCurrency_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    soldCurrency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    boughtAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    soldAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
    settlement_ = XMLUtils::getChildValue(fxNode, "Settlement", false);
    if (settlement_ == "")
        settlement_ = "Physical";

    XMLNode* payDataNode = XMLUtils::getChildNode(fxNode, "SettlementData");
    payCurrency_ = XMLUtils::getChildValue(payDataNode, "Currency", false);
    fxIndex_ = XMLUtils::getChildValue(payDataNode, "FXIndex", false);

    XMLNode* datesNode;
    XMLNode* rulesNode;
    datesNode = XMLUtils::getChildNode(payDataNode, "Dates");
    if (datesNode) {
        payDate_ = XMLUtils::getChildValue(datesNode, "Date");
    } else {
        rulesNode = XMLUtils::getChildNode(payDataNode, "Rules");
        if (rulesNode) {
            payLag_ = XMLUtils::getChildValueAsPeriod(rulesNode, "Lag", false);

            string payCalendar = XMLUtils::getChildValue(rulesNode, "Calendar", false);
            payCalendar_ = parseCalendar(payCalendar);

            string payConvention = XMLUtils::getChildValue(rulesNode, "Convention", false);
            payConvention_ = payConvention.empty() ? Unadjusted : parseBusinessDayConvention(payConvention);
        }
    }
}

XMLNode* FxForward::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxForwardData");
    XMLUtils::appendNode(node, fxNode);
    XMLUtils::addChild(doc, fxNode, "ValueDate", maturityDate_);
    XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency_);
    XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount_);
    XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency_);
    XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount_);
    XMLUtils::addChild(doc, fxNode, "Settlement", settlement_);

    XMLNode* payDataNode = doc.allocNode("SettlementData");
    if (!payCurrency_.empty())
        XMLUtils::addChild(doc, payDataNode, "Currency", payCurrency_);
    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, payDataNode, "FXIndex", fxIndex_);

    // Only fill in PaymentData node if payDate_ is not equal to maturityDate_
    if (payDate_ != maturityDate_) {
        if (payLag_ != 0 * Days || payCalendar_ != NullCalendar() || payConvention_ != Unadjusted) {
            XMLNode* rulesNode = doc.allocNode("Rules");
            XMLUtils::appendNode(payDataNode, rulesNode);

            if (payLag_ != 0 * Days)
                XMLUtils::addChild(doc, rulesNode, "Lag", payLag_);
            if (payCalendar_ != NullCalendar())
                XMLUtils::addChild(doc, rulesNode, "Calendar", payCalendar_.name());
            if (payConvention_ != Unadjusted)
                XMLUtils::addChild(doc, rulesNode, "Convention", payConvention_);
        } else {
            XMLUtils::addChild(doc, payDataNode, "Date", payDate_);
        }

        XMLUtils::appendNode(fxNode, payDataNode);
    }

    return node;
}
} // namespace data
} // namespace ore
