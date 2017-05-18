/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
#include <qle/termstructures/subperiodsswaphelper.hpp>

namespace QuantExt {

namespace {
void no_deletion(YieldTermStructure*) {}
} // namespace

SubPeriodsSwapHelper::SubPeriodsSwapHelper(Handle<Quote> spread, const Period& swapTenor, const Period& fixedTenor,
                                           const Calendar& fixedCalendar, const DayCounter& fixedDayCount,
                                           BusinessDayConvention fixedConvention, const Period& floatPayTenor,
                                           const boost::shared_ptr<IborIndex>& iborIndex,
                                           const DayCounter& floatDayCount,
                                           const Handle<YieldTermStructure>& discountingCurve,
                                           SubPeriodsCoupon::Type type)
    : RelativeDateRateHelper(spread), iborIndex_(iborIndex), swapTenor_(swapTenor), fixedTenor_(fixedTenor),
      fixedCalendar_(fixedCalendar), fixedDayCount_(fixedDayCount), fixedConvention_(fixedConvention),
      floatPayTenor_(floatPayTenor), floatDayCount_(floatDayCount), type_(type), discountHandle_(discountingCurve) {

    iborIndex_ = iborIndex_->clone(termStructureHandle_);
    iborIndex_->unregisterWith(termStructureHandle_);

    registerWith(iborIndex_);
    registerWith(spread);
    registerWith(discountHandle_);

    initializeDates();
}

void SubPeriodsSwapHelper::initializeDates() {

    // build swap
    Date valuationDate = Settings::instance().evaluationDate();
    Calendar spotCalendar = iborIndex_->fixingCalendar();
    Natural spotDays = iborIndex_->fixingDays();
    // move val date forward in case it is a holiday
    valuationDate = spotCalendar.adjust(valuationDate);
    Date effectiveDate = spotCalendar.advance(valuationDate, spotDays * Days);

    swap_ = boost::shared_ptr<SubPeriodsSwap>(new SubPeriodsSwap(
        effectiveDate, 1.0, swapTenor_, true, fixedTenor_, 0.0, fixedCalendar_, fixedDayCount_, fixedConvention_,
        floatPayTenor_, iborIndex_, floatDayCount_, DateGeneration::Backward, type_));

    boost::shared_ptr<PricingEngine> engine(new DiscountingSwapEngine(discountRelinkableHandle_));
    swap_->setPricingEngine(engine);

    // set earliest and latest
    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

    boost::shared_ptr<FloatingRateCoupon> lastFloating =
        boost::dynamic_pointer_cast<FloatingRateCoupon>(swap_->floatLeg().back());
#ifdef QL_USE_INDEXED_COUPON
    /* May need to adjust latestDate_ if you are projecting libor based
    on tenor length rather than from accrual date to accrual date. */
    Date fixingValueDate = iborIndex_->valueDate(lastFloating->fixingDate());
    Date endValueDate = iborIndex_->maturityDate(fixingValueDate);
    latestDate_ = std::max(latestDate_, endValueDate);
#else
    /* Subperiods coupons do not have a par approximation either... */
    if (boost::dynamic_pointer_cast<SubPeriodsCoupon>(lastFloating)) {
        Date fixingValueDate = iborIndex_->valueDate(lastFloating->fixingDate());
        Date endValueDate = iborIndex_->maturityDate(fixingValueDate);
        latestDate_ = std::max(latestDate_, endValueDate);
    }
#endif
}

void SubPeriodsSwapHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;

    boost::shared_ptr<YieldTermStructure> temp(t, no_deletion);
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real SubPeriodsSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "Termstructure not set");
    swap_->recalculate();
    return swap_->fairRate();
}

void SubPeriodsSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<SubPeriodsSwapHelper>* v1 = dynamic_cast<Visitor<SubPeriodsSwapHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
