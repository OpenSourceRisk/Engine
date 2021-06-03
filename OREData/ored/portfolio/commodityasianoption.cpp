/*
 Copyright (C) 2020 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/portfolio/commodityasianoption.hpp>

#include <boost/make_shared.hpp>

#include <qle/indexes/commodityindex.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <ql/errors.hpp>

#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

void CommodityAsianOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    // Checks
    QL_REQUIRE(quantity_ > 0, "Commodity Asian option requires a positive quatity");
    QL_REQUIRE(strike_ >= 0, "Commodity Asian option requires a strike >= 0");

    // Get the price curve for the commodity.
    const boost::shared_ptr<Market>& market = engineFactory->market();
    Handle<QuantExt::PriceTermStructure> priceCurve =
        market->commodityPriceCurve(assetName_, engineFactory->configuration(MarketContext::pricing));

    // Populate the index_ in case the option is automatic exercise.
    // Intentionally use null calendar because we will ask for index value on the expiry date without adjustment.
    if (!isFuturePrice_ || *isFuturePrice_) {
        // Assume future price if isFuturePrice_ is not explicitly set or if it is and true.

        // If we are given an explicit future contract expiry date, use it, otherwise use option's expiry.
        Date expiryDate;
        if (futureExpiryDate_ != Date()) {
            expiryDate = futureExpiryDate_;
        } else {
            // Get the expiry date of the option. This is the expiry date of the commodity future index.
            const vector<string>& expiryDates = option_.exerciseDates();
            QL_REQUIRE(expiryDates.size() == 1, "Expected exactly one expiry date for CommodityAsianOption but got "
                                                    << expiryDates.size() << ".");
            expiryDate = parseDate(expiryDates[0]);
        }

        index_ = boost::make_shared<QuantExt::CommodityFuturesIndex>(assetName_, expiryDate, NullCalendar(), priceCurve);
    } else {
        // If the underlying is a commodity spot, create a spot index.
        index_ = boost::make_shared<QuantExt::CommoditySpotIndex>(assetName_, NullCalendar(), priceCurve);
    }

    AsianOptionTrade::build(engineFactory);
}

std::map<AssetClass, std::set<std::string>> CommodityAsianOption::underlyingIndices() const {
    return {{AssetClass::COM, std::set<std::string>({assetName_})}};
}

void CommodityAsianOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* commodityNode = XMLUtils::getChildNode(node, "CommodityAsianOptionData");
    QL_REQUIRE(commodityNode, "A commodity Asian option needs a 'CommodityAsianOptionData' node");

    option_.fromXML(XMLUtils::getChildNode(commodityNode, "OptionData"));
    QL_REQUIRE(option_.payoffType() == "Asian", "Expected PayoffType Asian for CommodityAsianOption.");
    XMLNode* asianNode = XMLUtils::getChildNode(commodityNode, "AsianData");
    asianData_.fromXML(asianNode);

    XMLNode* scheduleDataNode = XMLUtils::getChildNode(commodityNode, "ScheduleData");
    scheduleData_.fromXML(scheduleDataNode);

    assetName_ = XMLUtils::getChildValue(commodityNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityNode, "Currency", true);
    // Require explicit Strike
    strike_ = XMLUtils::getChildValueAsDouble(commodityNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityNode, "Quantity", true);

    isFuturePrice_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "IsFuturePrice"))
        isFuturePrice_ = parseBool(XMLUtils::getNodeValue(n));

    futureExpiryDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "FutureExpiryDate"))
        futureExpiryDate_ = parseDate(XMLUtils::getNodeValue(n));
}

XMLNode* CommodityAsianOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);

    XMLNode* comNode = doc.allocNode("CommodityAsianOptionData");
    XMLUtils::appendNode(node, comNode);

    XMLUtils::appendNode(comNode, option_.toXML(doc));
    XMLUtils::appendNode(comNode, asianData_.toXML(doc));
    XMLUtils::appendNode(comNode, scheduleData_.toXML(doc));

    XMLUtils::addChild(doc, comNode, "Name", assetName_);
    XMLUtils::addChild(doc, comNode, "Currency", currency_);
    XMLUtils::addChild(doc, comNode, "Strike", strike_);
    XMLUtils::addChild(doc, comNode, "Quantity", quantity_);

    if (isFuturePrice_)
        XMLUtils::addChild(doc, comNode, "IsFuturePrice", *isFuturePrice_);

    if (futureExpiryDate_ != Date())
        XMLUtils::addChild(doc, comNode, "FutureExpiryDate", to_string(futureExpiryDate_));

    return node;
}


} // namespace data
} // namespace ore
