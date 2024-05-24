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

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/instruments/brlcdiswap.hpp>
#include <qle/termstructures/brlcdiratehelper.hpp>

using namespace QuantLib;

namespace QuantExt {

BRLCdiRateHelper::BRLCdiRateHelper(const Period& swapTenor, const Handle<Quote>& fixedRate,
                                   const QuantLib::ext::shared_ptr<BRLCdi>& brlCdiIndex,
                                   const Handle<YieldTermStructure>& discountingCurve, bool telescopicValueDates)
    : RelativeDateRateHelper(fixedRate), swapTenor_(swapTenor), brlCdiIndex_(brlCdiIndex),
      telescopicValueDates_(telescopicValueDates), discountHandle_(discountingCurve) {

    bool onIndexHasCurve = !brlCdiIndex_->forwardingTermStructure().empty();
    bool haveDiscountCurve = !discountHandle_.empty();
    QL_REQUIRE(!(onIndexHasCurve && haveDiscountCurve), "Have both curves nothing to solve for.");

    if (!onIndexHasCurve) {
        QuantLib::ext::shared_ptr<IborIndex> clonedIborIndex(brlCdiIndex_->clone(termStructureHandle_));
        brlCdiIndex_ = QuantLib::ext::dynamic_pointer_cast<BRLCdi>(clonedIborIndex);
        brlCdiIndex_->unregisterWith(termStructureHandle_);
    }

    registerWith(brlCdiIndex_);
    registerWith(discountHandle_);
    initializeDates();
}

void BRLCdiRateHelper::initializeDates() {

    // Use the overnight index's calendar for all business day adjustments
    Calendar calendar = brlCdiIndex_->fixingCalendar();

    // Adjust reference date to next good business day if necessary
    Date referenceDate = Settings::instance().evaluationDate();
    referenceDate = calendar.adjust(referenceDate);

    // Determine the start date
    Date startDate = calendar.advance(referenceDate, 2 * Days);
    startDate = calendar.adjust(startDate, Following);

    // Determine the end date
    Date endDate = startDate + swapTenor_;

    // Create the BRL CDI swap
    swap_ = QuantLib::ext::make_shared<BRLCdiSwap>(OvernightIndexedSwap::Payer, 1.0, startDate, endDate, 0.01, brlCdiIndex_,
                                           0.0, telescopicValueDates_);

    // Set the pricing engine
    swap_->setPricingEngine(QuantLib::ext::make_shared<DiscountingSwapEngine>(discountRelinkableHandle_));

    // Update earliest and latest dates
    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
}

void BRLCdiRateHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;
    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real BRLCdiRateHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "BRLCdiRateHelper's term structure not set");
    swap_->deepUpdate();
    return swap_->fairRate();
}

void BRLCdiRateHelper::accept(AcyclicVisitor& v) {
    if (Visitor<BRLCdiRateHelper>* v1 = dynamic_cast<Visitor<BRLCdiRateHelper>*>(&v))
        v1->visit(*this);
    else
        RateHelper::accept(v);
}

DatedBRLCdiRateHelper::DatedBRLCdiRateHelper(const Date& startDate, const Date& endDate, const Handle<Quote>& fixedRate,
                                             const QuantLib::ext::shared_ptr<BRLCdi>& brlCdiIndex,
                                             const Handle<YieldTermStructure>& discountingCurve,
                                             bool telescopicValueDates)
    : RateHelper(fixedRate), brlCdiIndex_(brlCdiIndex), telescopicValueDates_(telescopicValueDates),
      discountHandle_(discountingCurve) {

    bool onIndexHasCurve = !brlCdiIndex_->forwardingTermStructure().empty();
    bool haveDiscountCurve = !discountHandle_.empty();
    QL_REQUIRE(!(onIndexHasCurve && haveDiscountCurve), "Have both curves nothing to solve for.");

    if (!onIndexHasCurve) {
        QuantLib::ext::shared_ptr<IborIndex> clonedIborIndex(brlCdiIndex_->clone(termStructureHandle_));
        brlCdiIndex_ = QuantLib::ext::dynamic_pointer_cast<BRLCdi>(clonedIborIndex);
        brlCdiIndex_->unregisterWith(termStructureHandle_);
    }

    registerWith(brlCdiIndex_);
    registerWith(discountHandle_);

    swap_ = QuantLib::ext::make_shared<BRLCdiSwap>(OvernightIndexedSwap::Payer, 1.0, startDate, endDate, 0.01, brlCdiIndex_,
                                           0.0, telescopicValueDates_);

    // Set the pricing engine
    swap_->setPricingEngine(QuantLib::ext::make_shared<DiscountingSwapEngine>(discountRelinkableHandle_));

    // Update earliest and latest dates
    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
}

void DatedBRLCdiRateHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;
    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RateHelper::setTermStructure(t);
}

Real DatedBRLCdiRateHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "DatedBRLCdiRateHelper's term structure not set");
    swap_->deepUpdate();
    return swap_->fairRate();
}

void DatedBRLCdiRateHelper::accept(AcyclicVisitor& v) {
    if (Visitor<DatedBRLCdiRateHelper>* v1 = dynamic_cast<Visitor<DatedBRLCdiRateHelper>*>(&v))
        v1->visit(*this);
    else
        RateHelper::accept(v);
}

} // namespace QuantExt
