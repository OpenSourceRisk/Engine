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

#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using QuantLib::CapFloor;
using QuantLib::Date;
using QuantLib::Period;

namespace ore {
namespace data {

CpiCapFloor::CpiCapFloor() : CalibrationInstrument("CpiCapFloor"), type_(CapFloor::Floor) {}

CpiCapFloor::CpiCapFloor(CapFloor::Type type,
    const boost::variant<QuantLib::Date, Period> & maturity,
    const QuantLib::ext::shared_ptr<BaseStrike>& strike)
    : CalibrationInstrument("CpiCapFloor"),
      type_(type),
      maturity_(maturity),
      strike_(strike) {}

CapFloor::Type CpiCapFloor::type() const {
    return type_;
}

const boost::variant<Date, Period>& CpiCapFloor::maturity() const {
    return maturity_;
}

const QuantLib::ext::shared_ptr<BaseStrike>& CpiCapFloor::strike() const { return strike_; }

void CpiCapFloor::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, instrumentType_);
    type_ = parseCapFloorType(XMLUtils::getChildValue(node, "Type", true));
    maturity_ = parseDateOrPeriod(XMLUtils::getChildValue(node, "Maturity", true));
    strike_ = parseBaseStrike(XMLUtils::getChildValue(node, "Strike", true));
}

XMLNode* CpiCapFloor::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(instrumentType_);
    XMLUtils::addChild(doc, node, "Type", to_string(type_));
    XMLUtils::addChild(doc, node, "Maturity", to_string(maturity_));
    XMLUtils::addChild(doc, node, "Strike", strike_->toString());
    return node;
}

} // namespace data
} // namespace ore
