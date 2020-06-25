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
                                                 const Dimension& dimension, const vector<string>& expiries,
                                                 const vector<string>& strikes, const string& fxSpotID,
                                                 const string& fxForeignCurveID, const string& fxDomesticCurveID,
                                                 const DayCounter& dayCounter, const Calendar& calendar,
                                                 const SmileInterpolation& interp, const string& conventionsID,
                                                 const QuantLib::Natural& smileDelta)

    : CurveConfig(curveID, curveDescription), dimension_(dimension), expiries_(expiries), dayCounter_(dayCounter),
      calendar_(calendar), fxSpotID_(fxSpotID), fxForeignYieldCurveID_(fxForeignCurveID),
      fxDomesticYieldCurveID_(fxDomesticCurveID), conventionsID_(conventionsID), smileDelta_(smileDelta),
      smileInterpolation_(interp) {
    populateRequiredYieldCurveIDs();
}

const vector<string>& FXVolatilityCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        vector<string> tokens;
        boost::split(tokens, fxSpotID(), boost::is_any_of("/"));
        string base = "FX_OPTION/RATE_LNVOL/" + tokens[1] + "/" + tokens[2] + "/";
        for (auto e : expiries_) {
            quotes_.push_back(base + e + "/ATM");
            if (dimension_ == Dimension::SmileVannaVolga) {
                quotes_.push_back(base + e + "/" + to_string(smileDelta_) + "RR");
                quotes_.push_back(base + e + "/" + to_string(smileDelta_) + "BF");
            } else if (dimension_ == Dimension::SmileDelta) {
                for (auto d : deltas_) {
                    quotes_.push_back(base + e + "/" + d);
                }
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
    string smileInterp = XMLUtils::getChildValue(node, "SmileInterpolation");

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

        conventionsID_ = XMLUtils::getChildValue(node, "Conventions", false);
        string smileType = XMLUtils::getChildValue(node, "SmileType");
        if (smileType == "" || smileType == "VannaVolga") {
            dimension_ = Dimension::SmileVannaVolga;

            // only read smile interpolation method if dimension is smile.
            if (smileInterp == "") {
                smileInterpolation_ = SmileInterpolation::VannaVolga2; // default to VannaVolga 2nd approximation
            } else if (smileInterp == "VannaVolga1") {
                smileInterpolation_ = SmileInterpolation::VannaVolga1;
            } else if (smileInterp == "VannaVolga2") {
                smileInterpolation_ = SmileInterpolation::VannaVolga2;
            } else {
                QL_FAIL("SmileInterpolation " << smileInterp << " not supported");
            }

            string sDelta = XMLUtils::getChildValue(node, "SmileDelta");
            if (sDelta == "")
                sDelta = "25";
            smileDelta_ = parseInteger(sDelta);
        } else if (smileType == "Delta") {
            dimension_ = Dimension::SmileDelta;
            // only read smile interpolation method if dimension is smile.
            if (smileInterp == "" || smileInterp == "Linear") {
                smileInterpolation_ = SmileInterpolation::Linear;
            } else if (smileInterp == "Cubic") {
                smileInterpolation_ = SmileInterpolation::Cubic;
            } else {
                QL_FAIL("SmileInterpolation " << smileInterp << " not supported");
            }
            deltas_ = XMLUtils::getChildrenValuesAsStrings(node, "Deltas", true);

            // check that these are valid deltas
            for (auto d : deltas_) {
                QL_REQUIRE(d == "ATM" || d.back() == 'P' || d.back() == 'C',
                           "this is not a valid value for delta, " << d);
                if (d != "ATM") {
                    parseReal(d.substr(0, d.size() - 1));
                }
            }
        }
    } else {
        QL_FAIL("Dimension " << dim << " not supported yet");
    }
    expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
    fxSpotID_ = XMLUtils::getChildValue(node, "FXSpotID", true);

    if (dimension_ == Dimension::SmileVannaVolga || dimension_ == Dimension::SmileDelta) {
        fxForeignYieldCurveID_ = XMLUtils::getChildValue(node, "FXForeignCurveID", true);
        fxDomesticYieldCurveID_ = XMLUtils::getChildValue(node, "FXDomesticCurveID", true);
    }

    populateRequiredYieldCurveIDs();
}

XMLNode* FXVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("FXVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else if (dimension_ == Dimension::SmileVannaVolga) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "VannaVolga");
        // only write smile interpolation if dimension is smile
        if (smileInterpolation_ == SmileInterpolation::VannaVolga1) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "VannaVolga1");
        } else if (smileInterpolation_ == SmileInterpolation::VannaVolga2) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "VannaVolga2");
        } else {
            QL_FAIL("Unknown SmileInterpolation in FXVolatilityCurveConfig::toXML()");
        }
        XMLUtils::addChild(doc, node, "SmileDelta", to_string(smileDelta_));
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
    } else if (dimension_ == Dimension::SmileDelta) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "Delta");
        // only write smile interpolation if dimension is smile
        if (smileInterpolation_ == SmileInterpolation::Linear) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Linear");
        } else if (smileInterpolation_ == SmileInterpolation::Cubic) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Cubic");
        } else {
            QL_FAIL("Unknown SmileInterpolation in FXVolatilityCurveConfig::toXML()");
        }
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
        XMLUtils::addGenericChildAsList(doc, node, "Deltas", deltas_);
    } else {
        QL_FAIL("Unkown Dimension in FXVolatilityCurveConfig::toXML()");
    }
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    XMLUtils::addChild(doc, node, "FXSpotID", fxSpotID_);
    if (dimension_ == Dimension::SmileVannaVolga || dimension_ == Dimension::SmileDelta) {
        XMLUtils::addChild(doc, node, "FXForeignCurveID", fxForeignYieldCurveID_);
        XMLUtils::addChild(doc, node, "FXDomesticCurveID", fxDomesticYieldCurveID_);
    }
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));

    return node;
}

void FXVolatilityCurveConfig::populateRequiredYieldCurveIDs() {

    if (dimension_ == Dimension::SmileVannaVolga || dimension_ == Dimension::SmileDelta) {
        std::vector<string> domTokens, forTokens;
        split(domTokens, fxDomesticYieldCurveID_, boost::is_any_of("/"));
        split(forTokens, fxForeignYieldCurveID_, boost::is_any_of("/"));

        if (domTokens.size() == 3 && domTokens[0] == "Yield") {
            requiredYieldCurveIDs_.insert(domTokens[2]);
        } else if (domTokens.size() == 1) {
            requiredYieldCurveIDs_.insert(fxDomesticYieldCurveID_);
        } else {
            QL_FAIL("Cannot determine the required domestic yield curve for fx vol curve " << curveID_);
        }

        if (forTokens.size() == 3 && forTokens[0] == "Yield") {
            requiredYieldCurveIDs_.insert(forTokens[2]);
        } else if (forTokens.size() == 1) {
            requiredYieldCurveIDs_.insert(fxForeignYieldCurveID_);
        } else {
            QL_FAIL("Cannot determine the required foreign yield curve for fx vol curve " << curveID_);
        }
    }
}
} // namespace data
} // namespace ore
