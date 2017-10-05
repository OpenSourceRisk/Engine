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
#include <qle/termstructures/oibasisswaphelper.hpp>

using boost::shared_ptr;

using namespace QuantLib;

namespace QuantExt {

namespace {
void no_deletion(YieldTermStructure*) {}
} // namespace

OIBSHelper::OIBSHelper(Natural settlementDays,
                       const Period& tenor, // swap maturity
                       const Handle<Quote>& oisSpread, const boost::shared_ptr<OvernightIndex>& overnightIndex,
                       const boost::shared_ptr<IborIndex>& iborIndex, const Handle<YieldTermStructure>& discount)
    : RelativeDateRateHelper(oisSpread), settlementDays_(settlementDays), tenor_(tenor),
      overnightIndex_(overnightIndex), iborIndex_(iborIndex), discount_(discount) {
    registerWith(overnightIndex_);
    registerWith(iborIndex_);
    if (!discount_.empty())
        registerWith(discount_);
    initializeDates();
}

void OIBSHelper::initializeDates() {

    boost::shared_ptr<IborIndex> clonedIborIndex = iborIndex_->clone(termStructureHandle_);
    // avoid notifications
    iborIndex_->unregisterWith(termStructureHandle_);

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
    swap_ = boost::shared_ptr<OvernightIndexedBasisSwap>(new OvernightIndexedBasisSwap(OvernightIndexedBasisSwap::Payer,
                                                                                       10000.0, // arbitrary
                                                                                       oisSchedule, overnightIndex_,
                                                                                       iborSchedule, clonedIborIndex));
    boost::shared_ptr<PricingEngine> engine(
        new DiscountingSwapEngine(discount_.empty() ? overnightIndex_->forwardingTermStructure() : discount_));
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
}

void OIBSHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer -
    // force recalculation when needed
    termStructureHandle_.linkTo(shared_ptr<YieldTermStructure>(t, no_deletion), false);
    RelativeDateRateHelper::setTermStructure(t);
}

Real OIBSHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    // we didn't register as observers - force calculation
    swap_->recalculate();
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
