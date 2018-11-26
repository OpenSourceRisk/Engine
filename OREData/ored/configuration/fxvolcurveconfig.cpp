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
#include <ored/configuration/fxvolcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

FXVolatilityCurveConfig::FXVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                 const Dimension& dimension, const vector<Period>& expiries,
                                                 const string& fxSpotID, const string& fxForeignCurveID,
                                                 const string& fxDomesticCurveID, const DayCounter& dayCounter,
                                                 const Calendar& calendar)

    : CurveConfig(curveID, curveDescription), dimension_(dimension), expiries_(expiries), dayCounter_(dayCounter),
      calendar_(calendar), fxSpotID_(fxSpotID), fxForeignYieldCurveID_(fxForeignCurveID),
      fxDomesticYieldCurveID_(fxDomesticCurveID) {}

const vector<string>& FXVolatilityCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        vector<string> tokens;
        boost::split(tokens, fxSpotID(), boost::is_any_of("/"));
        string base = "FX_OPTION/RATE_LNVOL/" + tokens[2] + "/" + tokens[3] + "/";
        for (auto e : expiries_) {
            quotes_.push_back(base + to_string(e) + "/ATM");
            if (dimension_ == Dimension::Smile) {
                quotes_.push_back(base + to_string(e) + "/25RR");
                quotes_.push_back(base + to_string(e) + "/25BF");
            }
        }
    }
    return quotes_;
}

void FXVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "FXVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    string cal = XMLUtils::getChildValue(node, "Calendar");
    if (cal == "")
        cal = "TARGET";
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter");
    if (dc == "")
        dc = "A365";
    dayCounter_ = parseDayCounter(dc);

    if (dim == "ATM") {
        dimension_ = Dimension::ATM;
    } else if (dim == "Smile") {
        dimension_ = Dimension::Smile;
    } else {
        QL_FAIL("Dimension " << dim << " not supported yet");
    }
    expiries_ = XMLUtils::getChildrenValuesAsPeriods(node, "Expiries", true);
    fxSpotID_ = XMLUtils::getChildValue(node, "FXSpotID", true);

    if (dimension_ == Dimension::Smile) {
        fxForeignYieldCurveID_ = XMLUtils::getChildValue(node, "FXForeignCurveID", true);
        fxDomesticYieldCurveID_ = XMLUtils::getChildValue(node, "FXDomesticCurveID", true);
    }
}

XMLNode* FXVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("FXVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else if (dimension_ == Dimension::Smile) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
    } else {
        QL_FAIL("Unkown Dimension in FXVolatilityCurveConfig::toXML()");
    }
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "FXSpotID", fxSpotID_);

    if (dimension_ == Dimension::Smile) {
        XMLUtils::addChild(doc, node, "FXForeignCurveID", fxForeignYieldCurveID_);
        XMLUtils::addChild(doc, node, "FXDomesticCurveID", fxDomesticYieldCurveID_);
    }

    return node;
}
} // namespace data
} // namespace ore
