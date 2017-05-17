/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/model/eqbsdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

bool EqBsData::operator==(const EqBsData& rhs) {

    if (name_ != rhs.name_ || ccy_ != rhs.ccy_ || calibrationType_ != rhs.calibrationType_ ||
        calibrateSigma_ != rhs.calibrateSigma_ || sigmaType_ != rhs.sigmaType_ || sigmaTimes_ != rhs.sigmaTimes_ ||
        sigmaValues_ != rhs.sigmaValues_ || optionExpiries_ != rhs.optionExpiries_ ||
        optionStrikes_ != rhs.optionStrikes_) {
        return false;
    }
    return true;
}

bool EqBsData::operator!=(const EqBsData& rhs) { return !(*this == rhs); }

void EqBsData::fromXML(XMLNode* node) {
    name_ = XMLUtils::getAttribute(node, "name");
    LOG("Cross-Asset Equity Name = " << name_);

    ccy_ = XMLUtils::getChildValue(node, "Currency", true);
    LOG("Cross-Asset Equity Currency = " << ccy_);

    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    calibrationType_ = parseCalibrationType(calibTypeString);
    LOG("Cross-Asset Equity calibration type = " << calibTypeString);

    XMLNode* sigmaNode = XMLUtils::getChildNode(node, "Sigma");
    calibrateSigma_ = XMLUtils::getChildValueAsBool(sigmaNode, "Calibrate", true);
    LOG("Cross-Asset Equity Sigma calibrate = " << calibrateSigma_);

    std::string sigmaTypeString = XMLUtils::getChildValue(sigmaNode, "ParamType", true);
    sigmaType_ = parseParamType(sigmaTypeString);
    LOG("Cross-Asset Equity Sigma parameter type = " << sigmaTypeString);

    sigmaTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(sigmaNode, "TimeGrid", true);
    LOG("Cross-Asset Equity Sigma time grid size = " << sigmaTimes_.size());

    sigmaValues_ = XMLUtils::getChildrenValuesAsDoublesCompact(sigmaNode, "InitialValue", true);
    LOG("Cross-Asset Equity Sigma initial values size = " << sigmaValues_.size());

    // Add FX Option calibration instruments
    XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationOptions");

    optionExpiries_ = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", true);
    optionStrikes_ = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);

    if (optionStrikes_.size() > 0) {
        QL_REQUIRE(optionExpiries_.size() == optionStrikes_.size(),
                   "size mismatch in equity option expiries/strike for equity name " << name_);
    } else // default ATMF
        optionStrikes_.resize(optionExpiries_.size(), "ATMF");
}

XMLNode* EqBsData::toXML(XMLDocument& doc) {

    XMLNode* crossCcyLGMNode = doc.allocNode("CrossAssetLGM");
    XMLUtils::addAttribute(doc, crossCcyLGMNode, "name", name_);

    XMLUtils::addChild(doc, crossCcyLGMNode, "Currency", ccy_);
    XMLUtils::addGenericChild(doc, crossCcyLGMNode, "CalibrationType", calibrationType_);

    XMLNode* sigmaNode = XMLUtils::addChild(doc, crossCcyLGMNode, "Sigma");
    XMLUtils::addChild(doc, sigmaNode, "Calibrate", calibrateSigma_);
    XMLUtils::addGenericChild(doc, sigmaNode, "ParamType", sigmaType_);
    XMLUtils::addGenericChildAsList(doc, sigmaNode, "TimeGrid", sigmaTimes_);
    XMLUtils::addGenericChildAsList(doc, sigmaNode, "InitialValue", sigmaValues_);

    XMLNode* calibrationOptionsNode = XMLUtils::addChild(doc, crossCcyLGMNode, "CalibrationOptions");
    XMLUtils::addGenericChildAsList(doc, calibrationOptionsNode, "Expiries", optionExpiries_);
    XMLUtils::addGenericChildAsList(doc, calibrationOptionsNode, "Strikes", optionStrikes_);

    return crossCcyLGMNode;
}
} // namespace data
} // namespace ore
