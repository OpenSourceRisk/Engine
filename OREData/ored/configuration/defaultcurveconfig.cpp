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

#include <ored/configuration/defaultcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

DefaultCurveConfig::DefaultCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                       const Type& type, const string& discountCurveID, const string& recoveryRateQuote,
                                       const DayCounter& dayCounter, const string& conventionID,
                                       const vector<string>& cdsQuotes, bool extrapolation)
    : CurveConfig(curveID, curveDescription), cdsQuotes_(cdsQuotes), currency_(currency), type_(type),
      discountCurveID_(discountCurveID), recoveryRateQuote_(recoveryRateQuote), dayCounter_(dayCounter),
      conventionID_(conventionID), extrapolation_(extrapolation) {
        quotes_ = cdsQuotes; 
 		quotes_.insert(quotes_.begin(), recoveryRateQuote_);
      }

void DefaultCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "DefaultCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    string type = XMLUtils::getChildValue(node, "Type", true);
    if (type == "SpreadCDS") {
        type_ = Type::SpreadCDS;
    } else if (type == "HazardRate") {
        type_ = Type::HazardRate;
    } else if (type == "Yield") {
        type_ = Type::Yield;
    } else {
        QL_FAIL("Type " << type << " not recognized");
    }

    discountCurveID_ = XMLUtils::getChildValue(node, "DiscountCurve", false);
    benchmarkCurveID_ = XMLUtils::getChildValue(node, "BenchmarkCurve", false);

    recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    conventionID_ = XMLUtils::getChildValue(node, "Conventions", true);    
    cdsQuotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    quotes_ = cdsQuotes_; 
 	quotes_.insert(quotes_.begin(), recoveryRateQuote_); 

    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation"); // defaults to true
}

XMLNode* DefaultCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("DefaultCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    bool rec = false;

    if (type_ == Type::SpreadCDS) {
        XMLUtils::addChild(doc, node, "Type", "SpreadCDS");
        rec = true;
    }
    if (type_ == Type::HazardRate) {
        XMLUtils::addChild(doc, node, "Type", "HazardRate");
        rec = true;
    }
    QL_REQUIRE(rec, "Unkown type in DefaultCurveConfig::toXML()");

    XMLUtils::addChild(doc, node, "DiscountCurve", discountCurveID_);
    XMLUtils::addChild(doc, node, "BenchmarkCurve", benchmarkCurveID_);
    XMLUtils::addChild(doc, node, "RecoveryRate", recoveryRateQuote_);
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "Conventions", conventionID_);
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    return node;
}
} // namespace data
} // namespace ore
