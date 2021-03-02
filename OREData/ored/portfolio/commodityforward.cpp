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

#include <boost/make_shared.hpp>

#include <ql/errors.hpp>
#include <qle/instruments/commodityforward.hpp>

#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/to_string.hpp>

using QuantExt::CommodityFuturesIndex;
using QuantExt::CommodityIndex;
using QuantExt::CommoditySpotIndex;
using QuantExt::PriceTermStructure;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CommodityForward::CommodityForward() : Trade("CommodityForward"), quantity_(0.0), strike_(0.0) {}

CommodityForward::CommodityForward(const Envelope& envelope, const string& position, const string& commodityName,
                                   const string& currency, Real quantity, const string& maturityDate, Real strike,
                                   const boost::optional<bool>& isFuturePrice, const Date& futureExpiryDate,
                                   const boost::optional<bool>& physicallySettled,
                                   const Date& paymentDate)
    : Trade("CommodityForward", envelope), position_(position), commodityName_(commodityName), currency_(currency),
      quantity_(quantity), maturityDate_(maturityDate), strike_(strike), isFuturePrice_(isFuturePrice),
      futureExpiryDate_(futureExpiryDate), physicallySettled_(physicallySettled), paymentDate_(paymentDate) {}

void CommodityForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Get the price curve for the commodity.
    const boost::shared_ptr<Market>& market = engineFactory->market();
    Handle<PriceTermStructure> priceCurve = market->commodityPriceCurve(commodityName_,
        engineFactory->configuration(MarketContext::pricing));

    // Create the underlying commodity index for the forward
    boost::shared_ptr<CommodityIndex> index;
    maturity_ = parseDate(maturityDate_);
    if (isFuturePrice_ && *isFuturePrice_) {
        Date expiryDate = futureExpiryDate_ == Date() ? maturity_ : futureExpiryDate_;
        index = boost::make_shared<CommodityFuturesIndex>(commodityName_, expiryDate,
            NullCalendar(), true, priceCurve);
    } else {
        index = boost::make_shared<CommoditySpotIndex>(commodityName_, NullCalendar(), priceCurve);
    }

    Date paymentDate = paymentDate_;
    bool physicallySettled = true;
    if (physicallySettled_ && !(*physicallySettled_)) {
        // If cash settled and given a payment date that is not greater than the maturity date, set it equal to the 
        // maturity date and log a warning to continue processing.
        physicallySettled = false;
        if (paymentDate_ != Date() && paymentDate_ < maturity_) {
            WLOG("Commodity forward " << id() << " has payment date (" << io::iso_date(paymentDate_) <<
                ") before the maturity date (" << io::iso_date(maturity_) << "). Setting payment date" <<
                " equal to the maturity date.");
            paymentDate = maturity_;
        }
    } else {
        // If physically settled and given a payment date, log a warning that it is ignored.
        if (paymentDate_ != Date()) {
            WLOG("Commodity forward " << id() << " supplies a payment date (" << io::iso_date(paymentDate_) <<
                ") but is physically settled. The payment date is ignored.");
            paymentDate = Date();
        }
    }

    // Create the commodity forward instrument
    Currency currency = parseCurrency(currency_);
    Position::Type position = parsePositionType(position_);
    boost::shared_ptr<Instrument> commodityForward = boost::make_shared<QuantExt::CommodityForward>(
        index, currency, position, quantity_, maturity_, strike_, physicallySettled, paymentDate);

    // Pricing engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<CommodityForwardEngineBuilder> commodityForwardEngineBuilder =
        boost::dynamic_pointer_cast<CommodityForwardEngineBuilder>(builder);
    commodityForward->setPricingEngine(commodityForwardEngineBuilder->engine(currency));

    // set up other Trade details
    instrument_ = boost::make_shared<VanillaInstrument>(commodityForward);
    npvCurrency_ = currency_;

    // notional_ = strike_ * quantity_;
    notional_ = Null<Real>(); // is handled by override of notional()
    notionalCurrency_ = currency_;
}

QuantLib::Real CommodityForward::notional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument()->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "currentNotional not provided"))
            ALOG("error when retrieving notional: " << e.what());
    }
    // if not provided, return null
    return Null<Real>();
}

std::map<AssetClass, std::set<std::string>>
CommodityForward::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({commodityName_})}};
}

void CommodityForward::fromXML(XMLNode* node) {

    Trade::fromXML(node);
    XMLNode* commodityDataNode = XMLUtils::getChildNode(node, "CommodityForwardData");

    position_ = XMLUtils::getChildValue(commodityDataNode, "Position", true);
    commodityName_ = XMLUtils::getChildValue(commodityDataNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityDataNode, "Currency", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityDataNode, "Quantity", true);
    maturityDate_ = XMLUtils::getChildValue(commodityDataNode, "Maturity", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityDataNode, "Strike", true);

    isFuturePrice_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "IsFuturePrice"))
        isFuturePrice_ = parseBool(XMLUtils::getNodeValue(n));

    futureExpiryDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "FutureExpiryDate"))
        futureExpiryDate_ = parseDate(XMLUtils::getNodeValue(n));

    physicallySettled_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "PhysicallySettled"))
        physicallySettled_ = parseBool(XMLUtils::getNodeValue(n));

    paymentDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "PaymentDate"))
        paymentDate_ = parseDate(XMLUtils::getNodeValue(n));
}

XMLNode* CommodityForward::toXML(XMLDocument& doc) {

    XMLNode* node = Trade::toXML(doc);
    XMLNode* commodityDataNode = doc.allocNode("CommodityForwardData");
    XMLUtils::appendNode(node, commodityDataNode);

    XMLUtils::addChild(doc, commodityDataNode, "Position", position_);
    XMLUtils::addChild(doc, commodityDataNode, "Maturity", maturityDate_);
    XMLUtils::addChild(doc, commodityDataNode, "Name", commodityName_);
    XMLUtils::addChild(doc, commodityDataNode, "Currency", currency_);
    XMLUtils::addChild(doc, commodityDataNode, "Strike", strike_);
    XMLUtils::addChild(doc, commodityDataNode, "Quantity", quantity_);

    if (isFuturePrice_)
        XMLUtils::addChild(doc, commodityDataNode, "IsFuturePrice", *isFuturePrice_);

    if (futureExpiryDate_ != Date())
        XMLUtils::addChild(doc, commodityDataNode, "FutureExpiryDate", to_string(futureExpiryDate_));

    if (physicallySettled_)
        XMLUtils::addChild(doc, commodityDataNode, "PhysicallySettled", *physicallySettled_);

    if (paymentDate_ != Date())
        XMLUtils::addChild(doc, commodityDataNode, "PaymentDate", to_string(paymentDate_));

    return node;
}

} // namespace data
} // namespace ore
