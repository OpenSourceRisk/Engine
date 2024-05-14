/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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
#include <ored/portfolio/builders/equitydoubletouchoption.hpp>
#include <ored/portfolio/equitydoubletouchoption.hpp>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/equityindex.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

EquityDoubleTouchOption::EquityDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier,
                                                 const EquityUnderlying& equityUnderlying, string payoffCurrency,
                                                 double payoffAmount, string startDate, string calendar)
    : ore::data::Trade("EquityDoubleTouchOption", env),
      EquitySingleAssetDerivative("", env, equityUnderlying),
      option_(option),
      barrier_(barrier), startDate_(startDate), calendar_(calendar), payoffAmount_(payoffAmount),
      payoffCurrency_(payoffCurrency) {
    DoubleBarrier::Type barrierType = parseDoubleBarrierType(barrier_.type());
    switch (barrierType) {
    case DoubleBarrier::KnockIn:
        type_ = "KnockIn";
        break;
    case DoubleBarrier::KnockOut:
        type_ = "KnockOut";
        break;
    default:
        QL_FAIL("unsupported barrier type " << barrierType);
    }
}

void EquityDoubleTouchOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    Date start = ore::data::parseDate(startDate_);
    Calendar cal = ore::data::parseCalendar(calendar_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxOption");
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barrier_.levels().size() == 2, "Invalid number of barrier levels");
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "American", "Only american barrier style suppported");

    // Parse trade data
    string assetName = equityName();
    Currency ccy = parseCurrency(payoffCurrency_);
    Date expiryDate = parseDate(option_.exerciseDates().front());
    DoubleBarrier::Type barrierType = parseDoubleBarrierType(barrier_.type());
    bool payoffAtExpiry = option_.payoffAtExpiry();
    double rebate = barrier_.rebate();
    Position::Type positionType = parsePositionType(option_.longShort());

    QL_REQUIRE(rebate == 0, "Rebates not supported for EquityDoubleTouchOptions");
    if (payoffAtExpiry == false) {
        payoffAtExpiry = true;
        DLOG("Payoff at hit not yet supported for EquityDoubleTouchOptions, setting to payoff at expiry");
    }

    Real levelLow = barrier_.levels()[0].value();
    Real levelHigh = barrier_.levels()[1].value();
    QL_REQUIRE(levelLow < levelHigh, "barrier levels are not in ascending order");


    // create payoff and exercise, as well as leg of underlying instrument
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(Option::Call, (levelLow + levelHigh) / 2, 1.0));
    Leg leg;

    leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(1.0, expiryDate)));

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    QuantLib::ext::shared_ptr<Instrument> doubleTouch =
        QuantLib::ext::make_shared<DoubleBarrierOption>(barrierType, levelLow, levelHigh, 0.0, payoff, exercise);
    QuantLib::ext::shared_ptr<Instrument> underlying = QuantLib::ext::make_shared<Swap>(Leg(), leg);

    

    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> eqIndex = engineFactory->market()->equityCurve(assetName).currentLink();

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<EquityDoubleTouchOptionEngineBuilder> eqDoubleTouchOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityDoubleTouchOptionEngineBuilder>(builder);
    doubleTouch->setPricingEngine(eqDoubleTouchOptBuilder->engine(assetName, ccy));
    setSensitivityTemplate(*eqDoubleTouchOptBuilder);
    if (type_ == "KnockIn") {
        // if a knock-in option is triggered it becomes a simple forward cashflow
        // which we price as a swap
        builder = engineFactory->builder("Swap");
        QL_REQUIRE(builder, "No builder found for Swap");
        QuantLib::ext::shared_ptr<SwapEngineBuilderBase> swapBuilder =
            QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        underlying->setPricingEngine(swapBuilder->engine(parseCurrency(payoffCurrency_), std::string(), std::string()));
    }

    bool isLong = (positionType == Position::Long) ? true : false;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate = addPremiums(
        additionalInstruments, additionalMultipliers, (isLong ? 1.0 : -1.0) * payoffAmount_, option_.premiumData(),
        isLong ? -1.0 : 1.0, ccy, engineFactory, builder->configuration(MarketContext::pricing));

    Handle<Quote> spot = market->equitySpot(assetName);
    instrument_ = QuantLib::ext::make_shared<DoubleBarrierOptionWrapper>(
        doubleTouch, isLong, expiryDate, false, underlying, barrierType, spot, levelLow, levelHigh, 0, ccy, start, eqIndex, cal,
        payoffAmount_, payoffAmount_, additionalInstruments, additionalMultipliers);
    npvCurrency_ = payoffCurrency_;
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_;
    maturity_ = std::max(lastPremiumDate, expiryDate);

    if (start != Date()) {
        for (Date d = start; d <= expiryDate; d = eqIndex->fixingCalendar().advance(d, 1 * Days))
            requiredFixings_.addFixingDate(d, "EQ-" + assetName, expiryDate);
    }

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;
}

bool EquityDoubleTouchOption::checkBarrier(Real spot, Barrier::Type type, Real barrier) {
    switch (type) {
    case Barrier::DownIn:
    case Barrier::DownOut:
        return spot <= barrier;
    case Barrier::UpIn:
    case Barrier::UpOut:
        return spot >= barrier;
    default:
        QL_FAIL("unknown barrier type " << type);
    }
}

void EquityDoubleTouchOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityDoubleTouchOptionData");
    QL_REQUIRE(eqNode, "No EquityDoubleTouchOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(eqNode, "BarrierData"));
    DoubleBarrier::Type barrierType = parseDoubleBarrierType(barrier_.type());
    switch (barrierType) {
    case DoubleBarrier::KnockIn:
        type_ = "KnockIn";
        break;
    case DoubleBarrier::KnockOut:
        type_ = "KnockOut";
        break;
    default:
        QL_FAIL("unsupported barrier type " << barrierType);
    }

	XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    payoffCurrency_ = XMLUtils::getChildValue(eqNode, "PayoffCurrency", true);
    startDate_ = XMLUtils::getChildValue(eqNode, "StartDate", false);
    calendar_ = XMLUtils::getChildValue(eqNode, "Calendar", false);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(eqNode, "PayoffAmount", true);
}

XMLNode* EquityDoubleTouchOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityDoubleTouchOptionData");
    XMLUtils::appendNode(node, eqNode);
    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, barrier_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, eqNode, "PayoffAmount", payoffAmount_);
    if (startDate_ != "")
        XMLUtils::addChild(doc, eqNode, "StartDate", startDate_);
    if (calendar_ != "")
        XMLUtils::addChild(doc, eqNode, "Calendar", calendar_);

    return node;
}

} // namespace data
} // namespace oreplus
