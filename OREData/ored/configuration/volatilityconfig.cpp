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
#include <ored/utilities/to_string.hpp>

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

namespace ore {
namespace data {

void VolatilityConfig::fromBaseNode(XMLNode* node) {
    string qType = XMLUtils::getChildValue(node, "QuoteType", false);
    if (qType == "ImpliedVolatility" || qType == "") {
        string volType = XMLUtils::getChildValue(node, "VolatilityType", false);
        if (volType == "Lognormal" || qType == "") {
            quoteType_ = MarketDatum::QuoteType::RATE_LNVOL;
        } else if (volType == "ShiftedLognormal") {
            quoteType_ = MarketDatum::QuoteType::RATE_SLNVOL;
        } else if (volType == "Normal") {
            quoteType_ = MarketDatum::QuoteType::RATE_NVOL;
        } else {
            QL_FAIL("Volatiltiy type " << volType << " is not supported;");
        }
    } else if (qType == "Premium") {
        quoteType_ = MarketDatum::QuoteType::PRICE;
        // If we have premiums the exercise type is required
        exerciseType_ = parseExerciseType(XMLUtils::getChildValue(node, "ExerciseType", true));
    } else {
        QL_FAIL("Invalid quote type for volitility curve , quote type must be ImpliedVolatility or Premium");
    }
}

void VolatilityConfig::addBaseNode(XMLDocument& doc, XMLNode* node) {

    // Check first for premium
    if (quoteType_ == MarketDatum::QuoteType::PRICE) {
        XMLUtils::addChild(doc, node, "QuoteType", "Premium");
        XMLUtils::addChild(doc, node, "ExerciseType", to_string(exerciseType_));
        return;
    }

    // Must be a volatility (or possibly fail)
    XMLUtils::addChild(doc, node, "QuoteType", "ImpliedVolatility");
    if (quoteType_ == MarketDatum::QuoteType::RATE_LNVOL) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
    } else if (quoteType_ == MarketDatum::QuoteType::RATE_SLNVOL) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
    } else if (quoteType_ == MarketDatum::QuoteType::RATE_NVOL) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
    } else {
        QL_FAIL("Invalid quote type");
    }
}

ConstantVolatilityConfig::ConstantVolatilityConfig(MarketDatum::QuoteType quoteType,
                                                   QuantLib::Exercise::Type exerciseType)
    : VolatilityConfig(quoteType, exerciseType) {}

ConstantVolatilityConfig::ConstantVolatilityConfig(const string& quote, MarketDatum::QuoteType quoteType,
                                                   QuantLib::Exercise::Type exerciseType)
    : VolatilityConfig(quoteType, exerciseType), quote_(quote) {}

const string& ConstantVolatilityConfig::quote() const { return quote_; }

void ConstantVolatilityConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Constant");
    VolatilityConfig::fromBaseNode(node);
    quote_ = XMLUtils::getChildValue(node, "Quote", true);
}

XMLNode* ConstantVolatilityConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Constant");
    VolatilityConfig::addBaseNode(doc, node);
    XMLUtils::addChild(doc, node, "Quote", quote_);
    return node;
}

VolatilityCurveConfig::VolatilityCurveConfig(MarketDatum::QuoteType quoteType, QuantLib::Exercise::Type exerciseType,
                                             bool enforceMontoneVariance)
    : VolatilityConfig(quoteType, exerciseType), enforceMontoneVariance_(enforceMontoneVariance) {}

VolatilityCurveConfig::VolatilityCurveConfig(const vector<string>& quotes, const string& interpolation,
                                             const string& extrapolation, MarketDatum::QuoteType quoteType,
                                             QuantLib::Exercise::Type exerciseType, bool enforceMontoneVariance)
    : VolatilityConfig(quoteType, exerciseType), quotes_(quotes), interpolation_(interpolation),
      extrapolation_(extrapolation), enforceMontoneVariance_(enforceMontoneVariance) {}

const vector<string>& VolatilityCurveConfig::quotes() const { return quotes_; }

const string& VolatilityCurveConfig::interpolation() const { return interpolation_; }

const string& VolatilityCurveConfig::extrapolation() const { return extrapolation_; }

bool VolatilityCurveConfig::enforceMontoneVariance() const { return enforceMontoneVariance_; }

void VolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Curve");
    VolatilityConfig::fromBaseNode(node);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    interpolation_ = XMLUtils::getChildValue(node, "Interpolation", true);
    extrapolation_ = XMLUtils::getChildValue(node, "Extrapolation", true);

    enforceMontoneVariance_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "EnforceMontoneVariance"))
        enforceMontoneVariance_ = parseBool(XMLUtils::getNodeValue(n));
}

