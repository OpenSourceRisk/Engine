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

#include <ored/model/infdkdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

void InfDkData::fromXML(XMLNode* node) {
    index_ = XMLUtils::getAttribute(node, "index");
    LOG("Cross-Asset Inflation Index = " << index_);

    currency_ = XMLUtils::getChildValue(node, "Currency");
    LOG("Cross-Asset Inflation Index = " << currency_);

    // Calibration CapFloors

    XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationCapFloors");

    capfloor_ = XMLUtils::getChildValue(optionsNode, "CapFloor");
    optionExpiries() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", false);
    optionStrikes() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);
    if (optionStrikes().size() > 0) {
        QL_REQUIRE(optionStrikes().size() == optionExpiries().size(),
            "vector size mismatch in cap floor expiries/strikes for inflation index " << index_);
    }
    else // Default: ATM
        optionStrikes().resize(optionExpiries().size(), "ATM");

    for (Size i = 0; i < optionExpiries().size(); i++) {
        LOG("LGM calibration capfloors " << optionExpiries()[i] << " " << optionStrikes()[i]);
    }
    LgmData::fromXML(node);
}

XMLNode* InfDkData::toXML(XMLDocument& doc) {
    XMLNode* node = LgmData::toXML(doc);
    XMLUtils::addAttribute(doc, node, "index", index_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    // capfloor calibration
    XMLNode* calibrationCapFloorNode = XMLUtils::addChild(doc, node, "CalibrationCapFloors");
    XMLUtils::addGenericChildAsList(doc, calibrationCapFloorNode, "Expiries", optionExpiries());
    XMLUtils::addGenericChildAsList(doc, calibrationCapFloorNode, "Strikes", optionStrikes());

    return node;
}
} // namespace data
} // namespace ore
