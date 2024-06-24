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
                                           const QuantLib::ext::shared_ptr<IborIndex> payIndex,
                                           const QuantLib::ext::shared_ptr<IborIndex> receiveIndex,
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

       Case | PAY | REC  | Discount | Action
       =========================================
         0  |  .  |   .  |    .     | throw exception
         1  |  .  |   .  |    x     | throw exception
         2  |  .  |   x  |    .     | imply PAY = Discount
         3  |  .  |   x  |    x     | imply PAY
         4  |  x  |   .  |    .     | imply REC = Discount
         5  |  x  |   .  |    x     | imply REC
         6  |  x  |   x  |    .     | imply Discount
         7  |  x  |   x  |    x     | throw exception

        Overnight(ON) vs. IBOR CASE:
        Case 2 from above:
            if REC (given) is ON, REC = Discount = OIS, imply PAY only
            else : PAY (missing) is ON, imply PAY = Discount = OIS (as before)

        Case 4 from above:
            if PAY (given) is ON, PAY = Discount = OIS , imply REC only
            else : REC (missing) is ON, then imply REC = Discount = OIS (as before)

        */

       automaticDiscountRelinkableHandle_ = false;

       bool payGiven = !payIndex_->forwardingTermStructure().empty();
       bool recGiven = !receiveIndex_->forwardingTermStructure().empty();
       bool discountGiven = !discountHandle_.empty();

       QuantLib::ext::shared_ptr<OvernightIndex> payIndexON = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(payIndex_);
       QuantLib::ext::shared_ptr<OvernightIndex> recIndexON = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(receiveIndex_);

       if(discountGiven)
           discountRelinkableHandle_.linkTo(*discountHandle_);

       if (!payGiven && !recGiven && !discountGiven) {
           // case 0
           QL_FAIL("no curve given");
       } else if (!payGiven && !recGiven && discountGiven) {
           // case 1
           QL_FAIL("no index curve given");
       } else if (!payGiven && recGiven && !discountGiven) {
           // case 2
           payIndex_ = QuantLib::ext::static_pointer_cast<IborIndex>(payIndex_->clone(termStructureHandle_));
           payIndex_->unregisterWith(termStructureHandle_);
           if (!payIndexON && recIndexON) {
               discountRelinkableHandle_.linkTo(*receiveIndex_->forwardingTermStructure());
           } else {
               automaticDiscountRelinkableHandle_ = true;
           }
       } else if (!payGiven && recGiven && discountGiven) {
           // case 3
           payIndex_ = QuantLib::ext::static_pointer_cast<IborIndex>(payIndex_->clone(termStructureHandle_));
           payIndex_->unregisterWith(termStructureHandle_);
       } else if (payGiven && !recGiven && !discountGiven) {
           // case 4
           receiveIndex_ = QuantLib::ext::static_pointer_cast<IborIndex>(receiveIndex_->clone(termStructureHandle_));
           receiveIndex_->unregisterWith(termStructureHandle_);
           if (payIndexON && !recIndexON) {
               discountRelinkableHandle_.linkTo(*payIndex_->forwardingTermStructure());
           } else {
               automaticDiscountRelinkableHandle_ = true;
           }
       } else if (payGiven && !recGiven && discountGiven) {
           // case 5
           receiveIndex_ = QuantLib::ext::static_pointer_cast<IborIndex>(receiveIndex_->clone(termStructureHandle_));
           receiveIndex_->unregisterWith(termStructureHandle_);
       } else if (payGiven && recGiven && !discountGiven) {
           // case 6
           automaticDiscountRelinkableHandle_ = true;
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

    QuantLib::ext::shared_ptr<Libor> payIndexAsLibor = QuantLib::ext::dynamic_pointer_cast<Libor>(payIndex_);
    Calendar spotCalendar = payIndexAsLibor != NULL ? payIndexAsLibor->jointCalendar() : payIndex_->fixingCalendar();
    Natural spotDays = payIndex_->fixingDays();

    Date valuationDate = Settings::instance().evaluationDate();
    // if the evaluation date is not a business day
    // then move to the next business day
    valuationDate = spotCalendar.adjust(valuationDate);

    Date effectiveDate = spotCalendar.advance(valuationDate, spotDays * Days);

    swap_ = QuantLib::ext::make_shared<TenorBasisSwap>(effectiveDate, 1.0, swapTenor_, payIndex_, 0.0, payFrequency_,
                                                       receiveIndex_, 0.0, recFrequency_, DateGeneration::Backward,
                                                       includeSpread_, spreadOnRec_, type_, telescopicValueDates_);

    auto engine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountRelinkableHandle_);
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

    QuantLib::ext::shared_ptr<FloatingRateCoupon> lastFloating = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(
        termStructureHandle_ == receiveIndex_->forwardingTermStructure() ? swap_->recLeg().back()
                                                                       : swap_->payLeg().back());
    if (IborCoupon::Settings::instance().usingAtParCoupons()) {
        /* Subperiods coupons do not have a par approximation either... */
        if (QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(lastFloating)) {
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

    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    if (automaticDiscountRelinkableHandle_)
        discountRelinkableHandle_.linkTo(temp, observer);

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
