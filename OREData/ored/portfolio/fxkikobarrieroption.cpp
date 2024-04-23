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
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxdoublebarrieroption.hpp>
#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/fxdoublebarrieroption.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/multiccycompositeinstrument.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxKIKOBarrierOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Barrier");  
    additionalData_["isdaTransaction"] = string("");  

    additionalData_["boughAmount"] = boughtAmount_;
    additionalData_["boughtCurrency"] = boughtCurrency_;
    additionalData_["soldAmount"] = soldAmount_;
    additionalData_["soldCurrency"] = soldCurrency_;

    npvCurrency_ = soldCurrency_; // sold is the domestic
    notional_ = soldAmount_;
    notionalCurrency_ = soldCurrency_;

    Date today = Settings::instance().evaluationDate();
    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    Date start = ore::data::parseDate(startDate_);
    Calendar cal = ore::data::parseCalendar(calendar_);

    // Only European Barrier supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barriers_.size() == 2, "Invalid number of barriers");
    QL_REQUIRE(barriers_[0].levels().size() == 1, "Invalid number of barrier levels");
    QL_REQUIRE(barriers_[1].levels().size() == 1, "Invalid number of barrier levels");
    QL_REQUIRE(barriers_[0].rebate() == 0, "rebates are not supported for KIKO options");
    QL_REQUIRE(barriers_[1].rebate() == 0, "rebates are not supported for KIKO options");
    QL_REQUIRE(barriers_[0].style().empty() || barriers_[0].style() == "American",
               "only american barrier style supported");
    QL_REQUIRE(barriers_[1].style().empty() || barriers_[1].style() == "American",
               "only american barrier style supported");
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxBarrierOption");

    Currency boughtCcy = parseCurrency(boughtCurrency_);
    Currency soldCcy = parseCurrency(soldCurrency_);

    // Payoff
    Real strike = soldAmount_ / boughtAmount_;
    Option::Type type = parseOptionType(option_.callPut());
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike));

    // Exercise
    Date expiryDate = parseDate(option_.exerciseDates().front());
    maturity_ = std::max(expiryDate, option_.premiumData().latestPremiumDate());

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    // build underlying vanilla option
    QuantLib::ext::shared_ptr<Instrument> vanilla = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);

    // we extract our KnockIn/KnockOut barrier data
    Size knockInIndex;
    Size knockOutIndex;

    Barrier::Type knockInType;
    Barrier::Type knockOutType;

    Barrier::Type tmpBarrier = parseBarrierType(barriers_[0].type());
    switch (tmpBarrier) {
    case Barrier::Type::UpIn:
    case Barrier::Type::DownIn:
        knockInIndex = 0;
        knockOutIndex = 1;
        knockInType = tmpBarrier;
        knockOutType = parseBarrierType(barriers_[1].type());
        break;
    case Barrier::Type::UpOut:
    case Barrier::Type::DownOut:
        knockInIndex = 1;
        knockOutIndex = 0;
        knockOutType = tmpBarrier;
        knockInType = parseBarrierType(barriers_[1].type());
        break;
    default:
        QL_FAIL("unsupported barrier type provided");
    }
    QL_REQUIRE(knockOutType == Barrier::Type::UpOut || knockOutType == Barrier::Type::DownOut,
               "KIKO barrier requires one KnockOut barrier");
    QL_REQUIRE(knockInType == Barrier::Type::UpIn || knockInType == Barrier::Type::DownIn,
               "KIKO barrier requires one KnockIn barrier");

    Real knockInLevel = barriers_[knockInIndex].levels()[0].value();
    Real knockOutLevel = barriers_[knockOutIndex].levels()[0].value();

    QL_REQUIRE(knockInLevel != knockOutLevel, "different levels must be provided");
    // Check if the barrier has been triggered already
    bool knockedIn = false;
    bool knockedOut = false;
    Handle<Quote> spot = market->fxSpot(boughtCurrency_ + soldCurrency_);
    
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex;
    if (!fxIndex_.empty()) {
        auto fxi = market->fxIndex(fxIndex_);
        if (!fxi.empty()) {
            fxIndex = fxi.currentLink();
        }
    }
        

    // checking fixings
    if (startDate_ != "" && parseDate(startDate()) < today) {

        QL_REQUIRE(fxIndex_ != "", "no fxIndex provided");
        QL_REQUIRE(calendar_ != "", "no calendar provided");
        bool inverted = false;
        if (fxIndex->sourceCurrency() == soldCcy && fxIndex->targetCurrency() == boughtCcy) {
            inverted = true;
        } else {
            QL_REQUIRE(fxIndex->sourceCurrency() == boughtCcy && fxIndex->targetCurrency() == soldCcy,
                       "Invalid FX Index " << fxIndex_ << "for bought " << boughtCcy << "and sold " << soldCcy);
        }

        Date d = start;
        while (d < today && !knockedIn && !knockedOut) {
            Real fixing = Null<Real>();
            if (fxIndex->fixingCalendar().isBusinessDay(d)) {
                 fixing = fxIndex->pastFixing(d);
            } 
            if (fixing == 0.0 || fixing == Null<Real>()) {
                ALOG("Got invalid FX fixing for index " << fxIndex_ << " on " << d
                                                        << "Skipping this date, assuming no trigger");
            } else {
                if (inverted)
                    fixing = 1.0 / fixing;
                ALOG("Checking FX fixing for index " << fxIndex_ << " on " << d << ", value " << fixing);

                if (!knockedIn)
                    knockedIn = checkBarrier(fixing, knockInType, knockInLevel);
                if (!knockedOut)
                    knockedOut = checkBarrier(fixing, knockOutType, knockOutLevel);
            }
            d = cal.advance(d, 1, Days);
        }
    }

    // All possible instruments require an underlying vanilla option so we set this up
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("FxOption");
    QL_REQUIRE(builder, "No FxOption builder found");
    QuantLib::ext::shared_ptr<FxEuropeanOptionEngineBuilder> fxOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<FxEuropeanOptionEngineBuilder>(builder);
    vanilla->setPricingEngine(fxOptBuilder->engine(boughtCcy, soldCcy, expiryDate));

    // Add additional premium payments
    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers,
                (positionType == Position::Long ? 1.0 : -1.0) * boughtAmount_, option_.premiumData(), -bsInd, soldCcy,
                engineFactory, fxOptBuilder->configuration(MarketContext::pricing));

    // we build a knock out option
    QuantLib::ext::shared_ptr<Instrument> barrier =
        QuantLib::ext::make_shared<QuantLib::BarrierOption>(knockOutType, knockOutLevel, 0, payoff, exercise);

    builder = engineFactory->builder("FxBarrierOption");
    QL_REQUIRE(builder, "No FxBarrierOption builder found");
    QuantLib::ext::shared_ptr<FxBarrierOptionEngineBuilder> fxBarrierOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<FxBarrierOptionEngineBuilder>(builder);
    barrier->setPricingEngine(fxBarrierOptBuilder->engine(boughtCcy, soldCcy, expiryDate, expiryDate));
    setSensitivityTemplate(*fxBarrierOptBuilder);
    Settlement::Type settleType = parseSettlementType(option_.settlement());

    QuantLib::ext::shared_ptr<InstrumentWrapper> koInstrument =
        QuantLib::ext::shared_ptr<InstrumentWrapper>(new SingleBarrierOptionWrapper(
            barrier, positionType == Position::Long ? true : false, expiryDate,
            settleType == Settlement::Physical ? true : false, vanilla, knockOutType, spot, knockOutLevel, 0, soldCcy,
            start, fxIndex, cal, boughtAmount_, boughtAmount_, additionalInstruments, additionalMultipliers));

    // if the trade is knocked in this is all we price
    if (knockedIn || knockedOut) {
        DLOG("This trade has been knocked-in, building a knock out option");
        instrument_ = koInstrument;
        // Otherwise we build a composite instrument
    } else {

        Handle<Quote> fx = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
        vector<QuantLib::ext::shared_ptr<InstrumentWrapper>> iw;
        std::vector<Handle<Quote>> fxRates;

        DLOG("adding a knock out option to our composite trade");
        iw.push_back(koInstrument);
        fxRates.push_back(fx);

        // the second trade's additional instruments should have the opposite multipliers
        std::vector<Real> flippedAdditionalMultipliers;
        for (auto a : additionalMultipliers)
            flippedAdditionalMultipliers.push_back(-1 * a);

        // For an UpIn/UpOut or a DownIn/DownOut the kiko option is replicated as follows:
        // V_kiko(L,U) = V_knockout(L) - V_knockout(U)
        // where L is the knockout level, U is the knockIn level
        // If the option touches U the trade is knocked in, if it then touches L it is knocked out.
        if ((knockOutType == Barrier::Type::UpOut && knockInType == Barrier::Type::UpIn) || 
            (knockOutType == Barrier::Type::DownOut && knockInType == Barrier::Type::DownIn)) {
            DLOG("Barrier Types are UpIn/UpOut or DownIn/DownOut, we add a single Barrier Knock Out Option to our composite trade");

            QuantLib::ext::shared_ptr<Instrument> barrier2 =
                QuantLib::ext::make_shared<QuantLib::BarrierOption>(knockOutType, knockInLevel, 0, payoff, exercise);
            barrier2->setPricingEngine(fxBarrierOptBuilder->engine(boughtCcy, soldCcy, expiryDate, expiryDate));
            setSensitivityTemplate(*fxBarrierOptBuilder);
            QuantLib::ext::shared_ptr<InstrumentWrapper> koInstrument2 =
                QuantLib::ext::shared_ptr<InstrumentWrapper>(new SingleBarrierOptionWrapper(
                    barrier2, positionType == Position::Long ? false : true, expiryDate,
                    settleType == Settlement::Physical ? true : false, vanilla, knockOutType, spot, knockInLevel, 0,
                    soldCcy, start, fxIndex, cal, boughtAmount_, boughtAmount_, additionalInstruments, flippedAdditionalMultipliers));

            iw.push_back(koInstrument2);
            fxRates.push_back(fx);
            // For all other cases the KIKO is replicated as follows:
            // V_kiko(L,U) = V_knockout(L) - V_doubleknockout(L,U)
            // where L is the knockout level, U is the knockIn level
            // the Option is only exercised if L is never touched, and U has been touched
        } else {
            DLOG("We add a Double Barrier Knock Out Option to our composite trade");
            builder = engineFactory->builder("FxDoubleBarrierOption");
            QL_REQUIRE(builder, "No FxDoubleBarrierOption builder found");
            QuantLib::ext::shared_ptr<FxDoubleBarrierOptionEngineBuilder> fxDoubleBarrierOptBuilder =
                QuantLib::ext::dynamic_pointer_cast<FxDoubleBarrierOptionEngineBuilder>(builder);

            QuantLib::ext::shared_ptr<Instrument> doubleBarrier = QuantLib::ext::make_shared<DoubleBarrierOption>(
                DoubleBarrier::Type::KnockOut, std::min(knockInLevel, knockOutLevel),
                std::max(knockInLevel, knockOutLevel), 0, payoff, exercise);
            doubleBarrier->setPricingEngine(fxDoubleBarrierOptBuilder->engine(boughtCcy, soldCcy, expiryDate));
            setSensitivityTemplate(*fxDoubleBarrierOptBuilder);

            QuantLib::ext::shared_ptr<InstrumentWrapper> dkoInstrument =
                QuantLib::ext::shared_ptr<InstrumentWrapper>(new DoubleBarrierOptionWrapper(
                    doubleBarrier, positionType == Position::Long ? false : true, expiryDate,
                    settleType == Settlement::Physical ? true : false, vanilla, DoubleBarrier::Type::KnockOut, spot,
                    std::min(knockInLevel, knockOutLevel), std::max(knockInLevel, knockOutLevel), 0, soldCcy,
                    start, fxIndex, cal, boughtAmount_, boughtAmount_, additionalInstruments, flippedAdditionalMultipliers));

            iw.push_back(dkoInstrument);
            fxRates.push_back(fx);
        }
        instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new CompositeInstrumentWrapper(iw, fxRates, today));
    }

    if (start != Date()) {
        for (Date d = start; d <= expiryDate; d = cal.advance(d, 1 * Days))
            requiredFixings_.addFixingDate(d, fxIndex_, expiryDate);
    }
}

