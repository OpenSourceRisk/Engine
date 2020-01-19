/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/configuration/volatilityconfig.hpp>
#include <ored/utilities/parsers.hpp>

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

namespace ore {
namespace data {

ConstantVolatilityConfig::ConstantVolatilityConfig() {}

ConstantVolatilityConfig::ConstantVolatilityConfig(const string& quote) : quote_(quote) {}

const string& ConstantVolatilityConfig::quote() const { return quote_; }

void ConstantVolatilityConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Constant");
    quote_ = XMLUtils::getChildValue(node, "Quote", true);
}

XMLNode* ConstantVolatilityConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Constant");
    XMLUtils::addChild(doc, node, "Quote", quote_);
    return node;
}

VolatilityCurveConfig::VolatilityCurveConfig() {}

VolatilityCurveConfig::VolatilityCurveConfig(const vector<string>& quotes,
    const string& interpolation,
    const string& extrapolation)
    : quotes_(quotes),
      interpolation_(interpolation),
      extrapolation_(extrapolation) {}

const vector<string>& VolatilityCurveConfig::quotes() const { return quotes_; }

const string& VolatilityCurveConfig::interpolation() const { return interpolation_; }

const string& VolatilityCurveConfig::extrapolation() const { return extrapolation_; }

void VolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Curve");
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    interpolation_ = XMLUtils::getChildValue(node, "Interpolation", true);
    extrapolation_ = XMLUtils::getChildValue(node, "Extrapolation", true);
}

XMLNode* VolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Curve");
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    XMLUtils::addChild(doc, node, "Interpolation", interpolation_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    return node;
}

VolatilitySurfaceConfig::VolatilitySurfaceConfig() {}

VolatilitySurfaceConfig::VolatilitySurfaceConfig(
    const vector<string>& expiries,
    const string& timeInterpolation,
    const string& strikeInterpolation,
    bool extrapolation,
    const string& timeExtrapolation,
    const string& strikeExtrapolation)
    : expiries_(expiries),
      timeInterpolation_(timeInterpolation),
      strikeInterpolation_(strikeInterpolation),
      extrapolation_(extrapolation),
      timeExtrapolation_(timeExtrapolation),
      strikeExtrapolation_(strikeExtrapolation) {}

const vector<string>& VolatilitySurfaceConfig::expiries() const { return expiries_; }

const string& VolatilitySurfaceConfig::timeInterpolation() const { return timeInterpolation_; }

const string& VolatilitySurfaceConfig::strikeInterpolation() const { return strikeInterpolation_; }

bool VolatilitySurfaceConfig::extrapolation() const { return extrapolation_; }

const string& VolatilitySurfaceConfig::timeExtrapolation() const { return timeExtrapolation_; }

const string& VolatilitySurfaceConfig::strikeExtrapolation() const { return strikeExtrapolation_; }

void VolatilitySurfaceConfig::fromNode(XMLNode* node) {
    expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
    timeInterpolation_ = XMLUtils::getChildValue(node, "TimeInterpolation", true);
    strikeInterpolation_ = XMLUtils::getChildValue(node, "StrikeInterpolation", true);
    extrapolation_ = parseBool(XMLUtils::getChildValue(node, "Extrapolation", true));
    timeExtrapolation_ = XMLUtils::getChildValue(node, "TimeExtrapolation", true);
    strikeExtrapolation_ = XMLUtils::getChildValue(node, "StrikeExtrapolation", true);
}

void VolatilitySurfaceConfig::addNodes(XMLDocument& doc, XMLNode* node) const {
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    XMLUtils::addChild(doc, node, "TimeInterpolation", timeInterpolation_);
    XMLUtils::addChild(doc, node, "StrikeInterpolation", strikeInterpolation_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    XMLUtils::addChild(doc, node, "TimeExtrapolation", timeExtrapolation_);
    XMLUtils::addChild(doc, node, "StrikeExtrapolation", strikeExtrapolation_);
}

VolatilityStrikeSurfaceConfig::VolatilityStrikeSurfaceConfig() {}

VolatilityStrikeSurfaceConfig::VolatilityStrikeSurfaceConfig(
    const vector<string>& strikes,
    const vector<string>& expiries,
    const string& timeInterpolation,
    const string& strikeInterpolation,
    bool extrapolation,
    const string& timeExtrapolation,
    const string& strikeExtrapolation)
    : VolatilitySurfaceConfig(expiries, timeInterpolation, strikeInterpolation, extrapolation,
        timeExtrapolation, strikeExtrapolation), strikes_(strikes) {}

const vector<string>& VolatilityStrikeSurfaceConfig::strikes() const { return strikes_; }

vector<pair<string, string>> VolatilityStrikeSurfaceConfig::quotes() const {
    
    vector<pair<string, string>> result;
    
    for (const string& e : expiries()) {
        for (const string& s : strikes_) {
            result.push_back(make_pair(e, s));
        }
    }

    return result;
}

void VolatilityStrikeSurfaceConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "StrikeSurface");
    strikes_ = XMLUtils::getChildrenValuesAsStrings(node, "Strikes", true);
    fromNode(node);
}

XMLNode* VolatilityStrikeSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("StrikeSurface");
    XMLUtils::addGenericChildAsList(doc, node, "Strikes", strikes_);
    addNodes(doc, node);
    return node;
}

VolatilityDeltaSurfaceConfig::VolatilityDeltaSurfaceConfig() {}

