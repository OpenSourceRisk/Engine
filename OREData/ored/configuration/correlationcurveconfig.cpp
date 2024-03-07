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
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, CorrelationCurveConfig::CorrelationType t) {
    switch (t) {
    case CorrelationCurveConfig::CorrelationType::CMSSpread:
        return out << "CMSSpread";
    case CorrelationCurveConfig::CorrelationType::Generic:
        return out << "Generic";
    default:
        QL_FAIL("unknown QuoteType(" << Integer(t) << ")");
    }
}
std::ostream& operator<<(std::ostream& out, CorrelationCurveConfig::Dimension t) {
    switch (t) {
    case CorrelationCurveConfig::Dimension::ATM:
        return out << "ATM";
    case CorrelationCurveConfig::Dimension::Constant:
        return out << "Constant";
    default:
        QL_FAIL("unknown Dimension(" << Integer(t) << ")");
    }
}

CorrelationCurveConfig::CorrelationCurveConfig(const string& curveID, const string& curveDescription,
                                               const Dimension& dimension, const CorrelationType& corrType,
                                               const string& convention, const MarketDatum::QuoteType& quoteType,
                                               const bool extrapolate, const vector<string>& optionTenors,
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

    populateRequiredCurveIds();
}

void CorrelationCurveConfig::populateRequiredCurveIds() {
    if (!swaptionVolatility().empty())
        requiredCurveIds_[CurveSpec::CurveType::SwaptionVolatility].insert(swaptionVolatility());
    if (!discountCurve().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(discountCurve());
}

const vector<string>& CorrelationCurveConfig::quotes() {

    if (quotes_.size() == 0) {

        std::stringstream ssBase;
        ssBase << "CORRELATION/" << quoteType_ << "/" << index1_ << "/" << index2_;
        string base = ssBase.str();

        for (auto o : optionTenors_) {
            std::stringstream ss;
            ss << base << "/" << o << "/ATM";
            quotes_.push_back(ss.str());
        }
    }
    return quotes_;
}

void CorrelationCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Correlation");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    string corrType = XMLUtils::getChildValue(node, "CorrelationType", true);
    if (corrType == "CMSSpread") {
        correlationType_ = CorrelationType::CMSSpread;
    } else if (corrType == "Generic") {
        correlationType_ = CorrelationType::Generic;
    } else {
        QL_FAIL("Correlation type " << corrType << " not recognized");
    }

    string quoteType = XMLUtils::getChildValue(node, "QuoteType", true);
    // For QuoteType, we use an insensitive compare because we previously used "Rate" here
    // But now we want to be consistent with the market datum name
    if (boost::iequals(quoteType, "RATE")) {
        quoteType_ = MarketDatum::QuoteType::RATE;
    } else if (boost::iequals(quoteType, "PRICE")) {
        quoteType_ = MarketDatum::QuoteType::PRICE;
    } else if (boost::iequals(quoteType, "NULL")) {
        quoteType_ = MarketDatum::QuoteType::NONE;
    } else {
        QL_FAIL("Quote type " << quoteType << " not recognized");
    }

    string cal, dc;
    if (quoteType_ == MarketDatum::QuoteType::NONE) {

        cal = XMLUtils::getChildValue(node, "Calendar", false);
        cal == "" ? calendar_ = QuantLib::NullCalendar() : calendar_ = parseCalendar(cal);

        dc = XMLUtils::getChildValue(node, "DayCounter", false);
        dc == "" ? dayCounter_ = QuantLib::ActualActual(ActualActual::ISDA) : dayCounter_ = parseDayCounter(dc);
    } else { // Compulsory information for Rate and Price QuoteTypes
        cal = XMLUtils::getChildValue(node, "Calendar", true);
        calendar_ = parseCalendar(cal);

        dc = XMLUtils::getChildValue(node, "DayCounter", true);
        dayCounter_ = parseDayCounter(dc);

        optionTenors_ = XMLUtils::getChildrenValuesAsStrings(node, "OptionTenors", true);
        QL_REQUIRE(optionTenors_.size() > 0, "no option tenors supplied");

        string dim = XMLUtils::getChildValue(node, "Dimension", true);
        QL_REQUIRE(dim == "ATM" || dim == "Constant", "Dimension " << dim << " not recognised");

        if (dim == "Constant") {
            dimension_ = Dimension::Constant;
            QL_REQUIRE(optionTenors_.size() == 1, "Only one tenor should be supplied for a constant correlation termstructure");
        } else {
            dimension_ = Dimension::ATM;
        }

        if (dimension_ == Dimension::ATM) {
            string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
            businessDayConvention_ = parseBusinessDayConvention(bdc);
        }

        string extr = XMLUtils::getChildValue(node, "Extrapolation", true);
        extrapolate_ = parseBool(extr);

        if (correlationType_ == CorrelationType::Generic) {
            QL_REQUIRE(quoteType_ == MarketDatum::QuoteType::RATE, "For CorrelationType::Generic calibration is not supported!");
        }
        // needed for Rate and Price QuoteTyope to build the quote string
        index1_ = XMLUtils::getChildValue(node, "Index1", true);
        index2_ = XMLUtils::getChildValue(node, "Index2", true);

        swaptionVol_ = "";

        // Index1, Index2, Currency, Conventions, SwaptionVolatility, DiscountCurve are relevant for calibration which
        // is only supported for
        // CMSSpread type correlation
        if (correlationType_ == CorrelationType::CMSSpread && quoteType_ == MarketDatum::QuoteType::PRICE) {
            currency_ = XMLUtils::getChildValue(node, "Currency", true);
            conventions_ = XMLUtils::getChildValue(node, "Conventions");
            swaptionVol_ = XMLUtils::getChildValue(node, "SwaptionVolatility", true);
            discountCurve_ = XMLUtils::getChildValue(node, "DiscountCurve", true);
        }
    }
    populateRequiredCurveIds();
}

