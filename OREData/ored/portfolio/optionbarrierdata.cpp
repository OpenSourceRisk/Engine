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

#include <ored/portfolio/optionbarrierdata.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <string>

using namespace QuantLib;
using std::ostream;
using std::string;
using std::vector;

namespace ore {
namespace data {

OptionBarrierData::OptionBarrierData() : rebate_(0.0) {}

OptionBarrierData::OptionBarrierData(const Barrier::Type barrierType, const Real level, const string& windowStyle,
                                     const Real rebate)
    : barrierType_(barrierType), levels_({level}), windowStyle_(windowStyle), rebate_(rebate),
      doubleBarrierType_(boost::none) {}

OptionBarrierData::OptionBarrierData(const DoubleBarrier::Type doubleBarrierType, const std::vector<Real>& levels,
                                     const string& windowStyle, const Real rebate)
    : doubleBarrierType_(doubleBarrierType), levels_(levels), windowStyle_(windowStyle), rebate_(rebate),
      barrierType_(boost::none) {
    QL_REQUIRE(levels.size() == 2, "expected double barrier levels");
}

void OptionBarrierData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BarrierData");

    barrierType_ = Barrier::Type::DownIn; // Initialize to dummy value so we can dereference below
    doubleBarrierType_ = boost::none;
    if (!tryParse<Barrier::Type>(XMLUtils::getChildValue(node, "Type", true), *barrierType_, &parseBarrierType)) {
        // Not a single barrier so we reset to uninitialized state, and try to parse a double barrier type
        barrierType_ = boost::none;
        doubleBarrierType_ =
            boost::optional<DoubleBarrier::Type>(parseDoubleBarrierType(XMLUtils::getChildValue(node, "Type", true)));
    }

    windowStyle_ = "American"; // Default to American unless an explicit style is given
    if (XMLNode* styleNode = XMLUtils::getChildNode(node, "Style"))
        windowStyle_ = XMLUtils::getChildValue(node, "Style", true);

    levels_ = XMLUtils::getChildrenValuesAsDoubles(node, "Levels", "Level", true);
    rebate_ = XMLUtils::getChildValueAsDouble(node, "Rebate", false);
}

XMLNode* OptionBarrierData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("BarrierData");
    if (barrierType_) {
        string temp = to_string(*barrierType_);
        // Replace & with And in temp, this simplifies the XML input/reading
        XMLUtils::addChild(doc, node, "Type", temp.replace(temp.find("&"), sizeof("&") - 1, "And"));
    }

    if (doubleBarrierType_)
        XMLUtils::addChild(doc, node, "Type", to_string(*doubleBarrierType_));

    XMLUtils::addChild(doc, node, "WindowStyle", windowStyle_);
    XMLUtils::addChildren(doc, node, "Levels", "Level", levels_);
    XMLUtils::addChild(doc, node, "Rebate", rebate_);
    return node;
}

} // namespace data
} // namespace ore
