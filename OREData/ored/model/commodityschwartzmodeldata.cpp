/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/model/commodityschwartzmodeldata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

bool CommoditySchwartzData::operator==(const CommoditySchwartzData& rhs) {

    if (name_ != rhs.name_ || ccy_ != rhs.ccy_ || calibrationType_ != rhs.calibrationType_ ||
        calibrateSigma_ != rhs.calibrateSigma_ || sigmaType_ != rhs.sigmaType_ || sigmaValue_ != rhs.sigmaValue_ ||
        calibrateKappa_ != rhs.calibrateKappa_ || kappaType_ != rhs.kappaType_ || kappaValue_ != rhs.kappaValue_ ||
        optionExpiries_ != rhs.optionExpiries_ || optionStrikes_ != rhs.optionStrikes_ || driftFreeState_ == rhs.driftFreeState_) {
        return false;
    }
    return true;
}

bool CommoditySchwartzData::operator!=(const CommoditySchwartzData& rhs) { return !(*this == rhs); }

void CommoditySchwartzData::fromXML(XMLNode* node) {
    name_ = XMLUtils::getAttribute(node, "name");
    LOG("Cross-Asset Commodity Name = " << name_);

    ccy_ = XMLUtils::getChildValue(node, "Currency", true);
    LOG("Cross-Asset Commodity Currency = " << ccy_);

    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    calibrationType_ = parseCalibrationType(calibTypeString);
    LOG("Cross-Asset Commodity calibration type = " << calibTypeString);

    XMLNode* sigmaNode = XMLUtils::getChildNode(node, "Sigma");
    calibrateSigma_ = XMLUtils::getChildValueAsBool(sigmaNode, "Calibrate", true);
    LOG("Cross-Asset Commodity Sigma calibrate = " << calibrateSigma_);
    sigmaValue_ = XMLUtils::getChildValueAsDouble(sigmaNode, "InitialValue", true);
    LOG("Cross-Asset Commodity Sigma initial value = " << sigmaValue_);

    XMLNode* kappaNode = XMLUtils::getChildNode(node, "Kappa");
    calibrateKappa_ = XMLUtils::getChildValueAsBool(kappaNode, "Calibrate", true);
    LOG("Cross-Asset Commodity Kappa calibrate = " << calibrateKappa_);
    kappaValue_ = XMLUtils::getChildValueAsDouble(kappaNode, "InitialValue", true);
    LOG("Cross-Asset Commodity Kappa initial value = " << kappaValue_);

    XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationOptions");
    if (optionsNode) {
        optionExpiries_ = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", true);
        optionStrikes_ = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);
        if (optionStrikes_.size() > 0) {
            QL_REQUIRE(optionExpiries_.size() == optionStrikes_.size(),
                       "size mismatch in commodity option expiries/strike for name " << name_);
        } else // default ATMF
            optionStrikes_.resize(optionExpiries_.size(), "ATMF");
    }

    driftFreeState_ = XMLUtils::getChildValueAsBool(node, "DriftFreeState", false);
}

XMLNode* CommoditySchwartzData::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CommoditySchwartz");
    XMLUtils::addAttribute(doc, node, "name", name_);

    XMLUtils::addChild(doc, node, "Currency", ccy_);
    XMLUtils::addGenericChild(doc, node, "CalibrationType", calibrationType_);

    XMLNode* sigmaNode = XMLUtils::addChild(doc, node, "Sigma");
    XMLUtils::addChild(doc, sigmaNode, "Calibrate", calibrateSigma_);
    XMLUtils::addChild(doc, sigmaNode, "InitialValue", sigmaValue_);

    XMLNode* kappaNode = XMLUtils::addChild(doc, node, "Kappa");
    XMLUtils::addChild(doc, kappaNode, "Calibrate", calibrateKappa_);
    XMLUtils::addChild(doc, kappaNode, "InitialValue", kappaValue_);

    XMLNode* calibrationOptionsNode = XMLUtils::addChild(doc, node, "CalibrationOptions");
    XMLUtils::addGenericChildAsList(doc, calibrationOptionsNode, "Expiries", optionExpiries_);
    XMLUtils::addGenericChildAsList(doc, calibrationOptionsNode, "Strikes", optionStrikes_);

    XMLUtils::addChild(doc, node, "DriftFreeState", driftFreeState_);

    return node;
}
} // namespace data
} // namespace ore
