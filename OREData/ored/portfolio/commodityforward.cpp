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
#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/marketdata.hpp>

using QuantExt::CommodityIndex;
using QuantExt::PriceTermStructure;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CommodityForward::CommodityForward() : Trade("CommodityForward"), quantity_(0.0), strike_(0.0) {}

CommodityForward::CommodityForward(const Envelope& envelope, const string& position, const string& commodityName,
                                   const string& currency, Real quantity, const string& maturityDate, Real strike)
    : Trade("CommodityForward", envelope), position_(position), commodityName_(commodityName), currency_(currency),
      quantity_(quantity), maturityDate_(maturityDate), strike_(strike),
      fixingDate_(Date()), fxIndex_(""), payCcy_(currency){}

CommodityForward::CommodityForward(const Envelope& envelope, const string& position, const string& commodityName,
                                   const string& currency, Real quantity, const string& maturityDate, Real strike,
                                   const Date& futureExpiryDate, const boost::optional<bool>& physicallySettled,
                                   const Date& paymentDate)
    : Trade("CommodityForward", envelope), position_(position), commodityName_(commodityName), currency_(currency),
      quantity_(quantity), maturityDate_(maturityDate), strike_(strike), isFuturePrice_(true),
      futureExpiryDate_(futureExpiryDate), physicallySettled_(physicallySettled), paymentDate_(paymentDate),
      fixingDate_(Date()), fxIndex_(""), payCcy_(currency) {}

CommodityForward::CommodityForward(const Envelope& envelope, const string& position, const string& commodityName,
                                   const string& currency, Real quantity, const string& maturityDate, Real strike,
                                   const Period& futureExpiryOffset, const Calendar& offsetCalendar,
                                   const boost::optional<bool>& physicallySettled,
                                   const Date& paymentDate)
    : Trade("CommodityForward", envelope), position_(position), commodityName_(commodityName), currency_(currency),
      quantity_(quantity), maturityDate_(maturityDate), strike_(strike), isFuturePrice_(true),
      futureExpiryOffset_(futureExpiryOffset), offsetCalendar_(offsetCalendar), physicallySettled_(physicallySettled),
      paymentDate_(paymentDate),
      fixingDate_(Date()), fxIndex_(""), payCcy_(currency){}

void CommodityForward::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Commodity");
    additionalData_["isdaBaseProduct"] = string("Forward");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

    // Create the underlying commodity index for the forward
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex = nullptr;

    npvCurrency_ = fixingDate_ == Date() ? currency_ : payCcy_;

    // notional_ = strike_ * quantity_;
    notional_ = strike_ * quantity_;
    notionalCurrency_ = currency_;

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = strike_;
    additionalData_["strikeCurrency"] = currency_;
    if (fixingDate_ != Date()) {
        additionalData_["settlementCurrency"] = payCcy_;
        additionalData_["fixingDate"] = fixingDate_;
        additionalData_["fxIndex"] = fxIndex;
    }

    maturity_ = parseDate(maturityDate_);
    auto index = *market->commodityIndex(commodityName_, engineFactory->configuration(MarketContext::pricing));
    bool isFutureAccordingToConventions = InstrumentConventions::instance().conventions()->has(commodityName_, Convention::Type::CommodityFuture);

    // adjust the maturity date if not a valid fixing date for the index
    maturity_ = index->fixingCalendar().adjust(maturity_, Preceding);

    if ((isFuturePrice_ && *isFuturePrice_) || isFutureAccordingToConventions) {

        // Get the commodity index from the market.
        index = *market->commodityIndex(commodityName_, engineFactory->configuration(MarketContext::pricing));

        // May have been given an explicit future expiry date or an offset and calendar or neither.
        Date expiryDate = maturity_;
        if (futureExpiryDate_ != Date()) {
            expiryDate = futureExpiryDate_;
        } else if (futureExpiryOffset_ != Period()) {
            Calendar cal = offsetCalendar_.empty() ? NullCalendar() : offsetCalendar_;
            expiryDate = cal.advance(maturity_, futureExpiryOffset_);
        }

        // Clone the index with the relevant expiry date.
        index = index->clone(expiryDate);
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

    // add required commodity fixing
    DLOG("commodity forward " << id() << " paymentDate is " << paymentDate);
    requiredFixings_.addFixingDate(maturity_, index->name(),
                                   paymentDate == Date() ? maturity_ : paymentDate);

    // Create the commodity forward instrument
    Currency currency = parseCurrency(currency_);
    Position::Type position = parsePositionType(position_);
    auto payCcy = Currency();
    if(!fxIndex_.empty()){
        payCcy = parseCurrency(payCcy_);
        requiredFixings_.addFixingDate( fixingDate_, fxIndex_, paymentDate);
        fxIndex = buildFxIndex(fxIndex_, currency.code(), payCcy.code(), engineFactory->market(),
                               engineFactory->configuration(MarketContext::pricing));
        npvCurrency_ = payCcy_;
    }
    QuantLib::ext::shared_ptr<Instrument> commodityForward = QuantLib::ext::make_shared<QuantExt::CommodityForward>(
        index, currency, position, quantity_, maturity_, strike_, physicallySettled, paymentDate,
        payCcy, fixingDate_, fxIndex);

    // Pricing engine
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<CommodityForwardEngineBuilder> commodityForwardEngineBuilder =
        QuantLib::ext::dynamic_pointer_cast<CommodityForwardEngineBuilder>(builder);
    commodityForward->setPricingEngine(commodityForwardEngineBuilder->engine(currency)); // the engine accounts for NDF if settlement data are present
    setSensitivityTemplate(*commodityForwardEngineBuilder);

    // set up other Trade details
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(commodityForward);
}

