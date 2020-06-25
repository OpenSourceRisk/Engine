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

#include <boost/algorithm/string.hpp>
#include <ored/configuration/genericyieldvolcurveconfig.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, GenericYieldVolatilityCurveConfig::VolatilityType t) {
    switch (t) {
    case GenericYieldVolatilityCurveConfig::VolatilityType::Lognormal:
        return out << "RATE_LNVOL";
    case GenericYieldVolatilityCurveConfig::VolatilityType::Normal:
        return out << "RATE_NVOL";
    case GenericYieldVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
        return out << "RATE_SLNVOL";
    default:
        QL_FAIL("unknown VolatilityType(" << Integer(t) << ")");
    }
}

GenericYieldVolatilityCurveConfig::GenericYieldVolatilityCurveConfig(
    const std::string& underlyingLabel, const std::string& rootNodeLabel, const std::string& marketDatumInstrumentLabel,
    const std::string& qualifierLabel, const string& curveID, const string& curveDescription,
    const std::string& qualifier, const Dimension& dimension, const VolatilityType& volatilityType,
    const bool extrapolate, const bool flatExtrapolation, const vector<string>& optionTenors,
    const vector<string>& underlyingTenors, const DayCounter& dayCounter, const Calendar& calendar,
    const BusinessDayConvention& businessDayConvention, const string& shortSwapIndexBase, const string& swapIndexBase,
    const vector<string>& smileOptionTenors, const vector<string>& smileUnderlyingTenors,
    const vector<string>& smileSpreads)
    : CurveConfig(curveID, curveDescription), underlyingLabel_(underlyingLabel), rootNodeLabel_(rootNodeLabel),
      marketDatumInstrumentLabel_(marketDatumInstrumentLabel), qualifierLabel_(qualifierLabel), allowSmile_(true),
      requireSwapIndexBases_(false), qualifier_(qualifier), dimension_(dimension), volatilityType_(volatilityType),
      extrapolate_(extrapolate), flatExtrapolation_(flatExtrapolation), optionTenors_(optionTenors),
      underlyingTenors_(underlyingTenors), dayCounter_(dayCounter), calendar_(calendar),
      businessDayConvention_(businessDayConvention), shortSwapIndexBase_(shortSwapIndexBase),
      swapIndexBase_(swapIndexBase), smileOptionTenors_(smileOptionTenors),
      smileUnderlyingTenors_(smileUnderlyingTenors), smileSpreads_(smileSpreads) {

    QL_REQUIRE(dimension == Dimension::ATM || dimension == Dimension::Smile, "Invalid dimension");

    if (dimension != Dimension::Smile) {
        QL_REQUIRE(smileOptionTenors.size() == 0 && smileUnderlyingTenors.size() == 0 && smileSpreads.size() == 0,
                   "Smile tenors/strikes/spreads should only be set when dim=Smile");
    }

    if (qualifier_ == "")
        qualifier_ = ccyFromSwapIndexBase();
}

const vector<string>& GenericYieldVolatilityCurveConfig::quotes() {

    if (quotes_.size() == 0) {
        std::stringstream ssBase;
        ssBase << marketDatumInstrumentLabel_ << "/" << volatilityType_ << "/" << qualifier_ << "/";
        string base = ssBase.str();

        // add atm vols (always required)
        for (auto o : optionTenors_) {
            for (auto s : underlyingTenors_) {
                std::stringstream ss;
                ss << base << o << "/" << s << "/ATM";
                quotes_.push_back(ss.str());
            }
        }

        // add Smile Spreads, if dimension is Smile
        if (dimension_ == Dimension::Smile) {
            for (auto o : smileOptionTenors_) {
                for (auto s : smileUnderlyingTenors_) {
                    for (auto sp : smileSpreads_) {
                        std::stringstream ss;
                        ss << base << o << "/" << s << "/Smile/" << sp;
                        quotes_.push_back(ss.str());
                    }
                }
            }
        }
        // add SHIFT quotes, if vol type is SLN
        for (auto s : underlyingTenors_) {
            if (volatilityType_ == VolatilityType::ShiftedLognormal) {
                std::stringstream ss;
                ss << marketDatumInstrumentLabel_ << "/SHIFT/" << qualifier_ << "/" << s;
                quotes_.push_back(ss.str());
            }
        }
    }
    return quotes_;
}

string GenericYieldVolatilityCurveConfig::ccyFromSwapIndexBase() {
    std::vector<string> tokens;
    split(tokens, swapIndexBase_, boost::is_any_of("-"));
    QL_REQUIRE(!tokens.empty() && tokens[0] != "",
               "GenericYieldVolatilityCurveConfig::fromXML(): can not derive qualifier from SwapIndexBase ("
                   << swapIndexBase_ << ")");
    return tokens[0];
}

void GenericYieldVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, rootNodeLabel_);

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    if (allowSmile_) {
        string dim = XMLUtils::getChildValue(node, "Dimension", true);
        if (dim == "ATM") {
            dimension_ = Dimension::ATM;
        } else if (dim == "Smile") {
            dimension_ = Dimension::Smile;
        } else {
            QL_FAIL("Dimension " << dim << " not recognized");
        }
    } else {
        dimension_ = Dimension::ATM;
    }

    string volType = XMLUtils::getChildValue(node, "VolatilityType", true);
    if (volType == "Normal") {
        volatilityType_ = VolatilityType::Normal;
    } else if (volType == "Lognormal") {
        volatilityType_ = VolatilityType::Lognormal;
    } else if (volType == "ShiftedLognormal") {
        volatilityType_ = VolatilityType::ShiftedLognormal;
    } else {
        QL_FAIL("Volatility type " << volType << " not recognized");
    }

    string extr = XMLUtils::getChildValue(node, "Extrapolation", true);
    extrapolate_ = true;
    flatExtrapolation_ = true;
    if (extr == "Linear") {
        flatExtrapolation_ = false;
    } else if (extr == "Flat") {
        flatExtrapolation_ = true;
    } else if (extr == "None") {
        extrapolate_ = false;
    } else {
        QL_FAIL("Extrapolation " << extr << " not recognized");
    }

    optionTenors_ = XMLUtils::getChildrenValuesAsStrings(node, "OptionTenors", true);

    underlyingTenors_ = XMLUtils::getChildrenValuesAsStrings(node, underlyingLabel_ + "Tenors", false);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    businessDayConvention_ = parseBusinessDayConvention(bdc);

    if (requireSwapIndexBases_ || dimension_ == Dimension::Smile) {
        shortSwapIndexBase_ = XMLUtils::getChildValue(node, "ShortSwapIndexBase", true);
        swapIndexBase_ = XMLUtils::getChildValue(node, "SwapIndexBase", true);
    }

    // smile data
    if (dimension_ == Dimension::Smile) {
        smileOptionTenors_ = XMLUtils::getChildrenValuesAsStrings(node, "SmileOptionTenors", true);
        smileUnderlyingTenors_ =
            XMLUtils::getChildrenValuesAsStrings(node, "Smile" + underlyingLabel_ + "Tenors", true);
        smileSpreads_ = XMLUtils::getChildrenValuesAsStrings(node, "SmileSpreads", true);
    }

    // read qualifier from explicit field
    if (qualifierLabel_ != "")
        qualifier_ = XMLUtils::getChildValue(node, qualifierLabel_, true);

    // derive qualifier (=ccy) from swapIndexBase if not given explicitly
    if (qualifier_ == "")
        qualifier_ = ccyFromSwapIndexBase();
}

XMLNode* GenericYieldVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(rootNodeLabel_);
    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    if (qualifierLabel_ != "")
        XMLUtils::addChild(doc, node, qualifierLabel_, qualifier_);

    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else if (dimension_ == Dimension::Smile) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
    } else {
        QL_FAIL("Unkown Dimension in GenericYieldVolatilityCurveConfig::toXML()");
    }

    if (volatilityType_ == VolatilityType::Normal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
    } else if (volatilityType_ == VolatilityType::Lognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
    } else if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
    } else {
        QL_FAIL("Unknown VolatilityType in GenericYieldVolatilityCurveConfig::toXML()");
    }

    string extr_str = flatExtrapolation_ ? "Flat" : "Linear";
    if (!extrapolate_)
        extr_str = "None";
    XMLUtils::addChild(doc, node, "Extrapolation", extr_str);

    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);
    XMLUtils::addGenericChildAsList(doc, node, underlyingLabel_ + "Tenors", underlyingTenors_);

    if (requireSwapIndexBases_ || dimension_ == Dimension::Smile) {
        XMLUtils::addChild(doc, node, "ShortSwapIndexBase", shortSwapIndexBase_);
        XMLUtils::addChild(doc, node, "SwapIndexBase", swapIndexBase_);
    }

    if (dimension_ == Dimension::Smile) {
        XMLUtils::addGenericChildAsList(doc, node, "SmileOptionTenors", smileOptionTenors_);
        XMLUtils::addGenericChildAsList(doc, node, "Smile" + underlyingLabel_ + "Tenors", smileUnderlyingTenors_);
        XMLUtils::addGenericChildAsList(doc, node, "SmileSpreads", smileSpreads_);
    }

    return node;
}

} // namespace data
} // namespace ore
