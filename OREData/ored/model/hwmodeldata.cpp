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

#include <ored/model/hwmodeldata.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

bool HwModelData::operator==(const HwModelData& rhs) {

    if (qualifier_ != rhs.qualifier_ || calibrationType_ != rhs.calibrationType_ || 
        calibrateKappa_ != rhs.calibrateKappa_ || kappaType_ != rhs.kappaType_ || kappaTimes_ != rhs.kappaTimes_ ||
        kappaValues_ != rhs.kappaValues_ || calibrateSigma_ != rhs.calibrateSigma_ || sigmaType_ != rhs.sigmaType_ ||
        sigmaTimes_ != rhs.sigmaTimes_ || sigmaValues_ != rhs.sigmaValues_ || shiftHorizon_ != rhs.shiftHorizon_ ||
        scaling_ != rhs.scaling_) {
        return false;
    }
    return true;
}

bool HwModelData::operator!=(const LgmData& rhs) { return !(*this == rhs); }

void HwModelData::clear() {
}

void HwModelData::reset() {
    clear();
    qualifier_ = "";
    calibrationType_ = CalibrationType::None;
    calibrateKappa_ = false;
    kappaType_ = ParamType::Constant;
    kappaTimes = {};
    kappaValues = {Array(1, 0.01)};
    calibrateSigma_ = false;
    sigmaType_ = ParamType::Constant;
    sigmaTimes_ = {};
    sigmaValues_ = {Matrix(1,1,0.03)};
    shiftHorizon_ = 0.0;
    scaling_ = 1.0;
}

void HwModelData::fromXML(XMLNode* node) {
    // XMLUtils::checkNode(node, "Models");
    // XMLNode* modelNode = XMLUtils::getChildNode(node, "LGM");

    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    calibrationType_ = parseCalibrationType(calibTypeString);
    LOG("HwModel calibration type = " << calibTypeString);


    // Volatility config

    XMLNode* volNode = XMLUtils::getChildNode(node, "Volatility");

    calibrateA_ = XMLUtils::getChildValueAsBool(volNode, "Calibrate", true);
    LOG("LGM Volatility calibrate = " << calibrateA_);

    std::string volTypeString = XMLUtils::getChildValue(volNode, "VolatilityType", true);
    volType_ = parseVolatilityType(volTypeString);
    LOG("LGM Volatility type = " << volTypeString);

    std::string alphaTypeString = XMLUtils::getChildValue(volNode, "ParamType", true);
    aType_ = parseParamType(alphaTypeString);
    LOG("LGM Volatility param type = " << alphaTypeString);

    aTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(volNode, "TimeGrid", true);
    LOG("LGM Volatility time grid size = " << aTimes_.size());

    aValues_ = XMLUtils::getChildrenValuesAsDoublesCompact(volNode, "InitialValue", true);
    LOG("LGM Volatility initial values size = " << aValues_.size());

    // Reversion config

    XMLNode* revNode = XMLUtils::getChildNode(node, "Reversion");

    calibrateH_ = XMLUtils::getChildValueAsBool(revNode, "Calibrate", true);
    LOG("LGM Reversion calibrate = " << calibrateH_);

    std::string revTypeString = XMLUtils::getChildValue(revNode, "ReversionType", true);
    revType_ = parseReversionType(revTypeString);
    LOG("LGM Reversion type = " << revTypeString);

    std::string hTypeString = XMLUtils::getChildValue(revNode, "ParamType", true);
    hType_ = parseParamType(hTypeString);
    LOG("LGM Reversion parameter type = " << hTypeString);

    hTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(revNode, "TimeGrid", true);
    LOG("LGM Reversion time grid size = " << hTimes_.size());

    hValues_ = XMLUtils::getChildrenValuesAsDoublesCompact(revNode, "InitialValue", true);
    LOG("LGM Reversion initial values size = " << hValues_.size());

    // Parameter transformation config

    XMLNode* tranformNode = XMLUtils::getChildNode(node, "ParameterTransformation");
    shiftHorizon_ = XMLUtils::getChildValueAsDouble(tranformNode, "ShiftHorizon", true);
    LOG("LGM shift horizon = " << shiftHorizon_);

    scaling_ = XMLUtils::getChildValueAsDouble(tranformNode, "Scaling", true);
    LOG("LGM scaling = " << scaling_);

    LOG("LgmData done");
}

XMLNode* HwModelData::toXML(XMLDocument& doc) {

    XMLNode* lgmNode = doc.allocNode("LGM");

    XMLUtils::addGenericChild(doc, lgmNode, "CalibrationType", calibrationType_);

    // volatility
    XMLNode* volatilityNode = XMLUtils::addChild(doc, lgmNode, "Volatility");
    XMLUtils::addChild(doc, volatilityNode, "Calibrate", calibrateA_);

    XMLNode* volatilityTypeNode = doc.allocNode("VolatilityType", to_string(volType_));
    XMLUtils::appendNode(volatilityNode, volatilityTypeNode);

    XMLUtils::addGenericChild(doc, volatilityNode, "ParamType", aType_);
    XMLUtils::addGenericChildAsList(doc, volatilityNode, "TimeGrid", aTimes_);
    XMLUtils::addGenericChildAsList(doc, volatilityNode, "InitialValue", aValues_);

    // reversion
    XMLNode* reversionNode = XMLUtils::addChild(doc, lgmNode, "Reversion");
    XMLUtils::addChild(doc, reversionNode, "Calibrate", calibrateH_);

    XMLNode* reversionTypeNode = doc.allocNode("ReversionType", to_string(revType_));
    XMLUtils::appendNode(reversionNode, reversionTypeNode);

    XMLUtils::addGenericChild(doc, reversionNode, "ParamType", hType_);
    XMLUtils::addGenericChildAsList(doc, reversionNode, "TimeGrid", hTimes_);
    XMLUtils::addGenericChildAsList(doc, reversionNode, "InitialValue", hValues_);

    // parameter transformation
    XMLNode* parameterTransformationNode = XMLUtils::addChild(doc, lgmNode, "ParameterTransformation");
    XMLUtils::addChild(doc, parameterTransformationNode, "ShiftHorizon", shiftHorizon_);
    XMLUtils::addChild(doc, parameterTransformationNode, "Scaling", scaling_);

    return lgmNode;
}

} // namespace data
} // namespace ore