Real CommodityForward::notional() const { return notional_; }

std::map<AssetClass, std::set<std::string>>
CommodityForward::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
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

    // If not given an explicit future expiry date, check for offset and calendar.
    if (futureExpiryDate_ == Date()) {
        futureExpiryOffset_ = Period();
        if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "FutureExpiryOffset"))
            futureExpiryOffset_ = parsePeriod(XMLUtils::getNodeValue(n));

        offsetCalendar_ = Calendar();
        if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "FutureExpiryOffsetCalendar"))
            offsetCalendar_ = parseCalendar(XMLUtils::getNodeValue(n));
    }

    physicallySettled_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "PhysicallySettled"))
        physicallySettled_ = parseBool(XMLUtils::getNodeValue(n));

    paymentDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "PaymentDate"))
        paymentDate_ = parseDate(XMLUtils::getNodeValue(n));

    if (XMLNode* settlementDataNode = XMLUtils::getChildNode(commodityDataNode, "SettlementData")) {
        // this node is used to provide data for NDF. This includes a fixing date, a settlement currency and the
        // quote/settlement fx index.
        payCcy_ = XMLUtils::getChildValue(settlementDataNode, "PayCurrency", true);
        fxIndex_ = XMLUtils::getChildValue(settlementDataNode, "FXIndex", true);
        fixingDate_ = parseDate(XMLUtils::getChildValue(settlementDataNode, "FixingDate", true));
    }
}

XMLNode* CommodityForward::toXML(XMLDocument& doc) const {

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

    if (futureExpiryOffset_ != Period())
        XMLUtils::addChild(doc, commodityDataNode, "FutureExpiryOffset", to_string(futureExpiryOffset_));

    if (offsetCalendar_ != Calendar())
        XMLUtils::addChild(doc, commodityDataNode, "FutureExpiryOffsetCalendar", to_string(offsetCalendar_));

    if (physicallySettled_)
        XMLUtils::addChild(doc, commodityDataNode, "PhysicallySettled", *physicallySettled_);

    if (paymentDate_ != Date())
        XMLUtils::addChild(doc, commodityDataNode, "PaymentDate", to_string(paymentDate_));

    if(fixingDate_!=Date()){ //NDF
        XMLNode* settlementDataNode = doc.allocNode("SettlementData");
        XMLUtils::appendNode(commodityDataNode, settlementDataNode);
        XMLUtils::addChild(doc, settlementDataNode, "PayCurrency", payCcy_);
        XMLUtils::addChild(doc, settlementDataNode, "FXIndex", fxIndex_);
        XMLUtils::addChild(doc, settlementDataNode, "FixingDate", to_string(fixingDate_));
    }

    return node;
}

} // namespace data
} // namespace ore
