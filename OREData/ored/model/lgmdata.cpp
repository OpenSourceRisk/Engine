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

#include <ored/model/lgmdata.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

bool LgmData::operator==(const LgmData& rhs) {

    if (qualifier_ != rhs.qualifier_ || calibrationType_ != rhs.calibrationType_ || revType_ != rhs.revType_ ||
        volType_ != rhs.volType_ || calibrateH_ != rhs.calibrateH_ || hType_ != rhs.hType_ || hTimes_ != rhs.hTimes_ ||
        hValues_ != rhs.hValues_ || calibrateA_ != rhs.calibrateA_ || aType_ != rhs.aType_ || aTimes_ != rhs.aTimes_ ||
        aValues_ != rhs.aValues_ || shiftHorizon_ != rhs.shiftHorizon_ || scaling_ != rhs.scaling_ ||
        optionExpiries_ != rhs.optionExpiries_ || optionTerms_ != rhs.optionTerms_ ||
        optionStrikes_ != rhs.optionStrikes_) {
        return false;
    }
    return true;
}

bool LgmData::operator!=(const LgmData& rhs) { return !(*this == rhs); }

std::ostream& operator<<(std::ostream& oss, const ParamType& type) {
    if (type == ParamType::Constant)
        oss << "CONSTANT";
    else if (type == ParamType::Piecewise)
        oss << "PIECEWISE";
    else
        QL_FAIL("Parameter type not covered by <<");
    return oss;
}

ParamType parseParamType(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "CONSTANT")
        return ParamType::Constant;
    else if (boost::algorithm::to_upper_copy(s) == "PIECEWISE")
        return ParamType::Piecewise;
    else
        QL_FAIL("Parameter type " << s << " not recognized");
}

CalibrationType parseCalibrationType(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "BOOTSTRAP")
        return CalibrationType::Bootstrap;
    else if (boost::algorithm::to_upper_copy(s) == "BESTFIT")
        return CalibrationType::BestFit;
    else if (boost::algorithm::to_upper_copy(s) == "NONE")
        return CalibrationType::None;
    else
        QL_FAIL("Calibration type " << s << " not recognized");
}

std::ostream& operator<<(std::ostream& oss, const CalibrationType& type) {
    if (type == CalibrationType::Bootstrap)
        oss << "BOOTSTRAP";
    else if (type == CalibrationType::BestFit)
        oss << "BESTFIT";
    else if (type == CalibrationType::None)
        oss << "NONE";
    else
        QL_FAIL("Calibration type not covered");
    return oss;
}

LgmData::ReversionType parseReversionType(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "HULLWHITE")
        return LgmData::ReversionType::HullWhite;
    else if (boost::algorithm::to_upper_copy(s) == "HAGAN")
        return LgmData::ReversionType::Hagan;
    else
        QL_FAIL("Reversion type " << s << " not recognized");
}

std::ostream& operator<<(std::ostream& oss, const LgmData::ReversionType& type) {
    if (type == LgmData::ReversionType::HullWhite)
        oss << "HULLWHITE";
    else if (type == LgmData::ReversionType::Hagan)
        oss << "HAGAN";
    else
        QL_FAIL("Reversion type not covered");
    return oss;
}

LgmData::VolatilityType parseVolatilityType(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "HULLWHITE")
        return LgmData::VolatilityType::HullWhite;
    else if (boost::algorithm::to_upper_copy(s) == "HAGAN")
        return LgmData::VolatilityType::Hagan;
    else
        QL_FAIL("Volatility type " << s << " not recognized");
}

std::ostream& operator<<(std::ostream& oss, const LgmData::VolatilityType& type) {
    if (type == LgmData::VolatilityType::HullWhite)
        oss << "HULLWHITE";
    else if (type == LgmData::VolatilityType::Hagan)
        oss << "HAGAN";
    else
        QL_FAIL("Volatility type not covered");
    return oss;
}

CalibrationStrategy parseCalibrationStrategy(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "COTERMINALATM")
        return CalibrationStrategy::CoterminalATM;
    else if (boost::algorithm::to_upper_copy(s) == "COTERMINALDEALSTRIKE")
        return CalibrationStrategy::CoterminalDealStrike;
    else if (boost::algorithm::to_upper_copy(s) == "UNDERLYINGATM")
        return CalibrationStrategy::UnderlyingATM;
    else if (boost::algorithm::to_upper_copy(s) == "UNDERLYINGDEALSTRIKE")
        return CalibrationStrategy::UnderlyingDealStrike;
    else if (boost::algorithm::to_upper_copy(s) == "NONE")
        return CalibrationStrategy::None;
    else
        QL_FAIL("Calibration strategy " << s << " not recognized");
}

std::ostream& operator<<(std::ostream& oss, const CalibrationStrategy& type) {
    if (type == CalibrationStrategy::CoterminalATM)
        oss << "COTERMINALATM";
    else if (type == CalibrationStrategy::CoterminalDealStrike)
        oss << "COTERMINALDEALSTRIKE";
    else if (type == CalibrationStrategy::UnderlyingATM)
        oss << "UNDERLYINGATM";
    else if (type == CalibrationStrategy::UnderlyingDealStrike)
        oss << "UNDERLYINGDEALSTRIKE";
    else if (type == CalibrationStrategy::None)
        oss << "NONE";
    else
        QL_FAIL("Calibration strategy not covered");
    return oss;
}

// TODO: use fromXML here, filtering is not done during parsing.
void LgmData::fromFile(const std::string& fileName, const std::string& qualifier) {
    LOG("load model configuration from " << fileName);
    clear();
    XMLDocument doc(fileName);
    XMLNode* root = doc.getFirstNode("Models");
    Size count = 0;
    for (XMLNode* child = XMLUtils::getChildNode(root, "LGM"); child; child = XMLUtils::getNextSibling(child, "LGM")) {
        std::string childCcy = XMLUtils::getAttribute(child, "qualifier");
        if (qualifier == childCcy) {
            fromXML(child);
            count++;
            break;
        }
    }
    QL_REQUIRE(count == 1, "LGM configuration not found for qualifier '" << qualifier << "'");
    LOG("load model configuration from " << fileName << " done.");
}

void LgmData::clear() {

    optionExpiries_.clear();
    optionTerms_.clear();
    optionStrikes_.clear();
}

void LgmData::reset() {
    clear();

    qualifier_ = "";
    calibrationType_ = CalibrationType::Bootstrap;
    revType_ = ReversionType::HullWhite;
    volType_ = VolatilityType::HullWhite;
    calibrateH_ = false;
    hType_ = ParamType::Constant;
    hTimes_ = {};
    hValues_ = {0.03};
    calibrateA_ = false;
    aType_ = ParamType::Constant;
    aTimes_ = {};
    aValues_ = {0.01};
    shiftHorizon_ = 0.0;
    scaling_ = 1.0;
}

void LgmData::fromXML(XMLNode* node) {
    // XMLUtils::checkNode(node, "Models");
    // XMLNode* modelNode = XMLUtils::getChildNode(node, "LGM");

    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    calibrationType_ = parseCalibrationType(calibTypeString);
    LOG("LGM calibration type = " << calibTypeString);

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

XMLNode* LgmData::toXML(XMLDocument& doc) {

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
