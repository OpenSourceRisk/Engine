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
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {



BaseCorrelationCurveConfig::BaseCorrelationCurveConfig()
    : settlementDays_(0), businessDayConvention_(Following), extrapolate_(true), adjustForLosses_(true),
    quoteTypes_({MarketDatum::QuoteType::BASE_CORRELATION}), indexSpread_(Null<Real>()) {}

BaseCorrelationCurveConfig::BaseCorrelationCurveConfig(const string& curveID,
    const string& curveDescription,
    const vector<string>& detachmentPoints,
    const vector<string>& terms,
    Size settlementDays,
    const Calendar& calendar,
    BusinessDayConvention businessDayConvention,
    DayCounter dayCounter,
    bool extrapolate,
    const string& quoteName,
    const Date& startDate, 
    const Period& indexTerm,
    boost::optional<DateGeneration::Rule> rule,
    bool adjustForLosses,
    const std::vector<MarketDatum::QuoteType>& quoteTypes,
    double indexSpread, const std::string& currency)
    : CurveConfig(curveID, curveDescription),
      detachmentPoints_(detachmentPoints),
      terms_(terms),
      settlementDays_(settlementDays),
      calendar_(calendar),
      businessDayConvention_(businessDayConvention),
      dayCounter_(dayCounter),
      extrapolate_(extrapolate),
      quoteName_(quoteName.empty() ? curveID : quoteName),
      startDate_(startDate), 
      indexTerm_(indexTerm),
      rule_(rule),
      adjustForLosses_(adjustForLosses),
      quoteTypes_(quoteTypes),
      indexSpread_(indexSpread),
      currency_(currency) {
    QL_REQUIRE(!quoteTypes_.empty(), "Required at least one valid quote type");
    for (const auto& quoteType : quoteTypes) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::BASE_CORRELATION || quoteType == MarketDatum::QuoteType::PRICE,
            "Invalid quote type" << quoteType << " in BaseCorrelationCurveConfig");
    }
}

const vector<string>& BaseCorrelationCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        for (const auto& quoteType : quoteTypes_) {
            string base = "CDS_INDEX/" + to_string(quoteType) + "/" + quoteName_ + "/";
            for (auto t : terms_) {
                for (auto dp : detachmentPoints_) {
                    quotes_.push_back(base + t + "/" + dp);
                }
            }
        }
    }
    return quotes_;
}

void BaseCorrelationCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BaseCorrelation");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    terms_ = XMLUtils::getChildrenValuesAsStrings(node, "Terms", true);
    detachmentPoints_ = XMLUtils::getChildrenValuesAsStrings(node, "DetachmentPoints", true);
    settlementDays_ = parseInteger(XMLUtils::getChildValue(node, "SettlementDays", true));
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
    businessDayConvention_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "BusinessDayConvention", true));
    dayCounter_ = parseDayCounter(XMLUtils::getChildValue(node, "DayCounter", true));
    extrapolate_ = parseBool(XMLUtils::getChildValue(node, "Extrapolate", true));
    quoteName_ = XMLUtils::getChildValue(node, "QuoteName", false);
    if (quoteName_.empty())
        quoteName_ = curveID_;

    quoteTypes_.clear();
    auto quoteTypesStr = XMLUtils::getChildrenValues(node, "QuoteTypes", "QuoteType", false);
    for (auto t : quoteTypesStr) {
        quoteTypes_.push_back(parseQuoteType(t));
    }

    if (quoteTypes_.empty()) {
        std::string quoteTypeStr = XMLUtils::getChildValue(node, "QuoteType", false);
        if (!quoteTypeStr.empty()) {
            WLOG("Quote type is deprecated, use <QuoteTypes><QuoteType>QUOTE_TYPE<QuoteType></QuoteTypes> instead.");
            quoteTypes_.push_back(parseQuoteType(quoteTypeStr));
        } else {
            quoteTypes_.push_back(MarketDatum::QuoteType::BASE_CORRELATION);
        }
    }

    startDate_ = Date();
    if (auto n = XMLUtils::getChildNode(node, "StartDate"))
        startDate_ = parseDate(XMLUtils::getNodeValue(n));

    string t = XMLUtils::getChildValue(node, "IndexTerm", false);
    indexTerm_ = t.empty() ? 0 * Days : parsePeriod(t);

    if (auto n = XMLUtils::getChildNode(node, "Rule"))
        rule_ = parseDateGenerationRule(XMLUtils::getNodeValue(n));

    indexSpread_ = XMLUtils::getChildValueAsDouble(node, "IndexSpread", false, Null<Real>());

    currency_ = XMLUtils::getChildValue(node, "Currency", false);

    adjustForLosses_ = true;
    if (auto n = XMLUtils::getChildNode(node, "AdjustForLosses"))
        adjustForLosses_ = parseBool(XMLUtils::getNodeValue(n));
}

XMLNode* BaseCorrelationCurveConfig::toXML(XMLDocument& doc) const {
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
    XMLUtils::addChild(doc, node, "QuoteName", quoteName_);


    auto quoteTypesNode = XMLUtils::addChild(doc, node, "QuoteTypes");
    for (auto t : quoteTypes_) {
        XMLUtils::addChild(doc, quoteTypesNode, "QuoteType", to_string(t));
    }
    
    if (startDate_ != Date())
        XMLUtils::addChild(doc, node, "StartDate", to_string(startDate_));

    if (rule_)
        XMLUtils::addChild(doc, node, "Rule", to_string(*rule_));

    if (indexTerm_ != 0 * Days) {
        XMLUtils::addChild(doc, node, "IndexTerm", indexTerm_);
    }

    if (indexSpread_ != Null<Real>()){
        XMLUtils::addChild(doc, node, "IndexSpread", indexSpread_);
    }

    if (!currency_.empty()){
        XMLUtils::addChild(doc, node, "Currency", currency_);
    }

    XMLUtils::addChild(doc, node, "AdjustForLosses", adjustForLosses_);

    return node;
}
} // namespace data
} // namespace ore
