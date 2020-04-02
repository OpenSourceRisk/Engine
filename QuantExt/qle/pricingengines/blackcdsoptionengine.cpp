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

/*
 Copyright (C) 2008 Roland Stamm
 Copyright (C) 2009 Jose Aparicio

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/blackcdsoptionengine.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

BlackCdsOptionEngine::BlackCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                           Real recoveryRate, const Handle<YieldTermStructure>& termStructure,
                                           const Handle<BlackVolTermStructure>& volatility,
                                           boost::optional<bool> includeSettlementDateFlows)
    : BlackCdsOptionEngineBase(termStructure, volatility, includeSettlementDateFlows),
      probability_(probability), recoveryRate_(recoveryRate) {
    registerWith(probability_);
    registerWith(termStructure_);
    registerWith(volatility_);
}

Real BlackCdsOptionEngine::recoveryRate() const { return recoveryRate_; }

Real BlackCdsOptionEngine::defaultProbability(const Date& d) const { return probability_->defaultProbability(d); }

void BlackCdsOptionEngine::calculate() const {
    BlackCdsOptionEngineBase::calculate(*arguments_.swap, arguments_.exercise->dates().front(), arguments_.knocksOut,
                                        results_, termStructure_->referenceDate(), arguments_.strike, arguments_.strikeType);
}

BlackCdsOptionEngineBase::BlackCdsOptionEngineBase(const Handle<YieldTermStructure>& termStructure,
                                                   const Handle<BlackVolTermStructure>& vol,
                                                   boost::optional<bool> includeSettlementDateFlows)
    : termStructure_(termStructure), volatility_(vol), includeSettlementDateFlows_(includeSettlementDateFlows) {}

void BlackCdsOptionEngineBase::calculate(const CreditDefaultSwap& swap, const Date& exerciseDate, const bool knocksOut,
                                         CdsOption::results& results, const Date& refDate,
                                         const Real strike, const CdsOption::StrikeType strikeType) const {

    Date maturityDate = swap.coupons().front()->date();
    QL_REQUIRE(maturityDate > exerciseDate, "Underlying CDS should start after option maturity");
    Date settlement = termStructure_->referenceDate();

    Rate fairSpread = swap.fairSpread();
    Rate couponSpread = swap.runningSpread();

    DayCounter tSDc = termStructure_->dayCounter();

    // The sense of the underlying/option has to be sent this way
    // to the Black formula, no sign.
    Real riskyAnnuity = std::fabs(swap.couponLegNPV() / couponSpread);
    results.riskyAnnuity = riskyAnnuity;

    Real adjustedForwardSpread = fairSpread;
    Real adjustedStrikeSpread = couponSpread;
    Real strikeSpread = couponSpread;

    // TODO: accuont for defaults between option trade data evaluation date
    adjustedForwardSpread += (1 - recoveryRate()) * defaultProbability(swap.protectionStartDate()) /
                                ((1 - defaultProbability(swap.protectionStartDate())) * riskyAnnuity / swap.notional());
    if (strike != Null<Real>()) {
        if (strikeType == CdsOption::StrikeType::Spread) {
            strikeSpread = strike;
            // According to market standard, for exercise price calcualtion, risky annuity is calcualted on a credit curve
            // that has been fitted to a flat CDS term structure with spreads equal to strike
            // We use an approximation here
            double zeroRate1 = termStructure_->zeroRate(swap.protectionEndDate(), tSDc, Compounding::Continuous);
            double zeroRate2 = termStructure_->zeroRate(swap.protectionStartDate(), tSDc, Compounding::Continuous);
            double rate = (zeroRate1 + zeroRate2) / 2;
            Time maturity = tSDc.yearFraction(swap.protectionStartDate(), swap.maturity());
            double riskAnnuityStrike = (1 - exp(-(rate + strike / (1 - recoveryRate())) * maturity)) /
                                        (rate + strike / (1 - recoveryRate())) * 365 / 360;
            adjustedStrikeSpread += riskAnnuityStrike * (strike - couponSpread) /
                                    ((1 - defaultProbability(swap.protectionStartDate())) * riskyAnnuity / swap.notional());
        } else if (strikeType == CdsOption::StrikeType::Price) {
            adjustedStrikeSpread += (100 - strike) / (riskyAnnuity / swap.notional() * 100);
        } else {
            QL_FAIL("unrecognised strike type " << strikeType);
        }
    } // if strike is null, assume atm

    Time T = tSDc.yearFraction(settlement, exerciseDate);
    Real stdDev = volatility_->blackVol(exerciseDate, strikeSpread, true) * std::sqrt(T);
    Option::Type callPut = (swap.side() == Protection::Buyer) ? Option::Call : Option::Put;

    results.value = (1 - defaultProbability(exerciseDate)) *
                    blackFormula(callPut, adjustedStrikeSpread, adjustedForwardSpread, stdDev, riskyAnnuity);
}

Handle<YieldTermStructure> BlackCdsOptionEngineBase::termStructure() { return termStructure_; }

Handle<BlackVolTermStructure> BlackCdsOptionEngineBase::volatility() { return volatility_; }
} // namespace QuantExt
