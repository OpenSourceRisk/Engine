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
#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>

namespace QuantExt {

DiscountingForwardBondEngine::DiscountingForwardBondEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<YieldTermStructure>& incomeCurve,
    const Handle<YieldTermStructure>& bondReferenceYieldCurve, const Handle<Quote>& bondSpread,
    const Handle<DefaultProbabilityTermStructure>& bondDefaultCurve, const Handle<Quote>& bondRecoveryRate,
    Period timestepPeriod, boost::optional<bool> includeSettlementDateFlows, const Date& settlementDate,
    const Date& npvDate)
    : discountCurve_(discountCurve), incomeCurve_(incomeCurve), bondReferenceYieldCurve_(bondReferenceYieldCurve),
      bondSpread_(bondSpread), bondDefaultCurve_(bondDefaultCurve), bondRecoveryRate_(bondRecoveryRate),
      timestepPeriod_(timestepPeriod), includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate), npvDate_(npvDate) {

    bondReferenceYieldCurve_ =
        bondSpread_.empty() ? bondReferenceYieldCurve
                            : Handle<YieldTermStructure>(
                                  boost::make_shared<ZeroSpreadedTermStructure>(bondReferenceYieldCurve, bondSpread_));
    registerWith(discountCurve_);           // curve for discounting of the forward derivative contract. OIS, usually.
    registerWith(incomeCurve_);             // this is a curve for compounding of the bond
    registerWith(bondReferenceYieldCurve_); // this is the bond reference curve, for discounting, usually RePo
    registerWith(bondSpread_);
    registerWith(bondDefaultCurve_);
    registerWith(bondRecoveryRate_);
}

void DiscountingForwardBondEngine::calculate() const {
    // Do some checks on data
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    QL_REQUIRE(!incomeCurve_.empty(), "income term structure handle is empty");
    QL_REQUIRE(!bondReferenceYieldCurve_.empty(), "bond rerference term structure handle is empty");

    Date npvDate = npvDate_; // this is today when the valuation occurs
    if (npvDate == Null<Date>()) {
        npvDate = (*discountCurve_)->referenceDate();
    }
    Date settlementDate = settlementDate_; //
    if (settlementDate == Null<Date>()) {
        settlementDate = (*discountCurve_)->referenceDate();
    }

    Date maturityDate =
        arguments_.fwdMaturityDate; // this is the date when the forward is executed, i.e. cash and bond change hands

    Real cmpPayment = arguments_.compensationPayment;
    if (cmpPayment == Null<Real>()) {
        cmpPayment = 0.0;
    }
    Date cmpPaymentDate = arguments_.compensationPaymentDate;
    if (cmpPaymentDate == Null<Date>()) {
        cmpPaymentDate = npvDate;
    }

    // in case that the premium payment has occured in the past, we set the amount to 0. the date itself is set to the
    // npvDate to have a valid date for "discounting"
    Date cmpPaymentDate_use = cmpPaymentDate >= npvDate ? cmpPaymentDate : maturityDate;
    cmpPayment = cmpPaymentDate >= npvDate ? cmpPayment : 0.0; // premium cashflow is not relevant for npv if in the
                                                               // past

    // initialize
    results_.value = 0.0;               // this is today's npv of the forward contract
    results_.underlyingSpotValue = 0.0; // this is today's value of the "restricted bond". Restricted means that only
                                        // cashflows after maturity are taken into account.
    results_.forwardValue = 0.0;        // this the value of the forward contract just before maturity

    bool dirty = arguments_.settlementDirty;

    results_.underlyingSpotValue = calculateBondNpv(npvDate, maturityDate); // cashflows before maturity will be ignored

    boost::tie(results_.forwardValue, results_.value) = calculateForwardContractPresentValue(
        results_.underlyingSpotValue, cmpPayment, npvDate, maturityDate, cmpPaymentDate_use, dirty);
}

