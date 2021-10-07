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

#include <boost/algorithm/string.hpp>
#include <ored/configuration/cdsvolcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;
using QuantLib::Real;
using std::string;

namespace ore {
namespace data {

CDSVolatilityCurveConfig::CDSVolatilityCurveConfig(bool parseTerm) : parseTerm_(parseTerm) {}

CDSVolatilityCurveConfig::CDSVolatilityCurveConfig(const string& curveId, const string& curveDescription,
                                                   const boost::shared_ptr<VolatilityConfig>& volatilityConfig,
                                                   const string& dayCounter, const string& calendar,
                                                   const string& strikeType, const string& quoteName, Real strikeFactor,
                                                   bool parseTerm)
    : CurveConfig(curveId, curveDescription), volatilityConfig_(volatilityConfig), dayCounter_(dayCounter),
      calendar_(calendar), strikeType_(strikeType), quoteName_(quoteName), strikeFactor_(strikeFactor),
      parseTerm_(parseTerm) {

    populateNameTerm();
    populateQuotes();
    populateRequiredCurveIds();
}

const boost::shared_ptr<VolatilityConfig>& CDSVolatilityCurveConfig::volatilityConfig() const {
    return volatilityConfig_;
}

const string& CDSVolatilityCurveConfig::dayCounter() const { return dayCounter_; }

const string& CDSVolatilityCurveConfig::calendar() const { return calendar_; }

const string& CDSVolatilityCurveConfig::strikeType() const { return strikeType_; }

const string& CDSVolatilityCurveConfig::quoteName() const { return quoteName_; }

Real CDSVolatilityCurveConfig::strikeFactor() const { return strikeFactor_; }

bool CDSVolatilityCurveConfig::parseTerm() const { return parseTerm_; }

const pair<string, string>& CDSVolatilityCurveConfig::nameTerm() const { return nameTerm_; }

void CDSVolatilityCurveConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CDSVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    XMLNode* n;
    if ((n = XMLUtils::getChildNode(node, "ParseTerm")))
        parseTerm_ = parseBool(XMLUtils::getNodeValue(n));

    populateNameTerm();

    quoteName_ = "";
    if ((n = XMLUtils::getChildNode(node, "QuoteName")))
        quoteName_ = XMLUtils::getNodeValue(n);

    if (XMLUtils::getChildNode(node, "Expiries")) {

        // Giving just an Expiries node is allowed to be backwards compatible but is discouraged.
        WLOG("Using an Expiries node only in CDSVolatilityCurveConfig is deprecated. "
             << "A volatility configuration node should be used instead.");

        // Get the expiries
        vector<string> quotes = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
        QL_REQUIRE(quotes.size() > 0, "Need at least one expiry in the Expiries node.");

        // Build the quotes by appending the expiries to the quote stem.
        string stem = quoteStem();
        for (string& q : quotes) {
            q = stem + q;
        }

        // Create the relevant volatilityConfig_ object.
        if (quotes.size() == 1) {
            volatilityConfig_ = boost::make_shared<ConstantVolatilityConfig>(quotes[0]);
        } else {
            volatilityConfig_ = boost::make_shared<VolatilityCurveConfig>(quotes, "Linear", "Flat");
        }

    } else {
        XMLNode* n;
        if ((n = XMLUtils::getChildNode(node, "Constant"))) {
            volatilityConfig_ = boost::make_shared<ConstantVolatilityConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "Curve"))) {
            volatilityConfig_ = boost::make_shared<VolatilityCurveConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "StrikeSurface"))) {
            volatilityConfig_ = boost::make_shared<VolatilityStrikeSurfaceConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "DeltaSurface"))) {
            QL_FAIL("CDSVolatilityCurveConfig does not yet support a DeltaSurface.");
        } else if ((n = XMLUtils::getChildNode(node, "MoneynessSurface"))) {
            QL_FAIL("CDSVolatilityCurveConfig does not yet support a MoneynessSurface.");
        } else if ((n = XMLUtils::getChildNode(node, "ProxySurface"))) {
            volatilityConfig_ = boost::make_shared<CDSProxyVolatilityConfig>();
        } else {
            QL_FAIL("CDSVolatility node expects one child node with name in list: Constant,"
                    << " Curve, StrikeSurface, ProxySurface.");
        }
        volatilityConfig_->fromXML(n);
    }

    dayCounter_ = "A365";
    if ((n = XMLUtils::getChildNode(node, "DayCounter")))
        dayCounter_ = XMLUtils::getNodeValue(n);

    calendar_ = "NullCalendar";
    if ((n = XMLUtils::getChildNode(node, "Calendar")))
        calendar_ = XMLUtils::getNodeValue(n);

    strikeType_ = "";
    if ((n = XMLUtils::getChildNode(node, "StrikeType")))
        strikeType_ = XMLUtils::getNodeValue(n);

    strikeFactor_ = 1.0;
    if ((n = XMLUtils::getChildNode(node, "StrikeFactor")))
        strikeFactor_ = parseReal(XMLUtils::getNodeValue(n));

    populateQuotes();
    populateRequiredCurveIds();
}

