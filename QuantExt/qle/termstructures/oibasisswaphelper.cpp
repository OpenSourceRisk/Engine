/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/termstructures/oibasisswaphelper.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

using boost::shared_ptr;

namespace QuantLib {

    namespace {
        void no_deletion(YieldTermStructure*) {}
    }

    OIBSHelper::OIBSHelper(
                   Natural settlementDays,
                   const Period& tenor, // swap maturity
                   const Handle<Quote>& oisSpread,
                   const boost::shared_ptr<OvernightIndex>& overnightIndex,
                   const boost::shared_ptr<IborIndex>& iborIndex)
        : RelativeDateRateHelper(oisSpread),
          settlementDays_(settlementDays), 
          tenor_(tenor),
          overnightIndex_(overnightIndex), 
          iborIndex_(iborIndex) {
        registerWith(overnightIndex_);
        registerWith(iborIndex_);
        initializeDates();
    }

    void OIBSHelper::initializeDates() {

        boost::shared_ptr<IborIndex> clonedIborIndex =
            iborIndex_->clone(termStructureHandle_);
        // avoid notifications
        iborIndex_->unregisterWith(termStructureHandle_);

        Date asof = Settings::instance().evaluationDate();
        Date settlementDate = iborIndex_->fixingCalendar().advance(asof, settlementDays_, Days); 
        Schedule oisSchedule = MakeSchedule()
            .from(settlementDate)
            .to(settlementDate + tenor_)
            .withTenor(1*Years)
            .forwards();
        Schedule iborSchedule = MakeSchedule()
            .from(settlementDate)
            .to(settlementDate + tenor_)
            .withTenor(iborIndex_->tenor())
            .forwards();
        swap_ = boost::shared_ptr<OvernightIndexedBasisSwap>
            (new OvernightIndexedBasisSwap(
                        OvernightIndexedBasisSwap::Payer,
                        10000.0, // arbitrary
                        oisSchedule,
                        overnightIndex_,
                        iborSchedule,
                        clonedIborIndex));
        boost::shared_ptr<PricingEngine> engine(new DiscountingSwapEngine(overnightIndex_->forwardingTermStructure()));
        swap_->setPricingEngine(engine);

        earliestDate_ = swap_->startDate();
        latestDate_ = swap_->maturityDate();
    }

    void OIBSHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed
        termStructureHandle_.linkTo(
                         shared_ptr<YieldTermStructure>(t,no_deletion),
                         false);
        RelativeDateRateHelper::setTermStructure(t);
    }

    Real OIBSHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != 0, "term structure not set");
        // we didn't register as observers - force calculation
        swap_->recalculate();
        return swap_->fairOvernightSpread();
    }

    void OIBSHelper::accept(AcyclicVisitor& v) {
        Visitor<OIBSHelper>* v1 =
            dynamic_cast<Visitor<OIBSHelper>*>(&v);
        if (v1 != 0)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

}