XMLNode* VolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Curve");
    VolatilityConfig::addBaseNode(doc, node);
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    XMLUtils::addChild(doc, node, "Interpolation", interpolation_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    XMLUtils::addChild(doc, node, "EnforceMontoneVariance", enforceMontoneVariance_);
    return node;
}

VolatilitySurfaceConfig::VolatilitySurfaceConfig(MarketDatum::QuoteType quoteType,
                                                 QuantLib::Exercise::Type exerciseType)
    : VolatilityConfig(quoteType, exerciseType) {}

VolatilitySurfaceConfig::VolatilitySurfaceConfig(const string& timeInterpolation, const string& strikeInterpolation,
                                                 bool extrapolation, const string& timeExtrapolation,
                                                 const string& strikeExtrapolation, MarketDatum::QuoteType quoteType,
                                                 QuantLib::Exercise::Type exerciseType)
    : VolatilityConfig(quoteType, exerciseType), timeInterpolation_(timeInterpolation),
      strikeInterpolation_(strikeInterpolation), extrapolation_(extrapolation), timeExtrapolation_(timeExtrapolation),
      strikeExtrapolation_(strikeExtrapolation) {}

const string& VolatilitySurfaceConfig::timeInterpolation() const { return timeInterpolation_; }

const string& VolatilitySurfaceConfig::strikeInterpolation() const { return strikeInterpolation_; }

bool VolatilitySurfaceConfig::extrapolation() const { return extrapolation_; }

const string& VolatilitySurfaceConfig::timeExtrapolation() const { return timeExtrapolation_; }

const string& VolatilitySurfaceConfig::strikeExtrapolation() const { return strikeExtrapolation_; }

void VolatilitySurfaceConfig::fromNode(XMLNode* node) {
    timeInterpolation_ = XMLUtils::getChildValue(node, "TimeInterpolation", true);
    strikeInterpolation_ = XMLUtils::getChildValue(node, "StrikeInterpolation", true);
    extrapolation_ = parseBool(XMLUtils::getChildValue(node, "Extrapolation", true));
    timeExtrapolation_ = XMLUtils::getChildValue(node, "TimeExtrapolation", true);
    strikeExtrapolation_ = XMLUtils::getChildValue(node, "StrikeExtrapolation", true);
}

