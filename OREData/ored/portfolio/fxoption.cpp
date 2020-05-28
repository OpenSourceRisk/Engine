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
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    VanillaOptionTrade::build(engineFactory);
    const string& ccyPairCode = assetName_ + currency_;
    Handle<BlackVolTermStructure> blackVol = engineFactory->market()->fxVol(ccyPairCode);
    LOG("Implied vol for " << tradeType_ << " on " << ccyPairCode << " with maturity " << maturity_ << " and strike " << strike_
                                        << " is " << blackVol->blackVol(maturity_, strike_));

    Date expiryDate = parseDate(option_.exerciseDates().back());
    npvCurrency_ = currency_;
    notional_ = strike_ * quantity_;
    notionalCurrency_ = currency_;
    maturity_ = expiryDate;
}

void FxOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxOptionData");
    QL_REQUIRE(fxNode, "No FxOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    assetName_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    currency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    double boughtAmount = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    double soldAmount = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
    strike_ = soldAmount / boughtAmount;
    quantity_ = boughtAmount;
}

XMLNode* FxOption::toXML(XMLDocument& doc) {
    //TODO: Should call parent class to xml?
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency());
    XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount());
    XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency());
    XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount());

    return node;
}
} // namespace data
} // namespace ore
