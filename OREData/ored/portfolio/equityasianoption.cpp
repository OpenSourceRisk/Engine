/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/portfolio/asianoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityasianoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/errors.hpp>
#include <string>

namespace ore {
namespace data {

void EquityAsianOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    // Checks
    QL_REQUIRE(quantity_ > 0, "Equity Asian option requires a positive quatity");
    QL_REQUIRE(strike_ > 0, "Equity Asian option requires a positive strike");

    // Set the assetName_ as it may have changed after lookup
    assetName_ = equityName();

    // Populate the index_ in case the option is automatic exercise
    const boost::shared_ptr<Market>& market = engineFactory->market();
    index_ = *market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing));

    // Build the trade using shared functionality in the base class
    AsianOptionTrade::build(engineFactory);
}

std::map<AssetClass, std::set<std::string>> EquityAsianOption::underlyingIndices() const {
    return {{AssetClass::EQ, std::set<std::string>({equityName()})}};
}

void EquityAsianOption::fromXML(XMLNode* node) {
    AsianOptionTrade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityAsianOptionData");
    QL_REQUIRE(eqNode, "No EquityAsianOptionData node");

    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    QL_REQUIRE(option_.payoffType() == "Asian", "Expected PayoffType Asian for EquityAsianOption.");
    XMLNode* asianNode = XMLUtils::getChildNode(eqNode, "AsianData");
    asianData_.fromXML(asianNode);

    XMLNode* scheduleDataNode = XMLUtils::getChildNode(eqNode, "ScheduleData");
    scheduleData_.fromXML(scheduleDataNode);

    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);

    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    // Require explicit Strike
    strike_ = XMLUtils::getChildValueAsDouble(eqNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
}

XMLNode* EquityAsianOption::toXML(XMLDocument& doc) {
    XMLNode* node = AsianOptionTrade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityAsianOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, asianData_.toXML(doc));
    XMLUtils::appendNode(eqNode, scheduleData_.toXML(doc));

    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

} // namespace data
} // namespace ore
