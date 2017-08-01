/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <iostream>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

void ForwardRateAgreement::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    const boost::shared_ptr<Market> market = engineFactory->market();

    Date startDate = parseDate(startDate_);
    Date endDate = parseDate(endDate_);
    Position::Type positionType = parsePositionType(longShort_);

    Handle<YieldTermStructure> discountTS = market->discountCurve(currency_);
    Handle<QuantLib::IborIndex> index = market->iborIndex(index_);

    boost::shared_ptr<QuantLib::ForwardRateAgreement> fra(
        new QuantLib::ForwardRateAgreement(startDate, endDate, positionType, strike_, notional_, *index, discountTS));
    instrument_.reset(new VanillaInstrument(fra));
    npvCurrency_ = currency_;
    maturity_ = endDate;
    instrument_->qlInstrument()->update();
}

void ForwardRateAgreement::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fNode = XMLUtils::getChildNode(node, "ForwardRateAgreementData");

    currency_ = XMLUtils::getChildValue(fNode, "Currency", true);
    startDate_ = XMLUtils::getChildValue(fNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(fNode, "EndDate", true);
    longShort_ = XMLUtils::getChildValue(fNode, "LongShort", true);
    strike_ = XMLUtils::getChildValueAsDouble(fNode, "Strike", true);
    notional_ = XMLUtils::getChildValueAsDouble(fNode, "Notional", true);
    index_ = XMLUtils::getChildValue(fNode, "Index", true);
}

XMLNode* ForwardRateAgreement::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fNode = doc.allocNode("ForwardRateAgreementData");
    XMLUtils::appendNode(node, fNode);

    XMLUtils::addChild(doc, fNode, "Currency", currency_);
    XMLUtils::addChild(doc, fNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, fNode, "EndDate", endDate_);
    XMLUtils::addChild(doc, fNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, fNode, "Strike", strike_);
    XMLUtils::addChild(doc, fNode, "Notional", notional_);
    XMLUtils::addChild(doc, fNode, "Index", index_);
    return node;
}
} // namespace data
} // namespace ore