void VolatilitySurfaceConfig::addNodes(XMLDocument& doc, XMLNode* node) const {
    XMLUtils::addChild(doc, node, "TimeInterpolation", timeInterpolation_);
    XMLUtils::addChild(doc, node, "StrikeInterpolation", strikeInterpolation_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    XMLUtils::addChild(doc, node, "TimeExtrapolation", timeExtrapolation_);
    XMLUtils::addChild(doc, node, "StrikeExtrapolation", strikeExtrapolation_);
}

VolatilityStrikeSurfaceConfig::VolatilityStrikeSurfaceConfig(MarketDatum::QuoteType quoteType,
                                                             QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(quoteType, exerciseType) {}

VolatilityStrikeSurfaceConfig::VolatilityStrikeSurfaceConfig(
    const vector<string>& strikes, const vector<string>& expiries, const string& timeInterpolation,
    const string& strikeInterpolation, bool extrapolation, const string& timeExtrapolation,
    const string& strikeExtrapolation, MarketDatum::QuoteType quoteType, QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(timeInterpolation, strikeInterpolation, extrapolation, timeExtrapolation,
                              strikeExtrapolation, quoteType, exerciseType),
      strikes_(strikes), expiries_(expiries) {}

const vector<string>& VolatilityStrikeSurfaceConfig::strikes() const { return strikes_; }

const vector<string>& VolatilityStrikeSurfaceConfig::expiries() const { return expiries_; }

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
    VolatilityConfig::fromBaseNode(node);
    strikes_ = XMLUtils::getChildrenValuesAsStrings(node, "Strikes", true);
    expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
    fromNode(node);
}

XMLNode* VolatilityStrikeSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("StrikeSurface");
    VolatilityConfig::addBaseNode(doc, node);
    XMLUtils::addGenericChildAsList(doc, node, "Strikes", strikes_);
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    addNodes(doc, node);
    return node;
}

VolatilityDeltaSurfaceConfig::VolatilityDeltaSurfaceConfig(MarketDatum::QuoteType quoteType,
                                                           QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(quoteType, exerciseType) {}

VolatilityDeltaSurfaceConfig::VolatilityDeltaSurfaceConfig(
    const string& deltaType, const string& atmType, const vector<string>& putDeltas, const vector<string>& callDeltas,
    const vector<string>& expiries, const string& timeInterpolation, const string& strikeInterpolation,
    bool extrapolation, const string& timeExtrapolation, const string& strikeExtrapolation,
    const std::string& atmDeltaType, bool futurePriceCorrection, MarketDatum::QuoteType quoteType,
    QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(timeInterpolation, strikeInterpolation, extrapolation, timeExtrapolation,
                              strikeExtrapolation, quoteType, exerciseType),
      deltaType_(deltaType), atmType_(atmType), putDeltas_(putDeltas), callDeltas_(callDeltas), expiries_(expiries),
      atmDeltaType_(atmDeltaType), futurePriceCorrection_(futurePriceCorrection) {}

const string& VolatilityDeltaSurfaceConfig::deltaType() const { return deltaType_; }

const string& VolatilityDeltaSurfaceConfig::atmType() const { return atmType_; }

const vector<string>& VolatilityDeltaSurfaceConfig::putDeltas() const { return putDeltas_; }

const vector<string>& VolatilityDeltaSurfaceConfig::callDeltas() const { return callDeltas_; }

const vector<string>& VolatilityDeltaSurfaceConfig::expiries() const { return expiries_; }

const string& VolatilityDeltaSurfaceConfig::atmDeltaType() const { return atmDeltaType_; }

bool VolatilityDeltaSurfaceConfig::futurePriceCorrection() const { return futurePriceCorrection_; }

vector<pair<string, string>> VolatilityDeltaSurfaceConfig::quotes() const {

    vector<pair<string, string>> result;

    // ATM strike string
    string atmString = "ATM/" + atmType_;
    if (!atmDeltaType_.empty())
        atmString += "/DEL/" + atmDeltaType_;

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
    VolatilityConfig::fromBaseNode(node);
    deltaType_ = XMLUtils::getChildValue(node, "DeltaType", true);
    atmType_ = XMLUtils::getChildValue(node, "AtmType", true);
    atmDeltaType_ = XMLUtils::getChildValue(node, "AtmDeltaType", false);
    putDeltas_ = XMLUtils::getChildrenValuesAsStrings(node, "PutDeltas", true);
    callDeltas_ = XMLUtils::getChildrenValuesAsStrings(node, "CallDeltas", true);
    expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
    futurePriceCorrection_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "FuturePriceCorrection"))
        futurePriceCorrection_ = parseBool(XMLUtils::getNodeValue(n));
    fromNode(node);
}

XMLNode* VolatilityDeltaSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("DeltaSurface");
    VolatilityConfig::addBaseNode(doc, node);
    XMLUtils::addChild(doc, node, "DeltaType", deltaType_);
    XMLUtils::addChild(doc, node, "AtmType", atmType_);
    if (!atmDeltaType_.empty())
        XMLUtils::addChild(doc, node, "AtmDeltaType", atmDeltaType_);
    XMLUtils::addGenericChildAsList(doc, node, "PutDeltas", putDeltas_);
    XMLUtils::addGenericChildAsList(doc, node, "CallDeltas", callDeltas_);
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    addNodes(doc, node);
    XMLUtils::addChild(doc, node, "FuturePriceCorrection", futurePriceCorrection_);

    return node;
}

VolatilityMoneynessSurfaceConfig::VolatilityMoneynessSurfaceConfig(MarketDatum::QuoteType quoteType,
                                                                   QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(quoteType, exerciseType) {}

VolatilityMoneynessSurfaceConfig::VolatilityMoneynessSurfaceConfig(
    const string& moneynessType, const vector<string>& moneynessLevels, const vector<string>& expiries,
    const string& timeInterpolation, const string& strikeInterpolation, bool extrapolation,
    const string& timeExtrapolation, const string& strikeExtrapolation, bool futurePriceCorrection,
    MarketDatum::QuoteType quoteType, QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(timeInterpolation, strikeInterpolation, extrapolation, timeExtrapolation,
                              strikeExtrapolation, quoteType, exerciseType),
      moneynessType_(moneynessType), moneynessLevels_(moneynessLevels), expiries_(expiries),
      futurePriceCorrection_(futurePriceCorrection) {}

const string& VolatilityMoneynessSurfaceConfig::moneynessType() const { return moneynessType_; }

const vector<string>& VolatilityMoneynessSurfaceConfig::moneynessLevels() const { return moneynessLevels_; }

const vector<string>& VolatilityMoneynessSurfaceConfig::expiries() const { return expiries_; }

bool VolatilityMoneynessSurfaceConfig::futurePriceCorrection() const { return futurePriceCorrection_; }

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
    VolatilityConfig::fromBaseNode(node);
    moneynessType_ = XMLUtils::getChildValue(node, "MoneynessType", true);
    moneynessLevels_ = XMLUtils::getChildrenValuesAsStrings(node, "MoneynessLevels", true);
    expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
    futurePriceCorrection_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "FuturePriceCorrection"))
        futurePriceCorrection_ = parseBool(XMLUtils::getNodeValue(n));
    fromNode(node);
}

