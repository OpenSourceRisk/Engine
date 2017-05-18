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

#include <iostream>
#include <ql/currencies/europe.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <qle/termstructures/oiccbasisswaphelper.hpp>

using boost::shared_ptr;

using namespace QuantLib;

namespace QuantExt {

namespace {
void no_deletion(YieldTermStructure*) {}
} // namespace

OICCBSHelper::OICCBSHelper(Natural settlementDays,
                           const Period& term, // swap maturity
                           const boost::shared_ptr<OvernightIndex>& payIndex, const Period& payTenor,
                           const boost::shared_ptr<OvernightIndex>& recIndex,
                           const Period& recTenor, // swap maturity
                           const Handle<Quote>& spreadQuote, const Handle<YieldTermStructure>& fixedDiscountCurve,
                           bool spreadQuoteOnPayLeg, bool fixedDiscountOnPayLeg)
    : RelativeDateRateHelper(spreadQuote), settlementDays_(settlementDays), term_(term), payIndex_(payIndex),
      payTenor_(payTenor), recIndex_(recIndex), recTenor_(recTenor), fixedDiscountCurve_(fixedDiscountCurve),
      spreadQuoteOnPayLeg_(spreadQuoteOnPayLeg), fixedDiscountOnPayLeg_(fixedDiscountOnPayLeg) {

    registerWith(payIndex_);
    registerWith(recIndex_);
    registerWith(fixedDiscountCurve_);
    initializeDates();
}

void OICCBSHelper::initializeDates() {
    Date asof = Settings::instance().evaluationDate();
    Date settlementDate = payIndex_->fixingCalendar().advance(asof, settlementDays_, Days);
    Schedule paySchedule = MakeSchedule().from(settlementDate).to(settlementDate + term_).withTenor(payTenor_);
    Schedule recSchedule = MakeSchedule().from(settlementDate).to(settlementDate + term_).withTenor(recTenor_);
    Currency payCurrency = EURCurrency(); // arbitrary here
    Currency recCurrency = GBPCurrency(); // recCcy != payCcy, but FX=1
    boost::shared_ptr<Quote> fx(new SimpleQuote(1.0));
    swap_ = boost::shared_ptr<OvernightIndexedCrossCcyBasisSwap>(
        new OvernightIndexedCrossCcyBasisSwap(10000.0, // arbitrary payNominal
                                              payCurrency, paySchedule, payIndex_,
                                              0.0,     // zero pay spread
                                              10000.0, // recNominal consistent with FX rate used
                                              recCurrency, recSchedule, recIndex_,
                                              0.0)); // target receive spread
    if (fixedDiscountOnPayLeg_) {
        boost::shared_ptr<PricingEngine> engine(new OvernightIndexedCrossCcyBasisSwapEngine(
            fixedDiscountCurve_, payCurrency, termStructureHandle_, recCurrency, Handle<Quote>(fx)));
        swap_->setPricingEngine(engine);
    } else {
        boost::shared_ptr<PricingEngine> engine(new OvernightIndexedCrossCcyBasisSwapEngine(
            termStructureHandle_, payCurrency, fixedDiscountCurve_, recCurrency, Handle<Quote>(fx)));
        swap_->setPricingEngine(engine);
    }

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
}

void OICCBSHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer -
    // force recalculation when needed
    termStructureHandle_.linkTo(shared_ptr<YieldTermStructure>(t, no_deletion), false);
    RelativeDateRateHelper::setTermStructure(t);
}

Real OICCBSHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    // we didn't register as observers - force calculation
    swap_->recalculate();
    if (spreadQuoteOnPayLeg_)
        return swap_->fairPayLegSpread();
    else
        return swap_->fairRecLegSpread();
}

void OICCBSHelper::accept(AcyclicVisitor& v) {
    Visitor<OICCBSHelper>* v1 = dynamic_cast<Visitor<OICCBSHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