VolatilityDeltaSurfaceConfig::VolatilityDeltaSurfaceConfig(
    const string& deltaType,
    const string& atmType,
    const vector<string>& putDeltas,
    const vector<string>& callDeltas,
    const vector<string>& expiries,
    const string& timeInterpolation,
    const string& strikeInterpolation,
    bool extrapolation,
    const string& timeExtrapolation,
    const string& strikeExtrapolation,
    const std::string& atmDeltaType,
    bool futurePriceCorrection)
    : VolatilitySurfaceConfig(expiries, timeInterpolation, strikeInterpolation, extrapolation,
        timeExtrapolation, strikeExtrapolation), deltaType_(deltaType), atmType_(atmType),
        putDeltas_(putDeltas), callDeltas_(callDeltas), atmDeltaType_(atmDeltaType),
        futurePriceCorrection_(futurePriceCorrection) {
}

const string& VolatilityDeltaSurfaceConfig::deltaType() const { return deltaType_; }

const string& VolatilityDeltaSurfaceConfig::atmType() const { return atmType_; }

const vector<string>& VolatilityDeltaSurfaceConfig::putDeltas() const { return putDeltas_; }

const vector<string>& VolatilityDeltaSurfaceConfig::callDeltas() const { return callDeltas_; }

const string& VolatilityDeltaSurfaceConfig::atmDeltaType() const { return atmDeltaType_; }

bool VolatilityDeltaSurfaceConfig::futurePriceCorrection() const { return futurePriceCorrection_; }

vector<pair<string, string>> VolatilityDeltaSurfaceConfig::quotes() const {

    vector<pair<string, string>> result;

    // ATM strike string
    string atmString = "ATM/" + atmType_;
    if (!atmDeltaType_.empty())
        atmString += "/" + atmDeltaType_;

    // Delta stem
    string stem = "DEL/" + deltaType_ + "/";

    for (const string& e : expiries()) {
        result.push_back(make_pair(e, atmString));
        for (const string& d : putDeltas_) {
            result.push_back(make_pair(e, stem + "Put/" + d));
        }
        for (const string& d : callDeltas_) {
            result.push_back(make_pair(e, stem + "Call/" + d));
        }
    }

    return result;
}

void VolatilityDeltaSurfaceConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "DeltaSurface");
    deltaType_ = XMLUtils::getChildValue(node, "DeltaType", true);
    atmType_ = XMLUtils::getChildValue(node, "AtmType", true);
    atmDeltaType_ = XMLUtils::getChildValue(node, "AtmDeltaType", false);
    putDeltas_ = XMLUtils::getChildrenValuesAsStrings(node, "PutDeltas", true);
    callDeltas_ = XMLUtils::getChildrenValuesAsStrings(node, "CallDeltas", true);
    futurePriceCorrection_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "FuturePriceCorrection"))
        futurePriceCorrection_ = parseBool(XMLUtils::getNodeValue(n));
    fromNode(node);
}

XMLNode* VolatilityDeltaSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("DeltaSurface");
    XMLUtils::addChild(doc, node, "DeltaType", deltaType_);
    XMLUtils::addChild(doc, node, "AtmType", atmType_);
    if (!atmDeltaType_.empty())
        XMLUtils::addChild(doc, node, "AtmDeltaType", atmDeltaType_);
    XMLUtils::addGenericChildAsList(doc, node, "PutDeltas", putDeltas_);
    XMLUtils::addGenericChildAsList(doc, node, "CallDeltas", callDeltas_);
    addNodes(doc, node);
    XMLUtils::addChild(doc, node, "FuturePriceCorrection", futurePriceCorrection_);
    return node;
}

VolatilityMoneynessSurfaceConfig::VolatilityMoneynessSurfaceConfig() {}

VolatilityMoneynessSurfaceConfig::VolatilityMoneynessSurfaceConfig(
    const string& moneynessType,
    const vector<string>& moneynessLevels,
    const vector<string>& expiries,
    const string& timeInterpolation,
    const string& strikeInterpolation,
    bool extrapolation,
    const string& timeExtrapolation,
    const string& strikeExtrapolation)
    : VolatilitySurfaceConfig(expiries, timeInterpolation, strikeInterpolation, extrapolation,
        timeExtrapolation, strikeExtrapolation), moneynessType_(moneynessType), moneynessLevels_(moneynessLevels) {
}

const string& VolatilityMoneynessSurfaceConfig::moneynessType() const { return moneynessType_; }

const vector<string>& VolatilityMoneynessSurfaceConfig::moneynessLevels() const { return moneynessLevels_; }

vector<pair<string, string>> VolatilityMoneynessSurfaceConfig::quotes() const {

    vector<pair<string, string>> result;

    // Moneyness stem
    string stem = "MNY/" + moneynessType_ + "/";

    for (const string& e : expiries()) {
        for (const string& m : moneynessLevels_) {
            result.push_back(make_pair(e, stem + m));
        }
    }

    return result;
}

void VolatilityMoneynessSurfaceConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "MoneynessSurface");
    moneynessType_ = XMLUtils::getChildValue(node, "MoneynessType", true);
    moneynessLevels_ = XMLUtils::getChildrenValuesAsStrings(node, "MoneynessLevels", true);
    fromNode(node);
}

XMLNode* VolatilityMoneynessSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("MoneynessSurface");
    XMLUtils::addChild(doc, node, "MoneynessType", moneynessType_);
    XMLUtils::addGenericChildAsList(doc, node, "MoneynessLevels", moneynessLevels_);
    addNodes(doc, node);
    return node;
}

}
}
