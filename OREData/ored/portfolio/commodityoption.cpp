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

#include <ored/portfolio/commodityoption.hpp>

#include <boost/make_shared.hpp>

#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

CommodityOption::CommodityOption() : VanillaOptionTrade(AssetClass::COM) {
    tradeType_ = "CommodityOption";
}

CommodityOption::CommodityOption(const Envelope& env, const OptionData& optionData, const string& commodityName,
    const string& currency, Real strike, Real quantity)
    : VanillaOptionTrade(env, AssetClass::COM, optionData, commodityName, currency, strike, quantity) {
    tradeType_ = "CommodityOption";
}

void CommodityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Checks
    QL_REQUIRE(quantity_ > 0, "Commodity option requires a positive quatity");
    QL_REQUIRE(strike_ > 0, "Commodity option requires a positive strike");

    VanillaOptionTrade::build(engineFactory);
}

std::map<AssetClass, std::set<std::string>> CommodityOption::underlyingIndices() const {
    return { {AssetClass::COM, std::set<std::string>({ assetName_ })} };
}

void CommodityOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* commodityNode = XMLUtils::getChildNode(node, "CommodityOptionData");
    QL_REQUIRE(commodityNode, "A commodity option needs a 'CommodityOptionData' node");

    option_.fromXML(XMLUtils::getChildNode(commodityNode, "OptionData"));

    assetName_ = XMLUtils::getChildValue(commodityNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityNode, "Quantity", true);
}

XMLNode* CommodityOption::toXML(XMLDocument& doc) {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* eqNode = doc.allocNode("CommodityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));

    XMLUtils::addChild(doc, eqNode, "Name", assetName_);
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

}
}
