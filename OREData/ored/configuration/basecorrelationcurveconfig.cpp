/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/configuration/basecorrelationcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

BaseCorrelationCurveConfig::BaseCorrelationCurveConfig(const string& curveID, const string& curveDescription,
                                                       const vector<Real>& detachmentPoints,
                                                       const vector<Period>& terms)
    : CurveConfig(curveID, curveDescription), detachmentPoints_(detachmentPoints), terms_(terms) {}

const vector<string>& BaseCorrelationCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        string base = "CDS_INDEX/BASE_CORRELATION/" + curveID_ + "/";
        for (auto t : terms_) {
            for (auto dp : detachmentPoints_) {
                quotes_.push_back(base + to_string(t) + "/" + to_string(dp));
            }
        }
    }
    return quotes_;
}

void BaseCorrelationCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BaseCorrelation");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    terms_ = XMLUtils::getChildrenValuesAsPeriods(node, "Terms", true);
    detachmentPoints_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "DetachmentPoints", true);
    settlementDays_ = parseInteger(XMLUtils::getChildValue(node, "SettlementDays", true));
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
    businessDayConvention_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "BusinessDayConvention", true));
    dayCounter_ = parseDayCounter(XMLUtils::getChildValue(node, "DayCounter", true));
    extrapolate_ = parseBool(XMLUtils::getChildValue(node, "Extrapolate", true));
}

XMLNode* BaseCorrelationCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("BaseCorrelation");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addGenericChildAsList(doc, node, "Terms", terms_);
    XMLUtils::addGenericChildAsList(doc, node, "DetachmentPoints", detachmentPoints_);
    XMLUtils::addChild(doc, node, "SettlementDays", int(settlementDays_));
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "Extrapolate", extrapolate_);
    return node;
}
} // namespace data
} // namespace ore
