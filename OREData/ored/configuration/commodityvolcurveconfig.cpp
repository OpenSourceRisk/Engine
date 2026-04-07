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
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

CommodityVolatilityConfig::CommodityVolatilityConfig()
    : optionExpiryRollDays_(0) {}

CommodityVolatilityConfig::CommodityVolatilityConfig(
    const string& curveId, const string& curveDescription, const std::string& currency,
    const vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& volatilityConfig, const string& dayCounter,
    const string& calendar, const std::string& futureConventionsId, QuantLib::Natural optionExpiryRollDays,
    const std::string& priceCurveId, const std::string& yieldCurveId, const std::string& quoteSuffix,
    const OneDimSolverConfig& solverConfig, const QuantLib::ext::optional<bool>& preferOutOfTheMoney,
    const MarketDatum::InstrumentType instrumentType, const int calendarSpreadOffset,
    const std::string& calendarSpreadUnderlyingName)
    : CurveConfig(curveId, curveDescription), currency_(currency), volatilityConfig_(volatilityConfig),
      dayCounter_(dayCounter), calendar_(calendar), futureConventionsId_(futureConventionsId),
      optionExpiryRollDays_(optionExpiryRollDays), priceCurveId_(priceCurveId), yieldCurveId_(yieldCurveId),
      quoteSuffix_(quoteSuffix), solverConfig_(solverConfig), preferOutOfTheMoney_(preferOutOfTheMoney),
      instrumentType_(instrumentType), calendarSpreadOffset_(calendarSpreadOffset),
      calendarSpreadUnderlyingName_(calendarSpreadUnderlyingName) {
    validate();
    populateQuotes();
}

void CommodityVolatilityConfig::validate() const {
    QL_REQUIRE(instrumentType_ == MarketDatum::InstrumentType::COMMODITY_OPTION ||
                   instrumentType_ == MarketDatum::InstrumentType::COMMODITY_CALENDAR_SPREAD_OPTION,
               "invalid instrument type " << to_string(instrumentType_));

    if (instrumentType_ != MarketDatum::InstrumentType::COMMODITY_CALENDAR_SPREAD_OPTION)
        return;

    QL_REQUIRE(calendarSpreadOffset_ > 0,
               "calendar spread offset should be positive for spread commodity options");

    string underlyingName;
    int parsedOffset = 0;
    QL_REQUIRE(parseCommodityCalendarSpreadVolSurfaceName(curveID_, underlyingName, parsedOffset),
               "curveId '" << curveID_
                            << "' should be of the form [underlying]_CALENDAR_SPREAD_[offset]");
    QL_REQUIRE(calendarSpreadOffset_ == parsedOffset,
               "CalendarSpreadOffset " << calendarSpreadOffset_ << " does not match curveId '" << curveID_
                                        << "' offset " << parsedOffset);
    QL_REQUIRE(calendarSpreadUnderlyingName_ == underlyingName,
               "CalendarSpreadUnderlyingName '" << calendarSpreadUnderlyingName_ << "' does not match curveId '"
                                               << curveID_ << "' underlying name '" << underlyingName << "'");

    QL_REQUIRE(!volatilityConfig_.empty(), "volatilityConfig must not be empty for spread commodity options");
    for (const auto& vc : volatilityConfig_) {
        if (auto cvc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
            QL_REQUIRE(cvc->quoteType() == MarketDatum::QuoteType::RATE_NVOL,
                       "calendar spread options only support RATE_NVOL quotes for ConstantVolatilityConfig");
            QL_REQUIRE(cvc->volType() == VolatilityConfig::VolatilityType::Normal,
                       "calendar spread options only support Normal vol type for ConstantVolatilityConfig");
        } else if (auto vcc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
            QL_REQUIRE(vcc->quoteType() == MarketDatum::QuoteType::RATE_NVOL,
                       "calendar spread options only support RATE_NVOL quotes for VolatilityCurveConfig");
            QL_REQUIRE(vcc->volType() == VolatilityConfig::VolatilityType::Normal,
                       "calendar spread options only support Normal vol type for VolatilityCurveConfig");
        } else if (auto vssc = QuantLib::ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
            QL_REQUIRE(vssc->quoteType() == MarketDatum::QuoteType::RATE_NVOL,
                       "calendar spread options only support RATE_NVOL quotes for VolatilityStrikeSurfaceConfig");
            QL_REQUIRE(vssc->volType() == VolatilityConfig::VolatilityType::Normal,
                       "calendar spread options only support Normal vol type for VolatilityStrikeSurfaceConfig");
        } else {
            QL_FAIL("calendar spread options only support ConstantVolatilityConfig, VolatilityCurveConfig and "
                    "VolatilityStrikeSurfaceConfig");
        }
    }
}

void CommodityVolatilityConfig::populateRequiredIds() const {
    if (!priceCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Commodity].insert(parseCurveSpec(priceCurveId())->curveConfigID());
    if (!yieldCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(yieldCurveId())->curveConfigID());
    for (auto vc : volatilityConfig()) {
        if (auto vapo = QuantLib::ext::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(vc)) {
            requiredCurveIds_[CurveSpec::CurveType::CommodityVolatility].insert(
                parseCurveSpec(vapo->baseVolatilityId())->curveConfigID());
        }
        if (auto p = QuantLib::ext::dynamic_pointer_cast<ProxyVolatilityConfig>(vc)) {
            requiredCurveIds_[CurveSpec::CurveType::Commodity].insert(p->proxyVolatilityCurve());
            requiredCurveIds_[CurveSpec::CurveType::CommodityVolatility].insert(p->proxyVolatilityCurve());
            if (!p->fxVolatilityCurve().empty())
                requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(p->fxVolatilityCurve());
            if (!p->correlationCurve().empty())
                requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(p->correlationCurve());
        }
    }
}

