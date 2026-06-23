/*
 Copyright (C) 2026 AcadiaSoft Inc
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

#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/intradaypowerforward.hpp>
#include <ored/portfolio/intradaypowerforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

using QuantExt::IntradayPowerIndex;
using QuantExt::IntradayPowerPriceTermStructure;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

IntradayPowerForward::IntradayPowerForward() : Trade("IntradayPowerForward"), quantity_(0.0), strike_(0.0) {}

IntradayPowerForward::IntradayPowerForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                         const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                         const QuantLib::Date& deliveryDate, int deliveryStart, int deliveryEnd, bool isDstHour,
                         QuantLib::Real strike, const QuantLib::ext::optional<bool>& physicallySettled ,
                         const QuantLib::Date& paymentDate)
    : Trade("IntradayPowerForward", envelope), position_(position), commodityName_(commodityName), currency_(currency),
      quantity_(quantity), maturityDate_(maturityDate), strike_(strike), deliveryDate_(deliveryDate), deliveryStart_(deliveryStart),
      deliveryEnd_(deliveryEnd), isDstHour_(isDstHour), physicallySettled_(physicallySettled), paymentDate_(paymentDate),
      fxIndex_(""), payCcy_(currency) {}

void IntradayPowerForward::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Commodity");
    additionalData_["isdaBaseProduct"] = string("IntradayPowerForward");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    // if TradePnLCurrency is not given, npv currency is set to settlement currency ( <PayCcy> if exists, else <Currency> )
    npvCurrency_ = envelope().additionalField("TradePnLCurrency", false, payCcy_ != "" ? payCcy_ : currency_);
    Currency npvCurrency = parseCurrency(npvCurrency_);
    notional_ = strike_ * quantity_;    
    notionalCurrency_ = npvCurrency_;

    // Create the underlying commodity index for the forward
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex = nullptr;

    maturity_ = parseDate(maturityDate_);
    DLOG("Building Intraday Power Forward with maturity " << io::iso_date(maturity_) << " and delivery date " << io::iso_date(deliveryDate_));
    auto index = *market->intradayPowerIndex(commodityName_, engineFactory->configuration(MarketContext::pricing));
    QL_REQUIRE(index, "No intraday power index found for " << commodityName_ << " with configuration " << engineFactory->configuration(MarketContext::pricing));
    maturity_ = index->fixingCalendar().adjust(maturity_, Preceding);
    DLOG("Adjusted maturity to " << io::iso_date(maturity_));
    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = strike_;

    Handle<IntradayPowerPriceTermStructure> priceCurve = engineFactory->market()->intradayPowerPriceCurve(
        commodityName_, engineFactory->configuration(MarketContext::pricing));
    
    auto underlyingCcy = priceCurve->currency();
    //notional ccy is in underlying currency
    notionalCurrency_ = underlyingCcy.code();
    additionalData_["strikeCurrency"] = underlyingCcy.code();
    
    if (payCcy_ != "") {
        additionalData_["settlementCurrency"] = payCcy_;
        additionalData_["fixingDate"] = fixingDate_;
        additionalData_["fxIndex"] = fxIndex;
    } // if <PayCcy> is not given, set settlementCurrenc to <Currency>
    else if (fixingDate_ != Date()) {
        additionalData_["settlementCurrency"] = currency_;
        additionalData_["fixingDate"] = fixingDate_;
        additionalData_["fxIndex"] = fxIndex;
    }
    std::vector<QuantExt::LoadFactor> factors(1, QuantExt::LoadFactor {deliveryStart_, deliveryEnd_, quantity_, isDstHour_});

    auto loadProfile = QuantLib::ext::make_shared<QuantExt::IntradayPowerLoadProfile>(factors);

    index = index->clone(deliveryDate_, loadProfile);

    Date paymentDate = paymentDate_;
    bool physicallySettled = physicallySettled_.value_or(true);
    
    if(physicallySettled && paymentDate != Date()) {
        WLOG("Intraday power forward " << id() << " is physically settled but has a payment date (" << io::iso_date(paymentDate_)
                                  << "). Setting physicallySettled to false and ignoring the payment date.");
        paymentDate = Date();
    }

    if (!physicallySettled && (paymentDate == Date() || paymentDate < maturity_)) {
        WLOG("Intraday power forward " << id() << " is cash settled but has no payment daye or  a payment date (" << io::iso_date(paymentDate_)
                                  << ") before the maturity date (" << io::iso_date(maturity_)
                                  << "). Setting payment date equal to the maturity date.");
        paymentDate = maturity_;
    } 

    TLOG("intraday power forward " << id() << " paymentDate is " << paymentDate);
    
    for(const auto& intraDayFixingNames : index->intraDayIndexNames()) {
        DLOG("Adding required fixing date " << io::iso_date(maturity_) << " for index " << intraDayFixingNames);
        requiredFixings_.addFixingDate(maturity_, intraDayFixingNames, paymentDate == Date() ? maturity_ : paymentDate);
    }
    // Create the commodity forward instrument
    Currency currency = parseCurrency(currency_);
    Position::Type position = parsePositionType(position_);
    auto payCcy = Currency();
    if (!fxIndex_.empty()) {
        // if <PayCcy> is not given, settlement currency is set to <Currrency>
        if (payCcy_ != "")
            payCcy = parseCurrency(payCcy_);
        else
            payCcy = currency;
        requiredFixings_.addFixingDate(fixingDate_, fxIndex_, paymentDate);
        fxIndex = buildFxIndex(fxIndex_, payCcy.code(), underlyingCcy.code(), engineFactory->market(),
                               engineFactory->configuration(MarketContext::pricing));
    }
    else if (underlyingCcy.code() == currency.code())// we need a pay ccy for the pricing engine
        payCcy = currency;
    else
        QL_FAIL("Quanto cashflow is not supported. Underlying commodity index currency "<< underlyingCcy.code() <<" is diffferent than the settlement currency "<<currency);
    
    QuantLib::ext::shared_ptr<Instrument> commodityForward = QuantLib::ext::make_shared<QuantExt::CommodityForward>(
        index, underlyingCcy, position, quantity_, maturity_, strike_, physicallySettled, paymentDate, payCcy, deliveryDate_,
        fxIndex);

    if (paymentDate != Date())
        maturity_ = std::max(maturity_, paymentDate);
    if (maturity_ == paymentDate)
        maturityType_ = "Payment Date";

    // Pricing engine
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<IntradayPowerForwardEngineBuilder> commodityForwardEngineBuilder =
        QuantLib::ext::dynamic_pointer_cast<IntradayPowerForwardEngineBuilder>(builder);
    
    commodityForward->setPricingEngine(
        commodityForwardEngineBuilder->engine(payCcy, npvCurrency,
        envelope().additionalField("discount_curve", false, std::string())));
    setSensitivityTemplate(*commodityForwardEngineBuilder);
    addProductModelEngine(*commodityForwardEngineBuilder);

    // set up other Trade details
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(commodityForward);
}

Real IntradayPowerForward::currentNotional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument(true)->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "currentNotional not provided"))
            ALOG("error when retrieving notional: " << e.what());
    }
    // if not provided, return null
    return Null<Real>();
}

std::map<AssetClass, std::set<std::string>>
IntradayPowerForward::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({commodityName_})}};
}

void IntradayPowerForward::fromXML(XMLNode* node) {

    Trade::fromXML(node);
    XMLNode* commodityDataNode = XMLUtils::getChildNode(node, "IntradayPowerForwardData");

    position_ = XMLUtils::getChildValue(commodityDataNode, "Position", true);
    commodityName_ = XMLUtils::getChildValue(commodityDataNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityDataNode, "Currency", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityDataNode, "Quantity", true);
    maturityDate_ = XMLUtils::getChildValue(commodityDataNode, "Maturity", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityDataNode, "Strike", true);

    deliveryDate_ = parseDate(XMLUtils::getChildValue(commodityDataNode, "DeliveryDate", true));
    deliveryStart_ = XMLUtils::getChildValueAsInt(commodityDataNode, "DeliveryStart", true);
    deliveryEnd_ = XMLUtils::getChildValueAsInt(commodityDataNode, "DeliveryEnd", true);
    isDstHour_ = XMLUtils::getChildValueAsBool(commodityDataNode, "IsDstHour", false, false);
    
    physicallySettled_ = QuantLib::ext::nullopt;
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "PhysicallySettled"))
        physicallySettled_ = parseBool(XMLUtils::getNodeValue(n));

    paymentDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityDataNode, "PaymentDate"))
        paymentDate_ = parseDate(XMLUtils::getNodeValue(n));

    if (XMLNode* settlementDataNode = XMLUtils::getChildNode(commodityDataNode, "SettlementData")) {
        // this node is used to provide data for NDF. This includes a fixing date, a settlement currency and the
        // quote/settlement fx index.
        payCcy_ = XMLUtils::getChildValue(settlementDataNode, "PayCurrency", false);
        fxIndex_ = XMLUtils::getChildValue(settlementDataNode, "FXIndex", true);
        fixingDate_ = parseDate(XMLUtils::getChildValue(settlementDataNode, "FixingDate", true));
    }
}

XMLNode* IntradayPowerForward::toXML(XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);
    XMLNode* commodityDataNode = doc.allocNode("IntradayPowerForwardData");
    XMLUtils::appendNode(node, commodityDataNode);

    XMLUtils::addChild(doc, commodityDataNode, "Position", position_);
    XMLUtils::addChild(doc, commodityDataNode, "Maturity", maturityDate_);
    XMLUtils::addChild(doc, commodityDataNode, "Name", commodityName_);
    XMLUtils::addChild(doc, commodityDataNode, "Currency", currency_);
    XMLUtils::addChild(doc, commodityDataNode, "Strike", strike_);
    XMLUtils::addChild(doc, commodityDataNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, commodityDataNode, "DeliveryDate", to_string(deliveryDate_));
    XMLUtils::addChild(doc, commodityDataNode, "DeliveryStart", to_string(deliveryStart_));
    XMLUtils::addChild(doc, commodityDataNode, "DeliveryEnd", to_string(deliveryEnd_));
    XMLUtils::addChild(doc, commodityDataNode, "IsDstHour", isDstHour_);
    if (physicallySettled_)
        XMLUtils::addChild(doc, commodityDataNode, "PhysicallySettled", *physicallySettled_);

    if (paymentDate_ != Date())
        XMLUtils::addChild(doc, commodityDataNode, "PaymentDate", to_string(paymentDate_));

    if (fixingDate_ != Date()) { // NDF
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
