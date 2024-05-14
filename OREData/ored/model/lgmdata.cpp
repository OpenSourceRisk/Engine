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
#include <ored/model/modelparameter.hpp>
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

QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping parseFloatSpreadMapping(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "NEXTCOUPON")
        return QuantExt::AnalyticLgmSwaptionEngine::nextCoupon;
    else if (boost::algorithm::to_upper_copy(s) == "PRORATA")
        return QuantExt::AnalyticLgmSwaptionEngine::proRata;
    else if (boost::algorithm::to_upper_copy(s) == "SIMPLE")
        return QuantExt::AnalyticLgmSwaptionEngine::simple;
    else
        QL_FAIL("FloatSpreadMapping '" << s << "' not recognized");
}

std::ostream& operator<<(std::ostream& oss, const QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping& m) {
    if (m == QuantExt::AnalyticLgmSwaptionEngine::nextCoupon)
        oss << "NEXTCOUPON";
    else if (m == QuantExt::AnalyticLgmSwaptionEngine::proRata)
        oss << "PRORATA";
    else if (m == QuantExt::AnalyticLgmSwaptionEngine::simple)
        oss << "SIMPLE";
    else
        QL_FAIL("FloatSpreadMapping type not covered");
    return oss;
}

void LgmData::clear() {
    optionExpiries_.clear();
    optionTerms_.clear();
    optionStrikes_.clear();
}

void LgmData::reset() {
    IrModelData::reset();
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

    if (XMLNode* tranformNode = XMLUtils::getChildNode(node, "ParameterTransformation")) {
        shiftHorizon_ = XMLUtils::getChildValueAsDouble(tranformNode, "ShiftHorizon", true);
        LOG("LGM shift horizon = " << shiftHorizon_);

        scaling_ = XMLUtils::getChildValueAsDouble(tranformNode, "Scaling", true);
        LOG("LGM scaling = " << scaling_);
    } else {
        shiftHorizon_ = 0.0;
        scaling_ = 1.0;
    }

    floatSpreadMapping_ =
        parseFloatSpreadMapping(XMLUtils::getChildValue(node, "FloatSpreadMapping", false, "proRata"));

    IrModelData::fromXML(node);

    LOG("LgmData done");
}

XMLNode* LgmData::toXML(XMLDocument& doc) const {

    XMLNode* lgmNode = IrModelData::toXML(doc);

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

    XMLUtils::addChild(doc, lgmNode, "FloatSpreadMapping", ore::data::to_string(floatSpreadMapping_));

    return lgmNode;
}

ReversionParameter LgmData::reversionParameter() const {
    return ReversionParameter(revType_, calibrateH_, hType_, hTimes_, hValues_);
}

VolatilityParameter LgmData::volatilityParameter() const {
    return VolatilityParameter(volType_, calibrateA_, aType_, aTimes_, aValues_);
}

LgmReversionTransformation::LgmReversionTransformation() : horizon_(0.0), scaling_(1.0) {}

LgmReversionTransformation::LgmReversionTransformation(Time horizon, Real scaling)
    : horizon_(horizon), scaling_(scaling) {}

Time LgmReversionTransformation::horizon() const { return horizon_; }

Real LgmReversionTransformation::scaling() const { return scaling_; }

void LgmReversionTransformation::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ParameterTransformation");
    horizon_ = XMLUtils::getChildValueAsDouble(node, "ShiftHorizon", true);
    scaling_ = XMLUtils::getChildValueAsDouble(node, "Scaling", true);
}

XMLNode* LgmReversionTransformation::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ParameterTransformation");
    XMLUtils::addChild(doc, node, "ShiftHorizon", horizon_);
    XMLUtils::addChild(doc, node, "Scaling", scaling_);
    return node;
}

} // namespace data
} // namespace ore
