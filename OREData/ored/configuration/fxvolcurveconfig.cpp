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

#include <ored/configuration/fxvolcurveconfig.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

FXVolatilityCurveConfig::FXVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                 const Dimension& dimension, const vector<Period>& expiries)
    : curveID_(curveID), curveDescription_(curveDescription), dimension_(dimension), expiries_(expiries) {}

void FXVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "FXVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    if (dim == "ATM") {
        dimension_ = Dimension::ATM;
    } else if (dim == "Smile") {
        dimension_ = Dimension::Smile;
    } else {
        QL_FAIL("Dimension " << dim << " not supported yet");
    }
    expiries_ = XMLUtils::getChildrenValuesAsPeriods(node, "Expiries", true);

    if (dimension_ == Dimension::Smile) {
        fxSpotID_ = XMLUtils::getChildValue(node, "FXSpotID", true);
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

    if (dimension_ == Dimension::Smile) {
        XMLUtils::addChild(doc, node, "FXSpotID", fxSpotID_);
        XMLUtils::addChild(doc, node, "FXForeignCurveID", fxForeignYieldCurveID_);
        XMLUtils::addChild(doc, node, "FXDomesticCurveID", fxDomesticYieldCurveID_);
    }

    return node;
}
} // namespace data
} // namespace ore
