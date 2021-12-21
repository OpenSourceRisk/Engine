/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <algorithm>

#include <ored/portfolio/barrieroption.hpp>
#include <ored/portfolio/builders/barrieroption.hpp>
#include <ql/experimental/barrieroption/doublebarrieroption.hpp>
#include <ql/instruments/barrieroption.hpp>

namespace ore {
namespace data {

void BarrierOptionTrade::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    Currency ccy = parseCurrency(currency_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for BarrierOption");

    // Payoff
    Option::Type type = parseOptionType(option_.callPut());
    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike_));

    QuantLib::Exercise::Type exerciseType = parseExerciseType(option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
    expiryDate_ = parseDate(option_.exerciseDates().front());

    // Set the maturity date equal to the expiry date.
    maturity_ = expiryDate_;

    // Exercise
    boost::shared_ptr<Exercise> exercise;
    switch (exerciseType) {
    case QuantLib::Exercise::Type::European: {
        exercise = boost::make_shared<EuropeanExercise>(expiryDate_);
        break;
    }
    default:
        // TODO?
        QL_FAIL("Barrier Option Style " << option_.style() << " is not supported");
    }

    // Create the instrument and the populate the name for the engine builder.
    boost::shared_ptr<Instrument> barrierInstrument;
    string tradeTypeBuilder = tradeType_;
    Settlement::Type settlementType = parseSettlementType(option_.settlement());
    if (exerciseType == Exercise::European && settlementType == Settlement::Cash) {
        // We have a European cash settled option.

        // Get the payment date.
        const boost::optional<OptionPaymentData>& opd = option_.paymentData();
        Date paymentDate = expiryDate_;
        if (opd) {
            if (opd->rulesBased()) {
                const Calendar& cal = opd->calendar();
                QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
                paymentDate = cal.advance(expiryDate_, opd->lag(), Days, opd->convention());
            } else {
                const vector<Date>& dates = opd->dates();
                QL_REQUIRE(dates.size() == 1, "Need exactly one payment date for cash settled European option.");
                paymentDate = dates[0];
            }
            QL_REQUIRE(paymentDate >= expiryDate_, "Payment date must be greater than or equal to expiry date.");
        }

        // TODO: The below check is kinda useless since we'll already fail in the if-statement if not equal.
        // And, when it doesn't fail above, it will never fail since paymentDate is assigned expiryDate_...
        QL_REQUIRE(paymentDate == expiryDate_, "deferred cash settled European-style barrier options not supported");
    }

    QL_REQUIRE(barrier_.barrierType() || barrier_.doubleBarrierType(),
               "must have either single or double barrier type specified in BarrierData");
    if (barrier_.barrierType()) {
        QL_REQUIRE(barrier_.levels().size() == 1, "expected single barrier level");
        if (barrier_.windowStyle() == "American") {
            barrierInstrument = boost::make_shared<BarrierOption>(*barrier_.barrierType(), barrier_.levels()[0],
                                                                  barrier_.rebate(), payoff, exercise);
        } else if (barrier_.windowStyle() == "PartialTime") {
            QL_REQUIRE(observationDates_.hasData(), "PartialTime style barrier options require ObservationDates");
            tradeTypeBuilder += barrier_.windowStyle();
            // The cover event date is equivalent to t1 in Haug's Option Pricing Formulas, p.160
            Date coverEventDate = makeSchedule(observationDates_).startDate();
            barrierInstrument = boost::make_shared<PartialTimeBarrierOption>(
                *barrier_.barrierType(), PartialBarrier::Range::EndB2, barrier_.levels()[0], barrier_.rebate(),
                coverEventDate, payoff, exercise);
        } else {
            QL_FAIL("expected window style American or PartialTime for single barrier option, got "
                    << barrier_.windowStyle());
        }
    } else if (barrier_.doubleBarrierType()) {
        QL_REQUIRE(barrier_.levels().size() == 2, "expected double barrier levels");
        Real lowerBarrier = std::min(barrier_.levels().front(), barrier_.levels().back());
        Real upperBarrier = std::max(barrier_.levels().front(), barrier_.levels().back());
        if (barrier_.windowStyle() == "American") {
            barrierInstrument = boost::make_shared<DoubleBarrierOption>(
                *barrier_.doubleBarrierType(), lowerBarrier, upperBarrier, barrier_.rebate(), payoff, exercise);
        } else {
            QL_FAIL("expected window style American for double barrier option, got " << barrier_.windowStyle());
        }
    }

    // Only try to set an engine on the option instrument if it is not expired. This avoids errors in
    // engine builders that rely on the expiry date being in the future.
    string configuration = Market::defaultConfiguration;
    if (!barrierInstrument->isExpired()) {
        boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeTypeBuilder);
        QL_REQUIRE(builder, "No builder found for " << tradeTypeBuilder);
        boost::shared_ptr<BarrierOptionEngineBuilder> barrierOptionBuilder =
            boost::dynamic_pointer_cast<BarrierOptionEngineBuilder>(builder);

        barrierInstrument->setPricingEngine(barrierOptionBuilder->engine(assetName_, ccy, expiryDate_));

        configuration = barrierOptionBuilder->configuration(MarketContext::pricing);

    } else {
        DLOG("No engine attached for barrier option on trade "
             << id() << " with expiry date " << io::iso_date(expiryDate_) << " because it is expired.");
    }

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = quantity_ * bsInd;

    // If premium data is provided
    // 1) build the fee trade and pass it to the instrument wrapper for pricing
    // 2) add fee payment as additional trade leg for cash flow reporting
    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    //if (option_.premiumPayDate() != "" && option_.premiumCcy() != "") {
    //    Real premiumAmount = -bsInd * option_.premium(); // pay if long, receive if short
    //    Currency premiumCurrency = parseCurrency(option_.premiumCcy());
    //    Date premiumDate = parseDate(option_.premiumPayDate());
    //    addPayment(additionalInstruments, additionalMultipliers, premiumDate, premiumAmount, premiumCurrency, ccy,
    //               engineFactory, configuration);
    //    DLOG("option premium added for barrier option " << id());
    //}

    instrument_ = boost::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(barrierInstrument, mult, additionalInstruments, additionalMultipliers));

    npvCurrency_ = currency_;

    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike_ * quantity_;
    notionalCurrency_ = currency_;
}

void BarrierOptionTrade::fromXML(XMLNode* node) { Trade::fromXML(node); }

XMLNode* BarrierOptionTrade::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    return node;
}

} // namespace data
} // namespace ore
