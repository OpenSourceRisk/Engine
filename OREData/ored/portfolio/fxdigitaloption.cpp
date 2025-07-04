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
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void FxDigitalOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Digital");  
    additionalData_["isdaTransaction"] = string("");  

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;

    // Only European Vanilla supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(option_.payoffAtExpiry() == true, "PayoffAtExpiry must be True for FxDigitalOption");
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxDigitalOption");
    QL_REQUIRE(strike_ > 0.0 && strike_ != Null<Real>(), "Invalid strike " << strike_);

    Currency domCcy = parseCurrency(domesticCurrency_);
    Currency forCcy = parseCurrency(foreignCurrency_);

    // Payoff Type
    Option::Type type = parseOptionType(option_.callPut());

    // Handle PayoffCurrency, we might have to flip the trade here
    Real strike = strike_;

    bool flipResults = false;
    if (payoffCurrency_ == "") {
        DLOG("PayoffCurrency defaulting to " << domesticCurrency_ << " for FxDigitalOption " << id());
    } else if (payoffCurrency_ == foreignCurrency_) {
        // Invert the trade, switch dom and for and flip Put/Call
        strike = 1.0 / strike;
        std::swap(domCcy, forCcy);
        type = type == Option::Call ? Option::Put : Option::Call;
        flipResults = true;
    } else if (payoffCurrency_ != domesticCurrency_) {
        QL_FAIL("Invalid Payoff currency (" << payoffCurrency_ << ") for FxDigitalOption " << forCcy << domCcy);
    }
    DLOG("Setting up FxDigitalOption with strike " << strike << " foreign " << forCcy << " domestic " << domCcy);

    // Set up the CashOrNothing
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, strike, payoffAmount_));

    npvCurrency_ = domCcy.code(); // don't use domesticCurrency_ as it might be flipped
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_ != "" ? payoffCurrency_ : domesticCurrency_; // see logic above

    // Exercise
    Date expiryDate = parseDate(option_.exerciseDates().front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);
    
    Date paymentDate = expiryDate;
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
    maturity_ = std::max(option_.premiumData().latestPremiumDate(), paymentDate);
    maturityType_ = maturity_ == expiryDate ? "Expiry Date" : "Option's Latest Premium Date";
    QuantLib::ext::shared_ptr<Instrument> vanilla;
    Real exercisePrice = Null<Real>();
    bool exercised = false;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex;
    if (paymentDate == expiryDate) {
        const boost::optional<OptionExerciseData>& oed = option_.exerciseData();
        if (oed) {
            QL_REQUIRE(oed->date() == expiryDate, "The supplied exercise date ("
                                                        << io::iso_date(oed->date())
                                                        << ") should equal the option's expiry date ("
                                                        << io::iso_date(expiryDate) << ").");
            exercised = true;
            exercisePrice = oed->price();
        }
        if (option_.isAutomaticExercise()) {

            fxIndex = buildFxIndex(fxIndex_, domCcy.code(), forCcy.code(), engineFactory->market(),
                                    engineFactory->configuration(MarketContext::pricing));
            requiredFixings_.addFixingDate(expiryDate, fxIndex_, paymentDate);
        }

        vanilla = QuantLib::ext::make_shared<CashSettledEuropeanOption>(type, strike, payoffAmount_, expiryDate,
                                                                        paymentDate, option_.isAutomaticExercise(),
                                                                        fxIndex, exercised, exercisePrice);
        // set pricing engines
        QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("FxDigitalOptionEuropeanCS");
        QL_REQUIRE(builder, "No builder found for " << tradeType_);
        QuantLib::ext::shared_ptr<FxDigitalCSOptionEngineBuilder> fxOptBuilder =
            QuantLib::ext::dynamic_pointer_cast<FxDigitalCSOptionEngineBuilder>(builder);
        vanilla->setPricingEngine(fxOptBuilder->engine(forCcy, domCcy, flipResults));
        setSensitivityTemplate(*fxOptBuilder);
        addProductModelEngine(*fxOptBuilder);
        Position::Type positionType = parsePositionType(option_.longShort());
        Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
        Real mult = bsInd;
        std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
        std::vector<Real> additionalMultipliers;
        addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(), -bsInd, domCcy,
                    engineFactory, fxOptBuilder->configuration(MarketContext::pricing));
        instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
            new VanillaInstrument(vanilla, mult, additionalInstruments, additionalMultipliers));
    } else {
        QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("FxOptionForward");
        vanilla = QuantLib::ext::make_shared<QuantExt::VanillaForwardOption>(payoff, exercise, paymentDate, paymentDate);
        QuantLib::ext::shared_ptr<VanillaOptionEngineBuilder> fxOptBuilder =
            QuantLib::ext::dynamic_pointer_cast<VanillaOptionEngineBuilder>(builder);
        vanilla->setPricingEngine(fxOptBuilder->engine(
            forCcy, domCcy, envelope().additionalField("discount_curve", false, std::string()), paymentDate));
        setSensitivityTemplate(*fxOptBuilder);
        addProductModelEngine(*fxOptBuilder);
        Position::Type positionType = parsePositionType(option_.longShort());
        Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
        Real mult = bsInd;
        std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
        std::vector<Real> additionalMultipliers;
        addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(), -bsInd, domCcy,
                    engineFactory, fxOptBuilder->configuration(MarketContext::pricing));
        instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
            new VanillaInstrument(vanilla, mult, additionalInstruments, additionalMultipliers));
    }
}

void FxDigitalOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxDigitalOptionData");
    QL_REQUIRE(fxNode, "No FxDigitalOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    strike_ = XMLUtils::getChildValueAsDouble(fxNode, "Strike", true);
    payoffCurrency_ = XMLUtils::getChildValue(fxNode, "PayoffCurrency", false);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "PayoffAmount", true);
    foreignCurrency_ = XMLUtils::getChildValue(fxNode, "ForeignCurrency", true);
    domesticCurrency_ = XMLUtils::getChildValue(fxNode, "DomesticCurrency", true);
}

XMLNode* FxDigitalOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxDigitalOptionData");
    XMLUtils::appendNode(node, fxNode);
    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "Strike", strike_);
    XMLUtils::addChild(doc, fxNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, fxNode, "PayoffAmount", payoffAmount_);
    XMLUtils::addChild(doc, fxNode, "ForeignCurrency", foreignCurrency_);
    XMLUtils::addChild(doc, fxNode, "DomesticCurrency", domesticCurrency_);

    return node;
}
} // namespace data
} // namespace oreplus
