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

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/termstructures/oibasisswaphelper.hpp>

using boost::shared_ptr;

using namespace QuantLib;

namespace QuantExt {

OIBSHelper::OIBSHelper(Natural settlementDays,
                       const Period& tenor, // swap maturity
                       const Handle<Quote>& oisSpread, const boost::shared_ptr<OvernightIndex>& overnightIndex,
                       const boost::shared_ptr<IborIndex>& iborIndex, const Handle<YieldTermStructure>& discount,
                       const bool telescopicValueDates)
    : RelativeDateRateHelper(oisSpread), settlementDays_(settlementDays), tenor_(tenor),
      overnightIndex_(overnightIndex), iborIndex_(iborIndex), discount_(discount),
      telescopicValueDates_(telescopicValueDates) {

    /* depending on the given curves we proceed as outlined in the following table

       x = curve is given
       . = curve is missing

       Case | OIS | Ibor | Discount | Action
       =========================================
         0  |  .  |   .  |    .     | throw exception
         1  |  .  |   .  |    x     | throw exception
         2  |  .  |   x  |    .     | imply OIS = Discount
         3  |  .  |   x  |    x     | imply OIS
         4  |  x  |   .  |    .     | imply Ibor, set Discount = OIS
         5  |  x  |   .  |    x     | imply Ibor
         6  |  x  |   x  |    .     | imply Discount
         7  |  x  |   x  |    x     | throw exception

    */

    bool oisGiven = !overnightIndex_->forwardingTermStructure().empty();
    bool iborGiven = !iborIndex_->forwardingTermStructure().empty();
    bool discountGiven = !discount_.empty();

    if (!oisGiven && !iborGiven && !discountGiven) {
        // case 0
        QL_FAIL("no curve given");
    } else if (!oisGiven && !iborGiven && discountGiven) {
        // case 1
        QL_FAIL("neither OIS nor Ibor curve is given");
    } else if (!oisGiven && iborGiven && !discountGiven) {
        // case 2
        overnightIndex_ = boost::static_pointer_cast<OvernightIndex>(overnightIndex_->clone(termStructureHandle_));
        overnightIndex_->unregisterWith(termStructureHandle_);
        discountRelinkableHandle_.linkTo(*termStructureHandle_, false);
    } else if (!oisGiven && iborGiven && discountGiven) {
        // case 3
        overnightIndex_ = boost::static_pointer_cast<OvernightIndex>(overnightIndex_->clone(termStructureHandle_));
        overnightIndex_->unregisterWith(termStructureHandle_);
    } else if (oisGiven && !iborGiven && !discountGiven) {
        // case 4
        iborIndex_ = iborIndex_->clone(termStructureHandle_);
        iborIndex_->unregisterWith(termStructureHandle_);
        discountRelinkableHandle_.linkTo(*overnightIndex_->forwardingTermStructure());
    } else if (oisGiven && !iborGiven && discountGiven) {
        // case 5
        iborIndex_ = iborIndex_->clone(termStructureHandle_);
        iborIndex_->unregisterWith(termStructureHandle_);
    } else if (oisGiven && iborGiven && !discountGiven) {
        // case 6
        discountRelinkableHandle_.linkTo(*termStructureHandle_, false);
    } else if (oisGiven && iborGiven && discountGiven) {
        // case 7
        QL_FAIL("OIS, Ibor and Discount curves are all given");
    }

    registerWith(overnightIndex_);
    registerWith(iborIndex_);
    registerWith(discount_);
    initializeDates();
}

void OIBSHelper::initializeDates() {

    Date asof = Settings::instance().evaluationDate();
    // if the evaluation date is not a business day
    // then move to the next business day
    asof = iborIndex_->fixingCalendar().adjust(asof);

    Date settlementDate = iborIndex_->fixingCalendar().advance(asof, settlementDays_, Days);
    Schedule oisSchedule = MakeSchedule()
                               .from(settlementDate)
                               .to(settlementDate + tenor_)
                               .withTenor(1 * Years)
                               .withCalendar(overnightIndex_->fixingCalendar())
                               .withConvention(overnightIndex_->businessDayConvention())
                               .forwards();
    Schedule iborSchedule = MakeSchedule()
                                .from(settlementDate)
                                .to(settlementDate + tenor_)
                                .withTenor(iborIndex_->tenor())
                                .withCalendar(iborIndex_->fixingCalendar())
                                .withConvention(iborIndex_->businessDayConvention())
                                .forwards();
    swap_ = boost::shared_ptr<OvernightIndexedBasisSwap>(
        new OvernightIndexedBasisSwap(OvernightIndexedBasisSwap::Payer,
                                      10000.0, // arbitrary
                                      oisSchedule, overnightIndex_, iborSchedule, iborIndex_, 0.0, 0.0, true));
    boost::shared_ptr<PricingEngine> engine(
        new DiscountingSwapEngine(discount_.empty() ? discountRelinkableHandle_ : discount_));
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
}

void OIBSHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer -
    // force recalculation when needed
    termStructureHandle_.linkTo(boost::shared_ptr<YieldTermStructure>(t, null_deleter()), false);
    RelativeDateRateHelper::setTermStructure(t);
}

Real OIBSHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    // we didn't register as observers - force calculation
    swap_->deepUpdate();
    return swap_->fairOvernightSpread();
}

void OIBSHelper::accept(AcyclicVisitor& v) {
    Visitor<OIBSHelper>* v1 = dynamic_cast<Visitor<OIBSHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
