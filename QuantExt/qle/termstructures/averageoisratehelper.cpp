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

#include <qle/instruments/makeaverageois.hpp>
#include <qle/termstructures/averageoisratehelper.hpp>

namespace QuantExt {

namespace {
void no_deletion(YieldTermStructure*) {}
} // namespace

AverageOISRateHelper::AverageOISRateHelper(const Handle<Quote>& fixedRate, const Period& spotLagTenor,
                                           const Period& swapTenor,
                                           // Fixed leg
                                           const Period& fixedTenor, const DayCounter& fixedDayCounter,
                                           const Calendar& fixedCalendar, BusinessDayConvention fixedConvention,
                                           BusinessDayConvention fixedPaymentAdjustment,
                                           // ON leg
                                           const boost::shared_ptr<OvernightIndex>& overnightIndex,
                                           const Period& onTenor, const Handle<Quote>& onSpread, Natural rateCutoff,
                                           // Exogenous discount curve
                                           const Handle<YieldTermStructure>& discountCurve)
    : RelativeDateRateHelper(fixedRate), spotLagTenor_(spotLagTenor), swapTenor_(swapTenor), fixedTenor_(fixedTenor),
      fixedDayCounter_(fixedDayCounter), fixedCalendar_(fixedCalendar), fixedConvention_(fixedConvention),
      fixedPaymentAdjustment_(fixedPaymentAdjustment), overnightIndex_(overnightIndex), onTenor_(onTenor),
      onSpread_(onSpread), rateCutoff_(rateCutoff), discountHandle_(discountCurve) {

    bool onIndexHasCurve = !overnightIndex_->forwardingTermStructure().empty();
    bool haveDiscountCurve = !discountHandle_.empty();
    QL_REQUIRE(!(onIndexHasCurve && haveDiscountCurve), "Have both curves nothing to solve for.");

    if (!onIndexHasCurve) {
        boost::shared_ptr<IborIndex> clonedIborIndex(overnightIndex_->clone(termStructureHandle_));
        overnightIndex_ = boost::dynamic_pointer_cast<OvernightIndex>(clonedIborIndex);
        overnightIndex_->unregisterWith(termStructureHandle_);
    }

    registerWith(overnightIndex_);
    registerWith(onSpread_);
    registerWith(discountHandle_);
    initializeDates();
}

void AverageOISRateHelper::initializeDates() {

    averageOIS_ =
        MakeAverageOIS(swapTenor_, overnightIndex_, onTenor_, 0.0, fixedTenor_, fixedDayCounter_, spotLagTenor_)
            .withFixedCalendar(fixedCalendar_)
            .withFixedConvention(fixedConvention_)
            .withFixedTerminationDateConvention(fixedConvention_)
            .withFixedPaymentAdjustment(fixedPaymentAdjustment_)
            .withRateCutoff(rateCutoff_)
            .withDiscountingTermStructure(discountRelinkableHandle_);

    earliestDate_ = averageOIS_->startDate();
    latestDate_ = averageOIS_->maturityDate();
}

Real AverageOISRateHelper::impliedQuote() const {

    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    averageOIS_->recalculate();

    // Calculate the fair fixed rate after accounting for the
    // spread in the spread quote. Recall, the spread quote was
    // intentionally not added to instrument averageOIS_.
    static const Spread basisPoint = 1.0e-4;
    Real onLegNPV = averageOIS_->overnightLegNPV();
    Spread onSpread = onSpread_.empty() ? 0.0 : onSpread_->value();
    Real spreadNPV = averageOIS_->overnightLegBPS() * onSpread / basisPoint;
    Real onLegNPVwithSpread = onLegNPV + spreadNPV;
    Real result = -onLegNPVwithSpread / (averageOIS_->fixedLegBPS() / basisPoint);
    return result;
}

void AverageOISRateHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;
    boost::shared_ptr<YieldTermStructure> temp(t, no_deletion);
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Spread AverageOISRateHelper::onSpread() const { return onSpread_.empty() ? 0.0 : onSpread_->value(); }

boost::shared_ptr<AverageOIS> AverageOISRateHelper::averageOIS() const { return averageOIS_; }

void AverageOISRateHelper::accept(AcyclicVisitor& v) {

    Visitor<AverageOISRateHelper>* v1 = dynamic_cast<Visitor<AverageOISRateHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
