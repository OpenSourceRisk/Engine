/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/optionpaymentdata.hpp>

#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using std::ostream;
using std::string;
using std::vector;

namespace ore {
namespace data {

OptionPaymentData::OptionPaymentData()
    : rulesBased_(false), lag_(0), convention_(Following), relativeTo_(RelativeTo::Expiry) {}

OptionPaymentData::OptionPaymentData(const vector<string>& dates)
    : strDates_(dates), rulesBased_(false), lag_(0), convention_(Following), relativeTo_(RelativeTo::Expiry) {
    init();
}

OptionPaymentData::OptionPaymentData(const string& lag, const string& calendar, const string& convention,
                                     const string& relativeTo)
    : strLag_(lag), strCalendar_(calendar), strConvention_(convention), strRelativeTo_(relativeTo), rulesBased_(true),
      lag_(0), convention_(Following), relativeTo_(RelativeTo::Expiry) {
    init();
}

void OptionPaymentData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "PaymentData");
    if (XMLUtils::getChildNode(node, "Dates")) {
        strDates_ = XMLUtils::getChildrenValues(node, "Dates", "Date", true);
        rulesBased_ = false;
    } else if (XMLNode* n = XMLUtils::getChildNode(node, "Rules")) {
        strLag_ = XMLUtils::getChildValue(n, "Lag", true);
        strCalendar_ = XMLUtils::getChildValue(n, "Calendar", true);
        strConvention_ = XMLUtils::getChildValue(n, "Convention", true);
        strRelativeTo_ = "Expiry";
        if (XMLNode* rtNode = XMLUtils::getChildNode(n, "RelativeTo"))
            strRelativeTo_ = XMLUtils::getNodeValue(rtNode);
        rulesBased_ = true;
    } else {
        QL_FAIL("Expected that PaymentData node has a PaymentDates or PaymentRules child node.");
    }
    init();
}

XMLNode* OptionPaymentData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("PaymentData");

    if (rulesBased_) {
        XMLNode* rulesNode = doc.allocNode("Rules");
        XMLUtils::addChild(doc, rulesNode, "Lag", strLag_);
        XMLUtils::addChild(doc, rulesNode, "Calendar", strCalendar_);
        XMLUtils::addChild(doc, rulesNode, "Convention", strConvention_);
        XMLUtils::addChild(doc, rulesNode, "RelativeTo", strRelativeTo_);
        XMLUtils::appendNode(node, rulesNode);
    } else {
        XMLUtils::addChildren(doc, node, "Dates", "Date", strDates_);
    }

    return node;
}

void OptionPaymentData::init() {
    if (rulesBased_) {
        lag_ = parseInteger(strLag_);
        calendar_ = parseCalendar(strCalendar_);
        convention_ = parseBusinessDayConvention(strConvention_);
        populateRelativeTo();
    } else {
        QL_REQUIRE(strDates_.size() > 0, "Expected at least 1 option payment date.");
        dates_.reserve(strDates_.size());
        for (const string& d : strDates_) {
            dates_.push_back(parseDate(d));
        }
    }
}

void OptionPaymentData::populateRelativeTo() {
    if (strRelativeTo_ == "Expiry") {
        relativeTo_ = RelativeTo::Expiry;
    } else if (strRelativeTo_ == "Exercise") {
        relativeTo_ = RelativeTo::Exercise;
    } else {
        QL_FAIL("Could not convert string " << strRelativeTo_ << " to a valid RelativeTo value.");
    }
}

ostream& operator<<(ostream& out, const OptionPaymentData::RelativeTo& relativeTo) {
    switch (relativeTo) {
    case OptionPaymentData::RelativeTo::Expiry:
        return out << "Expiry";
    case OptionPaymentData::RelativeTo::Exercise:
        return out << "Exercise";
    default:
        QL_FAIL("Could not convert the relativeTo enum value to string.");
    }
}

} // namespace data
} // namespace ore
