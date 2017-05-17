/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <qle/pricingengines/discountingswapenginemulticurve.hpp>

namespace QuantExt {

namespace {

class AmountGetter : public AcyclicVisitor,
                     public Visitor<CashFlow>,
                     public Visitor<Coupon>,
                     public Visitor<IborCoupon> {

public:
    AmountGetter() : amount_(0), callAmount_(true) {}
    virtual ~AmountGetter() {}

    Real amount() const { return amount_; }
    virtual Real bpsFactor() const { return 0.0; }
    void setCallAmount(bool flag) { callAmount_ = flag; }

    void visit(CashFlow& c);
    void visit(Coupon& c);
    void visit(IborCoupon& c);

private:
    Real amount_;
    bool callAmount_;
};

void AmountGetter::visit(CashFlow& c) { amount_ = c.amount(); }

void AmountGetter::visit(Coupon& c) { amount_ = c.amount(); }

void AmountGetter::visit(IborCoupon& c) {
    if (callAmount_) {
        amount_ = c.amount();
    } else {
        Handle<YieldTermStructure> forwardingCurve = c.iborIndex()->forwardingTermStructure();
        QL_REQUIRE(!forwardingCurve.empty(), "Forwarding curve is empty.");

        /* Assuming here that Libor value/maturity date = coupon accrual start/end date */
        DiscountFactor discAccStart = forwardingCurve->discount(c.accrualStartDate());
        DiscountFactor discAccEnd = forwardingCurve->discount(c.accrualEndDate());

        Real fixingTimesDcf;
        DayCounter indexBasis = c.iborIndex()->dayCounter();
        DayCounter couponBasis = c.dayCounter();
        if (indexBasis == couponBasis) {
            fixingTimesDcf = (discAccStart / discAccEnd - 1);
        } else {
            Time indexDcf = indexBasis.yearFraction(c.accrualStartDate(), c.accrualEndDate());
            fixingTimesDcf = (discAccStart / discAccEnd - 1) / indexDcf * c.accrualPeriod();
        }
        amount_ = (c.gearing() * fixingTimesDcf + c.spread() * c.accrualPeriod()) * c.nominal();
    }
}

class AdditionalAmountGetter : public AmountGetter {

public:
    AdditionalAmountGetter() {}
    Real bpsFactor() const { return bpsFactor_; }

    void visit(CashFlow& c);
    void visit(Coupon& c);
    void visit(IborCoupon& c);

private:
    Real bpsFactor_;
};

void AdditionalAmountGetter::visit(CashFlow& c) {
    AmountGetter::visit(c);
    bpsFactor_ = 0.0;
}

void AdditionalAmountGetter::visit(Coupon& c) {
    AmountGetter::visit(c);
    bpsFactor_ = c.accrualPeriod() * c.nominal();
}

void AdditionalAmountGetter::visit(IborCoupon& c) {
    AmountGetter::visit(c);
    bpsFactor_ = c.accrualPeriod() * c.nominal();
}
} // namespace

class DiscountingSwapEngineMultiCurve::AmountImpl {
public:
    boost::shared_ptr<AmountGetter> amountGetter_;
};

DiscountingSwapEngineMultiCurve::DiscountingSwapEngineMultiCurve(const Handle<YieldTermStructure>& discountCurve,
                                                                 bool minimalResults,
                                                                 boost::optional<bool> includeSettlementDateFlows,
                                                                 Date settlementDate, Date npvDate)
    : discountCurve_(discountCurve), minimalResults_(minimalResults),
      includeSettlementDateFlows_(includeSettlementDateFlows), settlementDate_(settlementDate), npvDate_(npvDate),
      impl_(new AmountImpl) {

    registerWith(discountCurve_);

    if (minimalResults_) {
        impl_->amountGetter_.reset(new AmountGetter);
    } else {
        impl_->amountGetter_.reset(new AdditionalAmountGetter);
    }
}

void DiscountingSwapEngineMultiCurve::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "Empty discounting "
                                        "term structure handle");

    // Instrument settlement date
    Date referenceDate = discountCurve_->referenceDate();
    Date settlementDate = settlementDate_;
    if (settlementDate_ == Date()) {
        settlementDate = referenceDate;
    } else {
        QL_REQUIRE(settlementDate >= referenceDate, "settlement date (" << settlementDate
                                                                        << ") before "
                                                                           "discount curve reference date ("
                                                                        << referenceDate << ")");
    }

    // - Instrument::results
    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();
    results_.valuationDate = npvDate_;
    if (npvDate_ == Date()) {
        results_.valuationDate = referenceDate;
    } else {
        QL_REQUIRE(npvDate_ >= referenceDate, "npv date (" << npvDate_
                                                           << ") before "
                                                              "discount curve reference date ("
                                                           << referenceDate << ")");
    }

    // - Swap::results
    Size numLegs = arguments_.legs.size();
    results_.legNPV.resize(numLegs);
    results_.legBPS.resize(numLegs);
    results_.startDiscounts.resize(numLegs);
    results_.endDiscounts.resize(numLegs);
    results_.npvDateDiscount = discountCurve_->discount(results_.valuationDate);

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    const Spread bp = 1.0e-4;

    for (Size i = 0; i < numLegs; i++) {

        Leg leg = arguments_.legs[i];
        results_.legNPV[i] = 0.0;
        results_.legBPS[i] = 0.0;

        // Call amount() method of underlying coupon for first coupon.
        impl_->amountGetter_->setCallAmount(true);

        for (Size j = 0; j < leg.size(); j++) {

            /* Exclude cashflows that have occured taking into account the
            settlement date and includeSettlementDateFlows flag */
            if (leg[j]->hasOccurred(settlementDate, includeRefDateFlows)) {
                continue;
            }

            DiscountFactor discount = discountCurve_->discount(leg[j]->date());
            leg[j]->accept(*(impl_->amountGetter_));
            results_.legNPV[i] += impl_->amountGetter_->amount() * discount;
            results_.legBPS[i] += impl_->amountGetter_->bpsFactor() * discount;

            // For all coupons after second do not call amount(), since for those
            // we can be sure that they are not fixed yet
            if (j == 1)
                impl_->amountGetter_->setCallAmount(false);
        }

        results_.legNPV[i] *= arguments_.payer[i];
        results_.legNPV[i] /= results_.npvDateDiscount;
        results_.legBPS[i] *= arguments_.payer[i] * bp;
        results_.legBPS[i] /= results_.npvDateDiscount;
        results_.value += results_.legNPV[i];
    }
}
} // namespace QuantExt
