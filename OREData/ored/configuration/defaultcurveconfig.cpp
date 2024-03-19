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

DefaultCurveConfig::DefaultCurveConfig(const string& curveId, const string& curveDescription, const string& currency,
                                       const std::map<int, Config>& configs)
    : CurveConfig(curveId, curveDescription), currency_(currency), configs_(configs) {
    populateQuotes();
    populateRequiredCurveIds();
    // ensure priority in config is consistent to the key used in the map
    for (auto& c : configs_)
        c.second.priority() = c.first;
}

void DefaultCurveConfig::populateRequiredCurveIds(const std::string& discountCurveID,
                                                  const std::string& benchmarkCurveID, const std::string& sourceCurveID,
                                                  const std::vector<std::string>& multiSectionSourceCurveIds) {
    if (!discountCurveID.empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(discountCurveID)->curveConfigID());
    if (!benchmarkCurveID.empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(benchmarkCurveID)->curveConfigID());
    if (!sourceCurveID.empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(sourceCurveID)->curveConfigID());
    for (auto const& s : multiSectionSourceCurveIds) {
        if (!s.empty())
            requiredCurveIds_[CurveSpec::CurveType::Default].insert(parseCurveSpec(s)->curveConfigID());
    }
}

void DefaultCurveConfig::populateRequiredCurveIds() {
    for (auto const& config : configs_) {
        populateRequiredCurveIds(config.second.discountCurveID(), config.second.benchmarkCurveID(),
                                 config.second.sourceCurveID(), config.second.multiSectionSourceCurveIds());
    }
}

void DefaultCurveConfig::populateQuotes() {
    quotes_.clear();
    for (auto const& config : configs_) {
        for (const auto& kv : config.second.cdsQuotes()) {
            quotes_.push_back(kv.first);
        }
        // recovery rate might be a hardcoded number, in which case we must not add this to the set of quotes
        Real dummy;
        if (!config.second.recoveryRateQuote().empty() && !tryParseReal(config.second.recoveryRateQuote(), dummy))
            quotes_.insert(quotes_.begin(), config.second.recoveryRateQuote());
    }
}

void DefaultCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "DefaultCurve");
    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    if (auto configs = XMLUtils::getChildNode(node, "Configurations")) {
        for (auto const& config : XMLUtils::getChildrenNodes(configs, "Configuration")) {
            Config tmp;
            tmp.fromXML(config);
            QL_REQUIRE(configs_.find(tmp.priority()) == configs_.end(),
                       "DefaultCurveConfig::fromXML(): several configurations with same priority '" << tmp.priority()
                                                                                                    << "' found.");
            configs_[tmp.priority()] = tmp;
        }
    } else {
        Config tmp;
        tmp.fromXML(node);
        configs_[0] = tmp;
    }
    populateQuotes();
    populateRequiredCurveIds();
}

XMLNode* DefaultCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("DefaultCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    auto configs = XMLUtils::addChild(doc, node, "Configurations");

    for (auto& tmp : configs_) {
        XMLUtils::appendNode(configs, tmp.second.toXML(doc));
    }

    return node;
}

DefaultCurveConfig::Config::Config(const Type& type, const string& discountCurveID, const string& recoveryRateQuote,
                                   const DayCounter& dayCounter, const string& conventionID,
                                   const std::vector<std::pair<std::string, bool>>& cdsQuotes, bool extrapolation,
                                   const string& benchmarkCurveID, const string& sourceCurveID,
                                   const std::vector<string>& pillars, const Calendar& calendar, const Size spotLag,
                                   const Date& startDate, const BootstrapConfig& bootstrapConfig,
                                   QuantLib::Real runningSpread, const QuantLib::Period& indexTerm,
                                   const boost::optional<bool>& implyDefaultFromMarket, const bool allowNegativeRates,
                                   const int priority)
    : cdsQuotes_(cdsQuotes), type_(type), discountCurveID_(discountCurveID), recoveryRateQuote_(recoveryRateQuote),
      dayCounter_(dayCounter), conventionID_(conventionID), extrapolation_(extrapolation),
      benchmarkCurveID_(benchmarkCurveID), sourceCurveID_(sourceCurveID), pillars_(pillars), calendar_(calendar),
      spotLag_(spotLag), startDate_(startDate), bootstrapConfig_(bootstrapConfig), runningSpread_(runningSpread),
      indexTerm_(indexTerm), implyDefaultFromMarket_(implyDefaultFromMarket), allowNegativeRates_(allowNegativeRates),
      priority_(priority) {}

