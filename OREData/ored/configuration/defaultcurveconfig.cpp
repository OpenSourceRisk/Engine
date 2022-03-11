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
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>

using QuantLib::Date;

namespace ore {
namespace data {

DefaultCurveConfig::DefaultCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                       const Type& type, const string& discountCurveID, const string& recoveryRateQuote,
                                       const DayCounter& dayCounter, const string& conventionID,
                                       const std::vector<std::pair<std::string, bool>>& cdsQuotes, bool extrapolation,
                                       const string& benchmarkCurveID, const string& sourceCurveID,
                                       const std::vector<string>& pillars, const Calendar& calendar, const Size spotLag,
                                       const QuantExt::CdsCurve::RefData& refData,
                                       const BootstrapConfig& bootstrapConfig,
                                       const boost::optional<bool>& implyDefaultFromMarket,
                                       const bool allowNegativeRates)
    : CurveConfig(curveID, curveDescription), cdsQuotes_(cdsQuotes), currency_(currency), type_(type),
      discountCurveID_(discountCurveID), recoveryRateQuote_(recoveryRateQuote), dayCounter_(dayCounter),
      conventionID_(conventionID), extrapolation_(extrapolation), benchmarkCurveID_(benchmarkCurveID),
      sourceCurveID_(sourceCurveID), pillars_(pillars), calendar_(calendar), spotLag_(spotLag), refData_(refData),
      bootstrapConfig_(bootstrapConfig), implyDefaultFromMarket_(implyDefaultFromMarket),
      allowNegativeRates_(allowNegativeRates) {

    for (const auto& kv : cdsQuotes) {
        quotes_.push_back(kv.first);
    }
    if (!recoveryRateQuote_.empty())
        quotes_.insert(quotes_.begin(), recoveryRateQuote_);

    populateRequiredCurveIds();
    generateAdditionalQuotes();
}

void DefaultCurveConfig::populateRequiredCurveIds() {
    if (!discountCurveID().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(discountCurveID())->curveConfigID());
    if (!benchmarkCurveID().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(benchmarkCurveID())->curveConfigID());
    if (!sourceCurveID().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(sourceCurveID())->curveConfigID());
    for (auto const& s : multiSectionSourceCurveIds_) {
        if (!s.empty())
            requiredCurveIds_[CurveSpec::CurveType::Default].insert(parseCurveSpec(s)->curveConfigID());
    }
}

void DefaultCurveConfig::generateAdditionalQuotes() {
    // FIXME workaround for QPR-10654: always request PRICE quote in addition to CREDIT_SPREAD quote
    std::set<std::string> addQuotes;
    for (auto q : quotes_) {
        boost::replace_first(q, "CREDIT_SPREAD", "PRICE");
        addQuotes.insert(q);
    }
    for (auto const& a : addQuotes) {
        if (std::find(quotes_.begin(), quotes_.end(), a) == quotes_.end())
            quotes_.push_back(a);
    }
}

void DefaultCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "DefaultCurve");

    // Just in case
    cdsQuotes_.clear();
    quotes_.clear();

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    string type = XMLUtils::getChildValue(node, "Type", true);
    if (type == "SpreadCDS") {
        type_ = Type::SpreadCDS;
    } else if (type == "HazardRate") {
        type_ = Type::HazardRate;
    } else if (type == "Price") {
        type_ = Type::Price;
    } else if (type == "Benchmark") {
        type_ = Type::Benchmark;
    } else if (type == "MultiSection") {
        type_ = Type::MultiSection;
    } else if (type == "Null") {
        type_ = Type::Null;
    } else {
        QL_FAIL("Type " << type << " not recognized");
    }

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);
    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation"); // defaults to true

    allowNegativeRates_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "AllowNegativeRates")) {
        allowNegativeRates_ = parseBool(XMLUtils::getNodeValue(n));
    }

    if (type_ == Type::Benchmark) {
        benchmarkCurveID_ = XMLUtils::getChildValue(node, "BenchmarkCurve", true);
        sourceCurveID_ = XMLUtils::getChildValue(node, "SourceCurve", true);
        pillars_ = XMLUtils::getChildrenValuesAsStrings(node, "Pillars", true);
        spotLag_ = parseInteger(XMLUtils::getChildValue(node, "SpotLag", true));
        calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
        discountCurveID_ = conventionID_ = "";
        recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
        // we allow for a numeric recovery rate, in which case we do not add it to the quotes
        Real dummy;
        if (!tryParseReal(recoveryRateQuote_, dummy))
            quotes_.insert(quotes_.end(), recoveryRateQuote_);
    } else if (type_ == Type::MultiSection) {
        multiSectionSourceCurveIds_ = XMLUtils::getChildrenValues(node, "SourceCurves", "SourceCurve", true);
        multiSectionSwitchDates_ = XMLUtils::getChildrenValues(node, "SwitchDates", "SwitchDate", true);
        discountCurveID_ = conventionID_ = "";
        recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
        // we allow for a numeric recovery rate, in which case we do not add it to the quotes
        Real dummy;
        if (!tryParseReal(recoveryRateQuote_, dummy))
            quotes_.insert(quotes_.end(), recoveryRateQuote_);
    } else {
        discountCurveID_ = XMLUtils::getChildValue(node, "DiscountCurve", false);
        conventionID_ = XMLUtils::getChildValue(node, "Conventions", true);

        XMLNode* quotesNode = XMLUtils::getChildNode(node, "Quotes");
        if (quotesNode) {
            for (auto n : XMLUtils::getChildrenNodes(quotesNode, "Quote")) {
                string attr = XMLUtils::getAttribute(n, "optional");
                bool opt = (!attr.empty() && parseBool(attr));
                cdsQuotes_.emplace_back(make_pair(XMLUtils::getNodeValue(n), opt));
                quotes_.push_back(XMLUtils::getNodeValue(n));
            }
        }

        recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
        quotes_.insert(quotes_.begin(), recoveryRateQuote_);
        benchmarkCurveID_ = sourceCurveID_ = "";
        calendar_ = Calendar();
        spotLag_ = 0;
        pillars_.clear();

        // ref data

        refData_ = QuantExt::CdsCurve::RefData();

        XMLNode* refDataNode = XMLUtils::getChildNode(node, "ReferenceData");
        if (refDataNode) {
            string type_str = XMLUtils::getChildValue(refDataNode, "Type", false);
            string startDate_str = XMLUtils::getChildValue(refDataNode, "StartDate", false);
            string terms_str = XMLUtils::getChildValue(refDataNode, "Terms", false);
            string terminationDates_str = XMLUtils::getChildValue(refDataNode, "TerminationDates", false);
            string tenor_str = XMLUtils::getChildValue(refDataNode, "Tenor", false);
            string calendar_str = XMLUtils::getChildValue(refDataNode, "Calendar", false);
            string bdc_str = XMLUtils::getChildValue(refDataNode, "BusinessDayConvention", false);
            string termbdc_str = XMLUtils::getChildValue(refDataNode, "TermConvention", false);
            string rule_str = XMLUtils::getChildValue(refDataNode, "Rule", false);
            string eom_str = XMLUtils::getChildValue(refDataNode, "EndOfMonth", false);
            string runningSpread_str = XMLUtils::getChildValue(refDataNode, "RunningSpread", false);
            string payConvention_str = XMLUtils::getChildValue(refDataNode, "PayConvention", false);
            string dayCounter_str = XMLUtils::getChildValue(refDataNode, "DayCounter", false);
            string lastPeriodDayCounter_str = XMLUtils::getChildValue(refDataNode, "LastPeriodDayCounter", false);
            string cashSettlementDays_str = XMLUtils::getChildValue(refDataNode, "CashSettlementDays", false);
            if (!type_str.empty())
                refData_.type = type_str;
            if (!startDate_str.empty())
                refData_.startDate = parseDate(startDate_str);
            if (!terms_str.empty())
                refData_.terms = parseListOfValues<QuantLib::Period>(terms_str, &parsePeriod);
            if (!terminationDates_str.empty())
                refData_.terminationDates = parseListOfValues<QuantLib::Date>(terminationDates_str, &parseDate);
            if (!tenor_str.empty())
                refData_.tenor = parsePeriod(tenor_str);
            if (!calendar_str.empty())
                refData_.calendar = parseCalendar(calendar_str);
            if (!bdc_str.empty())
                refData_.convention = parseBusinessDayConvention(bdc_str);
            if (!termbdc_str.empty())
                refData_.termConvention = parseBusinessDayConvention(termbdc_str);
            if (!rule_str.empty())
                refData_.rule = parseDateGenerationRule(rule_str);
            if (!eom_str.empty())
                refData_.endOfMonth = parseBool(eom_str);
            if (!runningSpread_str.empty())
                refData_.runningSpread = parseReal(runningSpread_str);
            if (!payConvention_str.empty())
                refData_.payConvention = parseBusinessDayConvention(payConvention_str);
            if (!dayCounter_str.empty())
                refData_.dayCounter = parseDayCounter(dayCounter_str);
            if (!lastPeriodDayCounter_str.empty())
                refData_.lastPeriodDayCounter = parseDayCounter(lastPeriodDayCounter_str);
            if (!cashSettlementDays_str.empty())
                refData_.cashSettlementDays = parseInteger(cashSettlementDays_str);
        } else {
            string deprecatedStartDate_str = XMLUtils::getChildValue(node, "StartDate", false);
            string deprecatedRunningSpread_str = XMLUtils::getChildValue(node, "RunningSpread", false);
            if (!deprecatedStartDate_str.empty()) {
                refData_.startDate = parseDate(deprecatedStartDate_str);
                WLOG("Using deprecated StartDate node, consider using the ReferenceData node instead");
            }
            if (!deprecatedRunningSpread_str.empty()) {
                refData_.runningSpread = parseReal(deprecatedRunningSpread_str);
                WLOG("Using deprecated RunningSpread node, consider using the ReferenceData node instead");
            }
        }

        implyDefaultFromMarket_ = boost::none;
        if (XMLNode* n = XMLUtils::getChildNode(node, "ImplyDefaultFromMarket"))
            implyDefaultFromMarket_ = parseBool(XMLUtils::getNodeValue(n));

        // Optional bootstrap configuration
        if (XMLNode* n = XMLUtils::getChildNode(node, "BootstrapConfig")) {
            bootstrapConfig_.fromXML(n);
        }
    }

    populateRequiredCurveIds();
    generateAdditionalQuotes();
}