XMLNode* VolatilityMoneynessSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("MoneynessSurface");
    VolatilityConfig::addBaseNode(doc, node);
    XMLUtils::addChild(doc, node, "MoneynessType", moneynessType_);
    XMLUtils::addGenericChildAsList(doc, node, "MoneynessLevels", moneynessLevels_);
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    addNodes(doc, node);
    XMLUtils::addChild(doc, node, "FuturePriceCorrection", futurePriceCorrection_);

    return node;
}

VolatilityApoFutureSurfaceConfig::VolatilityApoFutureSurfaceConfig(MarketDatum::QuoteType quoteType,
                                                                   QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(quoteType, exerciseType) {}

VolatilityApoFutureSurfaceConfig::VolatilityApoFutureSurfaceConfig(
    const std::vector<std::string>& moneynessLevels, const std::string& baseVolatilityId,
    const std::string& basePriceCurveId, const std::string& baseConventionsId, const std::string& timeInterpolation,
    const std::string& strikeInterpolation, bool extrapolation, const std::string& timeExtrapolation,
    const std::string& strikeExtrapolation, Real beta, const std::string& maxTenor, MarketDatum::QuoteType quoteType,
    QuantLib::Exercise::Type exerciseType)
    : VolatilitySurfaceConfig(timeInterpolation, strikeInterpolation, extrapolation, timeExtrapolation,
                              strikeExtrapolation, quoteType, exerciseType),
      moneynessLevels_(moneynessLevels), baseVolatilityId_(baseVolatilityId), basePriceCurveId_(basePriceCurveId),
      baseConventionsId_(baseConventionsId), beta_(beta), maxTenor_(maxTenor) {}

const vector<string>& VolatilityApoFutureSurfaceConfig::moneynessLevels() const { return moneynessLevels_; }

const string& VolatilityApoFutureSurfaceConfig::baseVolatilityId() const { return baseVolatilityId_; }

const string& VolatilityApoFutureSurfaceConfig::basePriceCurveId() const { return basePriceCurveId_; }

const string& VolatilityApoFutureSurfaceConfig::baseConventionsId() const { return baseConventionsId_; }

Real VolatilityApoFutureSurfaceConfig::beta() const { return beta_; }

const string& VolatilityApoFutureSurfaceConfig::maxTenor() const { return maxTenor_; }

vector<pair<string, string>> VolatilityApoFutureSurfaceConfig::quotes() const { return vector<pair<string, string>>(); }

void VolatilityApoFutureSurfaceConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ApoFutureSurface");
    VolatilityConfig::fromBaseNode(node);
    moneynessLevels_ = XMLUtils::getChildrenValuesAsStrings(node, "MoneynessLevels", true);
    baseVolatilityId_ = XMLUtils::getChildValue(node, "VolatilityId", true);
    basePriceCurveId_ = XMLUtils::getChildValue(node, "PriceCurveId", true);
    baseConventionsId_ = XMLUtils::getChildValue(node, "FutureConventions", true);
    maxTenor_ = XMLUtils::getChildValue(node, "MaxTenor", false);
    beta_ = XMLUtils::getChildValueAsDouble(node, "Beta", false);
    fromNode(node);
}

XMLNode* VolatilityApoFutureSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ApoFutureSurface");
    VolatilityConfig::addBaseNode(doc, node);
    XMLUtils::addGenericChildAsList(doc, node, "MoneynessLevels", moneynessLevels_);
    XMLUtils::addChild(doc, node, "VolatilityId", baseVolatilityId_);
    XMLUtils::addChild(doc, node, "PriceCurveId", basePriceCurveId_);
    XMLUtils::addChild(doc, node, "FutureConventions", baseConventionsId_);
    addNodes(doc, node);
    if (!maxTenor_.empty())
        XMLUtils::addChild(doc, node, "MaxTenor", maxTenor_);
    XMLUtils::addChild(doc, node, "Beta", beta_);

    return node;
}

} // namespace data
} // namespace ore
