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
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quote.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/pricingengines/midpointcdsengine.hpp>

namespace QuantExt {

namespace {

Real swapRiskyAnnuity(const QuantExt::CreditDefaultSwap& swap, bool isClean = true) {
    Real price = std::abs(swap.couponLegNPV());
    if (isClean)
        price -= std::fabs(swap.accrualRebateNPV());
    return price / swap.runningSpread() / swap.notional();
}

class PriceError {
public:
    PriceError(const Schedule& schedule, const boost::shared_ptr<QuantExt::CreditDefaultSwap>& swapCoupon,
               const Handle<YieldTermStructure> termStructure, Real recovery, SimpleQuote& hazardRateQuote,
               SimpleQuote& spreadQuote, Real targetValue)
        : schedule_(schedule), swapCoupon_(swapCoupon), termStructure_(termStructure), recovery_(recovery),
          hazardRateQuote_(hazardRateQuote), spreadQuote_(spreadQuote), targetValue_(targetValue) {}

    Real operator()(Real spread) const {
        spreadQuote_.setValue(spread);

        QuantExt::CreditDefaultSwap swap_strike(swapCoupon_->side(), 1.0, spread, schedule_, Following, Actual360(),
                                                swapCoupon_->settlesAccrual(), swapCoupon_->protectionPaymentTime(),
                                                swapCoupon_->protectionStartDate(), boost::shared_ptr<Claim>(),
                                                Actual360(true));

        auto hz = swap_strike.impliedHazardRate(0.0, termStructure_, Actual360(), recovery_, 1e-6);
        hazardRateQuote_.setValue(hz);

        auto riskAnnuityStrike = swapRiskyAnnuity(*swapCoupon_);
        auto calculatedPrice = riskAnnuityStrike * (spread - swapCoupon_->runningSpread());
        return calculatedPrice - (100.0 - targetValue_) / 100.0;
    }

private:
    const Schedule schedule_;
    const boost::shared_ptr<QuantExt::CreditDefaultSwap> swapCoupon_;
    const Handle<YieldTermStructure> termStructure_;
    const Real recovery_;
    SimpleQuote& hazardRateQuote_;
    SimpleQuote& spreadQuote_;
    Real targetValue_;
};

} // namespace

BlackCdsOptionEngine::BlackCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                           Real recoveryRate, const Handle<YieldTermStructure>& termStructure,
                                           const Handle<BlackVolTermStructure>& volatility)
    : BlackCdsOptionEngineBase(termStructure, volatility), probability_(probability), recoveryRate_(recoveryRate) {
    registerWith(probability_);
    registerWith(termStructure_);
    registerWith(volatility_);
}

Real BlackCdsOptionEngine::recoveryRate() const { return recoveryRate_; }

Real BlackCdsOptionEngine::defaultProbability(const Date& d) const { return probability_->defaultProbability(d); }

void BlackCdsOptionEngine::calculate() const {
    BlackCdsOptionEngineBase::calculate(*arguments_.swap, arguments_.exercise->dates().front(), arguments_.knocksOut,
                                        results_, arguments_.strike, arguments_.strikeType);
}

BlackCdsOptionEngineBase::BlackCdsOptionEngineBase(const Handle<YieldTermStructure>& termStructure,
                                                   const Handle<BlackVolTermStructure>& vol)
    : termStructure_(termStructure), volatility_(vol) {}

