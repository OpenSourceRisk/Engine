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

#include <ored/portfolio/trade.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

void Trade::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Trade");
    tradeType_ = XMLUtils::getChildValue(node, "TradeType", true);
    envelope_.fromXML(XMLUtils::getChildNode(node, "Envelope"));
    tradeActions_.clear();
    XMLNode* taNode = XMLUtils::getChildNode(node, "TradeActions");
    if (taNode)
        tradeActions_.fromXML(taNode);
}

XMLNode* Trade::toXML(XMLDocument& doc) {
    // Crete Trade Node with Id attribute.
    XMLNode* node = doc.allocNode("Trade");
    QL_REQUIRE(node, "Failed to create trade node");
    XMLUtils::addAttribute(doc, node, "id", id_);
    XMLUtils::addChild(doc, node, "TradeType", tradeType_);
    XMLUtils::appendNode(node, envelope_.toXML(doc));
    if (!tradeActions_.empty())
        XMLUtils::appendNode(node, tradeActions_.toXML(doc));
    return node;
}

void Trade::addPayment(std::vector<boost::shared_ptr<Instrument>>& addInstruments, std::vector<Real>& addMultipliers,
                       const Date& paymentDate, const Real& paymentAmount, const Currency& paymentCurrency,
                       const Currency& tradeCurrency, const boost::shared_ptr<EngineFactory>& factory,
                       const string& configuration) {
    boost::shared_ptr<QuantExt::Payment> fee(new QuantExt::Payment(paymentAmount, paymentCurrency, paymentDate));

    // assuming amount provided with correct sign
    addMultipliers.push_back(+1.0);

    // build discounting engine, discount in fee currency, convert into trade currency if different
    Handle<YieldTermStructure> yts = factory->market()->discountCurve(fee->currency().code(), configuration);
    Handle<Quote> fx;
    if (tradeCurrency != fee->currency()) {
        // FX quote for converting the fee NPV into trade currency by multiplication
        string ccypair = fee->currency().code() + tradeCurrency.code();
        fx = factory->market()->fxSpot(ccypair, configuration);
    }
    boost::shared_ptr<PricingEngine> discountingEngine(new QuantExt::PaymentDiscountingEngine(yts, fx));
    fee->setPricingEngine(discountingEngine);

    // 1) Add to additional instruments for pricing
    addInstruments.push_back(fee);

    // 2) Add a trade leg for cash flow reporting
    legs_.push_back(Leg(1, fee->cashFlow()));
    legCurrencies_.push_back(fee->currency().code());
    // amount comes with its correct sign, avoid switching by saying payer=false
    legPayers_.push_back(false); 
}

} // namespace data
} // namespace ore
