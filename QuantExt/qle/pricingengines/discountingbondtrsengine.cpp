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
#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <qle/pricingengines/discountingbondtrsengine.hpp>

namespace QuantExt {

DiscountingBondTRSEngine::DiscountingBondTRSEngine(const Handle<YieldTermStructure>& discountCurve,
                                                   const Handle<YieldTermStructure>& bondReferenceYieldCurve,
                                                   const Handle<Quote>& bondSpread,
                                                   const Handle<DefaultProbabilityTermStructure>& bondDefaultCurve,
                                                   const Handle<Quote>& bondRecoveryRate, Period timestepPeriod,
                                                   boost::optional<bool> includeSettlementDateFlows)

    : discountCurve_(discountCurve), bondReferenceYieldCurve_(bondReferenceYieldCurve), bondSpread_(bondSpread),
      bondDefaultCurve_(bondDefaultCurve), bondRecoveryRate_(bondRecoveryRate), timestepPeriod_(timestepPeriod),
      includeSettlementDateFlows_(includeSettlementDateFlows) {

    bondReferenceYieldCurve_ =
        Handle<YieldTermStructure>(boost::make_shared<ZeroSpreadedTermStructure>(bondReferenceYieldCurve, bondSpread));
    registerWith(discountCurve_);           // curve for discounting of the forward derivative contract. OIS, usually.
    registerWith(bondReferenceYieldCurve_); // this is the bond reference curve, for discounting, usually RePo
    registerWith(bondSpread_);
    registerWith(bondDefaultCurve_);
    registerWith(bondRecoveryRate_);
}

void DiscountingBondTRSEngine::calculate() const {
    // Do some checks on data
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    QL_REQUIRE(!bondReferenceYieldCurve_.empty(), "bond rerference term structure handle is empty");

    Date npvDate = npvDate_; // this is today when the valuation occurs
    if (npvDate == Null<Date>()) {
        npvDate = (*discountCurve_)->referenceDate();
    }

    QL_REQUIRE(arguments_.bondTRSStartDate >= arguments_.underlying->issueDate(),
               "BondTRSEngine: Only support TRS that start after underlying bond.");

    // initialize
    results_.value = 0.0; // this is today's npv of the trs contract
    results_.underlyingSpotValue =
        calculateBondNpv(npvDate, arguments_.bondTRSStartDate,
                         arguments_.bondTRSMaturityDate); // cashflows before npvDate will be ignored
    results_.fundingLegSpotValue =
        calculateFundingLegNpv(npvDate, arguments_.bondTRSStartDate,
                               arguments_.bondTRSMaturityDate); // cashflows before npvDate will be ignored
    results_.compensationPaymentsSpotValue =
        calculatecompensationPaymentsNpv(npvDate, arguments_.bondTRSStartDate, arguments_.bondTRSMaturityDate);
    results_.value =
        results_.underlyingSpotValue + results_.compensationPaymentsSpotValue - results_.fundingLegSpotValue;
    if (!arguments_.longInBond) {
        results_.value *= -1.0;
    }
}

Real DiscountingBondTRSEngine::calculateBondNpv(Date npvDate, Date trsStart, Date trsMaturity) const {
    Real npvValue = 0.0;
    Size numCoupons = 0;
    bool hasLiveCashFlow = false;

    // handle case where we wish to price simply with benchmark curve and scalar security spread
    // i.e. credit curve term structure (and recovery) have not been specified
    // we set the default probability and recovery rate to zero in this instance (issuer credit worthiness already
    // captured within security spread)
    boost::shared_ptr<DefaultProbabilityTermStructure> creditCurvePtr =
        bondDefaultCurve_.empty()
            ? boost::make_shared<QuantLib::FlatHazardRate>(npvDate, 0.0, bondReferenceYieldCurve_->dayCounter())
            : bondDefaultCurve_.currentLink();
    Rate recoveryVal = bondRecoveryRate_.empty() ? 0.0 : bondRecoveryRate_->value(); // setup default bond recovery rate

    // load the shared pointer into bd
    boost::shared_ptr<Bond> bd = arguments_.underlying;
    for (Size i = 0; i < bd->cashflows().size(); i++) {
        if (trsStart < npvDate)
            trsStart = npvDate;
        if (bd->cashflows()[i]->date() <= trsStart) // Cashflows before trsStart excluded from npv computation
            continue;
        if (bd->cashflows()[i]->date() > trsMaturity) // Cashflows after expiry of trs contract not relevant
            continue;

        /* The amount recovered in the case of default is the recoveryrate*Notional*Probability of
           Default; this is added to the NPV value. For coupon bonds the coupon periods are taken
           as the timesteps for integrating over the probability of default.
        */
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(bd->cashflows()[i]);

        if (coupon) {
            numCoupons++;
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= trsStart && trsStart <= endDate) ? trsStart : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, endDate);

            npvValue += coupon->nominal() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
        }

