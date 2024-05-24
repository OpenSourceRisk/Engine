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
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;
using QuantLib::Real;
using std::string;

namespace ore {
namespace data {

CDSVolatilityCurveConfig::CDSVolatilityCurveConfig(const string& curveId, const string& curveDescription,
                                                   const QuantLib::ext::shared_ptr<VolatilityConfig>& volatilityConfig,
                                                   const string& dayCounter, const string& calendar,
                                                   const string& strikeType, const string& quoteName, Real strikeFactor,
                                                   const std::vector<QuantLib::Period>& terms,
                                                   const std::vector<std::string>& termCurves)
    : CurveConfig(curveId, curveDescription), volatilityConfig_(volatilityConfig), dayCounter_(dayCounter),
      calendar_(calendar), strikeType_(strikeType), quoteName_(quoteName), strikeFactor_(strikeFactor), terms_(terms),
      termCurves_(termCurves) {

    QL_REQUIRE(terms_.size() == termCurves_.size(),
               "CDSVolatilityCurveConfig: " << curveId
                                            << " specifies different number of terms / curves (built via constructor)");
    populateQuotes();
    populateRequiredCurveIds();
}

const QuantLib::ext::shared_ptr<VolatilityConfig>& CDSVolatilityCurveConfig::volatilityConfig() const {
    return volatilityConfig_;
}

const string& CDSVolatilityCurveConfig::dayCounter() const { return dayCounter_; }

const string& CDSVolatilityCurveConfig::calendar() const { return calendar_; }

const string& CDSVolatilityCurveConfig::strikeType() const { return strikeType_; }

const string& CDSVolatilityCurveConfig::quoteName() const { return quoteName_; }

Real CDSVolatilityCurveConfig::strikeFactor() const { return strikeFactor_; }

const std::vector<QuantLib::Period>& CDSVolatilityCurveConfig::terms() const { return terms_; }

const std::vector<std::string>& CDSVolatilityCurveConfig::termCurves() const { return termCurves_; }

void CDSVolatilityCurveConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CDSVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    terms_.clear();
    termCurves_.clear();
    if (auto n = XMLUtils::getChildNode(node, "Terms")) {
        terms_.clear();
        termCurves_.clear();
        for (auto c : XMLUtils::getChildrenNodes(n, "Term")) {
            terms_.push_back(XMLUtils::getChildValueAsPeriod(c, "Label", true));
            termCurves_.push_back(XMLUtils::getChildValue(c, "Curve", true));
        }
    }

    quoteName_ = "";
    if (auto n = XMLUtils::getChildNode(node, "QuoteName"))
        quoteName_ = XMLUtils::getNodeValue(n);