bool FxKIKOBarrierOption::checkBarrier(Real spot, Barrier::Type type, Real barrier) {
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

void FxKIKOBarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxKIKOBarrierOptionData");
    QL_REQUIRE(fxNode, "No FxKIKOBarrierOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    XMLNode* barrierNode = XMLUtils::getChildNode(fxNode, "Barriers");
    QL_REQUIRE(barrierNode, "No Barriers node");
    auto barriers = XMLUtils::getChildrenNodes(barrierNode, "BarrierData");
    for (auto const& b : barriers) {
        barriers_.push_back(BarrierData());
        barriers_.back().fromXML(b);
    }
    QL_REQUIRE(barriers_.size() == 2, "A KIKO barrier requires two BarrierData nodes");

    startDate_ = XMLUtils::getChildValue(fxNode, "StartDate", false);
    calendar_ = XMLUtils::getChildValue(fxNode, "Calendar", false);
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
    boughtCurrency_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    soldCurrency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    boughtAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    soldAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
}

XMLNode* FxKIKOBarrierOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxKIKOBarrierOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLNode* barrierNode = doc.allocNode("Barriers");
    for (auto& b : barriers_) {
        XMLUtils::appendNode(barrierNode, b.toXML(doc));
    }
    XMLUtils::appendNode(fxNode, barrierNode);

    XMLUtils::addChild(doc, fxNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, fxNode, "Calendar", calendar_);
    XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);
    XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency_);
    XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount_);
    XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency_);
    XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount_);

    return node;
}

} // namespace data
} // namespace oreplus