XMLNode* CDSVolatilityCurveConfig::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CDSVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    XMLNode* n = volatilityConfig_->toXML(doc);
    XMLUtils::appendNode(node, n);

    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!strikeType_.empty())
        XMLUtils::addChild(doc, node, "StrikeType", strikeType_);
    if (!quoteName_.empty())
        XMLUtils::addChild(doc, node, "QuoteName", quoteName_);
    XMLUtils::addChild(doc, node, "StrikeFactor", strikeFactor_);
    XMLUtils::addChild(doc, node, "ParseTerm", parseTerm_);

    return node;
}

void CDSVolatilityCurveConfig::populateNameTerm() {

    // If parsing of term from ID has been turned off, just return early.
    if (!parseTerm_)
        return;

    // Attempt to parse "<name>-<tenor>" from curve configuration ID.
    // If not of this form, nameTerm_ is left as ("", "").
    nameTerm_ = extractTermFromId(curveID_);
}

void CDSVolatilityCurveConfig::populateQuotes() {

    // The quotes depend on the type of volatility structure that has been configured.
    if (auto vc = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(volatilityConfig_)) {
        quotes_ = {vc->quote()};
    } else if (auto vc = boost::dynamic_pointer_cast<VolatilityCurveConfig>(volatilityConfig_)) {
        quotes_ = vc->quotes();
    } else if (auto vc = boost::dynamic_pointer_cast<VolatilitySurfaceConfig>(volatilityConfig_)) {

        // Clear the quotes_ if necessary and populate with surface quotes
        quotes_.clear();

        // Build the quotes by appending the expiries and strikes to the quote stem.
        string stem = quoteStem();
        for (const pair<string, string>& p : vc->quotes()) {
            quotes_.push_back(stem + p.first + "/" + p.second);
        }

    } else if (auto vc = boost::dynamic_pointer_cast<CDSProxyVolatilityConfig>(volatilityConfig_)) {
        // no quotes required in this case
    } else {
        QL_FAIL("CDSVolatilityCurveConfig expected a constant, curve or surface");
    }
}

void CDSVolatilityCurveConfig::populateRequiredCurveIds() {
    if (auto vc = boost::dynamic_pointer_cast<CDSProxyVolatilityConfig>(volatilityConfig_)) {
        requiredCurveIds_[CurveSpec::CurveType::CDSVolatility].insert(vc->cdsVolatilityCurve());
    }
}

string CDSVolatilityCurveConfig::quoteStem() const {

    string stem{"INDEX_CDS_OPTION/RATE_LNVOL/"};

    if (!quoteName_.empty()) {
        // If an explicit quote name has been provided use this.
        stem += quoteName_;
    } else if (!nameTerm_.first.empty()) {
        // If we have parsed the curveID_ from <name>-<term> to a (name, term) pair, use name.
        stem += nameTerm_.first;
    } else {
        // Fallback to just using the curveID_.
        stem += curveID_;
    }
    stem += "/";

    // If we have parsed the curveID_ from <name>-<term> to a (name, term) pair, append term.
    if (!nameTerm_.second.empty())
        stem += nameTerm_.second + "/";

    return stem;
}

pair<string, string> extractTermFromId(const string& id) {

    pair<string, string> result;

    // Attempt to parse suffix "-<tenor_string>" from id.
    auto pos = id.find_last_of('-');
    if (pos != string::npos && pos != id.size() - 1 && pos != 0) {
        Period tmp;
        string term = id.substr(pos + 1);
        if (tryParse<Period>(id.substr(pos + 1), tmp, parsePeriod)) {
            DLOG("Parsed term suffix, " << term << ", from id " << id);
            result.first = id.substr(0, pos);
            result.second = term;
        } else {
            DLOG("Could not convert suffix, " << term << ", from id " << id << " to term.");
        }
    } else {
        DLOG("Could not parse a term suffix from id " << id);
    }

    return result;
}

} // namespace data
} // namespace ore
