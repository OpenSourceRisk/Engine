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

#include <ored/model/calibrationinstruments/yoycapfloor.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using QuantLib::Period;
using QuantLib::YoYInflationCapFloor;

namespace ore {
namespace data {

YoYCapFloor::YoYCapFloor() 
 : CalibrationInstrument("YoYCapFloor"), type_(YoYInflationCapFloor::Floor) {}

YoYCapFloor::YoYCapFloor(YoYInflationCapFloor::Type type,
    const Period& tenor,
    const QuantLib::ext::shared_ptr<BaseStrike>& strike)
    : CalibrationInstrument("YoYCapFloor"),
      type_(type),
      tenor_(tenor),
      strike_(strike) {}

YoYInflationCapFloor::Type YoYCapFloor::type() const {
    return type_;
}

const QuantLib::Period& YoYCapFloor::tenor() const {
    return tenor_;
}

const QuantLib::ext::shared_ptr<BaseStrike>& YoYCapFloor::strike() const {
    return strike_;
}

void YoYCapFloor::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, instrumentType_);
    type_ = parseYoYInflationCapFloorType(XMLUtils::getChildValue(node, "Type", true));
    tenor_ = parsePeriod(XMLUtils::getChildValue(node, "Tenor", true));
    strike_ = parseBaseStrike(XMLUtils::getChildValue(node, "Strike", true));
}

XMLNode* YoYCapFloor::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(instrumentType_);
    if (type_ == YoYInflationCapFloor::Cap)
        XMLUtils::addChild(doc, node, "Type", "Cap");
    else if (type_ == YoYInflationCapFloor::Floor)
        XMLUtils::addChild(doc, node, "Type", "Floor");
    else
        QL_FAIL("Failure in YoYCapFloor::toXML, unsupported YoY cap floor type.");
    XMLUtils::addChild(doc, node, "Tenor", to_string(tenor_));
    XMLUtils::addChild(doc, node, "Strike", strike_->toString());
    return node;
}

}
}
