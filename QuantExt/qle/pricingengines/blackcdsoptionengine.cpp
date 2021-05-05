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
#include <string>

using std::string;

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

Real BlackCdsOptionEngine::frontEndProtection(const Date& d) const {
    // Does it depend on the knocksOut argument? Main focus is index CDS options for now.
    return 0.0;
}

Real BlackCdsOptionEngine::expectedNotional(const Date& d) const {
    return (1 - defaultProbability(d)) * arguments_.swap->notional();
}

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

    // Get additional results of underlying CDS.
    swap.NPV();
    results.additionalResults = swap.additionalResults();

    Rate fairSpread = swap.fairSpread();
    Rate couponSpread = swap.runningSpread();
    results.additionalResults["runningSpread"] = couponSpread;

    // Discount factor to exercise
    DiscountFactor discToExercise = termStructure_->discount(exerciseDate);
    results.additionalResults["discToExercise"] = discToExercise;

    // The sense of the underlying/option has to be sent this way to the Black formula, no sign.
    results.riskyAnnuity = swapRiskyAnnuity(swap, false) * swap.notional();
    Real riskyAnnuity = swapRiskyAnnuity(swap);
    Real riskyAnnuityExp = riskyAnnuity / discToExercise;
    results.additionalResults["riskyAnnuityClean"] = riskyAnnuity;

    // TODO: accuont for defaults between option trade data evaluation date
    if (strike != Null<Real>()) {

        // Expected notional at expiry.
        Real expNtl = expectedNotional(exerciseDate);
        results.additionalResults["expectedNotional"] = expNtl;

        if (strikeType == CdsOption::StrikeType::Spread) {

            // Follows ICE Credit Index Options, Sep 2018, Section 3.2
            results.additionalResults["fairSpread"] = fairSpread;
            results.additionalResults["strikeSpread"] = strike;

            // Review the use of weighted recovery and default probability here. This original author was following 
            // the ICE paper but we possibly have the constituent recoveries and default curves.
            Real lgd = 1 - recoveryRate();
            results.additionalResults["lgd"] = lgd;
            Real dp = defaultProbability(exerciseDate);
            results.additionalResults["defaultProbToExercise"] = dp;
            Real sp = 1 - dp;

            // F' in ICE paper
            Real adjustedForwardSpread = fairSpread + (lgd * dp) / (sp * riskyAnnuityExp);
            results.additionalResults["adjustedForwardSpread"] = adjustedForwardSpread;

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

            Handle<DefaultProbabilityTermStructure> dph(boost::make_shared<FlatHazardRate>(
                termStructure_->referenceDate(), Handle<Quote>(boost::make_shared<SimpleQuote>(hz)), Actual365Fixed()));
            boost::shared_ptr<PricingEngine> swap_engine =
                boost::make_shared<QuantExt::MidPointCdsEngine>(dph, recoveryRate(), termStructure_);
            swap_ra->setPricingEngine(swap_engine);
            double riskAnnuityStrike =
                (std::fabs(swap_ra->couponLegNPV() - std::fabs(swap_ra->accrualRebateNPV()))) / couponSpread;
            results.additionalResults["riskAnnuityStrike"] = riskAnnuityStrike;

            // K' in the ICE paper
            Real adjustedStrikeSpread = couponSpread +
                riskAnnuityStrike * (strike - couponSpread) / (sp * riskyAnnuityExp);
            results.additionalResults["adjustedStrikeSpread"] = adjustedStrikeSpread;

            // Read the volatility from the volatility surface, assumed to have strike dimension in terms of spread.
            Real stdDev = sqrt(volatility_->blackVariance(exerciseDate, strike, true));
            results.additionalResults["volatility"] = volatility_->blackVol(exerciseDate, strike);
            results.additionalResults["standardDeviation"] = stdDev;

            Option::Type callPut = swap.side() == Protection::Buyer ? Option::Call : Option::Put;
            results.additionalResults["callPut"] = callPut == Option::Call ? string("Call") : string("Put");

            results.value = expNtl * discToExercise;
            results.value *= blackFormula(callPut, adjustedStrikeSpread, adjustedForwardSpread, stdDev, riskyAnnuity);

        } else if (strikeType == CdsOption::StrikeType::Price) {

            // Calculate the forward price of the underlying CDS.
            // TODO: review using the full notional in the denominator here.
            Real forwardPrice = swap.side() == Protection::Buyer ? swap.NPV() : -swap.NPV();
            forwardPrice /= discToExercise;
            forwardPrice = 1 - forwardPrice / swap.notional();
            results.additionalResults["forwardPrice"] = forwardPrice;

            // Calculate the front end protection adjusted forward price of the underlying CDS.
            Real fep = frontEndProtection(exerciseDate);
            Real fepForwardPrice = forwardPrice - fep / swap.notional();
            results.additionalResults["frontEndProtection"] = fep;
            results.additionalResults["fepForwardPrice"] = fepForwardPrice;

            // Read the volatility from the volatility surface, assumed to have strike dimension in terms of price.
            Real stdDev = sqrt(volatility_->blackVariance(exerciseDate, strike));
            results.additionalResults["strikePrice"] = strike;
            results.additionalResults["volatility"] = volatility_->blackVol(exerciseDate, strike);
            results.additionalResults["standardDeviation"] = stdDev;

            // If protection buyer, put on price.
            Option::Type cp = swap.side() == Protection::Buyer ? Option::Put : Option::Call;
            results.additionalResults["callPut"] = cp == Option::Put ? string("Put") : string("Call");

            results.value = expNtl * blackFormula(cp, strike, fepForwardPrice, stdDev, discToExercise);

        } else {
            QL_FAIL("unrecognised strike type " << strikeType);
        }
        
    } else {
        // old methodology

        // Take into account the NPV from the upfront amount
        // If buyer and upfront NPV > 0 => receiving upfront amount => should reduce the pay spread
        // If buyer and upfront NPV < 0 => paying upfront amount => should increase the pay spread
        // If seller and upfront NPV > 0 => receiving upfront amount => should increase the receive spread
        // If seller and upfront NPV < 0 => paying upfront amount => should reduce the receive spread
        Option::Type callPut = swap.side() == Protection::Buyer ? Option::Call : Option::Put;
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
