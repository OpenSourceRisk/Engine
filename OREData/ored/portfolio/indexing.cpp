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
    quantity_ = XMLUtils::getChildValueAsDouble(node, "Quantity", false); // defaults to 0.0
    index_ = XMLUtils::getChildValue(node, "Index", true);
    initialFixing_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "InitialFixing"))
        initialFixing_ = parseReal(XMLUtils::getNodeValue(n));
    if (auto tmp = XMLUtils::getChildNode(node, "ValuationSchedule"))
        valuationSchedule_.fromXML(tmp);
    fixingDays_ = 0;
    if (auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    fixingCalendar_ = XMLUtils::getChildNode(node, "FixingCalendar");
    fixingConvention_ = XMLUtils::getChildNode(node, "FixingConvention");
    inArrearsFixing_ = false;
    if (auto n = XMLUtils::getChildNode(node, "IsInArrears"))
        inArrearsFixing_ = parseBool(XMLUtils::getNodeValue(n));
}

XMLNode* Indexing::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Indexing");
    XMLUtils::addChild(doc, node, "Quantity", quantity_);
    XMLUtils::addChild(doc, node, "Index", index_);
    if (initialFixing_ != Null<Real>())
        XMLUtils::addChild(doc, node, "InitialFixing", initialFixing_);
    if (valuationSchedule_.hasData()) {
        XMLNode* tmp = doc.allocNode("ValuationSchedule");
        XMLUtils::appendNode(tmp, valuationSchedule_.toXML(doc));
        XMLUtils::appendNode(node, tmp);
    }
    XMLUtils::addChild<Size>(doc, node, "FixingDays", fixingDays_);
    XMLUtils::addChild(doc, node, "FixingCalendar", fixingCalendar_);
    XMLUtils::addChild(doc, node, "FixingConvention", fixingConvention_);
    XMLUtils::addChild(doc, node, "IsInArrears", inArrearsFixing_);
}

} // namespace data
} // namespace ore
