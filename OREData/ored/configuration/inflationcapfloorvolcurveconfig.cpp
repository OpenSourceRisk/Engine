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

#include <ored/configuration/inflationcapfloorvolcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>

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

std::ostream& operator<<(std::ostream& out, InflationCapFloorVolatilityCurveConfig::QuoteType t) {
    switch (t) {
    case InflationCapFloorVolatilityCurveConfig::QuoteType::Price:
        return out << "PRICE";
    case InflationCapFloorVolatilityCurveConfig::QuoteType::Volatility:
        return out << "VOLATILITY";
    default:
        QL_FAIL("unknown QuoteType(" << Integer(t) << ")");
    }
}

InflationCapFloorVolatilityCurveConfig::InflationCapFloorVolatilityCurveConfig(
    const string& curveID, const string& curveDescription, const Type type, const QuoteType& quoteType,
    const VolatilityType& volatilityType, const bool extrapolate, const vector<string>& tenors,
    const vector<string>& capStrikes, const vector<string>& floorStrikes, const vector<string>& strikes,
    const DayCounter& dayCounter, Natural settleDays, const Calendar& calendar,
    const BusinessDayConvention& businessDayConvention, const string& index, const string& indexCurve,
    const string& yieldTermStructure, const Period& observationLag, const string& quoteIndex, const string& conventions,
    const bool useLastAvailableFixingDate)
    : CurveConfig(curveID, curveDescription), type_(type), quoteType_(quoteType), volatilityType_(volatilityType),
      extrapolate_(extrapolate), tenors_(tenors), capStrikes_(capStrikes), floorStrikes_(floorStrikes),
      strikes_(strikes), dayCounter_(dayCounter), settleDays_(settleDays), calendar_(calendar),
      businessDayConvention_(businessDayConvention), index_(index), indexCurve_(indexCurve),
      yieldTermStructure_(yieldTermStructure), observationLag_(observationLag), quoteIndex_(quoteIndex),
      conventions_(conventions), useLastAvailableFixingDate_(useLastAvailableFixingDate) {
    populateRequiredCurveIds();
}

void InflationCapFloorVolatilityCurveConfig::populateRequiredCurveIds() {
    if (!yieldTermStructure().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(yieldTermStructure())->curveConfigID());
    if (!indexCurve().empty())
        requiredCurveIds_[CurveSpec::CurveType::Inflation].insert(parseCurveSpec(indexCurve())->curveConfigID());
}

