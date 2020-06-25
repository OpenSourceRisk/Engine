/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2017 Aareal Bank AG
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

#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                                       const Handle<Quote>& recoveryRate,
                                                       const Handle<Quote>& securitySpread, Period timestepPeriod,
                                                       boost::optional<bool> includeSettlementDateFlows)
    : defaultCurve_(defaultCurve), recoveryRate_(recoveryRate), securitySpread_(securitySpread),
      timestepPeriod_(timestepPeriod), includeSettlementDateFlows_(includeSettlementDateFlows) {
    discountCurve_ =
        securitySpread_.empty()
            ? discountCurve
            : Handle<YieldTermStructure>(boost::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    registerWith(discountCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
}

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<Quote>& securitySpread, Period timestepPeriod,
                                                       boost::optional<bool> includeSettlementDateFlows)
    : securitySpread_(securitySpread), timestepPeriod_(timestepPeriod),
      includeSettlementDateFlows_(includeSettlementDateFlows) {
    discountCurve_ =
        securitySpread_.empty()
            ? discountCurve
            : Handle<YieldTermStructure>(boost::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    registerWith(discountCurve_);
    registerWith(securitySpread_);
}

void DiscountingRiskyBondEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    results_.valuationDate = (*discountCurve_)->referenceDate();
    results_.value = calculateNpv(results_.valuationDate, arguments_.cashflows);

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();
    // a bond's cashflow on settlement date is never taken into
    // account, so we might have to play it safe and recalculate
    // same parameters as above, we can avoid another call
    if (!includeRefDateFlows && results_.valuationDate == arguments_.settlementDate) {
        results_.settlementValue = results_.value;
    } else {
        // no such luck
        results_.settlementValue = calculateNpv(arguments_.settlementDate, arguments_.cashflows);
    }
}

Real DiscountingRiskyBondEngine::calculateNpv(Date npvDate, const Leg& cashflows,
                                              const Handle<YieldTermStructure>& incomeCurve,
                                              const bool conditionalOnSurvival) const {
    Real npvValue = 0;

    // handle case where we wish to price simply with benchmark curve and scalar security spread
    // i.e. credit curve term structure (and recovery) have not been specified
    // we set the default probability and recovery rate to zero in this instance (issuer credit worthiness already
    // captured within security spread)
    boost::shared_ptr<DefaultProbabilityTermStructure> creditCurvePtr =
        defaultCurve_.empty()
            ? boost::make_shared<QuantLib::FlatHazardRate>(results_.valuationDate, 0.0, discountCurve_->dayCounter())
            : defaultCurve_.currentLink();
    Rate recoveryVal = recoveryRate_.empty() ? 0.0 : recoveryRate_->value();

    // compounding factors for npv date
    Real dfSettl = incomeCurve.empty() ? discountCurve_->discount(npvDate) : incomeCurve->discount(npvDate);
    Real spSettl = conditionalOnSurvival ? creditCurvePtr->survivalProbability(npvDate) : 1.0;

    Size numCoupons = 0;
    bool hasLiveCashFlow = false;
    for (Size i = 0; i < cashflows.size(); i++) {
        boost::shared_ptr<CashFlow> cf = cashflows[i];
        if (cf->hasOccurred(npvDate, includeSettlementDateFlows_))
            continue;
        hasLiveCashFlow = true;

        // Coupon value is discounted future payment times the survival probability
        Probability S = creditCurvePtr->survivalProbability(cf->date()) / spSettl;
        npvValue += cf->amount() * S * discountCurve_->discount(cf->date()) / dfSettl;

        /* The amount recovered in the case of default is the recoveryrate*Notional*Probability of
           Default; this is added to the NPV value. For coupon bonds the coupon periods are taken
           as the timesteps for integrating over the probability of default.
        */
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(cf);
        if (coupon) {
            numCoupons++;
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= npvDate && npvDate <= endDate) ? npvDate : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, endDate) / spSettl;

            npvValue += coupon->nominal() * recoveryVal * P * discountCurve_->discount(defaultDate) / dfSettl;
        }
    }

    // the ql instrument might not yet be expired and still have not anything to value if
    // the npvDate > evaluation date
    if (!hasLiveCashFlow)
        return 0.0;

    if (cashflows.size() > 1 && numCoupons == 0) {
        QL_FAIL("DiscountingRiskyBondEngine does not support bonds with multiple cashflows but no coupons");
    }

    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (cashflows.size() == 1) {
        boost::shared_ptr<Redemption> redemption = boost::dynamic_pointer_cast<Redemption>(cashflows[0]);
        if (redemption) {
            Date startDate = npvDate;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepPeriod_;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = creditCurvePtr->defaultProbability(startDate, endDate) / spSettl;

                npvValue += redemption->amount() * recoveryVal * P * discountCurve_->discount(defaultDate) / dfSettl;
                startDate = stepDate;
            }
        }
    }

    return npvValue;
}
} // namespace QuantExt
