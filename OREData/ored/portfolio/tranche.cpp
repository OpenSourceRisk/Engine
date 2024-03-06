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

#include <ored/portfolio/tranche.hpp>

using namespace QuantLib;
using std::string;
using std::vector;

namespace ore {
namespace data {

void TrancheData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Tranche");

    QL_REQUIRE(node, "No Tranche Node");

    name_ = XMLUtils::getChildValue(node, "Name", true);
    faceAmount_ = XMLUtils::getChildValueAsDouble(node, "Notional", true);
    icRatio_ = XMLUtils::getChildValueAsDouble(node, "ICRatio", false, -1.0);
    ocRatio_ = XMLUtils::getChildValueAsDouble(node, "OCRatio", false, -1.0);

    //FloatingLegData
    XMLNode* floater = XMLUtils::getChildNode(node, "FloatingLegData");
    if(floater){
        //Floating
        concreteLegData_ = LegDataFactory::instance().build("Floating");
        concreteLegData_->fromXML(XMLUtils::getChildNode(node, concreteLegData_->legNodeName()));
    }

    //FixedLegData
    XMLNode* fixed = XMLUtils::getChildNode(node, "FixedLegData");
    if(fixed){
        //Fixed
        concreteLegData_ = LegDataFactory::instance().build("Fixed");
        concreteLegData_->fromXML(XMLUtils::getChildNode(node, concreteLegData_->legNodeName()));
    }

}


XMLNode* TrancheData::toXML(ore::data::XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Tranche");

    XMLUtils::addChild(doc, node, "Name", name_);
    XMLUtils::addChild(doc, node, "Notional", faceAmount_);
    XMLUtils::addChild(doc, node, "ICRatio", icRatio_);
    XMLUtils::addChild(doc, node, "OCRatio", ocRatio_);
    XMLUtils::appendNode(node, concreteLegData_->toXML(doc));

    return node;
}

} // namespace data
} // namespace ore
