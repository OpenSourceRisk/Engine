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

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <qle/instruments/cashflowresults.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>

#include <boost/date_time.hpp>
#include <boost/make_shared.hpp>

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
                                  QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(bondReferenceYieldCurve, bondSpread_));
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
    QL_REQUIRE(!bondReferenceYieldCurve_.empty(), "bond reference term structure handle is empty");

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

    // in case that the premium payment has occurred in the past, we set the amount to 0. the date itself is set to the
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
        results_.underlyingSpotValue, cmpPayment, npvDate, maturityDate, arguments_.fwdSettlementDate,
        !arguments_.isPhysicallySettled, cmpPaymentDate_use, dirty);
}

Real DiscountingForwardBondEngine::calculateBondNpv(Date npvDate, Date computeDate) const {
    Real npvValue = 0.0;
    Size numCoupons = 0;
    bool hasLiveCashFlow = false;

    // handle case where we wish to price simply with benchmark curve and scalar security spread
    // i.e. credit curve term structure (and recovery) have not been specified
    // we set the default probability and recovery rate to zero in this instance (issuer credit worthiness already
    // captured within security spread)
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> creditCurvePtr =
        bondDefaultCurve_.empty()
            ? QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(npvDate, 0.0, bondReferenceYieldCurve_->dayCounter())
            : bondDefaultCurve_.currentLink();
    Rate recoveryVal = bondRecoveryRate_.empty() ? 0.0 : bondRecoveryRate_->value(); // setup default bond recovery rate

    std::vector<Date> bondCashflowPayDates;
    std::vector<Real> bondCashflows, bondCashflowSurvivalProbabilities, bondCashflowDiscountFactors;

    // load the shared pointer into bd
    QuantLib::ext::shared_ptr<Bond> bd = arguments_.underlying;

    std::vector<CashFlowResults> cashFlowResults;
    for (Size i = 0; i < bd->cashflows().size(); i++) {
        // Recovery amount is computed over the whole time interval (npvDate,maturityOfBond)

        if (bd->cashflows()[i]->hasOccurred(
                computeDate, includeSettlementDateFlows_)) // Cashflows before computeDate not relevant for npv
            continue;

        /* The amount recovered in the case of default is the recoveryrate*Notional*Probability of
           Default; this is added to the NPV value. For coupon bonds the coupon periods are taken
           as the timesteps for integrating over the probability of default.
        */
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(bd->cashflows()[i]);

        if (coupon) {
            numCoupons++;
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= computeDate && computeDate <= endDate) ? computeDate : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, endDate);

            Real cpnRecovery = coupon->nominal() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
            npvValue += cpnRecovery;
            if (!close_enough(cpnRecovery, 0.0)) {
                CashFlowResults recoveryFlow;
                recoveryFlow.payDate = defaultDate;
                recoveryFlow.accrualStartDate = effectiveStartDate;
                recoveryFlow.accrualEndDate = endDate;
                recoveryFlow.amount = coupon->nominal() * recoveryVal * arguments_.bondNotional;
                recoveryFlow.discountFactor = P * bondReferenceYieldCurve_->discount(defaultDate);
                recoveryFlow.presentValue = recoveryFlow.amount * recoveryFlow.discountFactor;
                recoveryFlow.legNumber = 0;
                recoveryFlow.type = "Bond_ExpectedRecovery";
                cashFlowResults.push_back(recoveryFlow);
            }
        }

        hasLiveCashFlow = true; // check if a cashflow  is available after the date of valuation.

        // Coupon value is discounted future payment times the survival probability
        Probability S = creditCurvePtr->survivalProbability(bd->cashflows()[i]->date()) /
                        creditCurvePtr->survivalProbability(computeDate);
        npvValue += bd->cashflows()[i]->amount() * S * bondReferenceYieldCurve_->discount(bd->cashflows()[i]->date());

        bondCashflows.push_back(bd->cashflows()[i]->amount());
        bondCashflowPayDates.push_back(bd->cashflows()[i]->date());
        bondCashflowSurvivalProbabilities.push_back(S);
        bondCashflowDiscountFactors.push_back(bondReferenceYieldCurve_->discount(bd->cashflows()[i]->date()));
        CashFlowResults cfResult = populateCashFlowResultsFromCashflow(bd->cashflows()[i], arguments_.bondNotional);
        cfResult.type = "Bond_" + cfResult.type;
        cfResult.discountFactor = S * bondReferenceYieldCurve_->discount(bd->cashflows()[i]->date());
        cfResult.presentValue = cfResult.amount * cfResult.discountFactor;
        cashFlowResults.push_back(cfResult);
    }

    // the ql instrument might not yet be expired and still have not anything to value if
    // the computeDate > evaluation date
    if (!hasLiveCashFlow)
        return 0.0;

    if (bd->cashflows().size() > 1 && numCoupons == 0) {
        QL_FAIL("DiscountingForwardBondEngine does not support bonds with multiple cashflows but no coupons");
    }

    Real bondRecovery = 0;
    QuantLib::ext::shared_ptr<Coupon> firstCoupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(bd->cashflows()[0]);
    if (firstCoupon) {
        Date startDate = computeDate; // face value recovery starting with computeDate
        while (startDate < bd->cashflows()[0]->date()) {
            Date stepDate = startDate + timestepPeriod_;
            Date endDate = (stepDate > bd->cashflows()[0]->date()) ? bd->cashflows()[0]->date() : stepDate;
            Date defaultDate = startDate + (endDate - startDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

            bondRecovery += firstCoupon->nominal() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
            startDate = stepDate;
        }
        if (!close_enough(bondRecovery, 0.0)) {
            CashFlowResults recoveryFlow;
            recoveryFlow.payDate = bd->cashflows()[0]->date();
            recoveryFlow.accrualStartDate = computeDate;
            recoveryFlow.accrualEndDate = bd->cashflows()[0]->date();
            recoveryFlow.amount = firstCoupon->nominal() * recoveryVal * arguments_.bondNotional;
            recoveryFlow.discountFactor = bondRecovery * arguments_.bondNotional / recoveryFlow.amount;
            recoveryFlow.presentValue = bondRecovery * arguments_.bondNotional;
            recoveryFlow.legNumber = 0;
            recoveryFlow.type = "Bond_ExpectedRecovery";
            cashFlowResults.push_back(recoveryFlow);
        }
    }

    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (bd->cashflows().size() == 1) {
        QuantLib::ext::shared_ptr<Redemption> redemption = QuantLib::ext::dynamic_pointer_cast<Redemption>(bd->cashflows()[0]);
        Real redemptionRecovery = 0;
        if (redemption) {
            Date startDate = computeDate;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepPeriod_;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

                redemptionRecovery +=
                    redemption->amount() * recoveryVal * P * bondReferenceYieldCurve_->discount(defaultDate);
                startDate = stepDate;
            }
            bondRecovery += redemptionRecovery;
            if (!close_enough(redemptionRecovery, 0.0)) {
                CashFlowResults recoveryFlow;
                recoveryFlow.payDate = bd->cashflows()[0]->date();
                recoveryFlow.accrualStartDate = computeDate;
                recoveryFlow.accrualEndDate = bd->cashflows()[0]->date();
                recoveryFlow.amount = redemption->amount() * recoveryVal * arguments_.bondNotional;
                recoveryFlow.discountFactor = redemptionRecovery * arguments_.bondNotional / recoveryFlow.amount;
                recoveryFlow.presentValue = redemptionRecovery * arguments_.bondNotional;
                recoveryFlow.legNumber = 0;
                recoveryFlow.type = "Bond_ExpectedRecovery";
                cashFlowResults.push_back(recoveryFlow);
            }
        }
    }

    npvValue += bondRecovery;

    // Add cashflowResults
    if (results_.additionalResults.find("cashFlowResults") != results_.additionalResults.end()) {
        auto tmp = results_.additionalResults["cashFlowResults"];
        QL_REQUIRE(tmp.type() == typeid(std::vector<CashFlowResults>), "internal error: cashflowResults type not handlded");
        std::vector<CashFlowResults> prevCfResults = boost::any_cast<std::vector<CashFlowResults>>(tmp);
        prevCfResults.insert(prevCfResults.end(), cashFlowResults.begin(), cashFlowResults.end());
        results_.additionalResults["cashFlowResults"] = prevCfResults;
    } else {
        results_.additionalResults["cashFlowResults"] = cashFlowResults;
    }

    results_.additionalResults["bondCashflow"] = bondCashflows;
    results_.additionalResults["bondCashflowPayDates"] = bondCashflowPayDates;
    results_.additionalResults["bondCashflowSurvivalProbabilities"] = bondCashflowSurvivalProbabilities;
    results_.additionalResults["bondCashflowDiscountFactors"] = bondCashflowDiscountFactors;
    results_.additionalResults["bondRecovery"] = bondRecovery;

    return npvValue * arguments_.bondNotional;
}

