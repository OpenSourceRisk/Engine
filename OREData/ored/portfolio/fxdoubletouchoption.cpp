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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/fxdoubletouchoption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/fxdoubletouchoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

FxDoubleTouchOption::FxDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier, string foreignCurrency,
                                         string domesticCurrency, string payoffCurrency, double payoffAmount,
                                         string startDate, string calendar, string fxIndex)
    : ore::data::Trade("FxDoubleTouchOption", env),
      FxSingleAssetDerivative("", env, foreignCurrency, domesticCurrency), option_(option),
      barrier_(barrier), startDate_(startDate), calendar_(calendar), fxIndex_(fxIndex), payoffAmount_(payoffAmount),
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

void FxDoubleTouchOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Barrier");  
    additionalData_["isdaTransaction"] = string("");  

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;

    npvCurrency_ = payoffCurrency_;
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_;

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    Date start = ore::data::parseDate(startDate_);
    Calendar cal = ore::data::parseCalendar(calendar_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxOption");
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barrier_.levels().size() == 2, "Invalid number of barrier levels");
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "American", "Only american barrier style suppported");

    // Parse trade data
    Currency fgnCcy = parseCurrency(foreignCurrency_);
    Currency domCcy = parseCurrency(domesticCurrency_);
    Date expiryDate = parseDate(option_.exerciseDates().front());
    DoubleBarrier::Type barrierType = parseDoubleBarrierType(barrier_.type());
    bool payoffAtExpiry = option_.payoffAtExpiry();
    double rebate = barrier_.rebate();
    Position::Type positionType = parsePositionType(option_.longShort());

    QL_REQUIRE(rebate == 0, "Rebates not supported for FxDoubleTouchOptions");
    if (payoffAtExpiry == false) {
        payoffAtExpiry = true;
        DLOG("Payoff at hit not yet supported for FxDoubleTouchOptions, setting to payoff at expiry");
    }

    Date payDate = expiryDate;
    const boost::optional<OptionPaymentData>& opd = option_.paymentData();
    if (opd) {
        if (opd->rulesBased()) {
            payDate = opd->calendar().advance(expiryDate, opd->lag(), Days, opd->convention());
        } else {
            if (opd->dates().size() > 1)
                ore::data::StructuredTradeWarningMessage(id(), tradeType(), "Trade build",
                                                         "Found more than 1 payment date. The first one will be used.")
                    .log();
            payDate = opd->dates().front();
        }
    }
    QL_REQUIRE(payDate >= expiryDate, "Settlement date cannot be earlier than expiry date");
    maturity_ = std::max(option_.premiumData().latestPremiumDate(), payDate);

    Real levelLow = barrier_.levels()[0].value();
    Real levelHigh = barrier_.levels()[1].value();
    QL_REQUIRE(levelLow < levelHigh, "barrier levels are not in ascending order");

    // Handle PayoffCurrency, we might have to flip the trade here
    bool flipResults = false;
    if (payoffCurrency_ == foreignCurrency_) {
        // Invert the trade, switch dom and for and flip Put/Call
        levelLow = 1.0 / levelLow;
        levelHigh = 1.0 / levelHigh;
        std::swap(levelLow, levelHigh);
        std::swap(fgnCcy, domCcy);
        flipResults = true;
    } else if (payoffCurrency_ != domesticCurrency_) {
        QL_FAIL("Invalid Payoff currency (" << payoffCurrency_ << ") for FxDoubleTouchOption " << foreignCurrency_
                                            << domesticCurrency_);
    }
    DLOG("Setting up FxDoubleTouchOption with level " << levelLow << ", " << levelHigh << " foreign/bought " << fgnCcy
                                                      << " domestic/sold " << domCcy);
    // from this point on it's important not to use domesticCurrency_, foreignCurrency_, barrier_.level(), etc
    // rather the local variables (fgnCcy, domCcy, level, etc) should be used as they may have been flipped.

    // create payoff and exercise, as well as leg of underlying instrument
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(Option::Call, (levelLow + levelHigh) / 2, 1.0));
    Leg leg;

    leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(1.0, payDate)));

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    QuantLib::ext::shared_ptr<Instrument> doubleTouch =
        QuantLib::ext::make_shared<DoubleBarrierOption>(barrierType, levelLow, levelHigh, 0.0, payoff, exercise);
    QuantLib::ext::shared_ptr<Instrument> underlying = QuantLib::ext::make_shared<Swap>(Leg(), leg);

    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex;
    if (!fxIndex_.empty())
        fxIndex = buildFxIndex(fxIndex_, domCcy.code(), fgnCcy.code(), engineFactory->market(),
                               engineFactory->configuration(MarketContext::pricing));

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<FxDoubleTouchOptionEngineBuilder> fxDoubleTouchOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<FxDoubleTouchOptionEngineBuilder>(builder);
    doubleTouch->setPricingEngine(fxDoubleTouchOptBuilder->engine(fgnCcy, domCcy, payDate, flipResults));
    setSensitivityTemplate(*fxDoubleTouchOptBuilder);

    // if a knock-in option is triggered it becomes a simple forward cashflow
    // which we price as a swap
    builder = engineFactory->builder("Swap");
    QL_REQUIRE(builder, "No builder found for Swap");
    QuantLib::ext::shared_ptr<SwapEngineBuilderBase> swapBuilder =
        QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
    underlying->setPricingEngine(swapBuilder->engine(parseCurrency(payoffCurrency_), std::string(), std::string()));

    bool isLong = (positionType == Position::Long) ? true : false;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, (isLong ? 1.0 : -1.0) * payoffAmount_,
                option_.premiumData(), isLong ? -1.0 : 1.0, parseCurrency(payoffCurrency_), engineFactory,
                builder->configuration(MarketContext::pricing));

    Handle<Quote> spot = market->fxSpot(fgnCcy.code() + domCcy.code());
    instrument_ = QuantLib::ext::make_shared<DoubleBarrierOptionWrapper>(
        doubleTouch, isLong, expiryDate, false, underlying, barrierType, spot, levelLow, levelHigh, 0, domCcy,
        start, fxIndex, cal, payoffAmount_, payoffAmount_, additionalInstruments, additionalMultipliers);


    Calendar fixingCal = fxIndex ? fxIndex->fixingCalendar() : cal;
    if (start != Date()) {
        for (Date d = start; d <= expiryDate; d = fixingCal.advance(d, 1 * Days))
            requiredFixings_.addFixingDate(d, fxIndex_, payDate);
    }

}

