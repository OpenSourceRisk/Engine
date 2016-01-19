/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file basisswaphelper.cpp
    \brief Basis Swap helpers
*/

#include <qle/termstructures/basisswaphelper.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/currencies/europe.hpp>
#include <iostream>

using boost::shared_ptr;
using namespace QuantLib;

namespace QuantExt {

    namespace {
        void no_deletion(YieldTermStructure*) {}
    }

    BasisSwapHelper::BasisSwapHelper(
                   Natural settlementDays,
                   const Period& term, // swap maturity
                   const boost::shared_ptr<QuantLib::IborIndex>& impliedIndex,
                   const boost::shared_ptr<QuantLib::IborIndex>& fixedIndex,
                   const Handle<Quote>& spreadQuote,
                   bool spreadQuoteOnPayLeg,
                   const Handle<YieldTermStructure>& fixedDiscountCurve)
        : RelativeDateRateHelper(spreadQuote),
          settlementDays_(settlementDays), 
          term_(term),
          impliedIndex_(impliedIndex), 
          fixedIndex_(fixedIndex), 
          spreadQuoteOnPayLeg_(spreadQuoteOnPayLeg),
          fixedDiscountCurve_(fixedDiscountCurve) {
        
        registerWith(impliedIndex_);
        registerWith(fixedIndex_);
        registerWith(fixedDiscountCurve_);
        initializeDates();
    }

    void BasisSwapHelper::initializeDates() {

        boost::shared_ptr<QuantLib::IborIndex> clonedIborIndex = impliedIndex_->clone(termStructureHandle_);
        // avoid notifications
        impliedIndex_->unregisterWith(termStructureHandle_);

        Date asof = Settings::instance().evaluationDate();
        Date settlementDate = impliedIndex_->fixingCalendar().advance(asof, settlementDays_, Days); 
        Date maturityDate = impliedIndex_->fixingCalendar().advance(settlementDate, term_);
        Schedule paySchedule = MakeSchedule()
            .from(settlementDate)
            .to(maturityDate)
            .withTenor(impliedIndex_->tenor());
        Schedule recSchedule = MakeSchedule()
            .from(settlementDate)
            .to(maturityDate)
            .withTenor(fixedIndex_->tenor());

        swap_ = boost::shared_ptr<BasisSwap>
            (new QuantExt::BasisSwap(QuantExt::BasisSwap::Payer,  // arbitrary
                           10000.0, // arbitrary 
                           paySchedule,
                           clonedIborIndex,
                           0.0,     // arbitrary
                           impliedIndex_->dayCounter(),
                           recSchedule,
                           fixedIndex_,
                           0.0,     // arbitrary
                           fixedIndex_->dayCounter())); 
        swap_->setPricingEngine(boost::shared_ptr<PricingEngine>(new DiscountingSwapEngine(fixedDiscountCurve_)));

        earliestDate_ = swap_->startDate();
        latestDate_ = swap_->maturityDate();
    }

    void BasisSwapHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed
        termStructureHandle_.linkTo(
                         shared_ptr<YieldTermStructure>(t,no_deletion),
                         false);
        RelativeDateRateHelper::setTermStructure(t);
    }

    Real BasisSwapHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != 0, "term structure not set");
        // we didn't register as observers - force calculation
        swap_->recalculate();
        if (spreadQuoteOnPayLeg_)
            return swap_->fairPaySpread();
        else
            return swap_->fairRecSpread();
    }

    void BasisSwapHelper::accept(AcyclicVisitor& v) {
        Visitor<BasisSwapHelper>* v1 =
            dynamic_cast<Visitor<BasisSwapHelper>*>(&v);
        if (v1 != 0)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

}