void DefaultCurveConfig::Config::fromXML(XMLNode* node) {
    auto prioStr = XMLUtils::getAttribute(node, "priority");
    if (!prioStr.empty())
        priority_ = parseInteger(prioStr);
    cdsQuotes_.clear();
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
    } else if (type == "TransitionMatrix") {
        type_ = Type::TransitionMatrix;
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
    } else if (type_ == Type::MultiSection) {
        multiSectionSourceCurveIds_ = XMLUtils::getChildrenValues(node, "SourceCurves", "SourceCurve", true);
        multiSectionSwitchDates_ = XMLUtils::getChildrenValues(node, "SwitchDates", "SwitchDate", true);
        discountCurveID_ = conventionID_ = "";
        recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
    } else if(type_ == Type::TransitionMatrix)  {
        initialState_ = XMLUtils::getChildValue(node, "InitialState", false);
        states_ = parseListOfValues(XMLUtils::getChildValue(node, "States", false));
        XMLNode* quotesNode = XMLUtils::getChildNode(node, "Quotes");
        if (quotesNode) {
            for (auto n : XMLUtils::getChildrenNodes(quotesNode, "Quote")) {
                string attr = XMLUtils::getAttribute(n, "optional");
                bool opt = (!attr.empty() && parseBool(attr));
                cdsQuotes_.emplace_back(make_pair(XMLUtils::getNodeValue(n), opt));
            }
        }
        recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
    } else {
        discountCurveID_ = XMLUtils::getChildValue(node, "DiscountCurve", false);
        conventionID_ = XMLUtils::getChildValue(node, "Conventions", true);
        XMLNode* quotesNode = XMLUtils::getChildNode(node, "Quotes");
        if (quotesNode) {
            for (auto n : XMLUtils::getChildrenNodes(quotesNode, "Quote")) {
                string attr = XMLUtils::getAttribute(n, "optional");
                bool opt = (!attr.empty() && parseBool(attr));
                cdsQuotes_.emplace_back(make_pair(XMLUtils::getNodeValue(n), opt));
            }
        }
        recoveryRateQuote_ = XMLUtils::getChildValue(node, "RecoveryRate", false);
        benchmarkCurveID_ = sourceCurveID_ = "";
        calendar_ = Calendar();
        spotLag_ = 0;
        pillars_.clear();
        // Read the optional start date
        string d = XMLUtils::getChildValue(node, "StartDate", false);
        if (d != "") {
            if (type_ == Type::SpreadCDS || type_ == Type::Price) {
                startDate_ = parseDate(d);
            } else {
                WLOG("'StartDate' is only used when type is 'SpreadCDS' or 'Price'");
            }
        }
        string s = XMLUtils::getChildValue(node, "RunningSpread", false);
        if (s.empty() && type_ == Type::Price) {
            DLOG("'RunningSpread' is empty and type is 'Price' for default curve "
                 "so the running spread will need to be provided in the market quote.");
        }
        if (!s.empty()) {
            runningSpread_ = parseReal(s);
        }
        string t = XMLUtils::getChildValue(node, "IndexTerm", false);
        indexTerm_ = t.empty() ? 0 * Days : parsePeriod(t);
        implyDefaultFromMarket_ = boost::none;
        if (XMLNode* n = XMLUtils::getChildNode(node, "ImplyDefaultFromMarket"))
            implyDefaultFromMarket_ = parseBool(XMLUtils::getNodeValue(n));
        // Optional bootstrap configuration
        if (XMLNode* n = XMLUtils::getChildNode(node, "BootstrapConfig")) {
            bootstrapConfig_.fromXML(n);
        }
    }
}

XMLNode* DefaultCurveConfig::Config::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Configuration");
    XMLUtils::addAttribute(doc, node, "priority", std::to_string(priority_));
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
    } else if ( type_ == Type::TransitionMatrix) {
        XMLUtils::addChild(doc, node, "InitialState", initialState_);
        XMLUtils::addChild(doc, node, "States", boost::algorithm::join(states_, ","));
    } else if (type_ == Type::Null) {
        XMLUtils::addChild(doc, node, "Type", "Null");
        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "DiscountCurve", discountCurveID_);
    } else {
        QL_FAIL("Unknown type in DefaultCurveConfig::toXML()");
    }
    XMLUtils::addChild(doc, node, "Conventions", conventionID_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    if (startDate_ != Date())
        XMLUtils::addChild(doc, node, "StartDate", to_string(startDate_));
    if (runningSpread_ != QuantLib::Null<Real>())
        XMLUtils::addChild(doc, node, "RunningSpread", to_string(runningSpread_));
    if (indexTerm_ != 0 * Days) {
        XMLUtils::addChild(doc, node, "IndexTerm", indexTerm_);
    }
    if (implyDefaultFromMarket_)
        XMLUtils::addChild(doc, node, "ImplyDefaultFromMarket", *implyDefaultFromMarket_);
    XMLUtils::appendNode(node, bootstrapConfig_.toXML(doc));
    XMLUtils::addChild(doc, node, "AllowNegativeRates", allowNegativeRates_);
    return node;
}

} // namespace data
} // namespace ore