const string& CommodityVolatilityConfig::currency() const { return currency_; }

const string& CommodityVolatilityConfig::dayCounter() const { return dayCounter_; }

const vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& CommodityVolatilityConfig::volatilityConfig() const {
    return volatilityConfig_;
}

const string& CommodityVolatilityConfig::calendar() const { return calendar_; }

const string& CommodityVolatilityConfig::futureConventionsId() const { return futureConventionsId_; }

Natural CommodityVolatilityConfig::optionExpiryRollDays() const { return optionExpiryRollDays_; }

const string& CommodityVolatilityConfig::priceCurveId() const { return priceCurveId_; }

const string& CommodityVolatilityConfig::yieldCurveId() const { return yieldCurveId_; }

const string& CommodityVolatilityConfig::quoteSuffix() const { return quoteSuffix_; }

const QuantLib::ext::optional<bool>& CommodityVolatilityConfig::preferOutOfTheMoney() const {
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

    VolatilityConfigBuilder vcb;
    vcb.fromXML(node);
    volatilityConfig_ = vcb.volatilityConfig();
    
    dayCounter_ = "A365";
    if (XMLNode* n = XMLUtils::getChildNode(node, "DayCounter"))
        dayCounter_ = XMLUtils::getNodeValue(n);

    calendar_ = "NullCalendar";
    if (XMLNode* n = XMLUtils::getChildNode(node, "Calendar"))
        calendar_ = XMLUtils::getNodeValue(n);

    futureConventionsId_ = XMLUtils::getChildValue(node, "FutureConventions", false);

    optionExpiryRollDays_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryRollDays"))
        optionExpiryRollDays_ = parseInteger(XMLUtils::getNodeValue(n));

    priceCurveId_ = XMLUtils::getChildValue(node, "PriceCurveId", false);
    yieldCurveId_ = XMLUtils::getChildValue(node, "YieldCurveId", false);

    quoteSuffix_ = XMLUtils::getChildValue(node, "QuoteSuffix", false);

    solverConfig_ = OneDimSolverConfig();
    if (XMLNode* n = XMLUtils::getChildNode(node, "OneDimSolverConfig")) {
        solverConfig_.fromXML(n);
    }

    instrumentType_ = parseInstrumentType(XMLUtils::getChildValue(node, "InstrumentType", false, "COMMODITY_OPTION"));
    calendarSpreadOffset_ = parseInteger(XMLUtils::getChildValue(node, "CalendarSpreadOffset", false, "0"));
    calendarSpreadUnderlyingName_ = XMLUtils::getChildValue(node, "CalendarSpreadUnderlyingName", false);
    validate();

    preferOutOfTheMoney_ = QuantLib::ext::nullopt;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PreferOutOfTheMoney")) {
        preferOutOfTheMoney_ = parseBool(XMLUtils::getNodeValue(n));
    }

    if(auto tmp = XMLUtils::getChildNode(node, "Report")){
        reportConfig_.fromXML(tmp);
    }
    populateQuotes();
}

XMLNode* CommodityVolatilityConfig::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CommodityVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    if (instrumentType_ == MarketDatum::InstrumentType::COMMODITY_CALENDAR_SPREAD_OPTION) {
        XMLUtils::addChild(doc, node, "InstrumentType", to_string(instrumentType_));
        XMLUtils::addChild(doc, node, "CalendarSpreadOffset", calendarSpreadOffset_);
        XMLUtils::addChild(doc, node, "CalendarSpreadUnderlyingName", calendarSpreadUnderlyingName_);
    }

    XMLNode* vnode = doc.allocNode("VolatilityConfig");
    for (auto vc : volatilityConfig_) {
        XMLNode* n = vc->toXML(doc);
        XMLUtils::appendNode(vnode, n);
    }
    XMLUtils::appendNode(node, vnode);

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
    XMLUtils::appendNode(node, reportConfig_.toXML(doc));
    return node;
}

void CommodityVolatilityConfig::populateQuotes() {

    for (auto config : volatilityConfig_) {
        // The quotes depend on the type of volatility structure that has been configured.
        if (auto vc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(config)) {
            quotes_.push_back(vc->quote());
        } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(config)) {
            auto qs = vc->quotes();
            quotes_.insert(quotes_.end(), qs.begin(), qs.end());
        } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<VolatilitySurfaceConfig>(config)) {
            string curveName = instrumentType() == MarketDatum::InstrumentType::COMMODITY_CALENDAR_SPREAD_OPTION
                                   ? calendarSpreadUnderlyingName_
                                   : curveID_;
            string quoteType = to_string(vc->quoteType());
            string stem = to_string(instrumentType()) + "/" + quoteType + "/" + curveName + "/";
            if (instrumentType_ == MarketDatum::InstrumentType::COMMODITY_CALENDAR_SPREAD_OPTION)
                stem += std::to_string(calendarSpreadOffset_) + "/";
            stem += currency_ + "/";
            for (const pair<string, string>& p : vc->quotes()) {
                string q = stem + p.first + "/" + p.second;
                if (!quoteSuffix_.empty())
                    q += "/" + quoteSuffix_;
                quotes_.push_back(q);
            }
        }
    }
}

} // namespace data
} // namespace ore
