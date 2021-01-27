/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orepbase/ored/portfolio/durationadjustedcmslegdata.hpp>

#include <ored/portfolio/legdata.hpp>

namespace oreplus {
namespace data {

XMLNode* DurationAdjustedCmsLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", swapIndex_);
    XMLUtils::addChild(doc, node, "Duration", static_cast<int>(duration_));
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    if (fixingDays_ != Null<Size>())
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

LegDataRegister<DurationAdjustedCmsLegData> DurationAdjustedCmsLegData::reg_("DurationAdjustedCMS");

void DurationAdjustedCmsLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    swapIndex_ = XMLUtils::getChildValue(node, "Index", true);
    duration_ = XMLUtils::getChildValueAsInt(node, "Duration", false);
    indices_.insert(swapIndex_);
    // These are all optional
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false; // default to fixing-in-advance
    if (auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
}

} // namespace data
} // namespace oreplus
