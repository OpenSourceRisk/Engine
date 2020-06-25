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

#include <ored/portfolio/commodityoption.hpp>

#include <boost/make_shared.hpp>

#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/commodityindex.hpp>

#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

using namespace std;
using namespace QuantLib;
using QuantExt::CommodityFuturesIndex;
using QuantExt::CommoditySpotIndex;
using QuantExt::PriceTermStructure;

namespace ore {
namespace data {

CommodityOption::CommodityOption() : VanillaOptionTrade(AssetClass::COM) { tradeType_ = "CommodityOption"; }

CommodityOption::CommodityOption(const Envelope& env, const OptionData& optionData, const string& commodityName,
                                 const string& currency, Real strike, Real quantity,
                                 const boost::optional<bool>& isFuturePrice, const Date& futureExpiryDate)
    : VanillaOptionTrade(env, AssetClass::COM, optionData, commodityName, currency, strike, quantity),
      isFuturePrice_(isFuturePrice), futureExpiryDate_(futureExpiryDate) {
    tradeType_ = "CommodityOption";
}

void CommodityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Checks
    QL_REQUIRE(quantity_ > 0, "Commodity option requires a positive quatity");
    QL_REQUIRE(strike_ > 0, "Commodity option requires a positive strike");

    // Get the price curve for the commodity.
    const boost::shared_ptr<Market>& market = engineFactory->market();
    Handle<PriceTermStructure> priceCurve =
        market->commodityPriceCurve(assetName_, engineFactory->configuration(MarketContext::pricing));

    // Populate the index_ in case the option is automatic exercise.
    // Intentionally use null calendar because we will ask for index value on the expiry date without adjustment.
    if (!isFuturePrice_ || *isFuturePrice_) {

        // Assume future price if isFuturePrice_ is not explicitly set of if it is and true.

        // If we are given an explicit future contract expiry date, use it, otherwise use option's expiry.
        Date expiryDate;
        if (futureExpiryDate_ != Date()) {
            expiryDate = futureExpiryDate_;
        } else {
            // Get the expiry date of the option. This is the expiry date of the commodity future index.
            const vector<string>& expiryDates = option_.exerciseDates();
            QL_REQUIRE(expiryDates.size() == 1,
                       "Expected exactly one expiry date for CommodityOption but got " << expiryDates.size() << ".");
            expiryDate = parseDate(expiryDates[0]);
        }

        index_ = boost::make_shared<CommodityFuturesIndex>(assetName_, expiryDate, NullCalendar(), priceCurve);

    } else {
        // If the underlying is a commodity spot, create a spot index.
        index_ = boost::make_shared<CommoditySpotIndex>(assetName_, NullCalendar(), priceCurve);
    }

    VanillaOptionTrade::build(engineFactory);

    // LOG the volatility if the trade expiry date is in the future.
    if (expiryDate_ > Settings::instance().evaluationDate()) {
        DLOG("Implied vol for " << tradeType_ << " on " << assetName_ << " with expiry " << expiryDate_
                                << " and strike " << strike_ << " is "
                                << market->commodityVolatility(assetName_)->blackVol(expiryDate_, strike_));
    }
}

std::map<AssetClass, std::set<std::string>> CommodityOption::underlyingIndices() const {
    return {{AssetClass::COM, std::set<std::string>({assetName_})}};
}

void CommodityOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* commodityNode = XMLUtils::getChildNode(node, "CommodityOptionData");
    QL_REQUIRE(commodityNode, "A commodity option needs a 'CommodityOptionData' node");

    option_.fromXML(XMLUtils::getChildNode(commodityNode, "OptionData"));

    assetName_ = XMLUtils::getChildValue(commodityNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityNode, "Quantity", true);

    isFuturePrice_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "IsFuturePrice"))
        isFuturePrice_ = parseBool(XMLUtils::getNodeValue(n));

    futureExpiryDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "FutureExpiryDate"))
        futureExpiryDate_ = parseDate(XMLUtils::getNodeValue(n));
}

XMLNode* CommodityOption::toXML(XMLDocument& doc) {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* eqNode = doc.allocNode("CommodityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));

    XMLUtils::addChild(doc, eqNode, "Name", assetName_);
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    if (isFuturePrice_)
        XMLUtils::addChild(doc, eqNode, "IsFuturePrice", *isFuturePrice_);

    if (futureExpiryDate_ != Date())
        XMLUtils::addChild(doc, eqNode, "FutureExpiryDate", to_string(futureExpiryDate_));

    return node;
}

} // namespace data
} // namespace ore