XMLNode* DefaultCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("DefaultCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    if (type_ == Type::SpreadCDS || type_ == Type::HazardRate || type_ == Type::Price) {
        if (type_ == Type::SpreadCDS) {
            XMLUtils::addChild(doc, node, "Type", "SpreadCDS");
        } else if (type_ == Type::HazardRate) {
            XMLUtils::addChild(doc, node, "Type", "HazardRate");
        } else {
            XMLUtils::addChild(doc, node, "Type", "Price");
        }
        XMLUtils::addChild(doc, node, "DiscountCurve", discountCurveID_);
        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "RecoveryRate", recoveryRateQuote_);
        XMLNode* quotesNode = XMLUtils::addChild(doc, node, "Quotes");
        for (auto q : cdsQuotes_) {
            XMLNode* qNode = doc.allocNode("Quote", q.first);
            if (q.second)
                XMLUtils::addAttribute(doc, qNode, "optional", "true");
            XMLUtils::appendNode(quotesNode, qNode);
        }
    } else if (type_ == Type::Benchmark) {
        XMLUtils::addChild(doc, node, "Type", "Benchmark");
        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "RecoveryRate", recoveryRateQuote_);
        XMLUtils::addChild(doc, node, "BenchmarkCurve", benchmarkCurveID_);
        XMLUtils::addChild(doc, node, "SourceCurve", sourceCurveID_);
        XMLUtils::addGenericChildAsList(doc, node, "Pillars", pillars_);
        XMLUtils::addChild(doc, node, "SpotLag", (int)spotLag_);
        XMLUtils::addChild(doc, node, "Calendar", calendar_.name());
    } else if (type_ == Type::MultiSection) {
        XMLUtils::addChild(doc, node, "RecoveryRate", recoveryRateQuote_);
        XMLUtils::addChildren(doc, node, "SourceCurves", "SourceCurve", multiSectionSourceCurveIds_);
        XMLUtils::addChildren(doc, node, "SwitchDates", "SwitchDate", multiSectionSwitchDates_);
    } else if (type_ == Type::Null) {
        XMLUtils::addChild(doc, node, "Type", "Null");
        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "DiscountCurve", discountCurveID_);
    } else {
        QL_FAIL("Unkown type in DefaultCurveConfig::toXML()");
    }
    XMLUtils::addChild(doc, node, "Conventions", conventionID_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);

    XMLNode* refDataNode = doc.allocNode("ReferenceData");
    XMLUtils::addChild(doc, refDataNode, "Type", refData_.type);
    if(refData_.startDate != Null<Date>())
	XMLUtils::addChild(doc, refDataNode, "StartDate", ore::data::to_string(refData_.startDate));
    XMLUtils::addGenericChildAsList(doc,refDataNode,"Terms", refData_.terms);
    XMLUtils::addGenericChildAsList(doc,refDataNode,"TerminationDates", refData_.terminationDates);
    XMLUtils::addChild(doc, refDataNode, "Tenor", refData_.tenor);
    XMLUtils::addChild(doc, refDataNode, "Calendar", refData_.calendar.name());
    XMLUtils::addChild(doc, refDataNode, "Convention", ore::data::to_string(refData_.convention));
    XMLUtils::addChild(doc, refDataNode, "Rule", ore::data::to_string(refData_.rule));
    XMLUtils::addChild(doc, refDataNode, "EndOfMonth", refData_.endOfMonth);
    XMLUtils::addChild(doc, refDataNode, "RunningSpread", refData_.runningSpread);
    XMLUtils::addChild(doc, refDataNode, "PayConvention", ore::data::to_string(refData_.payConvention));
    XMLUtils::addChild(doc, refDataNode, "DayCounter", ore::data::to_string(refData_.dayCounter));
    XMLUtils::addChild(doc, refDataNode, "LastPeriodDayCounter", ore::data::to_string(refData_.lastPeriodDayCounter));
    XMLUtils::addChild(doc, refDataNode, "CashSettlementDays", (int)refData_.cashSettlementDays);
    XMLUtils::appendNode(node, refDataNode);

    if (implyDefaultFromMarket_)
        XMLUtils::addChild(doc, node, "ImplyDefaultFromMarket", *implyDefaultFromMarket_);

    XMLUtils::appendNode(node, bootstrapConfig_.toXML(doc));

    XMLUtils::addChild(doc, node, "AllowNegativeRates", allowNegativeRates_);

    return node;
}
} // namespace data
} // namespace ore