Real DiscountingForwardBondEngine::calculateBondNpv(Date npvDate, Date computeDate) const {
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
        // Recovery amount is computed over the whole time interval (npvDate,maturityOfBond)

        if (bd->cashflows()[i]->hasOccurred(
                computeDate, includeSettlementDateFlows_)) // Cashflows before computeDate not relevant for npv
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
            Date effectiveStartDate = (startDate <= computeDate && computeDate <= endDate) ? computeDate : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, endDate);

            npvValue += coupon->nominal() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
        }

        hasLiveCashFlow = true; // check if a cashflow  is available after the date of valuation.

        // Coupon value is discounted future payment times the survival probability
        Probability S = creditCurvePtr->survivalProbability(bd->cashflows()[i]->date()) /
                        creditCurvePtr->survivalProbability(computeDate);
        npvValue += bd->cashflows()[i]->amount() * S * bondReferenceYieldCurve_->discount(bd->cashflows()[i]->date());
    }

    // the ql instrument might not yet be expired and still have not anything to value if
    // the computeDate > evaluation date
    if (!hasLiveCashFlow)
        return 0.0;

    if (bd->cashflows().size() > 1 && numCoupons == 0) {
        QL_FAIL("DiscountingForwardBondEngine does not support bonds with multiple cashflows but no coupons");
    }

    boost::shared_ptr<Coupon> firstCoupon = boost::dynamic_pointer_cast<Coupon>(bd->cashflows()[0]);
    if (firstCoupon) {
        Date startDate = computeDate; // face value recovery starting with computeDate
        while (startDate < bd->cashflows()[0]->date()) {
            Date stepDate = startDate + timestepPeriod_;
            Date endDate = (stepDate > bd->cashflows()[0]->date()) ? bd->cashflows()[0]->date() : stepDate;
            Date defaultDate = startDate + (endDate - startDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

            npvValue += firstCoupon->nominal() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
            startDate = stepDate;
        }
    }
    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (bd->cashflows().size() == 1) {
        boost::shared_ptr<Redemption> redemption = boost::dynamic_pointer_cast<Redemption>(bd->cashflows()[0]);
        if (redemption) {
            Date startDate = computeDate;
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
    return npvValue * arguments_.bondNotional;
}

boost::tuple<Real, Real> DiscountingForwardBondEngine::calculateForwardContractPresentValue(
    Real spotValue, Real cmpPayment, Date npvDate, Date computeDate, Date cmpPaymentDate, bool dirty) const {
    // here we go with the true forward computation
    Real forwardBondValue = 0.0;
    Real forwardContractPresentValue = 0.0;
    Real forwardContractForwardValue = 0.0;

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

    // the case of dirty strike corresponds here to an accrual of 0.0. This will be convenient in the code.
    Real accruedAmount =
        dirty ? 0.0 : bd->accruedAmount(computeDate) * bd->notional(computeDate) / 100 * arguments_.bondNotional;

    /* Discounting and compounding, taking account of possible bond default before delivery*/

    forwardBondValue =
        spotValue / (incomeCurve_->discount(computeDate)); // compounding to date of maturity of forward contract

    // Subtract strike at maturity. Regarding accrual (i.e. strike is given clean vs dirty) there are two
    // cases: long or short.

    // Long: forwardBondValue - strike_dirt = (forwardBondValue - accrual) - strike_clean
    // Short: strike_dirt - forwardBondValue = strike_clean - (forwardBondValue - accrual)
    // In total:
    forwardContractForwardValue = (*arguments_.payoff)(forwardBondValue - accruedAmount);

    // forwardContractPresentValue adjusted for potential default before computeDate:
    forwardContractPresentValue =
        forwardContractForwardValue * (discountCurve_->discount(computeDate)) *
            creditCurvePtr->survivalProbability(computeDate) -
        cmpPayment *
            (discountCurve_->discount(cmpPaymentDate)); // The forward is a derivative. We use "OIS curve" to discount.
                                                        // We subtract the potential payment due to Premium.

    // Take account of face value recovery:
    // A) Recovery for time period when coupons are present
    for (Size i = 0; i < bd->cashflows().size(); i++) {
        if (bd->cashflows()[i]->hasOccurred(
                npvDate, includeSettlementDateFlows_)) // Cashflows before npvDate not relevant for npv
            continue;
        if (bd->cashflows()[i]->date() >=
            computeDate) // Cashflows after computeDate do not fall into the forward period
            continue;
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(bd->cashflows()[i]);

        if (coupon) {
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= npvDate && npvDate <= endDate) ? npvDate : startDate;
            Date effectiveEndDate = (startDate <= computeDate && computeDate <= endDate) ? computeDate : endDate;
            Date defaultDate = effectiveStartDate + (effectiveEndDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, effectiveEndDate);

            forwardContractPresentValue +=
                (*arguments_.payoff)(coupon->nominal() * arguments_.bondNotional * recoveryVal - accruedAmount) * P *
                (discountCurve_->discount(defaultDate));
        }
    }

    // B) Recovery for time period before coupons are present
    boost::shared_ptr<Coupon> firstCoupon = boost::dynamic_pointer_cast<Coupon>(bd->cashflows()[0]);
    if (firstCoupon) {
        Date startDate = npvDate; // face value recovery starting with npvDate
        Date stopDate = std::min(bd->cashflows()[0]->date(), computeDate);
        while (startDate < stopDate) {
            Date stepDate = startDate + timestepPeriod_;
            Date endDate = (stepDate > stopDate) ? stopDate : stepDate;
            Date defaultDate = startDate + (endDate - startDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

            forwardContractPresentValue +=
                (*arguments_.payoff)(firstCoupon->nominal() * arguments_.bondNotional * recoveryVal - accruedAmount) *
                P * (discountCurve_->discount(defaultDate));
            startDate = stepDate;
        }
    }
    // C) ZCB
    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (bd->cashflows().size() == 1) {
        boost::shared_ptr<Redemption> redemption = boost::dynamic_pointer_cast<Redemption>(bd->cashflows()[0]);
        if (redemption) {
            Date startDate = npvDate;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepPeriod_;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

                forwardContractPresentValue +=
                    (*arguments_.payoff)(redemption->amount() * arguments_.bondNotional * recoveryVal - accruedAmount) *
                    P * (discountCurve_->discount(defaultDate));
                startDate = stepDate;
            }
        }
    }

    return boost::make_tuple(forwardContractForwardValue, forwardContractPresentValue);
}

} // namespace QuantExt
