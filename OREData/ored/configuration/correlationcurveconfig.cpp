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
#include <ored/configuration/correlationcurveconfig.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, CorrelationCurveConfig::QuoteType t) {
    switch (t) {
    case CorrelationCurveConfig::QuoteType::Correlation:
        return out << "CORRELATION";
    case CorrelationCurveConfig::QuoteType::Price:
        return out << "PRICE";
    default:
        QL_FAIL("unknown QuoteType(" << Integer(t) << ")");
    }
}

CorrelationCurveConfig::CorrelationCurveConfig(
    const string& curveID, const string& curveDescription, const Dimension& dimension,
    const QuoteType& quoteType, const bool extrapolate,
    const vector<Period>& optionTenors, const DayCounter& dayCounter,
    const Calendar& calendar, const BusinessDayConvention& businessDayConvention, const string& index1,
    const string& index2)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), quoteType_(quoteType),
      extrapolate_(extrapolate), optionTenors_(optionTenors),
      dayCounter_(dayCounter), calendar_(calendar),
      businessDayConvention_(businessDayConvention), index1_(index1),
      index2_(index2) {

    QL_REQUIRE(dimension == Dimension::ATM || dimension == Dimension::Flat, "Invalid dimension");

    if (dimension == Dimension::Flat) {
        QL_REQUIRE(optionTenors.size() == 0 ,
                   "Smile tenors should only be set when dim!=Flat");
    }
}

const vector<string>& CorrelationCurveConfig::quotes() {

    if (quotes_.size() == 0) {

        std::stringstream ssBase;
        ssBase << "SPREAD/" << quoteType_ << "/" << index1_ << "/" << index2_;
        string base = ssBase.str();

        if (dimension_ != Dimension::Flat) {
            for (auto o : optionTenors_) {
                std::stringstream ss;
                ss << base << "/" << to_string(o) << "/ATM";
                quotes_.push_back(ss.str());
            }

        } else {
            std::stringstream ss;
            ss << base << "/FLAT/ATM";
            quotes_.push_back(ss.str());
        }
    }
    return quotes_;
}

void CorrelationCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Correlation");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    if (dim == "ATM") {
        dimension_ = Dimension::ATM;
    } else if (dim == "Flat") {
        dimension_ = Dimension::Flat;
    } else {
        QL_FAIL("Dimension " << dim << " not recognized");
    }

    string quoteType = XMLUtils::getChildValue(node, "QuoteType", true);
    if (quoteType == "Correlation") {
        quoteType_ = QuoteType::Correlation;
    } else if (quoteType == "Price") {
        quoteType_ = QuoteType::Price;
    } else {
        QL_FAIL("Quote type " << quoteType << " not recognized");
    }

    string extr = XMLUtils::getChildValue(node, "Extrapolation", true);
    extrapolate_ = parseBool(extr);

    optionTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "OptionTenors", false);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    businessDayConvention_ = parseBusinessDayConvention(bdc);

    index1_ = XMLUtils::getChildValue(node, "Index1", true);
    index2_ = XMLUtils::getChildValue(node, "Index2", true);
    
}

XMLNode* CorrelationCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Correlation");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
        XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);
    } else if (dimension_ == Dimension::Flat) {
        XMLUtils::addChild(doc, node, "Dimension", "Flat");
    } else {
        QL_FAIL("Unkown Dimension in CorrelationCurveConfig::toXML()");
    }

    if (quoteType_ == QuoteType::Correlation) {
        XMLUtils::addChild(doc, node, "QuoteType", "Correlation");
    } else if (quoteType_ == QuoteType::Price) {
        XMLUtils::addChild(doc, node, "QuoteType", "Price");
    } else {
        QL_FAIL("Unknown QuoteType in CorrelationCurveConfig::toXML()");
    }

    XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);

    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "Index1", index1_);
    XMLUtils::addChild(doc, node, "Index2", index2_);

    return node;
}
} // namespace data
} // namespace ore
