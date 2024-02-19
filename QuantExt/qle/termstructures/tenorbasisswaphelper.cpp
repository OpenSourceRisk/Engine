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

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/ibor/libor.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/utilities/null_deleter.hpp>

#include <qle/termstructures/tenorbasisswaphelper.hpp>

namespace QuantExt {

TenorBasisSwapHelper::TenorBasisSwapHelper(Handle<Quote> spread, const Period& swapTenor,
                                           const boost::shared_ptr<IborIndex> payIndex,
                                           const boost::shared_ptr<IborIndex> receiveIndex,
                                           const Handle<YieldTermStructure>& discountingCurve, bool spreadOnRec,
                                           bool includeSpread, const Period& payFrequency, const Period& recFrequency,
                                           const bool telescopicValueDates, QuantExt::SubPeriodsCoupon1::Type type)
    : RelativeDateRateHelper(spread), swapTenor_(swapTenor), payIndex_(payIndex), receiveIndex_(receiveIndex),
      spreadOnRec_(spreadOnRec), includeSpread_(includeSpread), payFrequency_(payFrequency),
      recFrequency_(recFrequency), telescopicValueDates_(telescopicValueDates), type_(type),
      discountHandle_(discountingCurve) {

       /* depending on the given curves we proceed as outlined in the following table

       x = curve is given
       . = curve is missing

       Case | OI1 | OI2  | Discount | Action
       =========================================
         0  |  .  |   .  |    .     | throw exception
         1  |  .  |   .  |    x     | throw exception
         2  |  .  |   x  |    .     | imply OI1, set Discount is OI2
         3  |  .  |   x  |    x     | imply OI1
         4  |  x  |   .  |    .     | imply OI2, set Discount is OI1
         5  |  x  |   .  |    x     | imply OI2
         6  |  x  |   x  |    .     | imply Discount
         7  |  x  |   x  |    x     | throw exception

    */

    bool payGiven = !payIndex_->forwardingTermStructure().empty();
    bool recGiven = !receiveIndex_->forwardingTermStructure().empty();
    bool discountGiven = !discountHandle_.empty();

    if (!payGiven && !recGiven && !discountGiven) {
        // case 0
        QL_FAIL("no curve given");
    } else if (!payGiven && !recGiven && discountGiven) {
        // case 1
        QL_FAIL("neither Index nor Discount curve is given");
    } else if (!payGiven && recGiven && !discountGiven) {
        // case 2
        payIndex_ = boost::static_pointer_cast<IborIndex>(payIndex_->clone(termStructureHandle_));
        payIndex_->unregisterWith(termStructureHandle_);
        //discountRelinkableHandle_.linkTo(*termStructureHandle_, false);
        discountRelinkableHandle_.linkTo(*receiveIndex_->forwardingTermStructure());
    } else if (!payGiven && recGiven && discountGiven) {
        // case 3
        payIndex_ = boost::static_pointer_cast<IborIndex>(payIndex_->clone(termStructureHandle_));
        payIndex_->unregisterWith(termStructureHandle_);
    } else if (payGiven && !recGiven && !discountGiven) {
        // case 4
        receiveIndex_ = boost::static_pointer_cast<IborIndex>(receiveIndex_->clone(termStructureHandle_));
        receiveIndex_->unregisterWith(termStructureHandle_);
        discountRelinkableHandle_.linkTo(*payIndex_->forwardingTermStructure());
    } else if (payGiven && !recGiven && discountGiven) {
        // case 5
        receiveIndex_ = boost::static_pointer_cast<IborIndex>(receiveIndex_->clone(termStructureHandle_));
        receiveIndex_->unregisterWith(termStructureHandle_);
    } else if (payGiven && recGiven && !discountGiven) {
        // case 6
        //TODO this case won't work ... "empty Handle cannot be dereferenced"
        //discountRelinkableHandle_.linkTo(*termStructureHandle_, false);
    } else if (payGiven && recGiven && discountGiven) {
        // case 7
        QL_FAIL("Both Index and the Discount curves are all given");
    }

    payFrequency_ = payFrequency == Period() ? payIndex_->tenor() : payFrequency;
    recFrequency_ = recFrequency == Period() ? receiveIndex_->tenor() : recFrequency;

    registerWith(payIndex_);
    registerWith(receiveIndex_);
    registerWith(discountHandle_);
    initializeDates();
}

void TenorBasisSwapHelper::initializeDates() {

    //CHECK : should the spot shift be based on pay ore receive, here we have the pay leg... 
    boost::shared_ptr<Libor> payIndexAsLibor = boost::dynamic_pointer_cast<Libor>(payIndex_);
    Calendar spotCalendar = payIndexAsLibor != NULL ? payIndexAsLibor->jointCalendar() : payIndex_->fixingCalendar();
    Natural spotDays = payIndex_->fixingDays();

    Date valuationDate = Settings::instance().evaluationDate();
    // if the evaluation date is not a business day
    // then move to the next business day
    valuationDate = spotCalendar.adjust(valuationDate);

    Date effectiveDate = spotCalendar.advance(valuationDate, spotDays * Days);

    swap_ = boost::shared_ptr<TenorBasisSwap>(new TenorBasisSwap(
        effectiveDate, 1.0, swapTenor_, payIndex_, 0.0, payFrequency_, receiveIndex_, 0.0, recFrequency_,
        DateGeneration::Backward, includeSpread_, spreadOnRec_, type_, telescopicValueDates_));

    boost::shared_ptr<PricingEngine> engine(new DiscountingSwapEngine(discountRelinkableHandle_));
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

    boost::shared_ptr<FloatingRateCoupon> lastFloating = boost::dynamic_pointer_cast<FloatingRateCoupon>(
        termStructureHandle_ == receiveIndex_->forwardingTermStructure() ? swap_->recLeg().back()
                                                                       : swap_->payLeg().back());
    if (IborCoupon::Settings::instance().usingAtParCoupons()) {
        /* Subperiods coupons do not have a par approximation either... */
        if (boost::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(lastFloating)) {
            Date fixingValueDate = receiveIndex_->valueDate(lastFloating->fixingDate());
            Date endValueDate = receiveIndex_->maturityDate(fixingValueDate);
            latestDate_ = std::max(latestDate_, endValueDate);
        }
    } else {
        /* May need to adjust latestDate_ if you are projecting libor based
        on tenor length rather than from accrual date to accrual date. */
        Date fixingValueDate = receiveIndex_->valueDate(lastFloating->fixingDate());
        Date endValueDate = receiveIndex_->maturityDate(fixingValueDate);
        latestDate_ = std::max(latestDate_, endValueDate); 
    }
}

void TenorBasisSwapHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;

    boost::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real TenorBasisSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "Termstructure not set");
    // we didn't register as observers - force calculation
    swap_->deepUpdate();
    return (spreadOnRec_ ? swap_->fairRecLegSpread() : swap_->fairPayLegSpread());
}

void TenorBasisSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<TenorBasisSwapHelper>* v1 = dynamic_cast<Visitor<TenorBasisSwapHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
