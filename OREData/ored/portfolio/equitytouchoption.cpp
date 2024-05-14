/*
  Copyright (C) 2020 Quaternion Risk Management Ltd
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
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/equitytouchoption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/equitytouchoption.hpp>
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

EquityTouchOption::EquityTouchOption(Envelope& env, OptionData option, BarrierData barrier,
                                     const EquityUnderlying& equityUnderlying, string payoffCurrency,
                                     double payoffAmount, string startDate, string calendar, string eqIndex)
    : ore::data::Trade("EquityTouchOption", env),
      EquitySingleAssetDerivative("", env, equityUnderlying), option_(option), barrier_(barrier),
      startDate_(startDate), calendar_(calendar), eqIndex_(eqIndex), payoffAmount_(payoffAmount),
      payoffCurrency_(payoffCurrency) {
    Barrier::Type barrierType = parseBarrierType(barrier_.type());
    switch (barrierType) {
    case Barrier::DownIn:
    case Barrier::UpIn:
        type_ = "One-Touch";
        break;
    case Barrier::DownOut:
    case Barrier::UpOut:
        type_ = "No-Touch";
        break;
    default:
        QL_FAIL("unknown barrier type");
    }
}

void EquityTouchOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    Date today = Settings::instance().evaluationDate();
    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    // Parse trade data
    string assetName = equityName();
    Currency ccy = parseCurrency(payoffCurrency_);
    Real level = barrier_.levels()[0].value();
    Date expiryDate = parseDate(option_.exerciseDates().front());
    Barrier::Type barrierType = parseBarrierType(barrier_.type());
    Option::Type type;
    if (barrierType == Barrier::DownIn || barrierType == Barrier::DownOut)
        type = Option::Type::Put;
    else
        type = Option::Type::Call;
    bool payoffAtExpiry = option_.payoffAtExpiry();
    Real rebate = barrier_.rebate();
    Position::Type positionType = parsePositionType(option_.longShort());
    Date start = ore::data::parseDate(startDate_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for EquityOption");
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barrier_.levels().size() == 1, "Double barriers not supported for EquityTouchOptions");
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "American", "Only american barrier style suppported");
    QL_REQUIRE(rebate == 0, "Rebates not supported for EquityTouchOptions");
    QL_REQUIRE(payoffAtExpiry == true || barrierType == Barrier::Type::DownIn || barrierType == Barrier::Type::UpIn,
               "Payoff at hit not supported for EquityNoTouchOptions");

    // create payoff and exercise, as well as leg of underlying instrument
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, level, 1.0));
    Leg leg;

    leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(1.0, expiryDate)));

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<AmericanExercise>(expiryDate, payoffAtExpiry);

    QuantLib::ext::shared_ptr<Instrument> barrier = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
    QuantLib::ext::shared_ptr<Instrument> underlying = QuantLib::ext::make_shared<Swap>(Leg(), leg);

    QL_REQUIRE(eqIndex_ != "", "No eqIndex provided");
    QL_REQUIRE(calendar_ != "", "No calendar provided");

    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> eqIndex = parseEquityIndex(eqIndex_);

    // check if the barrier has been triggered already
    bool triggered = false;
    Calendar cal = eqIndex->fixingCalendar();
    if (startDate_ != "" && start < today) {

        Date d = start;

        while (d < today && !triggered) {

            Real fixing = eqIndex->pastFixing(d);

            if (fixing == 0.0 || fixing == Null<Real>()) {
                ALOG("Got invalid Equity fixing for index " << eqIndex_ << " on " << d
                                                            << "Skipping this date, assuming no trigger");
            } else {
                triggered = checkBarrier(fixing, barrierType, level);
            }

            d = cal.advance(d, 1, Days);
        }
    }

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<EquityTouchOptionEngineBuilder> eqTouchOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityTouchOptionEngineBuilder>(builder);
    barrier->setPricingEngine(eqTouchOptBuilder->engine(assetName, ccy, type_));
    setSensitivityTemplate(*eqTouchOptBuilder);
    if (type_ == "One-Touch") {
        // if a one-touch option is triggered it becomes a simple forward cashflow
        // which we price as a swap
        builder = engineFactory->builder("Swap");
        QL_REQUIRE(builder, "No builder found for Swap");
        QuantLib::ext::shared_ptr<SwapEngineBuilderBase> swapBuilder =
            QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        underlying->setPricingEngine(swapBuilder->engine(ccy, std::string(), std::string()));
    }

    bool isLong = (positionType == Position::Long) ? true : false;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate = addPremiums(
        additionalInstruments, additionalMultipliers, (isLong ? 1.0 : -1.0) * payoffAmount_, option_.premiumData(),
        isLong ? -1.0 : 1.0, ccy, engineFactory, builder->configuration(MarketContext::pricing));

    Handle<Quote> spot = market->equitySpot(assetName);
    instrument_ = QuantLib::ext::make_shared<SingleBarrierOptionWrapper>(
        barrier, isLong, expiryDate, false, underlying, barrierType, spot, level, rebate, ccy, start, eqIndex, cal, payoffAmount_,
        payoffAmount_, additionalInstruments, additionalMultipliers);
    npvCurrency_ = payoffCurrency_;
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_;
    maturity_ = std::max(lastPremiumDate, expiryDate);

    if (start != Date()) {
        for (Date d = start; d <= expiryDate; d = cal.advance(d, 1 * Days))
            requiredFixings_.addFixingDate(d, eqIndex_, expiryDate);
    }

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;
}

bool EquityTouchOption::checkBarrier(Real spot, Barrier::Type type, Real barrier) {
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

void EquityTouchOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityTouchOptionData");
    QL_REQUIRE(eqNode, "No EquityOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(eqNode, "BarrierData"));
    Barrier::Type barrierType = parseBarrierType(barrier_.type());
    switch (barrierType) {
    case Barrier::DownIn:
    case Barrier::UpIn:
        type_ = "One-Touch";
        break;
    case Barrier::DownOut:
    case Barrier::UpOut:
        type_ = "No-Touch";
        break;
    default:
        QL_FAIL("unknown barrier type");
    }

    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(eqNode, "PayoffAmount", true);
    payoffCurrency_ = XMLUtils::getChildValue(eqNode, "PayoffCurrency", true);
    startDate_ = XMLUtils::getChildValue(eqNode, "StartDate", false);
    calendar_ = XMLUtils::getChildValue(eqNode, "Calendar", false);
    eqIndex_ = XMLUtils::getChildValue(eqNode, "EQIndex", false);
}

XMLNode* EquityTouchOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityTouchOptionData");
    XMLUtils::appendNode(node, eqNode);
    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, barrier_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, eqNode, "PayoffAmount", payoffAmount_);
    if (startDate_ != "")
        XMLUtils::addChild(doc, eqNode, "StartDate", startDate_);
    if (eqIndex_ != "")
        XMLUtils::addChild(doc, eqNode, "EQIndex", eqIndex_);
    if (calendar_ != "")
        XMLUtils::addChild(doc, eqNode, "Calendar", calendar_);

    return node;
}

} // namespace data
} // namespace oreplus
