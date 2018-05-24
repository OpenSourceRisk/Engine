/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
#include <ored/configuration/inflationcapfloorvolcurveconfig.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, InflationCapFloorVolatilityCurveConfig::VolatilityType t) {
    switch (t) {
    case InflationCapFloorVolatilityCurveConfig::VolatilityType::Lognormal:
        return out << "RATE_LNVOL";
    case InflationCapFloorVolatilityCurveConfig::VolatilityType::Normal:
        return out << "RATE_NVOL";
    case InflationCapFloorVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
        return out << "RATE_SLNVOL";
    default:
        QL_FAIL("unknown VolatilityType(" << Integer(t) << ")");
    }
}

InflationCapFloorVolatilityCurveConfig::InflationCapFloorVolatilityCurveConfig(
    const string& curveID, const string& curveDescription, const Type type, const VolatilityType& volatilityType, 
    const bool extrapolate, const vector<Period>& tenors, const vector<double>& strikes, const DayCounter& dayCounter,
    Natural settleDays, const Calendar& calendar, const BusinessDayConvention& businessDayConvention,
    const string& index, const string& indexCurve, const string& yieldTermStructure)
    : CurveConfig(curveID, curveDescription), type_(type), volatilityType_(volatilityType), extrapolate_(extrapolate),
    tenors_(tenors), strikes_(strikes), dayCounter_(dayCounter), settleDays_(settleDays),
    calendar_(calendar), businessDayConvention_(businessDayConvention), index_(index), indexCurve_(indexCurve),
    yieldTermStructure_(yieldTermStructure) {}

const vector<string>& InflationCapFloorVolatilityCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        boost::shared_ptr<IborIndex> index = parseIborIndex(index_);
        Currency ccy = index->currency();
        Period tenor = index->tenor();

        string type;
        if (type_ == Type::ZC)
            type = "ZC";
        else if (type_ == Type::YY)
            type = "YY";

        std::stringstream ssBase;
        ssBase << type << "_INFLATIONCAPFLOOR/" << volatilityType_ << "/" << index_ << "/";
        string base = ssBase.str();

        // TODO: how to tell if atmFlag or relative flag should be true
        for (auto t : tenors_) {
            for (auto s : strikes_) {
                quotes_.push_back(base + to_string(t) + "/F/" + to_string(s));
            }
        }

        if (volatilityType_ == VolatilityType::ShiftedLognormal) {
            for (auto t : tenors_) {
                std::stringstream ss;
                quotes_.push_back(type + "_INFLATIONCAPFLOOR/SHIFT/" + index_ + "/" + to_string(t));
            }
        }
    }
    return quotes_;
}

void InflationCapFloorVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "InflationCapFloorVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    string type = XMLUtils::getChildValue(node, "Type", true);
    if (type == "ZC") {
        type_ = Type::ZC;
    }
    else if (type == "YY") {
        type_ = Type::YY;
    }
    else
        QL_FAIL("Type " << type << " not recognized");

    // We are requiring explicit strikes so there should be at least one strike
    strikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "Strikes", true);
    QL_REQUIRE(!strikes_.empty(), "Strikes node should not be empty");

    // Get the volatility type
    string volType = XMLUtils::getChildValue(node, "VolatilityType", true);
    if (volType == "Normal") {
        volatilityType_ = VolatilityType::Normal;
    }
    else if (volType == "Lognormal") {
        volatilityType_ = VolatilityType::Lognormal;
    }
    else if (volType == "ShiftedLognormal") {
        volatilityType_ = VolatilityType::ShiftedLognormal;
    }
    else {
        QL_FAIL("Volatility type, " << volType << ", not recognized");
    }
    extrapolate_ = XMLUtils::getChildValueAsBool(node, "Extrapolation", true);
    tenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "Tenors", true);
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
    dayCounter_ = parseDayCounter(XMLUtils::getChildValue(node, "DayCounter", true));
    businessDayConvention_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "BusinessDayConvention", true));

    index_ = XMLUtils::getChildValue(node, "Index", true);
    indexCurve_ = XMLUtils::getChildValue(node, "IndexCurve", true);
    yieldTermStructure_ = XMLUtils::getChildValue(node, "YieldTermStructure", true);
}

XMLNode* InflationCapFloorVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("CapFloorVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (type_ == Type::ZC) {
        XMLUtils::addChild(doc, node, "Type", "ZC");
    }
    else if (type_ == Type::YY) {
        XMLUtils::addChild(doc, node, "Type", "YY");
    }
    else
        QL_FAIL("Unknown Type in InflationCapFloorPriceSurfaceConfig::toXML()");

    if (volatilityType_ == VolatilityType::Normal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
    }
    else if (volatilityType_ == VolatilityType::Lognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
    }
    else if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
    }
    else {
        QL_FAIL("Unknown VolatilityType in InflationCapFloorVolatilityCurveConfig::toXML()");
    }

    XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);
    XMLUtils::addGenericChildAsList(doc, node, "Tenors", tenors_);
    XMLUtils::addChild(doc, node, "Strikes", strikes_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "IndexCurve", indexCurve_);
    XMLUtils::addChild(doc, node, "YieldTermStructure", yieldTermStructure_);

    return node;
}
} // namespace data
} // namespace ore