void BlackCdsOptionEngineBase::calculate(const CreditDefaultSwap& swap, const Date& exerciseDate, const bool knocksOut,
                                         CdsOption::results& results, const Real strike,
                                         const CdsOption::StrikeType strikeType) const {

    Date maturityDate = swap.coupons().back()->date();
    QL_REQUIRE(maturityDate > exerciseDate, "Underlying CDS maturity should be after the option expiry.");

    Rate fairSpread = swap.fairSpread();
    Rate couponSpread = swap.runningSpread();

    // The sense of the underlying/option has to be sent this way
    // to the Black formula, no sign.
    results.riskyAnnuity = swapRiskyAnnuity(swap, false) * swap.notional(); // with accural
    Real riskyAnnuity = swapRiskyAnnuity(swap);
    Real riskyAnnuityExp = riskyAnnuity / termStructure_->discount(exerciseDate);

    Option::Type callPut = (swap.side() == Protection::Buyer) ? Option::Call : Option::Put;

    results.additionalResults = swap.additionalResults();
    // TODO: accuont for defaults between option trade data evaluation date
    if (strike != Null<Real>()) {
        Real strikeSpread = couponSpread;
        Real adjustedForwardSpread = fairSpread;
        Real adjustedStrikeSpread = couponSpread;
        adjustedForwardSpread += (1 - recoveryRate()) * defaultProbability(exerciseDate) /
                                 ((1 - defaultProbability(exerciseDate)) * riskyAnnuityExp);
        // According to market standard, for exercise price calcualtion, risky annuity is calcualted on a credit curve
        // that has been fitted to a flat CDS term structure with spreads equal to strike
        if (strikeType == CdsOption::StrikeType::Spread) {
            strikeSpread = strike;
            // Find the flat hazard rate
            Schedule schedule = MakeSchedule()
                                    .from(swap.protectionStartDate())
                                    .to(swap.protectionEndDate())
                                    .withFrequency(Quarterly)
                                    .withConvention(Following)
                                    .withTerminationDateConvention(Unadjusted)
                                    .withRule(DateGeneration::CDS2015);
            auto swap_strike = boost::make_shared<QuantExt::CreditDefaultSwap>(
                Protection::Buyer, 1e8, strike, schedule, Following, Actual360(), swap.settlesAccrual(),
                swap.protectionPaymentTime(), swap.protectionStartDate(), boost::shared_ptr<Claim>(), Actual360(true));
            auto hz = swap_strike->impliedHazardRate(0.0, termStructure_, Actual360());

            // Re-create the underlying swap for risky annuity calculation
            auto swap_ra = boost::make_shared<QuantExt::CreditDefaultSwap>(
                Protection::Buyer, 1, couponSpread, schedule, Following, Actual360(), swap.settlesAccrual(),
                swap.protectionPaymentTime(), swap.protectionStartDate(), boost::shared_ptr<Claim>(), Actual360(true));

            Handle<DefaultProbabilityTermStructure> dp(boost::make_shared<FlatHazardRate>(
                termStructure_->referenceDate(), Handle<Quote>(boost::make_shared<SimpleQuote>(hz)), Actual365Fixed()));
            boost::shared_ptr<PricingEngine> swap_engine =
                boost::make_shared<QuantExt::MidPointCdsEngine>(dp, recoveryRate(), termStructure_);
            swap_ra->setPricingEngine(swap_engine);
            double riskAnnuityStrike =
                (std::fabs(swap_ra->couponLegNPV() - std::fabs(swap_ra->accrualRebateNPV()))) / couponSpread;

            adjustedStrikeSpread += riskAnnuityStrike * (strikeSpread - couponSpread) /
                                    ((1 - defaultProbability(exerciseDate)) * riskyAnnuityExp);
        } else if (strikeType == CdsOption::StrikeType::Price) {

            // Use solver to find the equivalent spread quoted strike
            Schedule schedule = MakeSchedule()
                                    .from(swap.protectionStartDate())
                                    .to(swap.protectionEndDate())
                                    .withFrequency(Quarterly)
                                    .withConvention(Following)
                                    .withTerminationDateConvention(Unadjusted)
                                    .withRule(DateGeneration::CDS2015);

            auto swap_coupon = boost::make_shared<QuantExt::CreditDefaultSwap>(
                swap.side(), 1.0, couponSpread, schedule, Following, Actual360(), swap.settlesAccrual(),
                swap.protectionPaymentTime(), swap.protectionStartDate(), boost::shared_ptr<Claim>(), Actual360(true));

            SimpleQuote spreadQuote;
            auto hazardRateQuote = boost::make_shared<SimpleQuote>();
            auto hazardRateQuoteHandle = Handle<Quote>(hazardRateQuote);

            Handle<DefaultProbabilityTermStructure> dp(boost::make_shared<FlatHazardRate>(
                termStructure_->referenceDate(), hazardRateQuoteHandle, Actual365Fixed()));
            boost::shared_ptr<PricingEngine> swap_engine =
                boost::make_shared<QuantExt::MidPointCdsEngine>(dp, recoveryRate(), termStructure_);
            swap_coupon->setPricingEngine(swap_engine);

            PriceError f(schedule, swap_coupon, termStructure_, recoveryRate(), *hazardRateQuote, spreadQuote, strike);
            Brent solver;
            solver.setMaxEvaluations(10000);
            strikeSpread = solver.solve(f, 1e-8, couponSpread, 0.0001, 1.0);

            Real riskAnnuityStrike = ::QuantExt::swapRiskyAnnuity(*swap_coupon);
            adjustedStrikeSpread += riskAnnuityStrike * (strikeSpread - couponSpread) /
                                    ((1 - defaultProbability(exerciseDate)) * riskyAnnuityExp);
        } else {
            QL_FAIL("unrecognised strike type " << strikeType);
        }
        Real stdDev = sqrt(volatility_->blackVariance(exerciseDate, strikeSpread, true));
        results.value =
            swap.notional() * termStructure_->discount(exerciseDate) * (1 - defaultProbability(exerciseDate));
        results.value *= blackFormula(callPut, adjustedStrikeSpread, adjustedForwardSpread, stdDev, riskyAnnuity);
    } else {
        // old methodology

        // Take into account the NPV from the upfront amount
        // If buyer and upfront NPV > 0 => receiving upfront amount => should reduce the pay spread
        // If buyer and upfront NPV < 0 => paying upfront amount => should increase the pay spread
        // If seller and upfront NPV > 0 => receiving upfront amount => should increase the receive spread
        // If seller and upfront NPV < 0 => paying upfront amount => should reduce the receive spread
        if (swap.side() == Protection::Buyer) {
            couponSpread -= swap.upfrontNPV() / (riskyAnnuity * swap.notional());
        } else {
            couponSpread += swap.upfrontNPV() / (riskyAnnuity * swap.notional());
        }
        Real stdDev = sqrt(volatility_->blackVariance(exerciseDate, 1.0, true));
        results.value = blackFormula(callPut, couponSpread, fairSpread, stdDev, riskyAnnuity * swap.notional());

        // if a non knock-out payer option, add front end protection value
        if (swap.side() == Protection::Buyer && !knocksOut) {
            Real frontEndProtection = callPut * swap.notional() * (1. - recoveryRate()) *
                                      defaultProbability(exerciseDate) * termStructure_->discount(exerciseDate);
            results.value += frontEndProtection;
        }
    }
}

Handle<YieldTermStructure> BlackCdsOptionEngineBase::termStructure() { return termStructure_; }

Handle<BlackVolTermStructure> BlackCdsOptionEngineBase::volatility() { return volatility_; }
} // namespace QuantExt
