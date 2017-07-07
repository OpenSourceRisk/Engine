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

#include <ored/configuration/inflationcapfloorpricesurfaceconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

#include <iomanip>

using namespace ore::data;

namespace ore {
namespace data {

InflationCapFloorPriceSurfaceConfig::InflationCapFloorPriceSurfaceConfig(
    const string& curveID, const string& curveDescription, const Type type, const Period& observationLag,
    const Calendar& calendar, const BusinessDayConvention& businessDayConvention, const DayCounter& dayCounter,
    const string& index, const string& indexCurve, const string& yieldTermStructure, const vector<Real>& capStrikes,
    const vector<Real>& floorStrikes, const vector<Period>& maturities)
    : CurveConfig(curveID, curveDescription), type_(type), observationLag_(observationLag),
      calendar_(calendar), businessDayConvention_(businessDayConvention), dayCounter_(dayCounter), index_(index),
      indexCurve_(indexCurve), yieldTermStructure_(yieldTermStructure), capStrikes_(capStrikes),
      floorStrikes_(floorStrikes), maturities_(maturities) {}

void InflationCapFloorPriceSurfaceConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "InflationCapFloorPriceSurface");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    string type = XMLUtils::getChildValue(node, "Type", true);
    if (type == "ZC") {
        type_ = Type::ZC;
    } else if (type == "YY") {
        type_ = Type::YY;
    } else
        QL_FAIL("Type " << type << " not recognized");

    string startRt = XMLUtils::getChildValue(node, "StartRate", true);
    startRate_ = parseReal(startRt);

    string lag = XMLUtils::getChildValue(node, "ObservationLag", true);
    observationLag_ = parsePeriod(lag);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    businessDayConvention_ = parseBusinessDayConvention(bdc);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    index_ = XMLUtils::getChildValue(node, "Index", true);
    indexCurve_ = XMLUtils::getChildValue(node, "IndexCurve", true);

    yieldTermStructure_ = XMLUtils::getChildValue(node, "YieldTermStructure", true);

    capStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "CapStrikes", true);
    floorStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "FloorStrikes", true);
    maturities_ = XMLUtils::getChildrenValuesAsPeriods(node, "Maturities", true);
}

XMLNode* InflationCapFloorPriceSurfaceConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("SwaptionVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (type_ == Type::ZC) {
        XMLUtils::addChild(doc, node, "Type", "ZC");
    } else if (type_ == Type::YY) {
        XMLUtils::addChild(doc, node, "Type", "YY");
    } else
        QL_FAIL("Unknown Type in InflationCapFloorPriceSurfaceConfig::toXML()");

    std::ostringstream startRt, lag, cal, bdc, dc, idx;
    startRt << std::fixed << std::setprecision(16) << startRate_;
    lag << observationLag_;
    cal << calendar_;
    bdc << businessDayConvention_;
    dc << dayCounter_;

    XMLUtils::addChild(doc, node, "StartRate", startRt.str());
    XMLUtils::addChild(doc, node, "ObservationLag", lag.str());
    XMLUtils::addChild(doc, node, "Calendar", cal.str());
    XMLUtils::addChild(doc, node, "BusinessDayConvention", bdc.str());
    XMLUtils::addChild(doc, node, "DayCounter", dc.str());

    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "IndexCurve", indexCurve_);
    XMLUtils::addChild(doc, node, "YieldTermStructure", yieldTermStructure_);
    XMLUtils::addChild(doc, node, "CapStrikes", capStrikes_);
    XMLUtils::addChild(doc, node, "FloorStrikes", floorStrikes_);
    XMLUtils::addGenericChildAsList(doc, node, "Maturities", maturities_);

    return node;
}
} // namespace data
} // namespace ore
