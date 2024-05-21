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
using QuantExt::PriceTermStructure;

namespace ore {
namespace data {

CommodityOption::CommodityOption() : VanillaOptionTrade(AssetClass::COM) { tradeType_ = "CommodityOption"; }

CommodityOption::CommodityOption(const Envelope& env, const OptionData& optionData, const string& commodityName,
                                 const string& currency, Real quantity, TradeStrike strike,
                                 const boost::optional<bool>& isFuturePrice, const Date& futureExpiryDate)
    : VanillaOptionTrade(env, AssetClass::COM, optionData, commodityName, currency, quantity, strike),
      isFuturePrice_(isFuturePrice), futureExpiryDate_(futureExpiryDate) {
    tradeType_ = "CommodityOption";
}

void CommodityOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy, assuming Commodity follows the Equity template
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = strike_.value();
    additionalData_["strikeCurrency"] = currency_;    

    // Checks
    QL_REQUIRE((strike_.value() > 0) || close_enough(strike_.value(),0.0), "Commodity option requires a non-negative strike");
    if (close_enough(strike_.value(), 0.0)) {
        strike_.setValue(0.0);
    }

    // This is called in VanillaOptionTrade::build(), but we want to call it first here,
    // in case the build fails before it reaches VanillaOptionTrade::build()
    VanillaOptionTrade::setNotionalAndCurrencies();

    // Populate the index_ in case the option is automatic exercise.
    // Intentionally use null calendar because we will ask for index value on the expiry date without adjustment.
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    index_ = *market->commodityIndex(assetName_, engineFactory->configuration(MarketContext::pricing));
    if (!isFuturePrice_ || *isFuturePrice_) {

        // Assume future price if isFuturePrice_ is not explicitly set or if it is and true.

        auto index = *market->commodityIndex(assetName_, engineFactory->configuration(MarketContext::pricing));

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

        // Clone the index with the relevant expiry date.
        index_ = index->clone(expiryDate);

        // Set the VanillaOptionTrade forwardDate_ if the index is a CommodityFuturesIndex - we possibly still have a 
        // CommoditySpotIndex at this point so check. Also, will only work for European exercise.
        auto et = parseExerciseType(option_.style());
        if (et == Exercise::European && QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(index_)) {
            forwardDate_ = expiryDate;
        }

    }

    VanillaOptionTrade::build(engineFactory);

    // LOG the volatility if the trade expiry date is in the future.
    if (expiryDate_ > Settings::instance().evaluationDate()) {
        DLOG("Implied vol for " << tradeType_ << " on " << assetName_ << " with expiry " << expiryDate_
                                << " and strike " << strike_.value() << " is "
                                << market->commodityVolatility(assetName_)->blackVol(expiryDate_, strike_.value()));
    }
}

std::map<AssetClass, std::set<std::string>>
CommodityOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({assetName_})}};
}

void CommodityOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* commodityNode = XMLUtils::getChildNode(node, "CommodityOptionData");
    QL_REQUIRE(commodityNode, "A commodity option needs a 'CommodityOptionData' node");

    option_.fromXML(XMLUtils::getChildNode(commodityNode, "OptionData"));

    assetName_ = XMLUtils::getChildValue(commodityNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityNode, "Currency", true);
    strike_.fromXML(commodityNode);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityNode, "Quantity", true);

    isFuturePrice_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "IsFuturePrice"))
        isFuturePrice_ = parseBool(XMLUtils::getNodeValue(n));

    futureExpiryDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "FutureExpiryDate"))
        futureExpiryDate_ = parseDate(XMLUtils::getNodeValue(n));
}

XMLNode* CommodityOption::toXML(XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* eqNode = doc.allocNode("CommodityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));

    XMLUtils::addChild(doc, eqNode, "Name", assetName_);
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::appendNode(eqNode, strike_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    if (isFuturePrice_)
        XMLUtils::addChild(doc, eqNode, "IsFuturePrice", *isFuturePrice_);

    if (futureExpiryDate_ != Date())
        XMLUtils::addChild(doc, eqNode, "FutureExpiryDate", to_string(futureExpiryDate_));

    return node;
}

} // namespace data
} // namespace ore
