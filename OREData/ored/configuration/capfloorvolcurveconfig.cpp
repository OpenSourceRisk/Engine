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

#include <ored/configuration/capfloorvolcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace data {

CapFloorVolatilityCurveConfig::CapFloorVolatilityCurveConfig(
    const string& curveID, const string& curveDescription, const VolatilityType& volatilityType, const bool extrapolate,
    bool inlcudeAtm, const vector<Period>& tenors, const vector<double>& strikes, const DayCounter& dayCounter,
    Natural settleDays, const Calendar& calendar, const BusinessDayConvention& businessDayConvention,
    const string& iborIndex, const string& discountCurve)
    : curveID_(curveID), curveDescription_(curveDescription), volatilityType_(volatilityType),
      extrapolate_(extrapolate), includeAtm_(inlcudeAtm), tenors_(tenors), strikes_(strikes), dayCounter_(dayCounter),
      settleDays_(settleDays), calendar_(calendar), businessDayConvention_(businessDayConvention),
      iborIndex_(iborIndex), discountCurve_(discountCurve) {}

void CapFloorVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CapFloorVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    // We are requiring explicit strikes so there should be at least one strike
    strikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "Strikes", true);
    QL_REQUIRE(!strikes_.empty(), "Strikes node should not be empty");

    // Get the volatility type
    string volType = XMLUtils::getChildValue(node, "VolatilityType", true);
    if (volType == "Normal") {
        volatilityType_ = VolatilityType::Normal;
    } else if (volType == "Lognormal") {
        volatilityType_ = VolatilityType::Lognormal;
    } else if (volType == "ShiftedLognormal") {
        volatilityType_ = VolatilityType::ShiftedLognormal;
    } else {
        QL_FAIL("Volatility type, " << volType << ", not recognized");
    }
    includeAtm_ = XMLUtils::getChildValueAsBool(node, "IncludeAtm", true);
    extrapolate_ = XMLUtils::getChildValueAsBool(node, "Extrapolation", true);
    tenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "Tenors", true);
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
    dayCounter_ = parseDayCounter(XMLUtils::getChildValue(node, "DayCounter", true));
    businessDayConvention_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "BusinessDayConvention", true));

    iborIndex_ = XMLUtils::getChildValue(node, "IborIndex", true);
    discountCurve_ = XMLUtils::getChildValue(node, "DiscountCurve", true);
}

XMLNode* CapFloorVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("CapFloorVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (volatilityType_ == VolatilityType::Normal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
    } else if (volatilityType_ == VolatilityType::Lognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
    } else if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
    } else {
        QL_FAIL("Unknown VolatilityType in CapFloorVolatilityCurveConfig::toXML()");
    }

    XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);
    XMLUtils::addChild(doc, node, "IncludeAtm", includeAtm_);
    XMLUtils::addGenericChildAsList(doc, node, "Tenors", tenors_);
    XMLUtils::addChild(doc, node, "Strikes", strikes_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "IborIndex", iborIndex_);
    XMLUtils::addChild(doc, node, "DiscountCurve", discountCurve_);

    return node;
}
} // namespace data
} // namespace ore