const vector<string>& InflationCapFloorVolatilityCurveConfig::quotes() {
    if (quotes_.size() == 0) {

        string type;
        if (type_ == Type::ZC)
            type = "ZC";
        else if (type_ == Type::YY)
            type = "YY";

        // Determine the index string to use for the quotes.
        string index = quoteIndex_.empty() ? index_ : quoteIndex_;

        std::stringstream ssBase;
        if (quoteType_ == QuoteType::Price)
            ssBase << type << "_INFLATIONCAPFLOOR/PRICE/" << index << "/";
        else
            ssBase << type << "_INFLATIONCAPFLOOR/" << volatilityType_ << "/" << index << "/";
        string base = ssBase.str();

        // TODO: how to tell if atmFlag or relative flag should be true
        for (auto t : tenors_) {
            if (quoteType_ == QuoteType::Price) {
                for (auto s : capStrikes_) {
                    quotes_.push_back(base + t + "/C/" + s);
                }
                for (auto s : floorStrikes_) {
                    quotes_.push_back(base + t + "/F/" + s);
                }
            } else {
                for (auto s : strikes_) {
                    quotes_.push_back(base + t + "/F/" + s);
                }
            }
        }

        if (volatilityType_ == VolatilityType::ShiftedLognormal) {
            for (auto t : tenors_) {
                std::stringstream ss;
                quotes_.push_back(type + "_INFLATIONCAPFLOOR/SHIFT/" + index + "/" + t);
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
    } else if (type == "YY") {
        type_ = Type::YY;
    } else
        QL_FAIL("Type " << type << " not recognized");

    // Get the quote type
    string quoteType = XMLUtils::getChildValue(node, "QuoteType", true);
    if (quoteType == "Price") {
        quoteType_ = QuoteType::Price;
    } else if (quoteType == "Volatility") {
        quoteType_ = QuoteType::Volatility;
    } else {
        QL_FAIL("Quote type, " << quoteType << ", not recognized");
    }

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
    extrapolate_ = XMLUtils::getChildValueAsBool(node, "Extrapolation", true);
    tenors_ = XMLUtils::getChildrenValuesAsStrings(node, "Tenors", true);

    // We are requiring explicit strikes so there should be at least one strike
    if (quoteType_ == QuoteType::Price) {
        capStrikes_ = XMLUtils::getChildrenValuesAsStrings(node, "CapStrikes", true);
        floorStrikes_ = XMLUtils::getChildrenValuesAsStrings(node, "FloorStrikes", true);
        QL_REQUIRE(!capStrikes_.empty() || !floorStrikes_.empty(),
                   "CapStrikes or FloorStrikes node should not be empty");
        // Set strikes to the sorted union of cap and floor strikes
        std::set<Real> strikeSet;
        for (Size i = 0; i < capStrikes_.size(); ++i)
            strikeSet.insert(parseReal(capStrikes_[i]));
        for (Size i = 0; i < floorStrikes_.size(); ++i)
            strikeSet.insert(parseReal(floorStrikes_[i]));
        strikes_.clear();
        for (auto s : strikeSet)
            strikes_.push_back(to_string(s));
        for (Size i = 0; i < strikes_.size(); ++i)
            DLOG("ZC Inflation Cap/Floor Strike " << i << " = " << strikes_[i]);
    } else {
        strikes_ = XMLUtils::getChildrenValuesAsStrings(node, "Strikes", true);
        QL_REQUIRE(!strikes_.empty(), "Strikes node should not be empty");
    }
    settleDays_ = 0; // optional
    if (XMLNode* n = XMLUtils::getChildNode(node, "SettlementDays")) {
        Integer d = parseInteger(XMLUtils::getNodeValue(n));
        QL_REQUIRE(d >= 0, "SettlementDays (" << d << ") must be non-negative");
        settleDays_ = static_cast<Natural>(d);
    }
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
    dayCounter_ = parseDayCounter(XMLUtils::getChildValue(node, "DayCounter", true));
    businessDayConvention_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "BusinessDayConvention", true));
    index_ = XMLUtils::getChildValue(node, "Index", true);
    indexCurve_ = XMLUtils::getChildValue(node, "IndexCurve", true);
    yieldTermStructure_ = XMLUtils::getChildValue(node, "YieldTermStructure", true);
    observationLag_ = parsePeriod(XMLUtils::getChildValue(node, "ObservationLag", true));
    quoteIndex_ = XMLUtils::getChildValue(node, "QuoteIndex", false);
    conventions_ = XMLUtils::getChildValue(node, "Conventions", false, "");
    useLastAvailableFixingDate_ =
        XMLUtils::getChildValueAsBool(node, "UseLastFixingDate", false, false);
    populateRequiredCurveIds();
}

XMLNode* InflationCapFloorVolatilityCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("InflationCapFloorVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (type_ == Type::ZC) {
        XMLUtils::addChild(doc, node, "Type", "ZC");
    } else if (type_ == Type::YY) {
        XMLUtils::addChild(doc, node, "Type", "YY");
    } else
        QL_FAIL("Unknown Type in InflationCapFloorVolatilityCurveConfig::toXML()");

    if (quoteType_ == QuoteType::Price) {
        XMLUtils::addChild(doc, node, "QuoteType", "Price");
    } else if (quoteType_ == QuoteType::Volatility) {
        XMLUtils::addChild(doc, node, "QuoteType", "Volatility");
    } else {
        QL_FAIL("Unknown QuoteType in InflationCapFloorVolatilityCurveConfig::toXML()");
    }

    if (volatilityType_ == VolatilityType::Normal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
    } else if (volatilityType_ == VolatilityType::Lognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
    } else if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
    } else {
        QL_FAIL("Unknown VolatilityType in InflationCapFloorVolatilityCurveConfig::toXML()");
    }

    XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);
    XMLUtils::addGenericChildAsList(doc, node, "Tenors", tenors_);
    XMLUtils::addChild(doc, node, "SettlementDays", static_cast<int>(settleDays_));
    XMLUtils::addGenericChildAsList(doc, node, "CapStrikes", capStrikes_);
    XMLUtils::addGenericChildAsList(doc, node, "FloorStrikes", floorStrikes_);
    XMLUtils::addGenericChildAsList(doc, node, "Strikes", strikes_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "IndexCurve", indexCurve_);
    XMLUtils::addChild(doc, node, "ObservationLag", to_string(observationLag_));
    XMLUtils::addChild(doc, node, "YieldTermStructure", yieldTermStructure_);
    if (!quoteIndex_.empty())
        XMLUtils::addChild(doc, node, "QuoteIndex", quoteIndex_);
    if (!conventions_.empty())
        XMLUtils::addChild(doc, node, "Conventions", conventions_);
    if (useLastAvailableFixingDate_)
        XMLUtils::addChild(doc, node, "UseLastFixingDate", useLastAvailableFixingDate_);
    return node;
}
} // namespace data
} // namespace ore
