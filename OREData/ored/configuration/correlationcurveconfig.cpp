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

std::ostream& operator<<(std::ostream& out, CorrelationCurveConfig::CorrelationType t) {
    switch (t) {
    case CorrelationCurveConfig::CorrelationType::CMSSpread:
        return out << "CMSSpread";
    default:
        QL_FAIL("unknown QuoteType(" << Integer(t) << ")");
    }
}

std::ostream& operator<<(std::ostream& out, CorrelationCurveConfig::QuoteType t) {
    switch (t) {
    case CorrelationCurveConfig::QuoteType::Rate:
        return out << "RATE";
    case CorrelationCurveConfig::QuoteType::Price:
        return out << "PRICE";
    default:
        QL_FAIL("unknown QuoteType(" << Integer(t) << ")");
    }
}

CorrelationCurveConfig::CorrelationCurveConfig(const string& curveID, const string& curveDescription,
                                               const Dimension& dimension, const CorrelationType& corrType,
                                               const string& convention, const QuoteType& quoteType,
                                               const bool extrapolate, const vector<Period>& optionTenors,
                                               const DayCounter& dayCounter, const Calendar& calendar,
                                               const BusinessDayConvention& businessDayConvention, const string& index1,
                                               const string& index2, const string& currency, const string& swaptionVol,
                                               const string& discountCurve)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), correlationType_(corrType),
      conventions_(convention), quoteType_(quoteType), extrapolate_(extrapolate), optionTenors_(optionTenors),
      dayCounter_(dayCounter), calendar_(calendar), businessDayConvention_(businessDayConvention), index1_(index1),
      index2_(index2), currency_(currency), swaptionVol_(swaptionVol), discountCurve_(discountCurve) {

    QL_REQUIRE(dimension == Dimension::ATM || dimension == Dimension::Constant, "Invalid dimension");

    if (dimension == Dimension::Constant) {
        QL_REQUIRE(optionTenors.size() == 1,
                   "Only one tenor should be supplied for a constant correlation termstructure");
    }
}

const vector<string>& CorrelationCurveConfig::quotes() {

    if (quotes_.size() == 0) {

        std::stringstream ssBase;
        ssBase << "CORRELATION/" << quoteType_ << "/" << index1_ << "/" << index2_;
        string base = ssBase.str();

        for (auto o : optionTenors_) {
            std::stringstream ss;
            ss << base << "/" << to_string(o) << "/ATM";
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

    QL_REQUIRE(dim == "ATM" || dim == "Constant", "Dimension " << dim << " not recognised");

    optionTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "OptionTenors", false);
    QL_REQUIRE(optionTenors_.size() > 0, "no option tenors supplied");

    if (optionTenors_.size() == 1) {
        dimension_ = Dimension::Constant;
    } else {
        QL_REQUIRE(dim != "Constant", "Only one tenor should be supplied for a constant correlation termstructure");
        dimension_ = Dimension::ATM;
    }

    string corrType = XMLUtils::getChildValue(node, "CorrelationType", true);
    if (corrType == "CMSSpread") {
        correlationType_ = CorrelationType::CMSSpread;
    } else {
        QL_FAIL("Correlation type " << corrType << " not recognized");
    }

    string quoteType = XMLUtils::getChildValue(node, "QuoteType", true);
    if (quoteType == "Rate") {
        quoteType_ = QuoteType::Rate;
    } else if (quoteType == "Price") {
        quoteType_ = QuoteType::Price;
    } else {
        QL_FAIL("Quote type " << quoteType << " not recognized");
    }

    string extr = XMLUtils::getChildValue(node, "Extrapolation", true);
    extrapolate_ = parseBool(extr);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    businessDayConvention_ = parseBusinessDayConvention(bdc);

    index1_ = XMLUtils::getChildValue(node, "Index1", true);
    index2_ = XMLUtils::getChildValue(node, "Index2", true);

    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    swaptionVol_ = "";

    if (quoteType_ == QuoteType::Price) {
        conventions_ = XMLUtils::getChildValue(node, "Conventions");
        swaptionVol_ = XMLUtils::getChildValue(node, "SwaptionVolatility", true);
        discountCurve_ = XMLUtils::getChildValue(node, "DiscountCurve", true);
    }
}

XMLNode* CorrelationCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Correlation");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (correlationType_ == CorrelationType::CMSSpread) {
        XMLUtils::addChild(doc, node, "CorrelationType", "CMSSpread");
    } else {
        QL_FAIL("Unknown CorrelationType in CorrelationCurveConfig::toXML()");
    }

    XMLUtils::addChild(doc, node, "Index1", index1_);
    XMLUtils::addChild(doc, node, "Index2", index2_);
    XMLUtils::addChild(doc, node, "Conventions", conventions_);

    if (quoteType_ == QuoteType::Price) {
        XMLUtils::addChild(doc, node, "SwaptionVolatility", swaptionVol_);
        XMLUtils::addChild(doc, node, "DiscountCurve", discountCurve_);
    }

    XMLUtils::addChild(doc, node, "Currency", currency_);

    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else if (dimension_ == Dimension::Constant) {
        XMLUtils::addChild(doc, node, "Dimension", "Constant");
    } else {
        QL_FAIL("Unknown Dimension in CorrelationCurveConfig::toXML()");
    }

    if (quoteType_ == QuoteType::Rate) {
        XMLUtils::addChild(doc, node, "QuoteType", "Rate");
    } else if (quoteType_ == QuoteType::Price) {
        XMLUtils::addChild(doc, node, "QuoteType", "Price");
    } else {
        QL_FAIL("Unknown QuoteType in CorrelationCurveConfig::toXML()");
    }
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);

    return node;
}
} // namespace data
} // namespace ore
