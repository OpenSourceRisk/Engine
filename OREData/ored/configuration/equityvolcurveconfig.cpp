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
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

EquityVolatilityCurveConfig::EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                         const string& currency,
                                                         const vector<boost::shared_ptr<VolatilityConfig>>& volatilityConfig,
                                                         const string& dayCounter, const string& calendar,
                                                         const OneDimSolverConfig& solverConfig,
                                                         const boost::optional<bool>& preferOutOfTheMoney)
    : CurveConfig(curveID, curveDescription), ccy_(currency), volatilityConfig_(volatilityConfig),
      dayCounter_(dayCounter), calendar_(calendar), solverConfig_(solverConfig),
      preferOutOfTheMoney_(preferOutOfTheMoney) {
    populateQuotes();
    populateRequiredCurveIds();
}

const string EquityVolatilityCurveConfig::quoteStem(const string& volType) const {
    return "EQUITY_OPTION/" + volType + "/" + curveID_ + "/" + ccy_ + "/";
}

void EquityVolatilityCurveConfig::populateQuotes() {
    // add quotes from all the volatility configs
    for (auto vc : volatilityConfig_) {
        if (boost::dynamic_pointer_cast<QuoteBasedVolatilityConfig>(vc)) {
            // The quotes depend on the type of volatility structure that has been configured.
            if (auto c = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
                quotes_.push_back(c->quote());
            } else if (auto c = boost::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
                auto qs = c->quotes();
                quotes_.insert(quotes_.end(), qs.begin(), qs.end());
            } else if (auto c = boost::dynamic_pointer_cast<VolatilitySurfaceConfig>(vc)) {
                // Clear the quotes_ if necessary and populate with surface quotes
                string quoteStr;
                string volType = to_string<MarketDatum::QuoteType>(c->quoteType());
                for (const pair<string, string>& p : c->quotes()) {
                    if (p.first == "*" || p.second == "*") {
                        quoteStr = quoteStem(volType) + "*";
                    } else {
                        quoteStr = quoteStem(volType) + p.first + "/" + p.second;
                    }
                    quotes_.push_back(quoteStr);
                }
            }
        }
    }
}

void EquityVolatilityCurveConfig::populateRequiredCurveIds() {
    for (auto vc : volatilityConfig_) {
        if (auto p = boost::dynamic_pointer_cast<EquityProxyVolatilityConfig>(vc)) {
            requiredCurveIds_[CurveSpec::CurveType::Equity].insert(p->equityVolatilityCurve());
            if (!p->fxVolatilityCurve().empty())
                requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(p->fxVolatilityCurve());
            if (!p->correlationCurve().empty())
                requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(p->correlationCurve());
        }
    }
}

void EquityVolatilityCurveConfig::loadVolatiltyConfigs(XMLNode* node) {
    if (XMLNode* n = XMLUtils::getChildNode(node, "Constant")) {
        auto vc = boost::make_shared<ConstantVolatilityConfig>();
        vc->fromXML(n);
        volatilityConfig_.push_back(vc);
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "Curve")) {
        auto vc = boost::make_shared<VolatilityCurveConfig>();
        vc->fromXML(n);
        volatilityConfig_.push_back(vc);
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "StrikeSurface")) {
        auto vc = boost::make_shared<VolatilityStrikeSurfaceConfig>();
        vc->fromXML(n);
        volatilityConfig_.push_back(vc);
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "DeltaSurface")) {
        auto vc = boost::make_shared<VolatilityDeltaSurfaceConfig>();
        vc->fromXML(n);
        volatilityConfig_.push_back(vc);
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "MoneynessSurface")) {
        auto vc = boost::make_shared<VolatilityMoneynessSurfaceConfig>();
        vc->fromXML(n);
        volatilityConfig_.push_back(vc);
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "ApoFutureSurface")) {
        QL_FAIL("ApoFutureSurface not supported for equity volatilities.");
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "ProxySurface")) {
        auto vc = boost::make_shared<EquityProxyVolatilityConfig>();
        vc->fromXML(n);
        volatilityConfig_.push_back(vc);
    }

    QL_REQUIRE(volatilityConfig_.size() > 0, "EquityVolatility node expects at least one child node of type: "
        "Constant, Curve, StrikeSurface, DeltaSurface, MoneynessSurface, ProxySurface.");

    // sort the volatility configs by priority
    if (volatilityConfig_.size() > 1)
        std::sort(volatilityConfig_.begin(), volatilityConfig_.end());
}

void EquityVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "EquityVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    ccy_ = XMLUtils::getChildValue(node, "Currency", true);

    XMLNode* n;
    calendar_ = "NullCalendar";
    if ((n = XMLUtils::getChildNode(node, "Calendar")))
        calendar_ = XMLUtils::getNodeValue(n);

    dayCounter_ = "A365";
    if ((n = XMLUtils::getChildNode(node, "DayCounter")))
        dayCounter_ = XMLUtils::getNodeValue(n);

    solverConfig_ = OneDimSolverConfig();
    if (XMLNode* n = XMLUtils::getChildNode(node, "OneDimSolverConfig")) {
        solverConfig_.fromXML(n);
    }

    preferOutOfTheMoney_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PreferOutOfTheMoney")) {
        preferOutOfTheMoney_ = parseBool(XMLUtils::getNodeValue(n));
    }

    // In order to remain backward compatible, we first check for a dimension node
    // If this is present we read the nodes as before but create volatilityConfigs from the inputs
    // If no dimension nodes then we expect VolatilityConfig nodes - this is the preferred configuration
    string dim = XMLUtils::getChildValue(node, "Dimension", false);
    if (dim == "ATM" || dim == "Smile") {
        vector<string> expiries = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
        string strikeExtrapolation = "Flat";
        string timeExtrapolation = "Flat";
        XMLNode* timeNode = XMLUtils::getChildNode(node, "TimeExtrapolation");
        if (timeNode) {
            timeExtrapolation = XMLUtils::getChildValue(node, "TimeExtrapolation", true);
        }
        XMLNode* strikeNode = XMLUtils::getChildNode(node, "StrikeExtrapolation");
        if (strikeNode) {
            strikeExtrapolation = XMLUtils::getChildValue(node, "StrikeExtrapolation", true);
        }
        vector<string> strikes = XMLUtils::getChildrenValuesAsStrings(node, "Strikes", false);
        if (dim == "ATM") {
            QL_REQUIRE(strikes.size() == 0,
                       "Dimension ATM, but multiple strikes provided for EquityVolatility " << curveID_);
            // if ATM create VolatilityCurveConfig which requires quotes to be provided
            vector<string> quotes(expiries.size());
            string quoteStem = "EQUITY_OPTION/RATE_LNVOL/" + curveID_ + "/" + ccy_ + "/";
            if (expiries.size() == 1 && expiries.front() == "*") {
                quotes[0] = (quoteStem + "*");
            } else {
                Size i = 0;
                for (auto ex : expiries) {
                    quotes[i] = (quoteStem + ex + "/ATMF");
                    i++;
                }
            }
            volatilityConfig_.push_back(boost::make_shared<VolatilityCurveConfig>(quotes, timeExtrapolation, timeExtrapolation));
        } else {
            // if Smile create VolatilityStrikeSurfaceConfig
            volatilityConfig_.push_back(boost::make_shared<VolatilityStrikeSurfaceConfig>(
                strikes, expiries, "Linear", "Linear", true, timeExtrapolation, strikeExtrapolation));
        }

    } else if (dim == "") {
        if (XMLNode* n = XMLUtils::getChildNode(node, "VolatilityConfig"))
            loadVolatiltyConfigs(n);
        else 
            loadVolatiltyConfigs(node);     
    } else {
        QL_FAIL("Only ATM and Smile dimensions, or Volatility Config supported for EquityVolatility " << curveID_);
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Report")) {
        reportConfig_.fromXML(tmp);
    }

    populateQuotes();
    populateRequiredCurveIds();
}

XMLNode* EquityVolatilityCurveConfig::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("EquityVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", ccy_);
    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);

    for (auto vc : volatilityConfig_) {
        XMLNode* n = vc->toXML(doc);
        XMLUtils::appendNode(node, n);
    }

    if (calendar_ != "NullCalendar")
        XMLUtils::addChild(doc, node, "Calendar", calendar_);

    if (!solverConfig_.empty())
        XMLUtils::appendNode(node, solverConfig_.toXML(doc));

    if (preferOutOfTheMoney_)
        XMLUtils::addChild(doc, node, "PreferOutOfTheMoney", *preferOutOfTheMoney_);

    XMLUtils::appendNode(node, reportConfig_.toXML(doc));

    return node;
}

bool EquityVolatilityCurveConfig::isProxySurface() {
    for (auto vc : volatilityConfig_) {
        if (auto p = boost::dynamic_pointer_cast<EquityProxyVolatilityConfig>(vc)) {
            return true;
        }
    }
    return false;
}

OneDimSolverConfig EquityVolatilityCurveConfig::solverConfig() const {
    return solverConfig_.empty() ? defaultSolverConfig() : solverConfig_;
}

OneDimSolverConfig EquityVolatilityCurveConfig::defaultSolverConfig() {

    // Backward compatible with code that existed before EquityVolatilityCurveConfig
    // accepted a solver configuration.
    static OneDimSolverConfig res(100, 0.2, 0.0001, 0.01, 0.0001);

    return res;
}

} // namespace data
} // namespace ore
