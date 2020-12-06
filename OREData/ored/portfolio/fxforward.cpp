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
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/instruments/fxforward.hpp>

namespace ore {
namespace data {

void FxForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    // If you Buy EURUSD forward, then you buy EUR and sell USD.
    // EUR = foreign, USD = Domestic.
    // You pay in USD, so the Domestic / Sold ccy is the "payer" currency
    Currency boughtCcy = parseCurrency(boughtCurrency_);
    Currency soldCcy = parseCurrency(soldCurrency_);
    Date maturityDate = parseDate(maturityDate_);
    
	// Derive pay date from payment data parameters
    Date payDate; 
    if (payDate_.empty())
        payDate = payCalendar_.advance(maturityDate, payLag_, payConvention_);
    else {
        payDate = parseDate(payDate_);
        QL_REQUIRE(payDate >= maturityDate,
			       "FX Forward settlement date should equal or exceed the maturity date.");
	}
    

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxForward");

    try {
        DLOG("Build FxForward with maturity date " << QuantLib::io::iso_date(maturityDate));

        boost::shared_ptr<QuantLib::Instrument> instrument = boost::make_shared<QuantExt::FxForward>(
            boughtAmount_, boughtCcy, soldAmount_, soldCcy, maturityDate, false, settlement_ == "Physical",
			payDate);
        instrument_.reset(new VanillaInstrument(instrument));

        npvCurrency_ = soldCurrency_;
        notional_ = soldAmount_;
        notionalCurrency_ = soldCurrency_;
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
    boost::shared_ptr<FxForwardEngineBuilderBase> fxBuilder = boost::dynamic_pointer_cast<FxForwardEngineBuilderBase>(builder);

    instrument_->qlInstrument()->setPricingEngine(fxBuilder->engine(boughtCcy, soldCcy));

    DLOG("FxForward leg 0: " << legs_[0][0]->date() << " " << legs_[0][0]->amount());
    DLOG("FxForward leg 1: " << legs_[1][0]->date() << " " << legs_[1][0]->amount());
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

    XMLNode* paymentData = XMLUtils::getChildNode(fxNode, "PaymentData");
    payDate_ = XMLUtils::getChildValue(paymentData, "Date", false);

	if (payDate_.empty()) {
        XMLNode* rules = XMLUtils::getChildNode(paymentData, "Rules");
        payLag_ = XMLUtils::getChildValueAsPeriod(rules, "Lag", false);

        string payCalendar = XMLUtils::getChildValue(rules, "Calendar", false);
        payCalendar_ = payCalendar.empty()
			         ? NullCalendar()
			         : parseCalendar(payCalendar);

        string payConvention = XMLUtils::getChildValue(rules, "Convention", false);
        payConvention_ = payConvention.empty()
			           ? Unadjusted
			           : parseBusinessDayConvention(payConvention);
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
    
	XMLNode* paymentDataNode = doc.allocNode("PaymentData");
    XMLUtils::appendNode(fxNode, paymentDataNode);
    if (!payDate_.empty())
        XMLUtils::addChild(doc, paymentDataNode, "Date", payDate_);
    else {
        XMLNode* rules = doc.allocNode("Rules");
        XMLUtils::addChild(doc, rules, "Lag", payLag_);
        XMLUtils::addChild(doc, rules, "Calendar", payCalendar_.name());
        XMLUtils::addChild(doc, rules, "Convention", payConvention_);
		XMLUtils::appendNode(paymentDataNode, rules);
	}

    return node;
}
} // namespace data
} // namespace ore
