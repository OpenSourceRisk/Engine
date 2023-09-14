/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/termstructures/doubleoibasisswaphelper.hpp>

using boost::shared_ptr;

using namespace QuantLib;

namespace QuantExt {

DoubleOIBSHelper::DoubleOIBSHelper(Natural settlementDays,
    const Period& swapTenor, const Handle<Quote>& spread, const boost::shared_ptr<OvernightIndex>& payIndex, 
    const boost::shared_ptr<OvernightIndex>& recIndex, const Handle<YieldTermStructure>& discount, const bool spreadOnPayLeg, 
    const Period& shortPayTenor, const Period& longPayTenor, const bool telescopicValueDates)
    : RelativeDateRateHelper(spread), settlementDays_(settlementDays), swapTenor_(swapTenor), payIndex_(payIndex),
      recIndex_(recIndex), discount_(discount), spreadOnPayLeg_(spreadOnPayLeg), telescopicValueDates_(telescopicValueDates) {

    /* depending on the given curves we proceed as outlined in the following table

       x = curve is given
       . = curve is missing

       Case | OI1 | OI2  | Discount | Action
       =========================================
         0  |  .  |   .  |    .     | throw exception
         1  |  .  |   .  |    x     | throw exception
         2  |  .  |   x  |    .     | imply OI1 = Discount
         3  |  .  |   x  |    x     | imply OI1
         4  |  x  |   .  |    .     | imply OI2, set Discount is OI1
         5  |  x  |   .  |    x     | imply OI2
         6  |  x  |   x  |    .     | imply Discount
         7  |  x  |   x  |    x     | throw exception

    */
    
    bool payGiven = !payIndex_->forwardingTermStructure().empty();
    bool recGiven = !recIndex_->forwardingTermStructure().empty();
    bool discountGiven = !discount_.empty();
    shortPayTenor_ = shortPayTenor == Period() ? payIndex_->tenor() : shortPayTenor;
    longPayTenor_ = shortPayTenor == Period() ? recIndex_->tenor() : shortPayTenor;

    if (!payGiven && !recGiven && !discountGiven) {
        // case 0
        QL_FAIL("no curve given");
    } else if (!payGiven && !recGiven && discountGiven) {
        // case 1
        QL_FAIL("neither OIS nor Discount curve is given");
    } else if (!payGiven && recGiven && !discountGiven) {
        // case 2
        payIndex_ = boost::static_pointer_cast<OvernightIndex>(payIndex_->clone(termStructureHandle_));
        payIndex_->unregisterWith(termStructureHandle_);
        discountRelinkableHandle_.linkTo(*termStructureHandle_, false);
    } else if (!payGiven && recGiven && discountGiven) {
        // case 3
        payIndex_ = boost::static_pointer_cast<OvernightIndex>(payIndex_->clone(termStructureHandle_));
        payIndex_->unregisterWith(termStructureHandle_);
    } else if (payGiven && !recGiven && !discountGiven) {
        // case 4
        recIndex_ = boost::static_pointer_cast<OvernightIndex>(recIndex_->clone(termStructureHandle_));
        recIndex_->unregisterWith(termStructureHandle_);
        discountRelinkableHandle_.linkTo(*payIndex_->forwardingTermStructure());
    } else if (payGiven && !recGiven && discountGiven) {
        // case 5
        recIndex_ = boost::static_pointer_cast<OvernightIndex>(recIndex_->clone(termStructureHandle_));
        recIndex_->unregisterWith(termStructureHandle_);
    } else if (payGiven && recGiven && !discountGiven) {
        // case 6
        discountRelinkableHandle_.linkTo(*termStructureHandle_, false);
    } else if (payGiven && recGiven && discountGiven) {
        // case 7
        QL_FAIL("Both OI and the Discount curves are all given");
    }

    registerWith(payIndex_);
    registerWith(recIndex_);
    registerWith(discount_);
    initializeDates();
}

void DoubleOIBSHelper::initializeDates() {

    Date asof = Settings::instance().evaluationDate();
    // if the evaluation date is not a business day
    // then move to the next business day
    asof = recIndex_->fixingCalendar().adjust(asof);

    Date settlementDate = recIndex_->fixingCalendar().advance(asof, settlementDays_, Days);
    Schedule paySchedule = MakeSchedule()
                                .from(settlementDate)
                                .to(settlementDate + swapTenor_)
                                .withTenor(shortPayTenor_)
                                .withCalendar(payIndex_->fixingCalendar())
                                .withConvention(payIndex_->businessDayConvention())
                                .forwards();
    Schedule recSchedule = MakeSchedule()
                                .from(settlementDate)
                                .to(settlementDate + swapTenor_)
                                .withTenor(longPayTenor_)
                                .withCalendar(recIndex_->fixingCalendar())
                                .withConvention(recIndex_->businessDayConvention())
                                .forwards();
    swap_ = boost::shared_ptr<DoubleOvernightIndexedBasisSwap>(
        new DoubleOvernightIndexedBasisSwap(10000.0, // arbitrary
                                      paySchedule, payIndex_, recSchedule, recIndex_, 0.0, 0.0, true));
    boost::shared_ptr<PricingEngine> engine(
        new DiscountingSwapEngine(discount_.empty() ? discountRelinkableHandle_ : discount_));
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
}

void DoubleOIBSHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer -
    // force recalculation when needed
    termStructureHandle_.linkTo(shared_ptr<YieldTermStructure>(t, null_deleter()), false);
    RelativeDateRateHelper::setTermStructure(t);
}

Real DoubleOIBSHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    // we didn't register as observers - force calculation
    swap_->deepUpdate();
    if (spreadOnPayLeg_)
        return swap_->fairPaySpread();
    else
        return swap_->fairRecSpread();
}

void DoubleOIBSHelper::accept(AcyclicVisitor& v) {
    Visitor<DoubleOIBSHelper>* v1 = dynamic_cast<Visitor<DoubleOIBSHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
