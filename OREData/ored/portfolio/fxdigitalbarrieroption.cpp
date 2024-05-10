/*
  Copyright (C) 2017 Quaternion Risk Management Ltd
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
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

bool checkBarrier(Real spot, Barrier::Type type, Real barrier);

void FxDigitalBarrierOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Digital");  
    additionalData_["isdaTransaction"] = string("");

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    // Only American supported for now
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxOption");
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(strike_ > 0.0 && strike_ != Null<Real>(), "Invalid strike " << strike_);

    Currency boughtCcy = parseCurrency(foreignCurrency_);
    Currency soldCcy = parseCurrency(domesticCurrency_);
    Real level = barrier_.levels()[0].value();
    Date start = ore::data::parseDate(startDate_);
    Real rebate = barrier_.rebate();
    QL_REQUIRE(rebate >= 0, "rebate must be non-negative");

    QL_REQUIRE(level > 0.0 && level != Null<Real>(), "Invalid level " << level);

    // Payoff and Barrier Type
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "American", "Only american barrier style suppported");
    Option::Type type = parseOptionType(option_.callPut());
    Barrier::Type barrierType = parseBarrierType(barrier_.type());

    // Handle PayoffCurrency, we might have to flip the trade here
    Real strike = strike_;
    bool flipResults = false;
    if (payoffCurrency_ == "") {
        DLOG("PayoffCurrency defaulting to " << domesticCurrency_ << " for FxDigitalBarrierOption " << id());
    } else if (payoffCurrency_ == foreignCurrency_) {
        // Invert the trade, switch dom and for and flip Put/Call
        strike = 1.0 / strike;
        level = 1.0 / level;
        std::swap(boughtCcy, soldCcy);
        type = type == Option::Call ? Option::Put : Option::Call;
        switch (barrierType) {
        case Barrier::DownIn:
            barrierType = Barrier::UpIn;
            break;
        case Barrier::UpIn:
            barrierType = Barrier::DownIn;
            break;
        case Barrier::DownOut:
            barrierType = Barrier::UpOut;
            break;
        case Barrier::UpOut:
            barrierType = Barrier::DownOut;
            break;
        }
        flipResults = true;
    } else if (payoffCurrency_ != domesticCurrency_) {
        QL_FAIL("Invalid Payoff currency (" << payoffCurrency_ << ") for FxDigitalBarrierOption " << foreignCurrency_
                                            << domesticCurrency_);
    }
    DLOG("Setting up FxDigitalBarrierOption with strike " << strike << " level " << level << " foreign/bought "
                                                          << boughtCcy << " domestic/sold " << soldCcy);

    // from this point on it's important not to use domesticCurrency_, foreignCurrency_, strike_, barrier_.level(), etc
    // rather the local variables (boughtCcy, soldCcy, strike, level, etc) should be used as they may have been flipped.

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;
    additionalData_["effectiveForeignCurrency"] = boughtCcy.code();
    additionalData_["effectiveDomesticCurrency"] = soldCcy.code();

    npvCurrency_ = soldCcy.code(); // sold is the domestic
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_ != "" ? payoffCurrency_ : domesticCurrency_; // see logic above
    
    // Exercise
    // Digital Barrier Options assume an American exercise that pays at expiry
    Date expiryDate = parseDate(option_.exerciseDates().front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);
    maturity_ = std::max(option_.premiumData().latestPremiumDate(), expiryDate);

    // Create a CashOrNothing payoff for digital options
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, strike, payoffAmount_));

    // QL does not have an FXDigitalBarrierOption, so we add a barrier option here and wrap
    // it in a composite
    QuantLib::ext::shared_ptr<Instrument> vanilla = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
    QuantLib::ext::shared_ptr<Instrument> barrier =
        QuantLib::ext::make_shared<BarrierOption>(barrierType, level, rebate, payoff, exercise);

    // Check if the barrier has been triggered already
    Calendar cal = ore::data::parseCalendar(calendar_);
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex;
    if (!fxIndex_.empty())
        fxIndex = buildFxIndex(fxIndex_, soldCcy.code(), boughtCcy.code(), engineFactory->market(),
                               engineFactory->configuration(MarketContext::pricing));

    // set pricing engines
    // we buy foreign with domestic(=sold ccy).
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<FxDigitalBarrierOptionEngineBuilder> fxBarrierOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<FxDigitalBarrierOptionEngineBuilder>(builder);
    // if an 'in' option is triggered it becomes an FxDigitalOption, so we need an fxDigitalOption pricer
    builder = engineFactory->builder("FxDigitalOption");
    QL_REQUIRE(builder, "No builder found for FxDigitalOption");
    QuantLib::ext::shared_ptr<FxDigitalOptionEngineBuilder> fxOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<FxDigitalOptionEngineBuilder>(builder);
    setSensitivityTemplate(*builder);

    barrier->setPricingEngine(fxBarrierOptBuilder->engine(boughtCcy, soldCcy, expiryDate));
    vanilla->setPricingEngine(fxOptBuilder->engine(boughtCcy, soldCcy, flipResults));

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    // If premium data is provided
    // 1) build the fee trade and pass it to the instrument wrapper for pricing
    // 2) add fee payment as additional trade leg for cash flow reporting
    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, positionType == Position::Long ? 1.0 : -1.0,
                option_.premiumData(), -bsInd, soldCcy, engineFactory,
                fxOptBuilder->configuration(MarketContext::pricing));

    Settlement::Type settleType = parseSettlementType(option_.settlement());

    Handle<Quote> spot = market->fxSpot(boughtCcy.code() + soldCcy.code());
    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new SingleBarrierOptionWrapper(
        barrier, positionType == Position::Long ? true : false, expiryDate,
        settleType == Settlement::Physical ? true : false, vanilla, barrierType, spot, level, rebate, soldCcy,
        start, fxIndex, cal, 1, 1, additionalInstruments, additionalMultipliers));

    if (start != Date()) {
        for (Date d = start; d <= expiryDate; d = cal.advance(d, 1 * Days)) {
            requiredFixings_.addFixingDate(d, fxIndex_, expiryDate);
        }
    }
}

bool checkBarrier(Real spot, Barrier::Type type, Real barrier) {
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

void FxDigitalBarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxDigitalBarrierOptionData");
    QL_REQUIRE(fxNode, "No FxDigitalBarrierOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(fxNode, "BarrierData"));
    startDate_ = XMLUtils::getChildValue(fxNode, "StartDate", false);
    calendar_ = XMLUtils::getChildValue(fxNode, "Calendar", false);
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
    strike_ = XMLUtils::getChildValueAsDouble(fxNode, "Strike", true);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "PayoffAmount", true);
    payoffCurrency_ = XMLUtils::getChildValue(fxNode, "PayoffCurrency", false); // optional
    foreignCurrency_ = XMLUtils::getChildValue(fxNode, "ForeignCurrency", true);
    domesticCurrency_ = XMLUtils::getChildValue(fxNode, "DomesticCurrency", true);
}

XMLNode* FxDigitalBarrierOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxDigitalBarrierOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::appendNode(fxNode, barrier_.toXML(doc));
    if (startDate_ != "")
        XMLUtils::addChild(doc, fxNode, "StartDate", startDate_);
    if (calendar_ != "")
        XMLUtils::addChild(doc, fxNode, "Calendar", calendar_);
    if (fxIndex_ != "")
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);
    XMLUtils::addChild(doc, fxNode, "Strike", strike_);
    XMLUtils::addChild(doc, fxNode, "PayoffAmount", payoffAmount_);
    if (payoffCurrency_ != "")
        XMLUtils::addChild(doc, fxNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, fxNode, "ForeignCurrency", foreignCurrency_);
    XMLUtils::addChild(doc, fxNode, "DomesticCurrency", domesticCurrency_);

    return node;
}

} // namespace data
} // namespace oreplus
