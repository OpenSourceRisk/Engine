/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/vanillaoption.hpp
\brief vanilla option representation
\ingroup tradedata
*/

#include <ored/portfolio/builders/vanillaoption.hpp>
#include <ored/portfolio/builders/quantovanillaoption.hpp>
#include <ored/portfolio/vanillaoption.hpp>
#include <ored/utilities/log.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/instruments/quantovanillaoption.hpp>
#include <qle/instruments/vanillaforwardoption.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>

using namespace QuantLib;
using QuantExt::CashSettledEuropeanOption;

namespace ore {
namespace data {

void VanillaOptionTrade::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    setNotionalAndCurrencies();

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for VanillaOption");

    // If underlying currency is empty, then set to payment currency by default.
    // If non-empty, then check if the currencies are different for a Quanto payoff
    Currency ccy = parseCurrencyWithMinors(currency_);
    Currency underlyingCurrency = underlyingCurrency_.empty() ? ccy : underlyingCurrency_;
    bool sameCcy = underlyingCurrency == ccy;
    
    if (strike_.currency().empty())
        strike_.setCurrency(ccy.code());

    // Payoff
    Option::Type type = parseOptionType(option_.callPut());
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike_.value()));
    QuantLib::Exercise::Type exerciseType = parseExerciseType(option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    expiryDate_ = parseDate(option_.exerciseDates().front());
    // Set the maturity date equal to the expiry date. It may get updated below if option is cash settled with
    // payment after expiry.
    maturity_ = expiryDate_;
    // Exercise
    QuantLib::ext::shared_ptr<Exercise> exercise;
    switch (exerciseType) {
    case QuantLib::Exercise::Type::European: {
        exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate_);
        break;
    }
    case QuantLib::Exercise::Type::American: {
        exercise = QuantLib::ext::make_shared<AmericanExercise>(expiryDate_, option_.payoffAtExpiry());
        break;
    }
    default:
        QL_FAIL("Option Style " << option_.style() << " is not supported");
    }
    // Create the instrument and then populate the name for the engine builder.
    QuantLib::ext::shared_ptr<Instrument> vanilla;
    string tradeTypeBuilder = tradeType_;
    Settlement::Type settlementType = parseSettlementType(option_.settlement());

    // For Quanto, check for European and Cash, except for an FX underlying
    if (!sameCcy) {
        QL_REQUIRE(exerciseType == Exercise::Type::European, "Option exercise must be European for a Quanto payoff.");
        if (settlementType == Settlement::Type::Physical) {
            QL_REQUIRE(assetClassUnderlying_ == AssetClass::FX,
                       "Physically settled Quanto options are allowed only for an FX underlying.");
        }
    }

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

        if (paymentDate > expiryDate_) {
            QL_REQUIRE(sameCcy, "Payment date must equal expiry date for a Quanto payoff. Trade: " << id() << ".");

            // Build a QuantExt::CashSettledEuropeanOption if payment date is strictly greater than expiry.

            // Has the option been marked as exercised
            const boost::optional<OptionExerciseData>& oed = option_.exerciseData();
            bool exercised = false;
            Real exercisePrice = Null<Real>();
            if (oed) {
                QL_REQUIRE(oed->date() == expiryDate_, "The supplied exercise date ("
                                                           << io::iso_date(oed->date())
                                                           << ") should equal the option's expiry date ("
                                                           << io::iso_date(expiryDate_) << ").");
                exercised = true;
                exercisePrice = oed->price();
            }

            // If automatic exercise, we will need an index fixing on the expiry date.
            if (option_.isAutomaticExercise()) {
                QL_REQUIRE(index_, "Option trade " << id() << " has automatic exercise so we need a valid index.");
                // If index name has not been populated, use logic here to populate it from the index object.
                string indexName = indexName_;
                if (indexName.empty()) {
                    indexName = index_->name();
                    if (assetClassUnderlying_ == AssetClass::EQ)
                        indexName = "EQ-" + indexName;
                }
                requiredFixings_.addFixingDate(expiryDate_, indexName, paymentDate);
            }

            // Build the instrument
            LOG("Build CashSettledEuropeanOption for trade " << id());
            vanilla = QuantLib::ext::make_shared<CashSettledEuropeanOption>(
                type, strike_.value(), expiryDate_, paymentDate, option_.isAutomaticExercise(), index_, exercised, exercisePrice);

            // Allow for a separate pricing engine that takes care of payment on a date after expiry. Do this by
            // appending 'EuropeanCS' to the trade type.
            tradeTypeBuilder = tradeType_ + "EuropeanCS";

            // Update the maturity date.
            maturity_ = paymentDate;

        } else {
            if (forwardDate_ == QuantLib::Date()) {
                // If payment date is not greater than expiry, build QuantLib::VanillaOption.
                if (sameCcy) {
                    LOG("Build VanillaOption for trade " << id());
                    vanilla = QuantLib::ext::make_shared<QuantLib::VanillaOption>(payoff, exercise);
                }
                else {
                    LOG("Build QuantoVanillaOption for trade " << id());
                    vanilla = QuantLib::ext::make_shared<QuantLib::QuantoVanillaOption>(payoff, exercise);
                    if (assetClassUnderlying_ == AssetClass::EQ)
                        tradeTypeBuilder = "QuantoEquityOption";
                    else if (assetClassUnderlying_ == AssetClass::COM)
                        tradeTypeBuilder = "QuantoCommodityOption";
                    else
                        QL_FAIL("Option Quanto payoff not supported for " << assetClassUnderlying_ << " class.");
                }
            } else {
                LOG("Build VanillaForwardOption for trade " << id());
                QL_REQUIRE(sameCcy, "Quanto payoff is not currently supported for Forward Options: Trade " << id());
                vanilla = QuantLib::ext::make_shared<QuantExt::VanillaForwardOption>(payoff, exercise, forwardDate_);
                if (assetClassUnderlying_ == AssetClass::COM)
                    tradeTypeBuilder = tradeType_ + "Forward";
            }
        }

    } else {
        if (forwardDate_ == QuantLib::Date()) {
            // If not European or not cash settled, build QuantLib::VanillaOption.
            if (sameCcy) {
                LOG("Build VanillaOption for trade " << id());
                vanilla = QuantLib::ext::make_shared<QuantLib::VanillaOption>(payoff, exercise);
            } else {
                LOG("Build QuantoVanillaOption for trade " << id());
                vanilla = QuantLib::ext::make_shared<QuantLib::QuantoVanillaOption>(payoff, exercise);
            }
        } else {
            QL_REQUIRE(exerciseType == QuantLib::Exercise::Type::European, "Only European Forward Options currently supported");
            LOG("Built VanillaForwardOption for trade " << id());
            vanilla = QuantLib::ext::make_shared<QuantExt::VanillaForwardOption>(payoff, exercise, forwardDate_, paymentDate_);
            if (assetClassUnderlying_ == AssetClass::COM)
                tradeTypeBuilder = tradeType_ + "Forward";
        }

        // If the tradeTypeBuilder has not been modified yet..
        if (tradeTypeBuilder == tradeType_) {
            if (sameCcy)
                tradeTypeBuilder = tradeType_ + (exerciseType == QuantLib::Exercise::Type::European ? "" : "American");
            else
                tradeTypeBuilder = "QuantoFxOption";
        }
    }
    LOG("tradeTypeBuilder set to " << tradeTypeBuilder << " for trade " << id());

    // Generally we need to set the pricing engine here even if the option is expired at build time, since the valuation date
    // might change after build, and we get errors for the edge case valuation date = expiry date for European options.
    string configuration = Market::defaultConfiguration;
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeTypeBuilder);
    QL_REQUIRE(builder, "No builder found for " << tradeTypeBuilder);

    if (sameCcy) {
        QuantLib::ext::shared_ptr<VanillaOptionEngineBuilder> vanillaOptionBuilder =
                QuantLib::ext::dynamic_pointer_cast<VanillaOptionEngineBuilder>(builder);
	QL_REQUIRE(vanillaOptionBuilder != nullptr, "No engine builder found for trade type " << tradeTypeBuilder);

    if (forwardDate_ != Date()) {
        vanilla->setPricingEngine(vanillaOptionBuilder->engine(assetName_, ccy, expiryDate_, false));
    } else {
        vanilla->setPricingEngine(vanillaOptionBuilder->engine(assetName_, ccy, expiryDate_, true));
    }
    setSensitivityTemplate(*vanillaOptionBuilder);

	configuration = vanillaOptionBuilder->configuration(MarketContext::pricing);
    } else {
        QuantLib::ext::shared_ptr<QuantoVanillaOptionEngineBuilder> quantoVanillaOptionBuilder =
                QuantLib::ext::dynamic_pointer_cast<QuantoVanillaOptionEngineBuilder>(builder);
	QL_REQUIRE(quantoVanillaOptionBuilder != nullptr, "No (Quanto) engine builder found for trade type "
                                                                << tradeTypeBuilder);

	vanilla->setPricingEngine(quantoVanillaOptionBuilder->engine(assetName_, underlyingCurrency, ccy, expiryDate_));
        setSensitivityTemplate(*quantoVanillaOptionBuilder);

	configuration = quantoVanillaOptionBuilder->configuration(MarketContext::pricing);
    }

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = quantity_ * bsInd;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, mult,
                                                option_.premiumData(), -bsInd, ccy, engineFactory, configuration));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(vanilla, mult, additionalInstruments, additionalMultipliers));
}

void VanillaOptionTrade::setNotionalAndCurrencies() {
    Currency ccy = parseCurrencyWithMinors(currency_);
    npvCurrency_ = ccy.code();

    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike_.value() * quantity_;
    // the following is correct for vanilla (sameCcy = true) and quanto (sameCcy = false)
    notionalCurrency_ = ccy.code();
}
} // namespace data
} // namespace ore