QuantLib::ext::tuple<Real, Real> DiscountingForwardBondEngine::calculateForwardContractPresentValue(
    Real spotValue, Real cmpPayment, Date npvDate, Date computeDate, Date settlementDate, bool cashSettlement,
    Date cmpPaymentDate, bool dirty) const {

    // here we go with the true forward computation
    Real forwardBondValue = 0.0;
    Real forwardContractPresentValue = 0.0;
    Real forwardContractForwardValue = 0.0;

    std::vector<Date> fwdBondCashflowPayDates;
    std::vector<Real> fwdBondCashflows, fwdBondCashflowSurvivalProbabilities, fwdBondCashflowDiscountFactors;

    // handle case where we wish to price simply with benchmark curve and scalar security spread
    // i.e. credit curve term structure (and recovery) have not been specified
    // we set the default probability and recovery rate to zero in this instance (issuer credit worthiness already
    // captured within security spread)
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> creditCurvePtr =
        bondDefaultCurve_.empty()
            ? QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(npvDate, 0.0, bondReferenceYieldCurve_->dayCounter())
            : bondDefaultCurve_.currentLink();
    Rate recoveryVal = bondRecoveryRate_.empty() ? 0.0 : bondRecoveryRate_->value(); // setup default bond recovery rate
    // load the shared pointer into bd
    QuantLib::ext::shared_ptr<Bond> bd = arguments_.underlying;

    // the case of dirty strike corresponds here to an accrual of 0.0. This will be convenient in the code.
    Date bondSettlementDate = bd->settlementDate(computeDate);
    Real accruedAmount = dirty ? 0.0
                               : bd->accruedAmount(bondSettlementDate) * bd->notional(bondSettlementDate) / 100.0 *
                                     arguments_.bondNotional;

    /* Discounting and compounding, taking account of possible bond default before delivery*/

    if (cashSettlement) {
        forwardBondValue = spotValue / (incomeCurve_->discount(bondSettlementDate));
        results_.additionalResults["incomeCompoundingDate"] = bondSettlementDate;
    } else {
        forwardBondValue = spotValue / (incomeCurve_->discount(settlementDate));
        results_.additionalResults["incomeCompoundingDate"] = settlementDate;
    }

    results_.additionalResults["spotForwardBondValue"] = spotValue;
    results_.additionalResults["forwardForwardBondValue"] = forwardBondValue;
    results_.additionalResults["incomeCompounding"] = 1.0 / incomeCurve_->discount(bondSettlementDate);

    results_.additionalResults["bondSettlementDate"] = bondSettlementDate;
    results_.additionalResults["forwardSettlementDate"] = settlementDate;

    results_.additionalResults["bondNotionalSettlementDate"] =
        bd->notional(bondSettlementDate) * arguments_.bondNotional;
    results_.additionalResults["accruedAmount"] = accruedAmount;

    // Subtract strike at maturity. Regarding accrual (i.e. strike is given clean vs dirty) there are two
    // cases: long or short.

    // Long: forwardBondValue - strike_dirt = (forwardBondValue - accrual) - strike_clean
    // Short: strike_dirt - forwardBondValue = strike_clean - (forwardBondValue - accrual)
    // In total:
    QuantLib::ext::shared_ptr<Payoff> effectivePayoff;
    if (arguments_.payoff) {
        // vanilla forward bond calculation
        forwardContractForwardValue = (*arguments_.payoff)(forwardBondValue - accruedAmount);
        effectivePayoff = arguments_.payoff;
    } else if (arguments_.lockRate != Null<Real>()) {
        // lock rate specified forward bond calculation, use hardcoded conventions (compounded / semi annual) here, from
        // treasury bonds
        Real price = forwardBondValue / arguments_.bondNotional / bd->notional(bondSettlementDate) * 100.0;
        Real yield = BondFunctions::yield(*bd, price, arguments_.lockRateDayCounter, Compounded, Semiannual,
                                          bondSettlementDate, 1E-10, 100, 0.05, Bond::Price::Dirty);
        Real dv01, modDur = Null<Real>();
        if (arguments_.dv01 != Null<Real>()) {
            dv01 = arguments_.dv01;
        } else {
            modDur = BondFunctions::duration(*bd, yield, arguments_.lockRateDayCounter, Compounded, Semiannual,
                                             Duration::Modified, bondSettlementDate);
            dv01 = price / 100.0 * modDur;
        }

        QL_REQUIRE(arguments_.longInForward, "DiscountingForwardBondEngine: internal error, longInForward must be "
                                             "populated if payoff is specified via lock-rate");
        Real multiplier = (*arguments_.longInForward) ? 1.0 : -1.0;
        forwardContractForwardValue = multiplier * (yield - arguments_.lockRate) * dv01 * arguments_.bondNotional *
                                      bd->notional(bondSettlementDate);

        effectivePayoff = QuantLib::ext::make_shared<ForwardBondTypePayoff>(
            (*arguments_.longInForward) ? Position::Long : Position::Short,
            arguments_.lockRate * dv01 * arguments_.bondNotional * bd->notional(bondSettlementDate));
        
        results_.additionalResults["dv01"] = dv01;
        results_.additionalResults["modifiedDuration"] = modDur;
        results_.additionalResults["yield"] = yield;
        results_.additionalResults["price"] = price;
        results_.additionalResults["lockRate"] = arguments_.lockRate;
    } else {
        QL_FAIL("DiscountingForwardBondEngine: internal error, no payoff and no lock rate given, expected exactly one "
                "of them to be populated.");
    }

    // forwardContractPresentValue adjusted for potential default before computeDate:
    forwardContractPresentValue =
        forwardContractForwardValue * (discountCurve_->discount(settlementDate)) *
            creditCurvePtr->survivalProbability(computeDate) -
        cmpPayment *
            (discountCurve_->discount(cmpPaymentDate)); // The forward is a derivative. We use "OIS curve" to discount.
                                                        // We subtract the potential payment due to Premium.

    results_.additionalResults["forwardContractForwardValue"] = forwardContractForwardValue;
    results_.additionalResults["forwardContractDiscountFactor"] = discountCurve_->discount(settlementDate);
    results_.additionalResults["forwardContractSurvivalProbability"] = creditCurvePtr->survivalProbability(computeDate);
    results_.additionalResults["compensationPayment"] = cmpPayment;
    results_.additionalResults["compensationPaymentDate"] = cmpPaymentDate;
    results_.additionalResults["compensationPaymentDiscount"] = discountCurve_->discount(cmpPaymentDate);

    fwdBondCashflows.push_back(forwardContractForwardValue);
    fwdBondCashflowPayDates.push_back(computeDate);
    fwdBondCashflowSurvivalProbabilities.push_back(creditCurvePtr->survivalProbability(computeDate));
    fwdBondCashflowDiscountFactors.push_back(discountCurve_->discount(computeDate));

    fwdBondCashflows.push_back(-1 * cmpPayment);
    fwdBondCashflowPayDates.push_back(cmpPaymentDate);
    fwdBondCashflowSurvivalProbabilities.push_back(1);
    fwdBondCashflowDiscountFactors.push_back(discountCurve_->discount(cmpPaymentDate));

    // Write Cashflow Results
    std::vector<CashFlowResults> forwardCashFlowResults;
    forwardCashFlowResults.reserve(2);

    // Forward Leg
    CashFlowResults fwdCfResult;
    fwdCfResult.payDate = settlementDate;
    fwdCfResult.legNumber = 1;
    fwdCfResult.amount = forwardContractForwardValue;
    fwdCfResult.discountFactor =
        discountCurve_->discount(settlementDate) * creditCurvePtr->survivalProbability(computeDate);
    fwdCfResult.presentValue = fwdCfResult.amount * fwdCfResult.discountFactor;
    fwdCfResult.type = "ForwardValue";
    forwardCashFlowResults.push_back(fwdCfResult);

    if (!close_enough(cmpPayment, 0.0)) {
        CashFlowResults cmpCfResult;
        cmpCfResult.payDate = cmpPaymentDate;
        cmpCfResult.legNumber = 2;
        cmpCfResult.amount = -1 * cmpPayment;
        cmpCfResult.discountFactor = discountCurve_->discount(cmpPaymentDate);
        cmpCfResult.presentValue = cmpCfResult.amount * cmpCfResult.discountFactor;
        cmpCfResult.type = "Premium";
        forwardCashFlowResults.push_back(cmpCfResult);
    }

    Real fwdBondRecovery = 0;
    // Take account of face value recovery:
    // A) Recovery for time period when coupons are present
    for (Size i = 0; i < bd->cashflows().size(); i++) {
        if (bd->cashflows()[i]->hasOccurred(
                npvDate, includeSettlementDateFlows_)) // Cashflows before npvDate not relevant for npv
            continue;
        if (bd->cashflows()[i]->date() >=
            computeDate) // Cashflows after computeDate do not fall into the forward period
            continue;
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(bd->cashflows()[i]);

        if (coupon) {
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= npvDate && npvDate <= endDate) ? npvDate : startDate;
            Date effectiveEndDate = (startDate <= computeDate && computeDate <= endDate) ? computeDate : endDate;
            Date defaultDate = effectiveStartDate + (effectiveEndDate - effectiveStartDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(effectiveStartDate, effectiveEndDate);

            Real couponRecovery = 
                (*effectivePayoff)(coupon->nominal() * arguments_.bondNotional * recoveryVal - accruedAmount) * P *
                (discountCurve_->discount(defaultDate));
            fwdBondRecovery += couponRecovery;
            if (!close_enough(couponRecovery, 0.0)) {
                CashFlowResults recoveryFlow;
                recoveryFlow.payDate = defaultDate;
                recoveryFlow.accrualStartDate = effectiveStartDate;
                recoveryFlow.accrualEndDate = endDate;
                recoveryFlow.amount =
                    (*effectivePayoff)(coupon->nominal() * arguments_.bondNotional * recoveryVal - accruedAmount);
                recoveryFlow.discountFactor = P * (discountCurve_->discount(defaultDate));
                recoveryFlow.presentValue = recoveryFlow.amount * recoveryFlow.discountFactor;
                recoveryFlow.legNumber = 3;
                recoveryFlow.type = "Forward_ExpectedRecovery";
                forwardCashFlowResults.push_back(recoveryFlow);
            }
        }
    }

    // B) Recovery for time period before coupons are present
    QuantLib::ext::shared_ptr<Coupon> firstCoupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(bd->cashflows()[0]);
    if (firstCoupon) {
        Date startDate = npvDate; // face value recovery starting with npvDate
        Date stopDate = std::min(bd->cashflows()[0]->date(), computeDate);
        Real recoveryBeforeCoupons = 0.0;
        while (startDate < stopDate) {
            Date stepDate = startDate + timestepPeriod_;
            Date endDate = (stepDate > stopDate) ? stopDate : stepDate;
            Date defaultDate = startDate + (endDate - startDate) / 2;
            Probability P = creditCurvePtr->defaultProbability(startDate, endDate);

            recoveryBeforeCoupons +=
                (*effectivePayoff)(firstCoupon->nominal() * arguments_.bondNotional * recoveryVal - accruedAmount) * P *
                (discountCurve_->discount(defaultDate));
            startDate = stepDate;
        }
        fwdBondRecovery += recoveryBeforeCoupons;
        if (!close_enough(recoveryBeforeCoupons, 0.0)) {
            CashFlowResults recoveryFlow;
            recoveryFlow.payDate = stopDate;
            recoveryFlow.accrualStartDate = startDate;
            recoveryFlow.accrualEndDate = stopDate;
            recoveryFlow.amount =
                (*effectivePayoff)(firstCoupon->nominal() * arguments_.bondNotional * recoveryVal - accruedAmount);
            recoveryFlow.discountFactor = recoveryBeforeCoupons / recoveryFlow.amount;
            recoveryFlow.presentValue = recoveryBeforeCoupons;
            recoveryFlow.legNumber = 4;
            recoveryFlow.type = "Forward_ExpectedRecovery";
            forwardCashFlowResults.push_back(recoveryFlow);
        }
    }
    // C) ZCB
    /* If there are no coupon, as in a Zero Bond, we must integrate over the entire period from npv date to
       maturity. The timestepPeriod specified is used as provide the steps for the integration. This only applies
       to bonds with 1 cashflow, identified as a final redemption payment.
    */
    if (bd->cashflows().size() == 1) {
        QuantLib::ext::shared_ptr<Redemption> redemption = QuantLib::ext::dynamic_pointer_cast<Redemption>(bd->cashflows()[0]);
        if (redemption) {
            Real redemptionRecovery = 0;
            Date startDate = npvDate;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepPeriod_;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = creditCurvePtr->defaultProbability(startDate, endDate);
                redemptionRecovery +=
                    (*effectivePayoff)(redemption->amount() * arguments_.bondNotional * recoveryVal - accruedAmount) *
                    P * (discountCurve_->discount(defaultDate));
                startDate = stepDate;
            }
            fwdBondRecovery += redemptionRecovery;
            if (!close_enough(redemptionRecovery, 0.0)) {
                CashFlowResults recoveryFlow;
                recoveryFlow.payDate = redemption->date();
                recoveryFlow.accrualStartDate = startDate;
                recoveryFlow.accrualEndDate = redemption->date();
                recoveryFlow.amount =
                    (*effectivePayoff)(redemption->amount() * arguments_.bondNotional * recoveryVal - accruedAmount);
                recoveryFlow.discountFactor = redemptionRecovery / recoveryFlow.amount;
                recoveryFlow.presentValue = redemptionRecovery;
                recoveryFlow.legNumber = 5;
                recoveryFlow.type = "Forward_ExpectedRecovery";
                forwardCashFlowResults.push_back(recoveryFlow);
            }
        }
    }

    results_.additionalResults["forwardBondRecovery"] = fwdBondRecovery;

    // Add cashflowResults
    if (results_.additionalResults.find("cashFlowResults") != results_.additionalResults.end()) {
        auto tmp = results_.additionalResults["cashFlowResults"];
        QL_REQUIRE(tmp.type() == typeid(std::vector<CashFlowResults>), "internal error: cashflowResults type not handlded");
        std::vector<CashFlowResults> prevCfResults = boost::any_cast<std::vector<CashFlowResults>>(tmp);
        prevCfResults.insert(prevCfResults.end(), forwardCashFlowResults.begin(), forwardCashFlowResults.end());
        results_.additionalResults["cashFlowResults"] = prevCfResults;
    } else {
        results_.additionalResults["cashFlowResults"] = forwardCashFlowResults;
    }

    forwardContractPresentValue += fwdBondRecovery;

    return QuantLib::ext::make_tuple(forwardContractForwardValue, forwardContractPresentValue);
}

} // namespace QuantExt
