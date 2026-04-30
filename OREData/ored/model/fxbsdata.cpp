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

#include <ored/model/fxbsdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

void FxBsData::fromXML(XMLNode* node) {
    FxData::fromXML(node);

    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    calibrationType_ = parseCalibrationType(calibTypeString);
    DLOG("CC-LGM calibration type = " << calibTypeString);

    XMLNode* sigmaNode = XMLUtils::getChildNode(node, "Sigma");
    calibrateSigma_ = XMLUtils::getChildValueAsBool(sigmaNode, "Calibrate", true);
    DLOG("CC-LGM Sigma calibrate = " << calibrateSigma_);

    std::string sigmaTypeString = XMLUtils::getChildValue(sigmaNode, "ParamType", true);
    sigmaType_ = parseParamType(sigmaTypeString);
    DLOG("CC-LGM Sigma parameter type = " << sigmaTypeString);

    sigmaTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(sigmaNode, "TimeGrid", true);
    DLOG("CC-LGM Sigma time grid size = " << sigmaTimes_.size());

    sigmaValues_ = XMLUtils::getChildrenValuesAsDoublesCompact(sigmaNode, "InitialValue", true);
    DLOG("CC-LGM Sigma initial values size = " << sigmaValues_.size());

    // Add FX Option calibration instruments
    if (XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationOptions")) {
        optionExpiries_ = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", false);
        optionStrikes_ = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);
        if (optionStrikes_.size() > 0) {
            QL_REQUIRE(optionExpiries_.size() == optionStrikes_.size(),
                       "size mismatch in FX option expiries/strike for foreign ccy " << foreignCcy_);
        } else {
            optionStrikes_.resize(optionExpiries_.size(), "ATMF");
        }
    }
}

XMLNode* FxBsData::toXML(XMLDocument& doc) const {

    XMLNode* crossCcyLGMNode = FxData::toXML(doc);

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

QuantLib::ext::shared_ptr<FxData> FxBsData::clone(std::string foreignCcy) const {
    return QuantLib::ext::make_shared<FxBsData>(std::move(foreignCcy), domesticCcy_, calibrationType_,
                                                calibrateSigma_, sigmaType_, sigmaTimes_, sigmaValues_,
                                                optionExpiries_, optionStrikes_);
}

} // namespace data
} // namespace ore
