/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/configuration/commodityvolcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

CommodityVolatilityConfig::CommodityVolatilityConfig()
    : optionExpiryRollDays_(0) {}

CommodityVolatilityConfig::CommodityVolatilityConfig(const string& curveId, const string& curveDescription,
                                                     const std::string& currency,
                                                     const boost::shared_ptr<VolatilityConfig>& volatilityConfig,
                                                     const string& dayCounter, const string& calendar,
                                                     const std::string& futureConventionsId,
                                                     QuantLib::Natural optionExpiryRollDays,
                                                     const std::string& priceCurveId, const std::string& yieldCurveId,
                                                     const std::string& quoteSuffix,
                                                     const OneDimSolverConfig& solverConfig,
                                                     const boost::optional<bool>& preferOutOfTheMoney)
    : CurveConfig(curveId, curveDescription), currency_(currency), volatilityConfig_(volatilityConfig),
      dayCounter_(dayCounter), calendar_(calendar), futureConventionsId_(futureConventionsId),
      optionExpiryRollDays_(optionExpiryRollDays), priceCurveId_(priceCurveId), yieldCurveId_(yieldCurveId),
      quoteSuffix_(quoteSuffix), solverConfig_(solverConfig), preferOutOfTheMoney_(preferOutOfTheMoney) {
    populateQuotes();
    populateRequiredCurveIds();
}

void CommodityVolatilityConfig::populateRequiredCurveIds() {
    if (!priceCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Commodity].insert(parseCurveSpec(priceCurveId())->curveConfigID());
    if (!yieldCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(yieldCurveId())->curveConfigID());
    if (auto vapo = boost::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(volatilityConfig())) {
        requiredCurveIds_[CurveSpec::CurveType::CommodityVolatility].insert(
            parseCurveSpec(vapo->baseVolatilityId())->curveConfigID());
    }
}

const string& CommodityVolatilityConfig::currency() const { return currency_; }

const string& CommodityVolatilityConfig::dayCounter() const { return dayCounter_; }

const boost::shared_ptr<VolatilityConfig>& CommodityVolatilityConfig::volatilityConfig() const {
    return volatilityConfig_;
}

const string& CommodityVolatilityConfig::calendar() const { return calendar_; }

const string& CommodityVolatilityConfig::futureConventionsId() const { return futureConventionsId_; }

Natural CommodityVolatilityConfig::optionExpiryRollDays() const { return optionExpiryRollDays_; }

const string& CommodityVolatilityConfig::priceCurveId() const { return priceCurveId_; }

const string& CommodityVolatilityConfig::yieldCurveId() const { return yieldCurveId_; }

const string& CommodityVolatilityConfig::quoteSuffix() const { return quoteSuffix_; }

const boost::optional<bool>& CommodityVolatilityConfig::preferOutOfTheMoney() const {
    return preferOutOfTheMoney_;
}

OneDimSolverConfig CommodityVolatilityConfig::solverConfig() const {
    return solverConfig_.empty() ? defaultSolverConfig() : solverConfig_;
}

OneDimSolverConfig CommodityVolatilityConfig::defaultSolverConfig() {

    // Some "reasonable" defaults for commodity volatility searches.
    // Max eval of 100. Initial guess of 35%. Search between 1 bp and 200%.
    static OneDimSolverConfig res(100, 0.35, 0.0001, std::make_pair(0.0001, 2.0));

    return res;
}

void CommodityVolatilityConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    XMLNode* n;
    if ((n = XMLUtils::getChildNode(node, "Constant"))) {
        volatilityConfig_ = boost::make_shared<ConstantVolatilityConfig>();
    } else if ((n = XMLUtils::getChildNode(node, "Curve"))) {
        volatilityConfig_ = boost::make_shared<VolatilityCurveConfig>();
    } else if ((n = XMLUtils::getChildNode(node, "StrikeSurface"))) {
        volatilityConfig_ = boost::make_shared<VolatilityStrikeSurfaceConfig>();
    } else if ((n = XMLUtils::getChildNode(node, "DeltaSurface"))) {
        volatilityConfig_ = boost::make_shared<VolatilityDeltaSurfaceConfig>();
    } else if ((n = XMLUtils::getChildNode(node, "MoneynessSurface"))) {
        volatilityConfig_ = boost::make_shared<VolatilityMoneynessSurfaceConfig>();
    } else if ((n = XMLUtils::getChildNode(node, "ApoFutureSurface"))) {
        volatilityConfig_ = boost::make_shared<VolatilityApoFutureSurfaceConfig>();
    } else {
        QL_FAIL("CommodityVolatility node expects one child node with name in list: Constant,"
                << " Curve, StrikeSurface, DeltaSurface, MoneynessSurface, ApoFutureSurface.");
    }
    volatilityConfig_->fromXML(n);

    dayCounter_ = "A365";
    if ((n = XMLUtils::getChildNode(node, "DayCounter")))
        dayCounter_ = XMLUtils::getNodeValue(n);

    calendar_ = "NullCalendar";
    if ((n = XMLUtils::getChildNode(node, "Calendar")))
        calendar_ = XMLUtils::getNodeValue(n);

    futureConventionsId_ = XMLUtils::getChildValue(node, "FutureConventions", false);

    optionExpiryRollDays_ = 0;
    if ((n = XMLUtils::getChildNode(node, "OptionExpiryRollDays")))
        optionExpiryRollDays_ = parseInteger(XMLUtils::getNodeValue(n));

    priceCurveId_ = XMLUtils::getChildValue(node, "PriceCurveId", false);
    yieldCurveId_ = XMLUtils::getChildValue(node, "YieldCurveId", false);

    quoteSuffix_ = XMLUtils::getChildValue(node, "QuoteSuffix", false);

    solverConfig_ = OneDimSolverConfig();
    if (XMLNode* n = XMLUtils::getChildNode(node, "OneDimSolverConfig")) {
        solverConfig_.fromXML(n);
    }

    preferOutOfTheMoney_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PreferOutOfTheMoney")) {
        preferOutOfTheMoney_ = parseBool(XMLUtils::getNodeValue(n));
    }

    populateQuotes();
    populateRequiredCurveIds();
}

XMLNode* CommodityVolatilityConfig::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CommodityVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    XMLNode* n = volatilityConfig_->toXML(doc);
    XMLUtils::appendNode(node, n);

    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!futureConventionsId_.empty())
        XMLUtils::addChild(doc, node, "FutureConventions", futureConventionsId_);
    XMLUtils::addChild(doc, node, "OptionExpiryRollDays", static_cast<int>(optionExpiryRollDays_));
    if (!priceCurveId_.empty())
        XMLUtils::addChild(doc, node, "PriceCurveId", priceCurveId_);
    if (!yieldCurveId_.empty())
        XMLUtils::addChild(doc, node, "YieldCurveId", yieldCurveId_);
    if (!quoteSuffix_.empty())
        XMLUtils::addChild(doc, node, "QuoteSuffix", quoteSuffix_);
    if (!solverConfig_.empty())
        XMLUtils::appendNode(node, solverConfig_.toXML(doc));
    if (preferOutOfTheMoney_)
        XMLUtils::addChild(doc, node, "PreferOutOfTheMoney", *preferOutOfTheMoney_);

    return node;
}

void CommodityVolatilityConfig::populateQuotes() {

    // The quotes depend on the type of volatility structure that has been configured.
    if (auto vc = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(volatilityConfig_)) {
        quotes_ = {vc->quote()};
    } else if (auto vc = boost::dynamic_pointer_cast<VolatilityCurveConfig>(volatilityConfig_)) {
        quotes_ = vc->quotes();
    } else if (auto vc = boost::dynamic_pointer_cast<VolatilitySurfaceConfig>(volatilityConfig_)) {
        // Clear the quotes_ if necessary and populate with surface quotes
        quotes_.clear();
        string quoteType = to_string(volatilityConfig()->quoteType());
        string stem = "COMMODITY_OPTION/" + quoteType + "/" + curveID_ + "/" + currency_ + "/";
        for (const pair<string, string>& p : vc->quotes()) {
            string q = stem + p.first + "/" + p.second;
            if (!quoteSuffix_.empty())
                q += "/" + quoteSuffix_;
            quotes_.push_back(q);
        }
    } else {
        QL_FAIL("CommodityVolatilityConfig expected a constant, curve or surface");
    }
}

} // namespace data
} // namespace ore
