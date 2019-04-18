/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qle/termstructures/brlcdiratehelper.hpp>
#include <qle/instruments/brlcdiswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

using namespace QuantLib;

namespace QuantExt {

BRLCdiRateHelper::BRLCdiRateHelper(const Period& swapTenor, const Handle<Quote>& fixedRate, 
    const boost::shared_ptr<BRLCdi>& brlCdiIndex, const Handle<YieldTermStructure>& discountingCurve,
    bool telescopicValueDates)
    : OISRateHelper(2, swapTenor, fixedRate, brlCdiIndex, brlCdiIndex->dayCounter(), 0, false, Once, 
      ModifiedFollowing, ModifiedFollowing, DateGeneration::Backward, discountingCurve, telescopicValueDates) {

    // Need to do this again
    if (brlCdiIndex->forwardingTermStructure().empty()) {
        boost::shared_ptr<IborIndex> clonedIborIndex = brlCdiIndex->clone(termStructureHandle_);
        overnightIndex_ = boost::dynamic_pointer_cast<BRLCdi>(clonedIborIndex);
        overnightIndex_->unregisterWith(termStructureHandle_);
    } else {
        overnightIndex_ = brlCdiIndex;
    }

    // Call BRLCdiRateHelper::initializeDates to override work done in OISRateHelper::initializeDates
    initializeDates();
}


void BRLCdiRateHelper::initializeDates() {

    // Use the overnight index's calendar for all business day adjustments
    Calendar calendar = overnightIndex_->fixingCalendar();

    // Adjust reference date to next good business day if necessary
    Date referenceDate = Settings::instance().evaluationDate();
    referenceDate = calendar.adjust(referenceDate);

    // Determine the start date
    Date startDate = calendar.advance(referenceDate, settlementDays_ * Days);
    startDate = calendar.adjust(startDate, Following);

    // Determine the end date
    Date endDate = startDate + swapTenor_;

    // We know this is the case by construction but check it in any case
    boost::shared_ptr<BRLCdi> brlCdiIndex = boost::dynamic_pointer_cast<BRLCdi>(overnightIndex_);
    QL_REQUIRE(brlCdiIndex, "BRLCdiRateHelper expected a BRLCdi overnight index");

    // Create the BRL CDI swap
    swap_ = boost::make_shared<BRLCdiSwap>(OvernightIndexedSwap::Payer, 1.0, startDate, 
        endDate, 0.01, brlCdiIndex, 0.0, telescopicValueDates_);

    // Set the pricing engine
    swap_->setPricingEngine(boost::make_shared<DiscountingSwapEngine>(discountRelinkableHandle_));

    // Update earliest and latest dates
    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
    if (paymentLag_ != 0) {
        Date date = calendar.advance(latestDate_, paymentLag_, Days, paymentAdjustment_, false);
        latestDate_ = std::max(date, latestDate_);
    }
}

void BRLCdiRateHelper::accept(AcyclicVisitor& v) {
    if (Visitor<BRLCdiRateHelper>* v1 = dynamic_cast<Visitor<BRLCdiRateHelper>*>(&v))
        v1->visit(*this);
    else
        OISRateHelper::accept(v);
}


DatedBRLCdiRateHelper::DatedBRLCdiRateHelper(const Date& startDate, const Date& endDate, 
    const Handle<Quote>& fixedRate, const boost::shared_ptr<BRLCdi>& brlCdiIndex, 
    const Handle<YieldTermStructure>& discountingCurve, bool telescopicValueDates) 
    : DatedOISRateHelper(startDate, endDate, fixedRate, brlCdiIndex, brlCdiIndex->dayCounter(), 0, 
      Once, ModifiedFollowing, ModifiedFollowing, DateGeneration::Backward, discountingCurve, telescopicValueDates) {

    // Need to do this again
    if (brlCdiIndex->forwardingTermStructure().empty()) {
        boost::shared_ptr<IborIndex> clonedIborIndex = brlCdiIndex->clone(termStructureHandle_);
        overnightIndex_ = boost::dynamic_pointer_cast<BRLCdi>(clonedIborIndex);
        overnightIndex_->unregisterWith(termStructureHandle_);
    } else {
        overnightIndex_ = brlCdiIndex;
    }

    // Check is redundant here
    boost::shared_ptr<BRLCdi> brlCdiIndexTmp = boost::dynamic_pointer_cast<BRLCdi>(overnightIndex_);
    QL_REQUIRE(brlCdiIndex, "BRLCdiRateHelper expected a BRLCdi overnight index");

    // Reset the swap_ member to a BRL CDI swap
    swap_ = boost::make_shared<BRLCdiSwap>(OvernightIndexedSwap::Payer, 1.0, startDate,
        endDate, 0.01, brlCdiIndexTmp, 0.0, telescopicValueDates_);

    // Set the pricing engine
    swap_->setPricingEngine(boost::make_shared<DiscountingSwapEngine>(discountRelinkableHandle_));

    // Update earliest and latest dates
    Calendar calendar = brlCdiIndex->fixingCalendar();
    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();
    if (paymentLag_ != 0) {
        Date date = calendar.advance(latestDate_, paymentLag_, Days, paymentAdjustment_, false);
        latestDate_ = std::max(date, latestDate_);
    }
}

void DatedBRLCdiRateHelper::accept(AcyclicVisitor& v) {
    if (Visitor<DatedBRLCdiRateHelper>* v1 = dynamic_cast<Visitor<DatedBRLCdiRateHelper>*>(&v))
        v1->visit(*this);
    else
        DatedOISRateHelper::accept(v);
}

}
