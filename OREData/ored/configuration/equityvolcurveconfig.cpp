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

#include <ored/configuration/equityvolcurveconfig.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

EquityVolatilityCurveConfig::EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                         const string& currency, const Dimension& dimension,
                                                         const vector<string>& expiries, const vector<string>& strikes)
    : CurveConfig(curveID, curveDescription), ccy_(currency), dimension_(dimension),
      expiries_(expiries), strikes_(strikes) {}

const vector<string>& EquityVolatilityCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        string base = "EQUITY_OPTION/RATE_LNVOL/" + curveID_ + "/" + ccy_ + "/";
        if (dimension_ == Dimension::ATM) {
            for (auto e : expiries_) {
                    quotes_.push_back(base + to_string(e) + "/ATMF");
            }
        } else {
            for (auto e : expiries_)
                for (auto s : strikes_)
                    quotes_.push_back(base + to_string(e) + "/" + to_string(s));
        }
    }
    return quotes_;
}
void EquityVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "EquityVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    ccy_ = XMLUtils::getChildValue(node, "Currency", true);
    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    if (dim == "ATM") {
        dimension_ = Dimension::ATM;
    } else if (dim == "Smile") {
        dimension_ = Dimension::Smile;
        strikes_= XMLUtils::getChildrenValuesAsStrings(node, "Strikes", true);
    }
    expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
}

XMLNode* EquityVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("EquityVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", ccy_);
    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addGenericChildAsList(doc, node, "Strikes", strikes_);
    }
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);

    return node;
}
} // namespace data
} // namespace ore
