/*
  Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/commodityapo.hpp>
#include <ored/portfolio/commoditydigitalapo.hpp>
#include <ql/instruments/compositeinstrument.hpp>

using namespace QuantExt;
using namespace QuantLib;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace data {

CommodityDigitalAveragePriceOption::CommodityDigitalAveragePriceOption(
    const Envelope& envelope, const OptionData& optionData, Real strike, Real digitalCashPayoff, const string& currency,
    const string& name, CommodityPriceType priceType, const string& startDate, const string& endDate,
    const string& paymentCalendar, const string& paymentLag, const string& paymentConvention,
    const string& pricingCalendar, const string& paymentDate, Real gearing, Spread spread,
    CommodityQuantityFrequency commodityQuantityFrequency, CommodityPayRelativeTo commodityPayRelativeTo,
    QuantLib::Natural futureMonthOffset, QuantLib::Natural deliveryRollDays, bool includePeriodEnd,
    const BarrierData& barrierData, const std::string& fxIndex)
    : Trade("CommodityDigitalAveragePriceOption", envelope), optionData_(optionData), barrierData_(barrierData),
      strike_(strike), digitalCashPayoff_(digitalCashPayoff), currency_(currency), name_(name),
      priceType_(priceType),
      startDate_(startDate), endDate_(endDate), paymentCalendar_(paymentCalendar), paymentLag_(paymentLag),
      paymentConvention_(paymentConvention), pricingCalendar_(pricingCalendar), paymentDate_(paymentDate),
      gearing_(gearing), spread_(spread), commodityQuantityFrequency_(commodityQuantityFrequency),
      commodityPayRelativeTo_(commodityPayRelativeTo), futureMonthOffset_(futureMonthOffset),
      deliveryRollDays_(deliveryRollDays), includePeriodEnd_(includePeriodEnd), fxIndex_(fxIndex) {}

void CommodityDigitalAveragePriceOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    reset();

    DLOG("CommodityDigitalAveragePriceOption::build() called for trade " << id());
    
    // ISDA taxonomy, assuming Commodity follows the Equity template
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");

    QL_REQUIRE(optionData_.exerciseDates().size() == 1, "Invalid number of excercise dates");
    Date exDate = parseDate(optionData_.exerciseDates().front());


    Real strikeSpread = strike_ * 0.01; // FIXME, what is a usual spread here, and where should we put it?
    Real strike1 = strike_ - strikeSpread / 2;
    Real strike2 = strike_ + strikeSpread / 2;
    CommodityAveragePriceOption opt1(
        envelope(), optionData_, 1.0, strike1, currency_, name_, priceType_, startDate_, endDate_, paymentCalendar_,
        paymentLag_, paymentConvention_, pricingCalendar_, paymentDate_, gearing_, spread_, commodityQuantityFrequency_,
        commodityPayRelativeTo_, futureMonthOffset_, deliveryRollDays_, includePeriodEnd_, barrierData_, fxIndex_);
    
    CommodityAveragePriceOption opt2(envelope(), optionData_, 1.0, strike2, currency_, name_, priceType_,
                                     startDate_, endDate_, paymentCalendar_,
        paymentLag_, paymentConvention_, pricingCalendar_, paymentDate_, gearing_, spread_, commodityQuantityFrequency_,
        commodityPayRelativeTo_, futureMonthOffset_, deliveryRollDays_, includePeriodEnd_, barrierData_, fxIndex_);

    opt1.build(engineFactory);
    opt2.build(engineFactory);

    setSensitivityTemplate(opt1.sensitivityTemplate());

    QuantLib::ext::shared_ptr<Instrument> inst1 = opt1.instrument()->qlInstrument();
    QuantLib::ext::shared_ptr<Instrument> inst2 = opt2.instrument()->qlInstrument();

    QuantLib::ext::shared_ptr<CompositeInstrument> composite = QuantLib::ext::make_shared<CompositeInstrument>();
    // add and subtract such that the long call spread and long put spread have positive values
    if (optionData_.callPut() == "Call") {
        composite->add(inst1);
        composite->subtract(inst2);
    } else if (optionData_.callPut() == "Put") {
        composite->add(inst2);
        composite->subtract(inst1);
    } else {
        QL_FAIL("OptionType Call or Put required in CommodityDigitalOption " << id());
    }

    Position::Type positionType = parsePositionType(optionData_.longShort());
    Real bsIndicator = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real multiplier = digitalCashPayoff_ * bsIndicator / strikeSpread;
    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    // FIXME: Do we need to retrieve the engine builder's configuration
    string configuration = Market::defaultConfiguration;
    Currency ccy = parseCurrencyWithMinors(currency_);
    
    maturity_ =
        std::max(exDate, addPremiums(additionalInstruments, additionalMultipliers, multiplier,
                                          optionData_.premiumData(), -bsIndicator, ccy, engineFactory, configuration));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(composite, multiplier, additionalInstruments, additionalMultipliers));

    npvCurrency_ = currency_;
    notional_ = digitalCashPayoff_;
    notionalCurrency_ = currency_;

    // LOG the volatility if the trade expiry date is in the future.
    if (exDate > Settings::instance().evaluationDate()) {
        DLOG("Implied vol for " << tradeType_ << " on " << name_ << " with expiry " << exDate << " and strike "
                                << strike_ << " is "
                                << engineFactory->market()->commodityVolatility(name_)->blackVol(exDate, strike_));
    }

    additionalData_["payoff"] = digitalCashPayoff_;
    additionalData_["strike"] = strike_;
    additionalData_["optionType"] = optionData_.callPut();
    additionalData_["strikeCurrency"] = currency_;
}

std::map<AssetClass, std::set<std::string>> CommodityDigitalAveragePriceOption::underlyingIndices(
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({name_})}};
}

void CommodityDigitalAveragePriceOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* apoNode = XMLUtils::getChildNode(node, "CommodityDigitalAveragePriceOptionData");
    QL_REQUIRE(apoNode, "No CommodityDigitalAveragePriceOptionData Node");

    optionData_.fromXML(XMLUtils::getChildNode(apoNode, "OptionData"));
    if(auto b = XMLUtils::getChildNode(apoNode, "BarrierData")) {
        barrierData_.fromXML(b);
    }
    name_ = XMLUtils::getChildValue(apoNode, "Name", true);
    currency_ = XMLUtils::getChildValue(apoNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(apoNode, "Strike", true);
    digitalCashPayoff_ = XMLUtils::getChildValueAsDouble(apoNode, "DigitalCashPayoff", true);
    priceType_ = parseCommodityPriceType(XMLUtils::getChildValue(apoNode, "PriceType", true));
    startDate_ = XMLUtils::getChildValue(apoNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(apoNode, "EndDate", true);
    paymentCalendar_ = XMLUtils::getChildValue(apoNode, "PaymentCalendar", true);
    paymentLag_ = XMLUtils::getChildValue(apoNode, "PaymentLag", true);
    paymentConvention_ = XMLUtils::getChildValue(apoNode, "PaymentConvention", true);
    pricingCalendar_ = XMLUtils::getChildValue(apoNode, "PricingCalendar", true);

    paymentDate_ = XMLUtils::getChildValue(apoNode, "PaymentDate", false);

    gearing_ = 1.0;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "Gearing")) {
        gearing_ = parseReal(XMLUtils::getNodeValue(n));
    }

    spread_ = XMLUtils::getChildValueAsDouble(apoNode, "Spread", false);

    commodityQuantityFrequency_ = CommodityQuantityFrequency::PerCalculationPeriod;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "CommodityQuantityFrequency")) {
        commodityQuantityFrequency_ = parseCommodityQuantityFrequency(XMLUtils::getNodeValue(n));
    }

    commodityPayRelativeTo_ = CommodityPayRelativeTo::CalculationPeriodEndDate;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "CommodityPayRelativeTo")) {
        commodityPayRelativeTo_ = parseCommodityPayRelativeTo(XMLUtils::getNodeValue(n));
    }

    futureMonthOffset_ = XMLUtils::getChildValueAsInt(apoNode, "FutureMonthOffset", false);
    deliveryRollDays_ = XMLUtils::getChildValueAsInt(apoNode, "DeliveryRollDays", false);

    includePeriodEnd_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "IncludePeriodEnd")) {
        includePeriodEnd_ = parseBool(XMLUtils::getNodeValue(n));
    }

    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "FXIndex")){
        fxIndex_ = XMLUtils::getNodeValue(n);
    }
}

XMLNode* CommodityDigitalAveragePriceOption::toXML(XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* apoNode = doc.allocNode("CommodityDigitalAveragePriceOptionData");
    XMLUtils::appendNode(node, apoNode);

    XMLUtils::appendNode(apoNode, optionData_.toXML(doc));
    if (barrierData_.initialized())
        XMLUtils::appendNode(apoNode, barrierData_.toXML(doc));
    XMLUtils::addChild(doc, apoNode, "Name", name_);
    XMLUtils::addChild(doc, apoNode, "Currency", currency_);
    XMLUtils::addChild(doc, apoNode, "Strike", strike_);
    XMLUtils::addChild(doc, apoNode, "DigitalCashPayoff", digitalCashPayoff_);
    XMLUtils::addChild(doc, apoNode, "PriceType", to_string(priceType_));
    XMLUtils::addChild(doc, apoNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, apoNode, "EndDate", endDate_);
    XMLUtils::addChild(doc, apoNode, "PaymentCalendar", paymentCalendar_);
    XMLUtils::addChild(doc, apoNode, "PaymentLag", paymentLag_);
    XMLUtils::addChild(doc, apoNode, "PaymentConvention", paymentConvention_);
    XMLUtils::addChild(doc, apoNode, "PricingCalendar", pricingCalendar_);
    XMLUtils::addChild(doc, apoNode, "PaymentDate", paymentDate_);
    XMLUtils::addChild(doc, apoNode, "Gearing", gearing_);
    XMLUtils::addChild(doc, apoNode, "Spread", spread_);
    XMLUtils::addChild(doc, apoNode, "CommodityQuantityFrequency", to_string(commodityQuantityFrequency_));
    XMLUtils::addChild(doc, apoNode, "CommodityPayRelativeTo", to_string(commodityPayRelativeTo_));
    XMLUtils::addChild(doc, apoNode, "FutureMonthOffset", static_cast<int>(futureMonthOffset_));
    XMLUtils::addChild(doc, apoNode, "DeliveryRollDays", static_cast<int>(deliveryRollDays_));
    XMLUtils::addChild(doc, apoNode, "IncludePeriodEnd", includePeriodEnd_);
    if(!fxIndex_.empty()){
        XMLUtils::addChild(doc, apoNode, "FXIndex", fxIndex_);
    }

    return node;
}

} // namespace data
} // namespace ore
