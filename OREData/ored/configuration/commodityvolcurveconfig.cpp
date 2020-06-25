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
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

CommodityVolatilityConfig::CommodityVolatilityConfig() {}

CommodityVolatilityConfig::CommodityVolatilityConfig(const string& curveId, const string& curveDescription,
                                                     const std::string& currency,
                                                     const boost::shared_ptr<VolatilityConfig>& volatilityConfig,
                                                     const string& dayCounter, const string& calendar,
                                                     const std::string& futureConventionsId,
                                                     QuantLib::Natural optionExpiryRollDays,
                                                     const std::string& priceCurveId, const std::string& yieldCurveId)
    : CurveConfig(curveId, curveDescription), currency_(currency), volatilityConfig_(volatilityConfig),
      dayCounter_(dayCounter), calendar_(calendar), futureConventionsId_(futureConventionsId),
      optionExpiryRollDays_(optionExpiryRollDays), priceCurveId_(priceCurveId), yieldCurveId_(yieldCurveId) {
    populateQuotes();
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

    populateQuotes();
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
        string stem = "COMMODITY_OPTION/RATE_LNVOL/" + curveID_ + "/" + currency_ + "/";
        for (const pair<string, string>& p : vc->quotes()) {
            quotes_.push_back(stem + p.first + "/" + p.second);
        }
    } else {
        QL_FAIL("CommodityVolatilityConfig expected a constant, curve or surface");
    }
}

} // namespace data
} // namespace ore
