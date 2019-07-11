/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file ored/portfolio/vanillaoption.hpp
\brief vanilla option representation
\ingroup tradedata
*/

#include <ored/portfolio/vanillaoption.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>
#include <ored/utilities/log.hpp>
#include <ql/instruments/oneassetoption.hpp>
#include <ql/instruments/vanillaoption.hpp>


using namespace QuantLib;

namespace ore {
namespace data {

void VanillaOptionTrade::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    Currency ccy = parseCurrency(currency_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for VanillaOption");

    // Payoff
    Option::Type type = parseOptionType(option_.callPut());
    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike_));

    QuantLib::Exercise::Type exerciseType = parseExerciseType(option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
    Date expiryDate = parseDate(option_.exerciseDates().front());

    // Exercise
    boost::shared_ptr<Exercise> exercise;
    switch(exerciseType) {
        case QuantLib::Exercise::Type::European: {
            exercise = boost::make_shared<EuropeanExercise>(expiryDate);
            break;
        }
        case QuantLib::Exercise::Type::American: {
            exercise = boost::make_shared<AmericanExercise>(expiryDate, option_.payoffAtExpiry());
            break;
        }
        default:
            QL_FAIL("Option Style " << option_.style() << " is not supported");
    }

    // Vanilla European/American.
    // If price adjustment is necessary we build a simple EU Option
    boost::shared_ptr<QuantLib::Instrument> instrument;

    // QL does not have an FXOption or EquityOption, so we add a vanilla one here and wrap
    // it in a composite to get the notional in.
    boost::shared_ptr<Instrument> vanilla = boost::make_shared<QuantLib::VanillaOption>(payoff, exercise);

    string tradeTypeBuider = tradeType_ + (exerciseType == QuantLib::Exercise::Type::European ? "" : "American");
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeTypeBuider);
    QL_REQUIRE(builder, "No builder found for " << tradeTypeBuider);
    boost::shared_ptr<VanillaOptionEngineBuilder> vanillaOptionBuilder =
        boost::dynamic_pointer_cast<VanillaOptionEngineBuilder>(builder);

    Time expiry = Actual365Fixed().yearFraction(Settings::instance().evaluationDate(), expiryDate);
    vanilla->setPricingEngine(vanillaOptionBuilder->engine(assetName_, ccy, expiry));

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = quantity_ * bsInd;

    // If premium data is provided
    // 1) build the fee trade and pass it to the instrument wrapper for pricing
    // 2) add fee payment as additional trade leg for cash flow reporting
    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    if (option_.premiumPayDate() != "" && option_.premiumCcy() != "") {
        Real premiumAmount = -bsInd * option_.premium(); // pay if long, receive if short
        Currency premiumCurrency = parseCurrency(option_.premiumCcy());
        Date premiumDate = parseDate(option_.premiumPayDate());
        addPayment(additionalInstruments, additionalMultipliers, premiumDate, premiumAmount, premiumCurrency, ccy,
                   engineFactory, vanillaOptionBuilder->configuration(MarketContext::pricing));
        DLOG("option premium added for asset option " << id());
    }

    instrument_ = boost::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(vanilla, mult, additionalInstruments, additionalMultipliers));

    npvCurrency_ = currency_;
    maturity_ = expiryDate;

    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike_ * quantity_;
}

void VanillaOptionTrade::fromXML(XMLNode* node) {
    Trade::fromXML(node);
}

XMLNode* VanillaOptionTrade::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    return node;
}

}
}
