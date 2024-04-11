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

#include <qle/pricingengines/blackbondoptionengine.hpp>

#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yield/impliedtermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

BlackBondOptionEngine::BlackBondOptionEngine(const Handle<YieldTermStructure>& discountCurve,
                                             const Handle<SwaptionVolatilityStructure>& volatility,
                                             const Handle<YieldTermStructure>& underlyingReferenceCurve,
                                             const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                             const Handle<Quote>& recoveryRate, const Handle<Quote>& securitySpread,
                                             Period timestepPeriod)
    : discountCurve_(discountCurve), volatility_(volatility), underlyingReferenceCurve_(underlyingReferenceCurve),
      defaultCurve_(defaultCurve), recoveryRate_(recoveryRate), securitySpread_(securitySpread),
      timestepPeriod_(timestepPeriod) {
    registerWith(discountCurve_);
    registerWith(volatility_);
    registerWith(underlyingReferenceCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
}

void BlackBondOptionEngine::calculate() const {

    QL_REQUIRE(!discountCurve_.empty(), "BlackBondOptionEngine::calculate(): empty discount curve");

    QL_REQUIRE(arguments_.putCallSchedule.size() == 1, "BlackBondOptionEngine: can only handle European options");
    Date exerciseDate = arguments_.putCallSchedule.front()->date();

    QL_REQUIRE(!underlyingReferenceCurve_.empty(), "BlackBondOptionEngine::calculate(): empty reference curve");

    auto fwdBondEngine = QuantLib::ext::make_shared<DiscountingRiskyBondEngine>(
        underlyingReferenceCurve_, defaultCurve_, recoveryRate_, securitySpread_, timestepPeriod_);
    auto bondNpvResults = fwdBondEngine->calculateNpv(exerciseDate, arguments_.underlying->settlementDate(exerciseDate),
                                              arguments_.underlying->cashflows());
    
    for (auto& cfRes : bondNpvResults.cashflowResults) {
        cfRes.legNumber = 0;
        cfRes.type = "Underlying_Bond__" + cfRes.type;
    }
    
    Real fwdNpv = bondNpvResults.npv;

   
    Real knockOutProbability = defaultCurve_.empty() ? 0.0 : 1.0 - defaultCurve_->survivalProbability(exerciseDate);

    // adjust forward if option does not knock out (option is on the recovery value if bond defaults before expiry)
    if (!arguments_.knocksOutOnDefault) {
        fwdNpv = (1.0 - knockOutProbability) * fwdNpv + knockOutProbability *
                                                            (recoveryRate_.empty() ? 0.0 : recoveryRate_->value()) *
                                                            arguments_.underlying->notional(exerciseDate);
    }

    // hard code yield compounding convention to annual
    Rate fwdYtm = CashFlows::yield(arguments_.underlying->cashflows(), fwdNpv, volatility_->dayCounter(), Compounded,
                                   Annual, false, exerciseDate, exerciseDate);
    InterestRate fwdRate(fwdYtm, volatility_->dayCounter(), Compounded, Annual);
    Time fwdDur = CashFlows::duration(arguments_.underlying->cashflows(), fwdRate, Duration::Modified, false,
                                      exerciseDate, exerciseDate);

    QL_REQUIRE(arguments_.putCallSchedule.size() == 1, "BlackBondOptionEngine: only European bond options allowed");

    // read atm yield vol
    Real underlyingLength = volatility_->swapLength(exerciseDate, arguments_.underlying->cashflows().back()->date());
    Volatility yieldVol = volatility_->volatility(exerciseDate, underlyingLength, fwdYtm);

    // compute price vol from yield vol
    Volatility fwdPriceVol;
    Real shift = 0.0;
    if (volatility_->volatilityType() == VolatilityType::Normal)
        fwdPriceVol = yieldVol * fwdDur;
    else {
        if (close_enough(volatility_->shift(exerciseDate, underlyingLength), 0.0)) {
            QL_REQUIRE(fwdYtm > 0, "BlackBondOptionEngine: input yield vols are lognormal, but yield is not positive ("
                                       << fwdYtm << ")");
            fwdPriceVol = yieldVol * fwdDur * fwdYtm;
        } else {
            shift = volatility_->shift(exerciseDate, underlyingLength);
            QL_REQUIRE(fwdYtm > -shift, "BlackBondOptionEngine: input yield vols are shifted lognormal "
                                            << shift << ", but yield (" << fwdYtm << ") is not greater than -shift ("
                                            << -shift);
            fwdPriceVol = yieldVol * fwdDur * (fwdYtm + shift);
        }
    }

    QL_REQUIRE(fwdPriceVol >= 0.0, "BlackBondOptionEngine: negative forward price vol ("
                                       << fwdPriceVol << "), yieldVol=" << yieldVol << ", fwdDur=" << fwdDur
                                       << ", fwdYtm=" << fwdYtm << ", shift="
                                       << (volatility_->volatilityType() == VolatilityType::Normal
                                               ? 0.0
                                               : volatility_->shift(exerciseDate, underlyingLength)));

    // strike could be a price or yield
    Real cashStrike;
    if (arguments_.putCallSchedule.front()->isBondPrice()) {
        // adjust cashCashStrike if given as clean
        // note that the accruedAmount is on the basis of notional 100, as cleanPrice and dirtyPrice
        cashStrike = arguments_.putCallSchedule.front()->price().amount();
        if (arguments_.putCallSchedule.front()->price().type() == QuantLib::Bond::Price::Clean)
            cashStrike += arguments_.underlying->accruedAmount(exerciseDate) / 100;
    } else {
        // for a yield get the strike using npv calculation, yield should always be dirty price
        // so no adjustment needed
        InterestRate yield = arguments_.putCallSchedule.front()->yield();
        cashStrike = CashFlows::npv(arguments_.underlying->cashflows(), yield, false, exerciseDate, exerciseDate);
    }

    Real optionValue = blackFormula(
        arguments_.putCallSchedule[0]->type() == Callability::Call ? Option::Call : Option::Put, cashStrike, fwdNpv,
        fwdPriceVol * std::sqrt(volatility_->timeFromReference(exerciseDate)), discountCurve_->discount(exerciseDate));

    // correct for knock out probability
    if (arguments_.knocksOutOnDefault && !defaultCurve_.empty())
        optionValue *= 1.0 - knockOutProbability;

    CashFlowResults optionFlow;
    optionFlow.payDate = exerciseDate;
    optionFlow.legNumber = 1;
    optionFlow.type = "ExpectedOptionPayoff";
    optionFlow.amount = optionValue / discountCurve_->discount(exerciseDate);
    optionFlow.discountFactor = discountCurve_->discount(exerciseDate);
    optionFlow.presentValue = optionValue;

    bondNpvResults.cashflowResults.push_back(optionFlow);

    results_.additionalResults["knockOutProbability"] = knockOutProbability;
    results_.additionalResults["cashFlowResults"] = bondNpvResults.cashflowResults;
    results_.additionalResults["CashStrike"] = cashStrike;
    results_.additionalResults["FwdCashPrice"] = fwdNpv;
    results_.additionalResults["PriceVol"] = fwdPriceVol;
    results_.additionalResults["timeToExpiry"] = volatility_->timeFromReference(exerciseDate);
    results_.additionalResults["optionValue"] = optionValue;
    results_.additionalResults["yieldVol"] = yieldVol;
    results_.additionalResults["yieldVolShift"] = shift;
    results_.additionalResults["fwdDuration"] = fwdDur;
    results_.additionalResults["fwdYieldToMaturity"] = fwdYtm;

    results_.additionalResults["AccruedAtExercise"] = arguments_.underlying->accruedAmount(exerciseDate)/100;
    // results_.additionalResults["CleanBondPrice"] = arguments_.underlying->cleanPrice();
    // results_.additionalResults["DirtyBondPrice"] = arguments_.underlying->dirtyPrice();
    if (!arguments_.knocksOutOnDefault) {
        results_.additionalResults["ExpectedBondRecovery"] = knockOutProbability *
                                                             (recoveryRate_.empty() ? 0.0 : recoveryRate_->value()) *
                                                             arguments_.underlying->notional(exerciseDate);
    }
    results_.value = optionValue;
}

} // namespace QuantExt