XMLNode* CorrelationCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Correlation");
    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "CorrelationType", to_string(correlationType_));
    XMLUtils::addChild(doc, node, "Index1", index1_);
    XMLUtils::addChild(doc, node, "Index2", index2_);
    XMLUtils::addChild(doc, node, "Conventions", conventions_);

    if (quoteType_ == MarketDatum::QuoteType::PRICE) {
        XMLUtils::addChild(doc, node, "SwaptionVolatility", swaptionVol_);
        XMLUtils::addChild(doc, node, "DiscountCurve", discountCurve_);
        XMLUtils::addChild(doc, node, "Currency", currency_);
    }
    if (quoteType_ != MarketDatum::QuoteType::NONE) {
        XMLUtils::addChild(doc, node, "Dimension", to_string(dimension_));
    }

    XMLUtils::addChild(doc, node, "QuoteType", to_string(quoteType_));

    if (quoteType_ != MarketDatum::QuoteType::NONE) {

        XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);
        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));

        if (dimension_ == Dimension::ATM)
            XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));

        XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);
    }

    if (quoteType_ == MarketDatum::QuoteType::NONE) {
        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    }

    return node;
}

bool indexNameLessThan(const std::string& index1, const std::string& index2) {
    vector<string> tokens1;
    boost::split(tokens1, index1, boost::is_any_of("-"));
    vector<string> tokens2;
    boost::split(tokens2, index2, boost::is_any_of("-"));

    QL_REQUIRE(tokens1.size() >= 2, "at least two tokens expected in " << index1);
    QL_REQUIRE(tokens2.size() >= 2, "at least two tokens expected in " << index2);

    Size s1, s2;

    if (tokens1[1] == "CMS")
        s1 = 4;
    else if (tokens1[0] == "FX")
        s1 = 2;
    else if (tokens1[0] == "EQ")
        s1 = 1;
    else if (tokens1[0] == "COMM")
        s1 = 0;
    else
        s1 = 3; // assume Ibor

    if (tokens2[1] == "CMS")
        s2 = 4;
    else if (tokens2[0] == "FX")
        s2 = 2;
    else if (tokens2[0] == "EQ")
        s2 = 1;
    else if (tokens2[0] == "COMM")
        s2 = 0;
    else
        s2 = 3; // assume Ibor

    if (s1 < s2)
        return true;
    else if (s2 < s1)
        return false;

    // both EQ or both COM
    if (s1 == 0 || s1 == 1)
        return tokens1[1] < tokens2[1];

    QL_REQUIRE(tokens1.size() >= 3, "at least three tokens expected in " << index1);
    QL_REQUIRE(tokens2.size() >= 3, "at least three tokens expected in " << index2);

    // both CMS or both Ibor, tenor is the last token (3rd or even 4th for customised CMS indices)
    if (s1 == 3 || s1 == 4)
        return parsePeriod(tokens1.back()) < parsePeriod(tokens2.back());

    QL_REQUIRE(tokens1.size() >= 4, "at least four tokens expected in " << index1);
    QL_REQUIRE(tokens2.size() >= 4, "at least four tokens expected in " << index2);

    // both FX, compare CCY1 then CCY2 alphabetical
    if (s1 == 2) {
        return (tokens1[2] + "-" + tokens1[3]) < (tokens2[2] + "-" + tokens2[3]);
    }

    QL_FAIL("indexNameLessThan(): internal error");
}

} // namespace data
} // namespace ore
