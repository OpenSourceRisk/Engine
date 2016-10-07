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

#include <ored/configuration/swaptionvolcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

SwaptionVolatilityCurveConfig::SwaptionVolatilityCurveConfig(
    const string& curveID, const string& curveDescription, const Dimension& dimension,
    const VolatilityType& volatilityType, const bool extrapolate, const bool flatExtrapolation,
    const vector<Period>& optionTenors, const vector<Period>& swapTenors, const DayCounter& dayCounter,
    const Calendar& calendar, const BusinessDayConvention& businessDayConvention, const string& shortSwapIndexBase,
    const string& swapIndexBase)
    : curveID_(curveID), curveDescription_(curveDescription), dimension_(dimension), volatilityType_(volatilityType),
      extrapolate_(extrapolate), flatExtrapolation_(flatExtrapolation), optionTenors_(optionTenors),
      swapTenors_(swapTenors), dayCounter_(dayCounter), calendar_(calendar),
      businessDayConvention_(businessDayConvention), shortSwapIndexBase_(shortSwapIndexBase),
      swapIndexBase_(swapIndexBase) {}

void SwaptionVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "SwaptionVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    bool rec = false;

    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    if (dim == "ATM") {
        dimension_ = Dimension::ATM;
        rec = true;
    }
    if (dim == "Smile") {
        dimension_ = Dimension::Smile;
        rec = true;
    }
    QL_REQUIRE(rec, "Dimension " << dim << " not recognized");

    string volType = XMLUtils::getChildValue(node, "VolatilityType", true);
    rec = false;
    if (volType == "Normal") {
        volatilityType_ = VolatilityType::Normal;
        rec = true;
    }
    if (volType == "Lognormal") {
        volatilityType_ = VolatilityType::Lognormal;
        rec = true;
    }
    if (volType == "ShiftedLognormal") {
        volatilityType_ = VolatilityType::ShiftedLognormal;
        rec = true;
    }
    QL_REQUIRE(rec, "Volatility type " << volType << " not recognized");

    string extr = XMLUtils::getChildValue(node, "Extrapolation", true);
    rec = false;
    extrapolate_ = true;
    flatExtrapolation_ = true;
    if (extr == "Linear") {
        flatExtrapolation_ = false;
        rec = true;
    }
    if (extr == "Flat") {
        flatExtrapolation_ = true;
        rec = true;
    }
    if (extr == "None") {
        extrapolate_ = false;
        rec = true;
    }
    QL_REQUIRE(rec, "Extrapolation " << extr << " not recognized");

    optionTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "OptionTenors", true);
    swapTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "SwapTenors", true);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    businessDayConvention_ = parseBusinessDayConvention(bdc);

    shortSwapIndexBase_ = XMLUtils::getChildValue(node, "ShortSwapIndexBase", true);
    swapIndexBase_ = XMLUtils::getChildValue(node, "SwapIndexBase", true);
}

XMLNode* SwaptionVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("SwaptionVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    bool rec = false;

    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
        rec = true;
    }
    QL_REQUIRE(rec, "Unkown Dimension in SwaptionVolatilityCurveConfig::toXML()");

    rec = false;
    if (volatilityType_ == VolatilityType::Normal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
        rec = true;
    }
    if (volatilityType_ == VolatilityType::Lognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
        rec = true;
    }
    if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
        rec = true;
    }
    QL_REQUIRE(rec, "Unknown VolatilityType in SwaptionVolatilityCurveConfig::toXML()");

    string extr_str = flatExtrapolation_ ? "Flat" : "Linear";
    if (!extrapolate_)
        extr_str = "None";
    XMLUtils::addChild(doc, node, "Extrapolation", extr_str);

    XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);
    XMLUtils::addGenericChildAsList(doc, node, "SwapTenors", swapTenors_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "ShortSwapIndexBase", shortSwapIndexBase_);
    XMLUtils::addChild(doc, node, "SwapIndexBase", swapIndexBase_);

    return node;
}
}
}
