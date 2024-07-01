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

#include <ored/model/calibrationbasket.hpp>
#include <ored/model/calibrationinstrumentfactory.hpp>

using std::string;
using std::vector;

namespace ore {
namespace data {

CalibrationInstrument::CalibrationInstrument(const string& instrumentType)
    : instrumentType_(instrumentType) {}

const string& CalibrationInstrument::instrumentType() const {
    return instrumentType_;
}

CalibrationBasket::CalibrationBasket() {}

CalibrationBasket::CalibrationBasket(const vector<QuantLib::ext::shared_ptr<CalibrationInstrument>>& instruments)
    : instruments_(instruments) {
    
    // Check that all instruments in the basket, if any, are the same type.
    for (const auto& instrument : instruments_) {
        string instType = instrument->instrumentType();
        if (instrumentType_.empty()) {
            instrumentType_ = instType;
        } else {
            QL_REQUIRE(instrumentType_ == instType, "All instruments in CalibrationBasket should have the same " <<
                "instrument type. Have " << instrumentType_ << " but current instrument is " << instType << ".");
        }
    }

}

const string& CalibrationBasket::instrumentType() const {
    return instrumentType_;
}

const vector<QuantLib::ext::shared_ptr<CalibrationInstrument>>& CalibrationBasket::instruments() const {
    return instruments_;
}

const string& CalibrationBasket::parameter() const {
    return parameter_;
}

void CalibrationBasket::fromXML(XMLNode* node) {
    
    QL_REQUIRE(empty(), "The calibration basket should be empty before calling fromXML.");
    XMLUtils::checkNode(node, "CalibrationBasket");
    
    for (XMLNode* cn = XMLUtils::getChildNode(node); cn; cn = XMLUtils::getNextSibling(cn)) {
        
        // Take the instrument type from the first node name.
        // All subsequent nodes should have the same instrument type.
        string name = XMLUtils::getNodeName(cn);
        if (instrumentType_.empty()) {
            instrumentType_ = name;
        } else {
            QL_REQUIRE(instrumentType_ == name, "All instruments in CalibrationBasket should have " <<
                "the same instrument type. Have " << instrumentType_ << " but current node is " << name << ".");
        }

        // Create an instance of the calibration instrument and read it from XML.
        auto instrument = CalibrationInstrumentFactory::instance().build(instrumentType_);
        QL_REQUIRE(instrument, "Calibration instrument type " << instrumentType_ <<
            " has not been registered with the calibration instrument factory.");
        instrument->fromXML(cn);

        // Add the instrument to the basket.
        instruments_.push_back(instrument);
    }

    QL_REQUIRE(!empty(), "The calibration basket should have at least one calibration instrument.");

    parameter_ = XMLUtils::getAttribute(node, "parameter");
}

XMLNode* CalibrationBasket::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CalibrationBasket");

    if (!parameter_.empty())
        XMLUtils::addAttribute(doc, node, "parameter", parameter_);

    for (const auto& instrument : instruments_) {
        XMLUtils::appendNode(node, instrument->toXML(doc));
    }

    return node;
}

bool CalibrationBasket::empty() const {
    return instruments_.empty();
}

}
}
