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
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    string name = equityName();
    // Look up reference data, if the equity name exists in reference data, use the equityId
    // if not continue with the current name
    if (engineFactory->referenceData() != nullptr && engineFactory->referenceData()->hasData("Equity", name)) {
        auto refData = engineFactory->referenceData()->getData("Equity", name);
        // Check it's equity reference data
        if (auto erd = boost::dynamic_pointer_cast<EquityReferenceDatum>(refData)) {
            name = erd->equityData().equityId;

            // check currency - if option currency and equity currency are different this is a quanto trade
            QL_REQUIRE(currency_ == erd->equityData().currency, 
                "Option Currency: " << currency_ << " does not match equity currency: " << erd->equityData().currency << ", EquityOption does not support quanto options.");
        }
    }

    // set the option assetname - may have changed after lookup
    assetName_ = name;

    VanillaOptionTrade::build(engineFactory);

    Handle<BlackVolTermStructure> blackVol = engineFactory->market()->equityVol(assetName_);
    LOG("Implied vol for " << tradeType_ << " on " << assetName_ << " with maturity " << maturity_ << " and strike " << strike_
                                        << " is " << blackVol->blackVol(maturity_, strike_));
}

void EquityOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityOptionData");
    QL_REQUIRE(eqNode, "No EquityOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(eqNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
}

XMLNode* EquityOption::toXML(XMLDocument& doc) {
    XMLNode* node = VanillaOptionTrade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

std::map<AssetClass, std::set<std::string>> EquityOption::underlyingIndices() const {
    return { {AssetClass::EQ, std::set<std::string>({equityName()})} };
}

} // namespace data
} // namespace ore
