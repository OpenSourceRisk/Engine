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
    quoteTypes_({MarketDatum::QuoteType::BASE_CORRELATION}), indexSpread_(Null<Real>()), calibrateConstituentsToIndexSpread_(false), 
    stochasticRecovery_(false) {}

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
    double indexSpread, const std::string& currency, const bool calibrateConstituentsToIndexSpread, 
    bool stochasticRecovery,
    const std::map<std::string, std::vector<double>>& rrGrids,
    const std::map<std::string, std::vector<double>>& rrProbs)
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
      currency_(currency),
      calibrateConstituentsToIndexSpread_(calibrateConstituentsToIndexSpread),
      stochasticRecovery_(stochasticRecovery),
      rrGrids_(rrGrids),
      rrProbs_(rrProbs) {
    QL_REQUIRE(!quoteTypes_.empty(), "Required at least one valid quote type");
    for (const auto& quoteType : quoteTypes) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::BASE_CORRELATION || quoteType == MarketDatum::QuoteType::PRICE,
                   "Invalid quote type" << quoteType << " in BaseCorrelationCurveConfig");
    }
}

void addPriceQuotes(vector<string>& quotes, const std::string& quoteName, const std::string& term, const std::vector<string>& detachmentPoints)  {
    const static  std::string quoteTypeStr = to_string(MarketDatum::QuoteType::PRICE);
    const std::string suffix = quoteTypeStr + "/" + quoteName + "/" + term;

    std::vector<string> attachmentPoints;
    if (detachmentPoints.size() == 1 &&  detachmentPoints.front() == "*") {
        attachmentPoints.push_back("*");
    } else {
        attachmentPoints.push_back("0");
        for (size_t i=1; i<detachmentPoints.size(); ++i) {
            attachmentPoints.push_back(detachmentPoints[i-1]);
        }
    }
    for (size_t i = 0; i<detachmentPoints.size(); ++i) {
        const auto& ap = attachmentPoints[i];
        const auto& dp = detachmentPoints[i];
        quotes.push_back("INDEX_CDS_TRANCHE/" + suffix + "/" + ap + "/" + dp);
    }
}

void addBaseCorrelationQuotes(vector<string>& quotes, const std::string & quoteName, const std::string& term, const std::vector<string>& detachmentPoints)  {
    const static  std::string quoteTypeStr = to_string(MarketDatum::QuoteType::BASE_CORRELATION);
    const std::string suffix = quoteTypeStr + "/" + quoteName + "/" + term;
    for (size_t i = 0; i<detachmentPoints.size(); ++i) {
        const auto& dp = detachmentPoints[i];
        quotes.push_back("INDEX_CDS_TRANCHE/" + suffix +  "/" + dp);
        // Add legacy quote name
        quotes.push_back("CDS_INDEX/" + suffix +  "/" + dp);
    }
}

const vector<string>& BaseCorrelationCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        for (const auto& quoteType : quoteTypes_) {
            for (auto t : terms_) {
                if (quoteType == MarketDatum::QuoteType::BASE_CORRELATION) {
                    addBaseCorrelationQuotes(quotes_, quoteName_, t, detachmentPoints_);
                } else if (quoteType == MarketDatum::QuoteType::PRICE) {
                    addPriceQuotes(quotes_, quoteName_, t, detachmentPoints_);
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

    calibrateConstituentsToIndexSpread_ =
        XMLUtils::getChildValueAsBool(node, "CalibrateConstituentsToIndexSpread", false, false);

    stochasticRecovery_ = XMLUtils::getChildValueAsBool(node, "StochasticRecovery", false, false);

    if (stochasticRecovery_) {
        auto stochasticRecoveryModelParameterNode =  XMLUtils::getChildNode(node, "StochasticRecoveryModelParameters");
        auto recoveryGridNode = XMLUtils::getChildNode(stochasticRecoveryModelParameterNode, "RecoveryGrid");
        auto recoveryGrids = XMLUtils::getChildrenAttributesAndValues(recoveryGridNode, "Grid", "seniority", true);
        auto recoveryProbabilityNode = XMLUtils::getChildNode(stochasticRecoveryModelParameterNode, "RecoveryProbabilities}");
        auto recoveryProbabilites = XMLUtils::getChildrenAttributesAndValues(recoveryProbabilityNode, "Probabilities", "seniority", true);
        for (const auto& seniority : recoveryGrids) {
            QL_REQUIRE(recoveryProbabilites.find(seniority.first) != recoveryProbabilites.end(),
                       "Recovery probabilities for seniority " << seniority.first << " not found");
        }
        for (const auto& [seniority, probs] : recoveryProbabilites) {
            std::vector<string> probTokens;
            boost::split(probTokens, probs, boost::is_any_of(","));
            std::vector<double> probsDouble;
            for (const auto& p : probTokens) {
                probsDouble.push_back(parseReal(p));
            }
            rrProbs_[seniority] = probsDouble;
        }
        for (const auto& [seniority, grid] : recoveryGrids) {
            std::vector<string> gridTokens;
            boost::split(gridTokens, grid, boost::is_any_of(","));
            std::vector<double> rrGrid;
            for (const auto& p : gridTokens) {
                rrGrid.push_back(parseReal(p));
            }
            rrGrids_[seniority] = rrGrid;
        }
    }
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

    XMLUtils::addChild(doc, node, "CalibrateConstituentsToIndexSpread", calibrateConstituentsToIndexSpread_);

    XMLUtils::addChild(doc, node, "AdjustForLosses", adjustForLosses_);

    if(stochasticRecovery_){
        XMLUtils::addChild(doc, node, "StochasticRecovery", stochasticRecovery_);
        auto stochasticRecoveryModelParameterNode = XMLUtils::addChild(doc, node, "StochasticRecoveryModelParameters");
        auto recoveryGridNode = XMLUtils::addChild(doc, stochasticRecoveryModelParameterNode, "RecoveryGrid");
        for (const auto& [seniority, grid] : rrGrids_) {
            XMLUtils::addGenericChildAsList(doc, recoveryGridNode, "Grid", grid, "seniority", seniority);
        }
        auto recoveryProbabilityNode = XMLUtils::addChild(doc, stochasticRecoveryModelParameterNode, "RecoveryProbabilities");
        for (const auto& [seniority, probs] : rrProbs_) {
            XMLUtils::addGenericChildAsList(doc, recoveryProbabilityNode, "Probabilities", probs, "seniority", seniority);
        }
    }

    return node;
}
} // namespace data
} // namespace ore

