/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/scripting/engines/amccgfxoptionengine.hpp>

#include <ql/cashflows/simplecashflow.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

void AmcCgFxOptionEngineBase::buildComputationGraph() const { AmcCgBaseEngine::buildComputationGraph(); }

void AmcCgFxOptionEngineBase::setupLegs() const {
    QL_REQUIRE(payoff_, "AmcCgFxOptionEngineBase: payoff has unexpected type");

    if (payDate_ == Null<Date>())
        payDate_ = exercise_->dates().back();

    Real w = payoff_->optionType() == Option::Call ? 1.0 : -1.0;
    Leg domesticLeg{QuantLib::ext::make_shared<SimpleCashFlow>(-w * payoff_->strike(), payDate_)};
    Leg foreignLeg{QuantLib::ext::make_shared<SimpleCashFlow>(w, payDate_)};

    leg_ = {domesticLeg, foreignLeg};
    currency_ = {domCcy_, forCcy_};
    payer_ = {false, false};
}

void AmcCgFxOptionEngineBase::calculateFxOptionBase() const {
    QL_REQUIRE(exercise_->type() == Exercise::European, "AmcCgFxOptionEngineBase: not an European option");
    QL_REQUIRE(!exercise_->dates().empty(), "AmcCgFxOptionEngineBase: exercise dates are empty");

    exerciseIntoIncludeSameDayFlows_ = true;

    AmcCgBaseEngine::calculate();
}

void AmcCgFxOptionEngine::calculate() const {
    payoff_ = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    exercise_ = arguments_.exercise;
    optionSettlement_ = Settlement::Physical;
    payDate_ = Date(); // will be set in calculateFxOptionBase()

    AmcCgFxOptionEngineBase::setupLegs();
    AmcCgFxOptionEngineBase::calculateFxOptionBase();
}

void AmcCgFxEuropeanForwardOptionEngine::calculate() const {
    payoff_ = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    exercise_ = arguments_.exercise;
    payDate_ = arguments_.paymentDate; // might be null, in which case it will be set in calculateFxOptionBase()
    optionSettlement_ = Settlement::Physical;

    AmcCgFxOptionEngineBase::setupLegs();
    AmcCgFxOptionEngineBase::calculateFxOptionBase();
}

void AmcCgFxEuropeanCSOptionEngine::calculate() const {
    QL_REQUIRE(arguments_.exercise->dates().size() == 1,
               "AmcCgFxEuropeanCSOptionEngine::calculate(): expected 1 exercise date, got "
                   << arguments_.exercise->dates().size());

    Date today = Settings::instance().evaluationDate();

    payDate_ = arguments_.paymentDate; // always given
    cashSettlementDates_ = {payDate_};

    if (arguments_.exercise->dates().back() < today) {

        // handle option expiry in the past, this means we have a deterministic payoff

        Real payoffAmount = 0.0;
        if (arguments_.automaticExercise) {
            QL_REQUIRE(arguments_.underlying, "Expect a valid underlying index when exercise is automatic.");
            payoffAmount = (*arguments_.payoff)(arguments_.underlying->fixing(arguments_.exercise->dates().back()));
        } else if (arguments_.exercised) {
            QL_REQUIRE(arguments_.priceAtExercise != Null<Real>(), "Expect a valid price at exercise when option "
                                                                       << "has been manually exercised.");
            payoffAmount = (*arguments_.payoff)(arguments_.priceAtExercise);
        }

        leg_ = {Leg{QuantLib::ext::make_shared<SimpleCashFlow>(payoffAmount, payDate_)}};
        currency_ = {domCcy_};
        payer_ = {false};

        AmcCgFxOptionEngineBase::calculateFxOptionBase();

    } else {

        // handle option expiry in the future (or today)

        payoff_ = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        exercise_ = arguments_.exercise;
        optionSettlement_ = Settlement::Cash;

        AmcCgFxOptionEngineBase::setupLegs();
        AmcCgFxOptionEngineBase::calculateFxOptionBase();
    }
}

} // namespace data
} // namespace ore
