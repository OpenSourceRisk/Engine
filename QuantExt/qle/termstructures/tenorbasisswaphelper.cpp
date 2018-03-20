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

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

#include <qle/termstructures/tenorbasisswaphelper.hpp>

namespace QuantExt {

namespace {
void no_deletion(YieldTermStructure*) {}
} // namespace

TenorBasisSwapHelper::TenorBasisSwapHelper(Handle<Quote> spread, const Period& swapTenor,
                                           const boost::shared_ptr<IborIndex> longIndex,
                                           const boost::shared_ptr<IborIndex> shortIndex, const Period& shortPayTenor,
                                           const Handle<YieldTermStructure>& discountingCurve, bool spreadOnShort,
                                           bool includeSpread, SubPeriodsCoupon::Type type)
    : RelativeDateRateHelper(spread), swapTenor_(swapTenor), longIndex_(longIndex), shortIndex_(shortIndex),
      spreadOnShort_(spreadOnShort), includeSpread_(includeSpread), type_(type), discountHandle_(discountingCurve) {

    bool longIndexHasCurve = !longIndex_->forwardingTermStructure().empty();
    bool shortIndexHasCurve = !shortIndex_->forwardingTermStructure().empty();
    bool haveDiscountCurve = !discountHandle_.empty();
    QL_REQUIRE(!(longIndexHasCurve && shortIndexHasCurve && haveDiscountCurve),
               "Have all curves nothing to solve for.");

    if (longIndexHasCurve && !shortIndexHasCurve) {
        shortIndex_ = shortIndex_->clone(termStructureHandle_);
        shortIndex_->unregisterWith(termStructureHandle_);
    } else if (!longIndexHasCurve && shortIndexHasCurve) {
        longIndex_ = longIndex_->clone(termStructureHandle_);
        longIndex_->unregisterWith(termStructureHandle_);
    } else if (!longIndexHasCurve && !shortIndexHasCurve) {
        QL_FAIL("Need at least one of the indices to have a valid curve.");
    }

    shortPayTenor_ = shortPayTenor == Period() ? shortIndex_->tenor() : shortPayTenor;

    registerWith(longIndex_);
    registerWith(shortIndex_);
    registerWith(discountHandle_);
    initializeDates();
}

void TenorBasisSwapHelper::initializeDates() {

    Calendar spotCalendar = longIndex_->fixingCalendar();
    Natural spotDays = longIndex_->fixingDays();

    Date valuationDate = Settings::instance().evaluationDate();
    // if the evaluation date is not a business day
    // then move to the next business day
    valuationDate = spotCalendar.adjust(valuationDate);

    Date effectiveDate = spotCalendar.advance(valuationDate, spotDays * Days);

    swap_ = boost::shared_ptr<TenorBasisSwap>(new TenorBasisSwap(effectiveDate, 1.0, swapTenor_, true, longIndex_, 0.0,
                                                                 shortIndex_, 0.0, shortPayTenor_,
                                                                 DateGeneration::Backward, includeSpread_, type_));

    boost::shared_ptr<PricingEngine> engine(new DiscountingSwapEngine(discountRelinkableHandle_));
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

    boost::shared_ptr<FloatingRateCoupon> lastFloating = boost::dynamic_pointer_cast<FloatingRateCoupon>(
        termStructureHandle_ == shortIndex_->forwardingTermStructure() ? swap_->shortLeg().back()
                                                                       : swap_->longLeg().back());
#ifdef QL_USE_INDEXED_COUPON
    /* May need to adjust latestDate_ if you are projecting libor based
   on tenor length rather than from accrual date to accrual date. */
    Date fixingValueDate = shortIndex_->valueDate(lastFloating->fixingDate());
    Date endValueDate = shortIndex_->maturityDate(fixingValueDate);
    latestDate_ = std::max(latestDate_, endValueDate);
#else
    /* Subperiods coupons do not have a par approximation either... */
    if (boost::dynamic_pointer_cast<SubPeriodsCoupon>(lastFloating)) {
        Date fixingValueDate = shortIndex_->valueDate(lastFloating->fixingDate());
        Date endValueDate = shortIndex_->maturityDate(fixingValueDate);
        latestDate_ = std::max(latestDate_, endValueDate);
    }
#endif
}

void TenorBasisSwapHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;

    boost::shared_ptr<YieldTermStructure> temp(t, no_deletion);
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real TenorBasisSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "Termstructure not set");
    swap_->recalculate();
    return (spreadOnShort_ ? swap_->fairShortLegSpread() : swap_->fairLongLegSpread());
}

void TenorBasisSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<TenorBasisSwapHelper>* v1 = dynamic_cast<Visitor<TenorBasisSwapHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
