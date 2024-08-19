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
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void FxEuropeanBarrierOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Barrier");  
    additionalData_["isdaTransaction"] = string("");  

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    // Only European Single Barrier supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barrier_.levels().size() == 1, "Invalid number of barrier levels");
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "European", "Only european barrier style suppported");
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxEuropeanBarrierOption");

    Currency boughtCcy = parseCurrency(boughtCurrency_);
    Currency soldCcy = parseCurrency(soldCurrency_);
    Real level = barrier_.levels()[0].value();
    Real rebate = barrier_.rebate();
    QL_REQUIRE(rebate >= 0, "Rebate must be non-negative");

    // Replicate the payoff of European Barrier Option (with strike K and barrier B) using combinations of options

    // Call
    //   Up
    //     In
    //       Long Up&Out Digital Option with barrier B payoff rebate
    //       B > K
    //         Long European Call Option with strike B
    //         Long Up&In Digital Option with barrier B payoff B - K
    //       B <= K
    //         Long European Call Option with strike K
    //     Out
    //       Long Up&In Digital Option with barrier B payoff rebate
    //       B > K
    //         Long European Call Option with strike K
    //         Short European Call Option with strike B
    //         Short Up&In Digital Option with barrier B payoff B - K
    //       B <= K
    //         0
    //   Down
    //     In
    //       Long Down&Out Digital Option with barrier B payoff rebate
    //       B > K
    //         Long European Call Option with strike K
    //       B <= K
    //         0
    //     Out
    //       Long Down&In Digital Option with barrier B payoff rebate
    //       B > K
    //         Long European Call Option with strike B
    //         Long Down&Out Digital Option with barrier B payoff B - K
    //       B <= K
    //         Long European Call Option with strike K

    // Put
    //   Up
    //     In
    //       Long Up&Out Digital Option with barrier B payoff rebate
    //       B > K
    //         0
    //       B <= K
    //         Long European Put Option with strike K
    //         Short European Put Option with strike B
    //         Short Up&Out Digital Option with barrier B payoff K - B
    //     Out
    //       Long Up&In Digital Option with barrier B payoff rebate
    //       B > K
    //         Long European Put Option with strike K
    //       B <= K
    //         Long European Put Option with strike B
    //         Long Up&Out Digital Option with barrier B payoff K - B
    //   Down
    //     In
    //       Long Down&Out Digital Option with barrier B payoff rebate
    //       B > K
    //         Long European Put Option with strike K
    //       B <= K
    //         Long European Put Option with strike B
    //         Long Down&In Digital Option with barrier B payoff K - B
    //     Out
    //       Long Down&In Digital Option with barrier B payoff rebate
    //       B > K
    //         0
    //       B <= K
    //         Long European Put Option with strike K

    Real strike = this->strike();
    Option::Type type = parseOptionType(option_.callPut());

    // Exercise
    Date expiryDate = parseDate(option_.exerciseDates().front());
    Date paymentDate = expiryDate;

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    const boost::optional<OptionPaymentData>& opd = option_.paymentData();
    if (opd) {
        if (opd->rulesBased()) {
            const Calendar& cal = opd->calendar();
            QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
            paymentDate = cal.advance(expiryDate, opd->lag(), Days, opd->convention());
        } else {
            const vector<Date>& dates = opd->dates();
            QL_REQUIRE(dates.size() == 1, "Need exactly one payment date for cash settled European option.");
            paymentDate = dates[0];
        }
        QL_REQUIRE(paymentDate >= expiryDate, "Payment date must be greater than or equal to expiry date.");
    }

    // delayed pay date is only affecting the maturity
    maturity_ = std::max({option_.premiumData().latestPremiumDate(), paymentDate});

    QuantLib::ext::shared_ptr<Instrument> digital;
    QuantLib::ext::shared_ptr<Instrument> vanillaK;
    QuantLib::ext::shared_ptr<Instrument> vanillaB;
    QuantLib::ext::shared_ptr<Instrument> rebateInstrument;

    bool exercised = false;
    Real exercisePrice = Null<Real>();
    Barrier::Type barrierType = parseBarrierType(barrier_.type());

    Option::Type rebateType;
    if (barrierType == Barrier::Type::UpIn || barrierType == Barrier::Type::DownOut) {
        // Payoff - Up&Out / Down&In Digital Option with barrier B payoff rebate
        rebateType = Option::Put;
    } else {
        // Payoff - Up&In / Down&Out Digital Option with barrier B payoff rebate
        rebateType = Option::Call;
    }

    if (paymentDate > expiryDate) {

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

        QuantLib::ext::shared_ptr<FxIndex> fxIndex;
        if (option_.isAutomaticExercise()) {
            QL_REQUIRE(!fxIndex_.empty(), "FX european barrier option trade with delay payment "
                                              << id() << ": the FXIndex node needs to be populated.");
            fxIndex = buildFxIndex(fxIndex_, soldCcy.code(), boughtCcy.code(), engineFactory->market(),
                                   engineFactory->configuration(MarketContext::pricing));
            requiredFixings_.addFixingDate(expiryDate, fxIndex_, paymentDate);
        }

        vanillaK = QuantLib::ext::make_shared<CashSettledEuropeanOption>(
            type, strike, expiryDate, paymentDate, option_.isAutomaticExercise(), fxIndex, exercised, exercisePrice);
        vanillaB = QuantLib::ext::make_shared<CashSettledEuropeanOption>(
            type, level, expiryDate, paymentDate, option_.isAutomaticExercise(), fxIndex, exercised, exercisePrice);
        digital = QuantLib::ext::make_shared<CashSettledEuropeanOption>(type, level, fabs(level - strike), expiryDate,
                                                                paymentDate, option_.isAutomaticExercise(), fxIndex,
                                                                exercised, exercisePrice);
        rebateInstrument = QuantLib::ext::make_shared<CashSettledEuropeanOption>(rebateType, level, rebate, expiryDate,
                                                                paymentDate, option_.isAutomaticExercise(), fxIndex,
                                                                exercised, exercisePrice);
    } else {
        // Payoff - European Option with strike K
        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoffVanillaK(new PlainVanillaPayoff(type, strike));
        // Payoff - European Option with strike B
        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoffVanillaB(new PlainVanillaPayoff(type, level));
        // Payoff - Digital Option with barrier B payoff abs(B - K)
        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoffDigital(new CashOrNothingPayoff(type, level, fabs(level - strike)));
        QuantLib::ext::shared_ptr<StrikedTypePayoff> rebatePayoff(new CashOrNothingPayoff(rebateType, level, rebate));

        vanillaK = QuantLib::ext::make_shared<VanillaOption>(payoffVanillaK, exercise);
        vanillaB = QuantLib::ext::make_shared<VanillaOption>(payoffVanillaB, exercise);
        digital = QuantLib::ext::make_shared<VanillaOption>(payoffDigital, exercise);
        rebateInstrument = QuantLib::ext::make_shared<VanillaOption>(rebatePayoff, exercise);
    }
    
    // This is for when/if a PayoffCurrency is added to the instrument,
    // which would require flipping the underlying currency pair
    const bool flipResults = false;

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder;
    QuantLib::ext::shared_ptr<EngineBuilder> digitalBuilder;
    QuantLib::ext::shared_ptr<VanillaOptionEngineBuilder> fxOptBuilder;

    if (paymentDate > expiryDate) {
        builder = engineFactory->builder("FxOptionEuropeanCS");
        QL_REQUIRE(builder, "No builder found for FxOptionEuropeanCS");
        fxOptBuilder = QuantLib::ext::dynamic_pointer_cast<FxEuropeanCSOptionEngineBuilder>(builder);

        digitalBuilder = engineFactory->builder("FxDigitalOptionEuropeanCS");
        QL_REQUIRE(digitalBuilder, "No builder found for FxDigitalOptionEuropeanCS");
        auto fxDigitalOptBuilder = QuantLib::ext::dynamic_pointer_cast<FxDigitalCSOptionEngineBuilder>(digitalBuilder);
        digital->setPricingEngine(fxDigitalOptBuilder->engine(boughtCcy, soldCcy));
        rebateInstrument->setPricingEngine(fxDigitalOptBuilder->engine(boughtCcy, soldCcy));
        setSensitivityTemplate(*fxDigitalOptBuilder);
    } else {
        builder = engineFactory->builder("FxOption");
        QL_REQUIRE(builder, "No builder found for FxOption");
        fxOptBuilder = QuantLib::ext::dynamic_pointer_cast<FxEuropeanOptionEngineBuilder>(builder);
        
        digitalBuilder = engineFactory->builder("FxDigitalOption");
        QL_REQUIRE(digitalBuilder, "No builder found for FxDigitalOption");
        auto fxDigitalOptBuilder = QuantLib::ext::dynamic_pointer_cast<FxDigitalOptionEngineBuilder>(digitalBuilder);
        digital->setPricingEngine(fxDigitalOptBuilder->engine(boughtCcy, soldCcy, flipResults));
        rebateInstrument->setPricingEngine(fxDigitalOptBuilder->engine(boughtCcy, soldCcy, flipResults));
        setSensitivityTemplate(*fxDigitalOptBuilder);
    }

    vanillaK->setPricingEngine(fxOptBuilder->engine(boughtCcy, soldCcy, paymentDate));
    vanillaB->setPricingEngine(fxOptBuilder->engine(boughtCcy, soldCcy, paymentDate));
    setSensitivityTemplate(*fxOptBuilder);

    QuantLib::ext::shared_ptr<CompositeInstrument> qlInstrument = QuantLib::ext::make_shared<CompositeInstrument>();
    qlInstrument->add(rebateInstrument);
    if (type == Option::Call) {
        if (barrierType == Barrier::Type::UpIn || barrierType == Barrier::Type::DownOut) {
            if (level > strike) {
                qlInstrument->add(vanillaB);
                qlInstrument->add(digital);
            } else {
                qlInstrument->add(vanillaK);
            }
        } else if (barrierType == Barrier::Type::UpOut || barrierType == Barrier::Type::DownIn) {
            if (level > strike) {
                qlInstrument->add(vanillaK);
                qlInstrument->add(vanillaB, -1);
                qlInstrument->add(digital, -1);
            } else {
                // empty
            }
        } else {
            QL_FAIL("Unknown Barrier Type: " << barrierType);
        }
    } else if (type == Option::Put) {
        if (barrierType == Barrier::Type::UpIn || barrierType == Barrier::Type::DownOut) {
            if (level > strike) {
                // empty
            } else {
                qlInstrument->add(vanillaK);
                qlInstrument->add(vanillaB, -1);
                qlInstrument->add(digital, -1);
            }
        } else if (barrierType == Barrier::Type::UpOut || barrierType == Barrier::Type::DownIn) {
            if (level > strike) {
                qlInstrument->add(vanillaK);
            } else {
                qlInstrument->add(vanillaB);
                qlInstrument->add(digital);
            }
        } else {
            QL_FAIL("Unknown Barrier Type: " << barrierType);
        }
    }

    // Add additional premium payments
    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = boughtAmount_ * bsInd;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(), -bsInd, soldCcy,
                engineFactory, fxOptBuilder->configuration(MarketContext::pricing));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(qlInstrument, mult, additionalInstruments, additionalMultipliers));

    npvCurrency_ = soldCurrency_; // sold is the domestic
    notional_ = soldAmount_;
    notionalCurrency_ = soldCurrency_;

    additionalData_["boughtCurrency"] = boughtCurrency_;
    additionalData_["boughtAmount"] = boughtAmount_;
    additionalData_["soldCurrency"] = soldCurrency_;
    additionalData_["soldAmount"] = soldAmount_;
    if (!fxIndex_.empty())
        additionalData_["FXIndex"] = fxIndex_;
}

void FxEuropeanBarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxEuropeanBarrierOptionData");
    QL_REQUIRE(fxNode, "No FxEuropeanBarrierOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(fxNode, "BarrierData"));
    boughtCurrency_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    soldCurrency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    boughtAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    soldAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false, "");
}

XMLNode* FxEuropeanBarrierOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxEuropeanBarrierOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::appendNode(fxNode, barrier_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency_);
    XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount_);
    XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency_);
    XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount_);

    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);

    return node;
}

Real FxEuropeanBarrierOption::strike() const {
    return soldAmount_ / boughtAmount_;
}
} // namespace data
} // namespace ore
