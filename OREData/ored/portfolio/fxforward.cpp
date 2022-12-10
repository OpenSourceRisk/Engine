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
#include <ored/utilities/marketdata.hpp>
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

    // Derive settlement date from payment data parameters
    Date payDate;
    if (payDate_.empty()) {
        Natural conventionalLag = 0;
        Calendar conventionalCalendar = NullCalendar();
        BusinessDayConvention conventionalBdc = Unadjusted;
        if (!fxIndex_.empty() && settlement_ == "Cash") {
            std::tie(conventionalLag, conventionalCalendar, conventionalBdc) =
                getFxIndexConventions(fxIndex_.empty() ? boughtCurrency_ + soldCurrency_ : fxIndex_);
        }
        PaymentLag paymentLag;
        if (payLag_.empty())
            paymentLag = conventionalLag;
        else
            paymentLag = parsePaymentLag(payLag_);
        Period payLag = boost::apply_visitor(PaymentLagPeriod(), paymentLag);
        Calendar payCalendar = payCalendar_.empty() ? conventionalCalendar : parseCalendar(payCalendar_);
        BusinessDayConvention payConvention =
            payConvention_.empty() ? conventionalBdc : parseBusinessDayConvention(payConvention_);
        payDate = payCalendar.advance(maturityDate, payLag, payConvention);
    } else {
        payDate = parseDate(payDate_);
        QL_REQUIRE(payDate >= maturityDate, "FX Forward settlement date should equal or exceed the maturity date.");
    }

    boost::shared_ptr<QuantExt::FxIndex> fxIndex;
    Currency payCcy;

    if (payCurrency_.empty()) {
        // If settlement currency is not set, set it to the domestic currency.
        //LOG("Settlement currency was not specified, defaulting to " << soldCcy.code());
        payCcy = soldCcy;
    } else {
        payCcy = parseCurrency(payCurrency_);
        QL_REQUIRE(payCcy == boughtCcy || payCcy == soldCcy,
                   "Settlement currency must be either " << boughtCcy.code() << " or " << soldCcy.code() << ".");
    }

    Date fixingDate;
    bool fixingRequired = (settlement_ == "Cash") && (payDate > maturityDate);
    if (fixingRequired) {
        QL_REQUIRE(!fxIndex_.empty(), "FX settlement index must be specified for non-deliverable forwards");
        Currency nonPayCcy = payCcy == boughtCcy ? soldCcy : boughtCcy;
        fxIndex = buildFxIndex(fxIndex_, nonPayCcy.code(), payCcy.code(), engineFactory->market(),
                                 engineFactory->configuration(MarketContext::pricing));
        fixingDate = fxIndex->fixingCalendar().adjust(maturityDate);
	requiredFixings_.addFixingDate(fixingDate, fxIndex_, payDate);
    }

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxForward");

    DLOG("Build FxForward with maturity date " << QuantLib::io::iso_date(maturityDate) << " and pay date "
                                               << QuantLib::io::iso_date(payDate));

    boost::shared_ptr<QuantLib::Instrument> instrument =
        boost::make_shared<QuantExt::FxForward>(boughtAmount_, boughtCcy, soldAmount_, soldCcy, maturityDate, false,
                                                settlement_ == "Physical", payDate, payCcy, fixingDate, fxIndex);
    instrument_.reset(new VanillaInstrument(instrument));

    npvCurrency_ = payCcy.code();
    notional_ = Null<Real>(); // soldAmount_;
    notionalCurrency_ = "";   // soldCurrency_;
    maturity_ = std::max(payDate, maturityDate);

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

    additionalData_["soldCurrency"] = soldCurrency_;
    additionalData_["boughtCurrency"] = boughtCurrency_;
    additionalData_["soldAmount"] = soldAmount_;
    additionalData_["boughtAmount"] = boughtAmount_;
}

QuantLib::Real FxForward::notional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument(true)->result<Real>("currentNotional");
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
        return instrument_->qlInstrument(true)->result<std::string>("notionalCurrency");
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

    if (XMLNode* settlementDataNode = XMLUtils::getChildNode(fxNode, "SettlementData")) {
        payCurrency_ = XMLUtils::getChildValue(settlementDataNode, "Currency", false);
        fxIndex_ = XMLUtils::getChildValue(settlementDataNode, "FXIndex", false);
        payDate_ = XMLUtils::getChildValue(settlementDataNode, "Date", false);

        if (payDate_.empty()) {
            if (XMLNode* rulesNode = XMLUtils::getChildNode(settlementDataNode, "Rules")) {
                payLag_ = XMLUtils::getChildValue(rulesNode, "PaymentLag", false);
                payCalendar_ = XMLUtils::getChildValue(rulesNode, "PaymentCalendar", false);
                payConvention_ = XMLUtils::getChildValue(rulesNode, "PaymentConvention", false);
            }
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

    XMLNode* settlementDataNode = doc.allocNode("SettlementData");
    XMLUtils::appendNode(fxNode, settlementDataNode);

    if (!payCurrency_.empty())
        XMLUtils::addChild(doc, settlementDataNode, "Currency", payCurrency_);
    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, settlementDataNode, "FXIndex", fxIndex_);
    if (!payDate_.empty()) {
        XMLUtils::addChild(doc, settlementDataNode, "Date", payDate_);
    } else {
        XMLNode* rulesNode = doc.allocNode("Rules");
        XMLUtils::appendNode(settlementDataNode, rulesNode);
        if (!payLag_.empty())
            XMLUtils::addChild(doc, rulesNode, "PaymentLag", payLag_);
        if (!payCalendar_.empty())
            XMLUtils::addChild(doc, rulesNode, "PaymentCalendar", payCalendar_);
        if (!payConvention_.empty())
            XMLUtils::addChild(doc, rulesNode, "PaymentConvention", payConvention_);
    }

    return node;
}
} // namespace data
} // namespace ore