bool FxDoubleTouchOption::checkBarrier(Real spot, Barrier::Type type, Real barrier) {
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

void FxDoubleTouchOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxDoubleTouchOptionData");
    QL_REQUIRE(fxNode, "No FxDoubleTouchOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(fxNode, "BarrierData"));
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

    foreignCurrency_ = XMLUtils::getChildValue(fxNode, "ForeignCurrency", true);
    domesticCurrency_ = XMLUtils::getChildValue(fxNode, "DomesticCurrency", true);
    payoffCurrency_ = XMLUtils::getChildValue(fxNode, "PayoffCurrency", true);
    startDate_ = XMLUtils::getChildValue(fxNode, "StartDate", false);
    calendar_ = XMLUtils::getChildValue(fxNode, "Calendar", false);
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "PayoffAmount", true);
}

XMLNode* FxDoubleTouchOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxDoubleTouchOptionData");
    XMLUtils::appendNode(node, fxNode);
    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::appendNode(fxNode, barrier_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "ForeignCurrency", foreignCurrency_);
    XMLUtils::addChild(doc, fxNode, "DomesticCurrency", domesticCurrency_);
    XMLUtils::addChild(doc, fxNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, fxNode, "PayoffAmount", payoffAmount_);
    if (startDate_ != "")
        XMLUtils::addChild(doc, fxNode, "StartDate", startDate_);
    if (fxIndex_ != "")
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);
    if (calendar_ != "")
        XMLUtils::addChild(doc, fxNode, "Calendar", calendar_);

    return node;
}

} // namespace data
} // namespace oreplus
