/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/portfolio/builders/equitycompositeoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = strike_.value();
    additionalData_["strikeCurrency"] = strike_.currency();

    // Set the assetName_ as it may have changed after lookup
    assetName_ = equityName();

    Currency ccy = parseCurrencyWithMinors(currency_);
    npvCurrency_ = notionalCurrency_ = ccy.code();

    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike_.value() * quantity_;

    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    expiryDate_ = parseDate(option_.exerciseDates().front());
    // Set the maturity date equal to the expiry date. It may get updated below if option is cash settled with
    // payment after expiry.
    maturity_ = expiryDate_;

    // Populate the index_ in case the option is automatic exercise.
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    index_ = *market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing));

    // check the equity currency
    underlyingCurrency_ =
        market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing))->currency();
    QL_REQUIRE(!underlyingCurrency_.empty(), "No equity currency in equityCurve for equity " << assetName_ << ".");
        
    // Set the strike currency - if we have a minor currency, convert the strike
    if (!strikeCurrency_.empty())
        strike_.setCurrency(strikeCurrency_);
    else if (strike_.currency().empty()) {
        // If payoff currency and underlying currency are equivalent (and payoff currency could be a minor currency)
        if (ccy == underlyingCurrency_) {
            TLOG("Setting strike currency to payoff currency " << ccy << " for trade " << id() << ".");
            strike_.setCurrency(ccy.code());
        } else {
            // If quanto payoff, then strike currency must be populated to avoid confusion over what the
            // currency of the strike payoff is: can be either underlying currency or payoff currency
            QL_FAIL("Strike currency must be specified for a quanto payoff for trade " << id() << ".");
        }
    }
    // Quanto payoff condition, i.e. currency_ != underlyingCurrency_, will be checked in VanillaOptionTrade::build()
    // Build the trade using the shared functionality in the base class.
    if (strike_.currency() != underlyingCurrency_.code()) {
   
        // We have a composite EQ Trade

        Currency strikeCcy = parseCurrencyWithMinors(strikeCurrency_);
        QL_REQUIRE(ccy == strikeCcy, "Equity composite option requires pay ccy ("
                                         << ccy.code() << ") to match strike ccy (" << strikeCcy.code()
                                         << "), quanto composite options are not supported (underlying currency is "
                                         << underlyingCurrency_.code() << ")");

        // Exercise
        QuantLib::Exercise::Type exerciseType = parseExerciseType(option_.style());
        QuantLib::ext::shared_ptr<Exercise> exercise;
        switch (exerciseType) {
        case QuantLib::Exercise::Type::European: {
            exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate_);
            break;
        }
        default:
            QL_FAIL("Option Style " << option_.style() << " is not supported for an composite equity option");
        }
        Settlement::Type settlementType = parseSettlementType(option_.settlement());
        // Create the instrument and then populate the name for the engine builder.
        QuantLib::ext::shared_ptr<Instrument> vanilla;
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

            QL_REQUIRE(paymentDate <= expiryDate_,
                       "Payment date must equal expiry date for a Composite payoff. Trade: " << id() << ".");
        }
        QL_REQUIRE(forwardDate_ ==
                    QuantLib::Date(), "Composite payoff is not currently supported for Forward Options: Trade "
                        << id());
        Option::Type type = parseOptionType(option_.callPut());
        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike_.value()));
        vanilla = QuantLib::ext::make_shared<QuantLib::VanillaOption>(payoff, exercise);

        string tradeTypeBuilder = "EquityEuropeanCompositeOption";

        QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeTypeBuilder);
        QL_REQUIRE(builder, "No builder found for " << tradeTypeBuilder);

        // TODO cast and set pricing engine

        auto compositeBuilder = QuantLib::ext::dynamic_pointer_cast<EquityEuropeanCompositeEngineBuilder>(builder);
        vanilla->setPricingEngine(compositeBuilder->engine(assetName_, underlyingCurrency_, 
            parseCurrency(strike_.currency()), expiryDate_));
        setSensitivityTemplate(*compositeBuilder);

        string configuration = Market::defaultConfiguration;
        Position::Type positionType = parsePositionType(option_.longShort());
        Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
        Real mult = quantity_ * bsInd;

        std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
        std::vector<Real> additionalMultipliers;
        maturity_ =
            std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(),
                                            -bsInd, ccy, engineFactory, configuration));

        instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
            new VanillaInstrument(vanilla, mult, additionalInstruments, additionalMultipliers));

    } else {
        VanillaOptionTrade::build(engineFactory);
    }
}

void EquityOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityOptionData");
    QL_REQUIRE(eqNode, "No EquityOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    strike_.fromXML(eqNode);
    
    strikeCurrency_ = XMLUtils::getChildValue(eqNode, "StrikeCurrency", false);
    if (!strikeCurrency_.empty())
        WLOG("EquityOption::fromXML: node StrikeCurrency is deprecated, please use StrikeData node");
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
}

XMLNode* EquityOption::toXML(XMLDocument& doc) const {
    XMLNode* node = VanillaOptionTrade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);

    XMLUtils::appendNode(eqNode, strike_.toXML(doc));
    if (!strikeCurrency_.empty())
        XMLUtils::addChild(doc, eqNode, "StrikeCurrency", strikeCurrency_);

    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

std::map<AssetClass, std::set<std::string>> EquityOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::EQ, std::set<std::string>({equityName()})}};
}

} // namespace data
} // namespace ore
