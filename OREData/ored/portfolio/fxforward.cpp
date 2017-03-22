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

#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/instruments/fxforward.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <boost/make_shared.hpp>

namespace ore {
namespace data {

void FxForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    // If you Buy EURUSD forward, then you buy EUR and sell USD.
    // EUR = foreign, USD = Domestic.
    // You pay in USD, so the Domestic / Sold ccy is the "payer" currency
    Currency boughtCcy = parseCurrency(boughtCurrency_);
    Currency soldCcy = parseCurrency(soldCurrency_);
    Date maturityDate = parseDate(maturityDate_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxForward");

    try {
        DLOG("Build FxForward with maturity date " << QuantLib::io::iso_date(maturityDate));

        boost::shared_ptr<QuantLib::Instrument> instrument = boost::make_shared<QuantExt::FxForward>(
            boughtAmount_, boughtCcy, soldAmount_, soldCcy, maturityDate, false);
        instrument_.reset(new VanillaInstrument(instrument));

        npvCurrency_ = soldCurrency_;
        notional_ = soldAmount_;
        maturity_ = maturityDate;

    } catch (std::exception&) {
        instrument_.reset();
        throw;
    }

    // Set up Legs
    legs_ = {{boost::make_shared<SimpleCashFlow>(boughtAmount_, maturityDate)},
             {boost::make_shared<SimpleCashFlow>(soldAmount_, maturityDate)}};
    legCurrencies_ = {boughtCurrency_, soldCurrency_};
    legPayers_ = {false, true};

    // set Pricing engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<FxForwardEngineBuilder> fxBuilder = boost::dynamic_pointer_cast<FxForwardEngineBuilder>(builder);

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
    return node;
}
}
}