    if (XMLUtils::getChildNode(node, "Expiries")) {

        // Giving just an Expiries node is allowed to be backwards compatible but is discouraged.
        WLOG("Using an Expiries node only in CDSVolatilityCurveConfig is deprecated. "
             << "A volatility configuration node should be used instead.");

        // Get the expiries
        vector<string> expiries = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
        QL_REQUIRE(expiries.size() > 0, "Need at least one expiry in the Expiries node.");

        // Build the quotes by appending the expiries and terms to the quote stem.
	std::vector<std::string> quotes;
        string stem = quoteStem();
        for (const string& exp : expiries) {
            for (auto const& p : terms_) {
                quotes.push_back(stem + exp + "/" + ore::data::to_string(p));
            }
        }

	// If we have at most 1 term specified, we add quotes without term as well
        for (const string& exp : expiries) {
	    quotes.push_back(stem + exp);
        }

        // Create the relevant volatilityConfig_ object.
        if (quotes.size() == 1) {
            volatilityConfig_ = QuantLib::ext::make_shared<ConstantVolatilityConfig>(quotes[0]);
        } else {
            volatilityConfig_ = QuantLib::ext::make_shared<VolatilityCurveConfig>(quotes, "Linear", "Flat");
        }

    } else {
        XMLNode* n;
        if ((n = XMLUtils::getChildNode(node, "Constant"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<ConstantVolatilityConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "Curve"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<VolatilityCurveConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "StrikeSurface"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<VolatilityStrikeSurfaceConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "DeltaSurface"))) {
            QL_FAIL("CDSVolatilityCurveConfig does not yet support a DeltaSurface.");
        } else if ((n = XMLUtils::getChildNode(node, "MoneynessSurface"))) {
            QL_FAIL("CDSVolatilityCurveConfig does not yet support a MoneynessSurface.");
        } else if ((n = XMLUtils::getChildNode(node, "ProxySurface"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<CDSProxyVolatilityConfig>();
        } else {
            QL_FAIL("CDSVolatility node expects one child node with name in list: Constant,"
                    << " Curve, StrikeSurface, ProxySurface.");
        }
        volatilityConfig_->fromXML(n);
    }

    dayCounter_ = "A365";
    if (auto n = XMLUtils::getChildNode(node, "DayCounter"))
        dayCounter_ = XMLUtils::getNodeValue(n);

    calendar_ = "NullCalendar";
    if (auto n = XMLUtils::getChildNode(node, "Calendar"))
        calendar_ = XMLUtils::getNodeValue(n);

    strikeType_ = "";
    if (auto n = XMLUtils::getChildNode(node, "StrikeType"))
        strikeType_ = XMLUtils::getNodeValue(n);

    strikeFactor_ = 1.0;
    if (auto n = XMLUtils::getChildNode(node, "StrikeFactor"))
        strikeFactor_ = parseReal(XMLUtils::getNodeValue(n));

    populateQuotes();
    populateRequiredCurveIds();
}

XMLNode* CDSVolatilityCurveConfig::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CDSVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (!terms_.empty()) {
        QL_REQUIRE(terms_.size() == termCurves_.size(),
                   "CDSVolatilityCurveConfig::toXML(): internal error, terms size ("
                       << terms_.size() << ") != termCurves size (" << termCurves_.size() << "), curveId = curveID_");
        auto termsNode = XMLUtils::addChild(doc, node, "Terms");
        for (Size i = 0; i < terms_.size(); ++i) {
            auto tmp = XMLUtils::addChild(doc, termsNode, "Term");
            XMLUtils::addChild(doc, tmp, "Label", ore::data::to_string(terms_[i]));
            XMLUtils::addChild(doc, tmp, "Curve", ore::data::to_string(termCurves_[i]));
        }
    }

    XMLNode* n = volatilityConfig_->toXML(doc);
    XMLUtils::appendNode(node, n);

    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!strikeType_.empty())
        XMLUtils::addChild(doc, node, "StrikeType", strikeType_);
    if (!quoteName_.empty())
        XMLUtils::addChild(doc, node, "QuoteName", quoteName_);
    XMLUtils::addChild(doc, node, "StrikeFactor", strikeFactor_);

    return node;
}

void CDSVolatilityCurveConfig::populateQuotes() {

    // The quotes depend on the type of volatility structure that has been configured.
    if (auto vc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(volatilityConfig_)) {
        quotes_ = {vc->quote()};
    } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(volatilityConfig_)) {
        quotes_ = vc->quotes();
    } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<VolatilitySurfaceConfig>(volatilityConfig_)) {

        // Clear the quotes_ if necessary and populate with surface quotes
        quotes_.clear();

        string stem = quoteStem();
        for (const pair<string, string>& p : vc->quotes()) {
            // build quotes of the form .../TERM/EXPIRY/STRIKE
            for (auto const& t : terms_) {
                quotes_.push_back(stem + ore::data::to_string(t) + "/" + p.first + "/" + p.second);
            }
            // if only one or even no term is configured, also build quotes of the form .../EXPIRY/STRIKE
            if (terms_.size() <= 1) {
                quotes_.push_back(stem + p.first + "/" + p.second);
            }
        }

    } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<CDSProxyVolatilityConfig>(volatilityConfig_)) {
        // no quotes required in this case
    } else {
        QL_FAIL("CDSVolatilityCurveConfig expected a constant, curve or surface");
    }
}

void CDSVolatilityCurveConfig::populateRequiredCurveIds() {
    if (auto vc = QuantLib::ext::dynamic_pointer_cast<CDSProxyVolatilityConfig>(volatilityConfig_)) {
        requiredCurveIds_[CurveSpec::CurveType::CDSVolatility].insert(vc->cdsVolatilityCurve());
    }
    for (auto const& c : termCurves_) {
        auto spec = parseCurveSpec(c);
        requiredCurveIds_[CurveSpec::CurveType::Default].insert(spec->curveConfigID());
    }
}

string CDSVolatilityCurveConfig::quoteStem() const {

    string stem{"INDEX_CDS_OPTION/RATE_LNVOL/"};

    if (!quoteName_.empty()) {
        // If an explicit quote name has been provided use this.
        stem += quoteName_;
    } else {
        // If not, fallback to just using the curveID_.
        stem += curveID_;
    }
    stem += "/";
    return stem;
}

} // namespace data
} // namespace ore
