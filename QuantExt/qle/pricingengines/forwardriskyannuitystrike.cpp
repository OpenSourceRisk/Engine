/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

#include <qle/pricingengines/forwardriskyannuitystrike.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

Real forwardRiskyAnnuityStrike(const Handle<YieldTermStructure>& discount, const Real& strike, const Date& exerciseDate,
                               const Real& recovery, const CreditDefaultSwap& cds) {

    // This method returns RPV01(0; t_e, T, K) / SP(t_e; K). This is the quantity in formula 11.9 of O'Kane 2008.
    // There is a slight modification in that we divide by the survival probability to t_E using the flat curve at
    // the strike spread that we create here.

    // Standard CDS schedule.
    Schedule schedule = MakeSchedule()
                            .from(cds.protectionStartDate())
                            .to(cds.maturity())
                            .withCalendar(WeekendsOnly())
                            .withFrequency(Quarterly)
                            .withConvention(Following)
                            .withTerminationDateConvention(Unadjusted)
                            .withRule(DateGeneration::CDS2015);

    // Derive hazard rate curve from a single forward starting CDS matching the characteristics of underlying (index)
    // CDS with a running spread equal to the strike.
    Real accuracy = 1e-8;

    auto strikeCds = QuantLib::ext::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 1 / accuracy, strike, schedule, Following, Actual360(), cds.settlesAccrual(),
        cds.protectionPaymentTime(), cds.protectionStartDate(), QuantLib::ext::shared_ptr<Claim>(), Actual360(true),
        true, cds.tradeDate(), cds.cashSettlementDays());
    // dummy engine
    strikeCds->setPricingEngine(QuantLib::ext::make_shared<MidPointCdsEngine>(
        Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0, Actual365Fixed())),
        0.0,
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed()))));

    Real hazardRate;
    try {
        hazardRate = strikeCds->impliedHazardRate(0.0, discount, Actual365Fixed(), recovery, accuracy);
    } catch (const std::exception& e) {
        QL_FAIL("can not imply fair hazard rate for CDS at option strike "
                << strike << ". Is the strike correct? Exception: " << e.what());
    }

    Handle<DefaultProbabilityTermStructure> dph(
        QuantLib::ext::make_shared<FlatHazardRate>(discount->referenceDate(), hazardRate, Actual365Fixed()));

    // Calculate the forward risky strike annuity.
    strikeCds->setPricingEngine(QuantLib::ext::make_shared<QuantLib::MidPointCdsEngine>(dph, recovery, discount));
    Real rpv01_K = std::abs(strikeCds->couponLegNPV() + strikeCds->accrualRebateNPV()) /
                   (strikeCds->notional() * strikeCds->runningSpread());
    QL_REQUIRE(rpv01_K > 0.0, "ForwardRiskyAnnuityStrike: strike based risky annuity must be positive.");

    // Survival to exercise
    Probability spToExercise = dph->survivalProbability(exerciseDate);
    Real discToExercise = discount->discount(exerciseDate);

    // Forward risky annuity strike (divides out the survival probability and discount to exercise)
    Real rpv01_K_fwd = rpv01_K / spToExercise / discToExercise;

    return rpv01_K_fwd;
}

}
