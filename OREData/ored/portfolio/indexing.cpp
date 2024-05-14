/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/indexing.hpp>

#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

void Indexing::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Indexing");
    if (auto n = XMLUtils::getChildNode(node, "Quantity")) {
        quantity_ = parseReal(XMLUtils::getNodeValue(n));
    } else {
        quantity_ = 1.0;
    }
    index_ = XMLUtils::getChildValue(node, "Index", false);
    indexFixingCalendar_ = XMLUtils::getChildValue(node, "IndexFixingCalendar", false);
    if (XMLUtils::getChildNode(node, "IndexFixingDays")) {
        WLOG("Indexing::fromXML, node IndexFixingDays has been deprecated, fixing days are "
             "taken from conventions.");
    }
    indexIsDirty_ = XMLUtils::getChildValueAsBool(node, "Dirty", false);
    indexIsRelative_ = XMLUtils::getChildValueAsBool(node, "Relative", false);
    indexIsConditionalOnSurvival_ = XMLUtils::getChildValueAsBool(node, "ConditionalOnSurvival", false);
    initialFixing_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "InitialFixing"))
        initialFixing_ = parseReal(XMLUtils::getNodeValue(n));
    initialNotionalFixing_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "InitialNotionalFixing"))
        initialNotionalFixing_ = parseReal(XMLUtils::getNodeValue(n));
    if (auto tmp = XMLUtils::getChildNode(node, "ValuationSchedule"))
        valuationSchedule_.fromXML(tmp);
    fixingDays_ = 0;
    if (auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    fixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar");
    fixingConvention_ = XMLUtils::getChildValue(node, "FixingConvention");
    inArrearsFixing_ = false;
    if (auto n = XMLUtils::getChildNode(node, "IsInArrears"))
        inArrearsFixing_ = parseBool(XMLUtils::getNodeValue(n));
    hasData_ = true;
}

XMLNode* Indexing::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Indexing");
    XMLUtils::addChild(doc, node, "Quantity", quantity_);
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "IndexFixingCalendar", indexFixingCalendar_);
    XMLUtils::addChild(doc, node, "Dirty", indexIsDirty_);
    XMLUtils::addChild(doc, node, "Relative", indexIsRelative_);
    XMLUtils::addChild(doc, node, "ConditionalOnSurvival", indexIsConditionalOnSurvival_);
    if (initialFixing_ != Null<Real>())
        XMLUtils::addChild(doc, node, "InitialFixing", initialFixing_);
    if (initialNotionalFixing_ != Null<Real>())
        XMLUtils::addChild(doc, node, "InitialNotionalFixing", initialNotionalFixing_);
    if (valuationSchedule_.hasData()) {
        XMLNode* schedNode = valuationSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, schedNode, "ValuationSchedule");
        XMLUtils::appendNode(node, schedNode);
    }
    XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChild(doc, node, "FixingCalendar", fixingCalendar_);
    XMLUtils::addChild(doc, node, "FixingConvention", fixingConvention_);
    XMLUtils::addChild(doc, node, "IsInArrears", inArrearsFixing_);
    return node;
}

} // namespace data
} // namespace ore