        hasLiveCashFlow = true; // check if a cashflow  is available after the date of valuation.

        // Coupon value is discounted future payment times the survival probability
        Probability S = creditCurvePtr->survivalProbability(bd->cashflows()[i]->date()) /
                        creditCurvePtr->survivalProbability(trsStart);
        npvValue += bd->cashflows()[i]->amount() * S * bondReferenceYieldCurve_->discount(bd->cashflows()[i]->date());
    }

    // the ql instrument might not yet be expired and still have not anything to value if
    // the trsStartDate > evaluation date
    if (!hasLiveCashFlow)
        return 0.0;

    if (bd->cashflows().size() > 1 && numCoupons == 0) {
        QL_FAIL("DiscountingBondTRSEngine: no support of bonds with multiple cashflows but no coupons");
    }

    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (bd->cashflows().size() == 1) {
        boost::shared_ptr<Redemption> redemption = boost::dynamic_pointer_cast<Redemption>(bd->cashflows()[0]);
        if (redemption) {
            Date startDate = trsStart;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepPeriod_;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

                npvValue += redemption->amount() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
                startDate = stepDate;
            }
        }
    }
    return npvValue;
}

Real DiscountingBondTRSEngine::calculateFundingLegNpv(Date npvDate, Date trsStart, Date trsMaturity) const {
    Real npvValue = 0.0;
    if (arguments_.fundingLeg.empty())
        return npvValue;
    for (Size i = 0; i < arguments_.fundingLeg.size(); ++i) {
        CashFlow& cf = *arguments_.fundingLeg[i];
        if (cf.date() <= npvDate) // Cashflows before trsStart not relevant for npv
            continue;
        if (cf.date() <= trsStart)
            continue;
        if (cf.date() > trsMaturity)
            continue;
        ext::shared_ptr<Coupon> cp = ext::dynamic_pointer_cast<Coupon>(arguments_.fundingLeg[i]);
        Real df = (*discountCurve_)->discount(cf.date());
        npvValue += cf.amount() * df;
    }
    return npvValue;
}

Real DiscountingBondTRSEngine::calculatecompensationPaymentsNpv(Date npvDate, Date trsStart, Date trsMaturity) const {
    Real npvValue = 0.0;

    const Schedule& schedule = arguments_.compensationPaymentsSchedule;
    bool firstPeriod = true;

    for (unsigned int i = 0; i < schedule.dates().size(); ++i) {
        if (trsStart < npvDate)
            trsStart = npvDate;
        if (schedule.dates().at(i) <= trsStart) // Cashflows before trsStart excluded from npv computation
            continue;
        if (schedule.dates().at(i) > trsMaturity) // Cashflows after expiry of trs contract not relevant
            continue;

        boost::shared_ptr<Bond> bdForAcc = arguments_.underlying;
        if (firstPeriod) {
            npvValue +=
                ((calculateBondNpv(npvDate, trsStart, trsMaturity) -
                  bdForAcc->accruedAmount(trsStart) * bdForAcc->notional(trsStart) / 100) *
                     discountCurve_->discount(trsStart) / bondReferenceYieldCurve_->discount(trsStart) -
                 (calculateBondNpv(npvDate, schedule.dates().at(i), trsMaturity) -
                  bdForAcc->accruedAmount(schedule.dates().at(i)) * bdForAcc->notional(schedule.dates().at(i)) / 100) *
                     discountCurve_->discount(schedule.dates().at(i)) /
                     bondReferenceYieldCurve_->discount(schedule.dates().at(i)));
            firstPeriod = false;
        } else {
            // note the below is inefficient. one could simply sum the teleskop series, i.e. only take account of first
            // and last term
            npvValue +=
                ((calculateBondNpv(npvDate, schedule.dates().at(i), trsMaturity) -
                  bdForAcc->accruedAmount(schedule.dates().at(i)) * bdForAcc->notional(schedule.dates().at(i)) / 100) *
                     discountCurve_->discount(schedule.dates().at(i)) /
                     bondReferenceYieldCurve_->discount(schedule.dates().at(i)) -
                 (calculateBondNpv(npvDate, schedule.dates().at(i - 1), trsMaturity) -
                  bdForAcc->accruedAmount(schedule.dates().at(i - 1)) * bdForAcc->notional(schedule.dates().at(i - 1)) /
                      100) *
                     discountCurve_->discount(schedule.dates().at(i - 1)) /
                     bondReferenceYieldCurve_->discount(schedule.dates().at(i - 1)));
        }
    }
    return npvValue;
}
} // namespace QuantExt
