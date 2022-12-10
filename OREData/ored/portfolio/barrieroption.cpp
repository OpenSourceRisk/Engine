/*
  Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/barrieroption.hpp>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/barriertype.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/fxindex.hpp>
#include <ql/experimental/barrieroption/doublebarrieroption.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void BarrierOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) { 
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxBarrierOption");

    checkBarriers();

    QL_REQUIRE(barrier_.levels().size() > 0 && barrier_.levels().size() <= 2, "BarrierOption must have 1 or 2 levels");
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
        
    // get the expiry date
    Date expiryDate = parseDate(option_.exerciseDates().front());
    Date payDate = expiryDate;
    const boost::optional<OptionPaymentData>& opd = option_.paymentData();
    if (opd) {
        if (opd->rulesBased()) {
            Calendar payCalendar = opd->calendar();
            payDate = payCalendar.advance(expiryDate, opd->lag(), Days, opd->convention());
        } else {
            if (opd->dates().size() > 1)
                WLOG(ore::data::StructuredTradeWarningMessage(
                    id(), tradeType(), "Trade build", "Found more than 1 payment date. The first one will be used."));
            payDate = opd->dates().front();
        }
    }
    QL_REQUIRE(payDate >= expiryDate, "Settlement date cannot be earlier than expiry date");

    Real rebate = barrier_.rebate() / tradeMultiplier();
    QL_REQUIRE(rebate >= 0, "rebate must be non-negative");

    // fx base
    // Payoff
    Option::Type type = parseOptionType(option_.callPut());
    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike()));

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate);
    // QL does not have an FXBarrierOption, so we add a barrier option and a vanilla option here and wrap
    // it in a composite to get the notional in.

    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Settlement::Type settleType = parseSettlementType(option_.settlement());
    Position::Type positionType = parsePositionType(option_.longShort());

    boost::shared_ptr<Instrument> vanilla;
    boost::shared_ptr<Instrument> barrier;
    boost::shared_ptr<InstrumentWrapper> instWrapper;

    bool exercised = false;
    Real exercisePrice = Null<Real>();

    if (payDate > expiryDate) {
        // Has the option been marked as exercised
        const boost::optional<OptionExerciseData>& oed = option_.exerciseData();
        if (oed) {
            QL_REQUIRE(oed->date() == expiryDate, "The supplied exercise date ("
                                                      << io::iso_date(oed->date())
                                                      << ") should equal the option's expiry date ("
                                                      << io::iso_date(expiryDate) << ").");
            exercised = true;
            exercisePrice = oed->price();
        }

        boost::shared_ptr<Index> index;
        if (option_.isAutomaticExercise()) {
            index = getIndex();
            QL_REQUIRE(index, "Barrier option trade with delayed payment "
                                  << id() << ": the FXIndex node needs to be populated.");
            requiredFixings_.addFixingDate(expiryDate, index->name(), payDate);
        }
        vanilla = boost::make_shared<CashSettledEuropeanOption>(payoff->optionType(), payoff->strike(), expiryDate,
                                                                payDate, option_.isAutomaticExercise(), index,
                                                                exercised, exercisePrice);
    } else {
        vanilla = boost::make_shared<VanillaOption>(payoff, exercise);
    }
    
    boost::variant<Barrier::Type, DoubleBarrier::Type> barrierType;
    if (barrier_.levels().size() < 2) {
        barrierType = parseBarrierType(barrier_.type());
        barrier = boost::make_shared<QuantLib::BarrierOption>(boost::get<Barrier::Type>(barrierType), barrier_.levels()[0].value(), 
            rebate, payoff, exercise);
    } else {
        barrierType = parseDoubleBarrierType(barrier_.type());
        barrier = boost::make_shared<QuantLib::DoubleBarrierOption>(boost::get<DoubleBarrier::Type>(barrierType),
            barrier_.levels()[0].value(), barrier_.levels()[1].value(), rebate, payoff, exercise);
    }

    boost::shared_ptr<QuantLib::PricingEngine> barrierEngine = barrierPricigingEngine(engineFactory, expiryDate, payDate);
    boost::shared_ptr<QuantLib::PricingEngine> vanillaEngine = vanillaPricigingEngine(engineFactory, expiryDate, payDate);
   
    // set pricing engines
    barrier->setPricingEngine(barrierEngine);
    vanilla->setPricingEngine(vanillaEngine);

    boost::shared_ptr<QuantLib::Index> index = getIndex();
    const QuantLib::Handle<QuantLib::Quote>& spot = spotQuote();
    if (barrier_.levels().size() < 2)
        instWrapper = boost::make_shared<SingleBarrierOptionWrapper>(
            barrier, positionType == Position::Long ? true : false, expiryDate,
            settleType == Settlement::Physical ? true : false, vanilla, boost::get<Barrier::Type>(barrierType),
            spot, barrier_.levels()[0].value(), rebate, tradeCurrency(), startDate_, index, calendar_,
            tradeMultiplier(), tradeMultiplier(), additionalInstruments, additionalMultipliers);
    else
        instWrapper = boost::make_shared<DoubleBarrierOptionWrapper>(
            barrier, positionType == Position::Long ? true : false, expiryDate,
            settleType == Settlement::Physical ? true : false, vanilla, boost::get<DoubleBarrier::Type>(barrierType),
            spot, barrier_.levels()[0].value(), barrier_.levels()[1].value(), rebate, tradeCurrency(), startDate_, index, calendar_,
            tradeMultiplier(), tradeMultiplier(), additionalInstruments, additionalMultipliers);

    instrument_ = instWrapper;

    Calendar fixingCal = index ? index->fixingCalendar() : calendar_;
    if (startDate_ != Null<Date>() && !indexFixingName().empty()) {
        for (Date d = fixingCal.adjust(startDate_); d <= expiryDate; d = fixingCal.advance(d, 1 * Days)) {
            requiredFixings_.addFixingDate(d, indexFixingName(), payDate);
        }
    }

    // Add additional premium payments
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Date lastPremiumDate =
        addPremiums(additionalInstruments, additionalMultipliers, tradeMultiplier(), option_.premiumData(), -bsInd,
                    tradeCurrency(), engineFactory, engineFactory->configuration(MarketContext::pricing));
    
    // set the maturity
    maturity_ = std::max(lastPremiumDate, payDate);
}


void BarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType_ + "Data");
    QL_REQUIRE(dataNode, "No " + tradeType_ + " Node");
    option_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(dataNode, "BarrierData"));
    startDate_ = ore::data::parseDate(XMLUtils::getChildValue(dataNode, "StartDate", false));
    calendarStr_ = XMLUtils::getChildValue(dataNode, "Calendar", false);
    calendar_ = parseCalendar(calendarStr_);

    additionalFromXml(dataNode);
}

XMLNode* BarrierOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* barNode = doc.allocNode(tradeType_ + "Data");
    XMLUtils::appendNode(node, barNode);

    XMLUtils::appendNode(barNode, option_.toXML(doc));
    XMLUtils::appendNode(barNode, barrier_.toXML(doc));
    if (startDate_ != Date())
        XMLUtils::addChild(doc, barNode, "StartDate", to_string(startDate_));
    if (!calendarStr_.empty())
        XMLUtils::addChild(doc, barNode, "Calendar", calendarStr_);
    additionalToXml(doc, barNode);

    return node;
}

std::string FxOptionWithBarrier::indexFixingName() {
    if (fxIndexStr_.empty())
        return boughtCurrency_ + soldCurrency_;
    else
        return fxIndexStr_;
}

void FxOptionWithBarrier::build(const boost::shared_ptr<ore::data::EngineFactory>& ef) { 
    
    spotQuote_ = ef->market()->fxSpot(boughtCurrency_ + soldCurrency_);
    fxIndex_ = ef->market()->fxIndex(indexFixingName(), ef->configuration(MarketContext::pricing)).currentLink();

    BarrierOption::build(ef); 
    
    npvCurrency_ = soldCurrency_; // sold is the domestic
    notional_ = soldAmount_;
    notionalCurrency_ = soldCurrency_; // sold is the domestic

    additionalData_["boughAmount"] = boughtAmount_;
    additionalData_["boughtCurrency"] = boughtCurrency_;
    additionalData_["soldAmount"] = soldAmount_;
    additionalData_["soldCurrency"] = soldCurrency_;
}

void FxOptionWithBarrier::additionalFromXml(XMLNode* node) {
    fxIndexStr_ = XMLUtils::getChildValue(node, "FXIndex", false);
    boughtCurrency_ = XMLUtils::getChildValue(node, "BoughtCurrency", true);
    soldCurrency_ = XMLUtils::getChildValue(node, "SoldCurrency", true);
    boughtAmount_ = XMLUtils::getChildValueAsDouble(node, "BoughtAmount", true);
    soldAmount_ = XMLUtils::getChildValueAsDouble(node, "SoldAmount", true);
}

void FxOptionWithBarrier::additionalToXml(XMLDocument& doc, XMLNode* node) {
    if (!fxIndexStr_.empty())
        XMLUtils::addChild(doc, node, "FXIndex", fxIndexStr_);
    XMLUtils::addChild(doc, node, "BoughtCurrency", boughtCurrency_);
    XMLUtils::addChild(doc, node, "BoughtAmount", boughtAmount_);
    XMLUtils::addChild(doc, node, "SoldCurrency", soldCurrency_);
    XMLUtils::addChild(doc, node, "SoldAmount", soldAmount_);
}


void EquityOptionWithBarrier::build(const boost::shared_ptr<ore::data::EngineFactory>& ef) {

    eqIndex_ = ef->market()->equityCurve(equityName()).currentLink();

    BarrierOption::build(ef);

    if (tradeStrike_.currency().empty())
        tradeStrike_.setCurrency(currencyStr_);

    npvCurrency_ = currency_.code();

    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = tradeStrike_.value() * quantity_;
    notionalCurrency_ = parseCurrencyWithMinors(tradeStrike_.currency()).code();

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = tradeStrike_.value();
    additionalData_["strikeCurrency"] = tradeStrike_.currency();
}

void EquityOptionWithBarrier::additionalFromXml(XMLNode* node) {
    XMLNode* tmp = XMLUtils::getChildNode(node, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(node, "Name");
    equityUnderlying_.fromXML(tmp);
    currencyStr_ = XMLUtils::getChildValue(node, "Currency", true);
    currency_ = parseCurrencyWithMinors(currencyStr_);
    tradeStrike_.fromXML(node);
    quantity_ = XMLUtils::getChildValueAsDouble(node, "Quantity", true);
}

void EquityOptionWithBarrier::additionalToXml(XMLDocument& doc, XMLNode* node) {
    XMLUtils::appendNode(node, equityUnderlying_.toXML(doc));
    XMLUtils::appendNode(node, tradeStrike_.toXML(doc));
    XMLUtils::addChild(doc, node, "Currency", currencyStr_);
    XMLUtils::addChild(doc, node, "Quantity", quantity_);
}

} // namespace data
} // namespace oreplus
