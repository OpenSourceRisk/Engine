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

#include <ored/model/calibrationinstruments/yoyswap.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using QuantLib::Period;

namespace ore {
namespace data {

YoYSwap::YoYSwap() : CalibrationInstrument("YoYSwap") {}

YoYSwap::YoYSwap(const Period& tenor)
    : CalibrationInstrument("YoYSwap"),
      tenor_(tenor) {}

const QuantLib::Period& YoYSwap::tenor() const {
    return tenor_;
}

void YoYSwap::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, instrumentType_);
    tenor_ = parsePeriod(XMLUtils::getChildValue(node, "Tenor", true));
}

XMLNode* YoYSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(instrumentType_);
    XMLUtils::addChild(doc, node, "Tenor", to_string(tenor_));
    return node;
}

}
}
