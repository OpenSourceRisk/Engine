/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#ifdef QL_USE_INDEXED_COUPON
    #include <ql/cashflows/floatingratecoupon.hpp>
#endif

#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>

namespace QuantExt {

    namespace {
        void no_deletion(YieldTermStructure*) {}
    }

    CrossCcyBasisSwapHelper::CrossCcyBasisSwapHelper(
        const Handle<Quote>& spreadQuote,
        const Handle<Quote>& spotFX,
        Natural settlementDays,
        const Calendar& settlementCalendar,
        const Period& swapTenor,
        BusinessDayConvention rollConvention,
        const boost::shared_ptr<QuantLib::IborIndex>& flatIndex,
        const boost::shared_ptr<QuantLib::IborIndex>& spreadIndex,
        const Handle<YieldTermStructure>& flatDiscountCurve,
        const Handle<YieldTermStructure>& spreadDiscountCurve,
        bool eom)
    : RelativeDateRateHelper(spreadQuote),
      spotFX_(spotFX),
      settlementDays_(settlementDays),
      settlementCalendar_(settlementCalendar),
      swapTenor_(swapTenor),
      rollConvention_(rollConvention),
      flatIndex_(flatIndex), 
      spreadIndex_(spreadIndex),
      flatDiscountCurve_(flatDiscountCurve), 
      spreadDiscountCurve_(spreadDiscountCurve),
      eom_(eom) {
        
        flatLegCurrency_ = flatIndex_->currency();
        spreadLegCurrency_ = spreadIndex_->currency();

        bool flatIndexHasCurve = !flatIndex_->
            forwardingTermStructure().empty();
        bool spreadIndexHasCurve = !spreadIndex_->
            forwardingTermStructure().empty();
        bool haveFlatDiscountCurve = !flatDiscountCurve_.empty();
        bool haveSpreadDiscountCurve = !spreadDiscountCurve_.empty();
        
        // Some initial checks.
        // TODO: remove?
        /*
        QL_REQUIRE(flatLegCurrency_ == spotFX_->source() || flatLegCurrency_
            == spotFX_->target(), "The flat leg currency must equal one of "
            "the currencies in the FX spot quote");

        QL_REQUIRE(spreadLegCurrency_ == spotFX_->source() || 
            spreadLegCurrency_ == spotFX_->target(), "The spread leg currency "
            "must equal one of the currencies in the FX spot quote");
            */

        QL_REQUIRE(!(flatIndexHasCurve && spreadIndexHasCurve &&
            haveFlatDiscountCurve && haveSpreadDiscountCurve), "Have all curves, "
            "nothing to solve for.");

        /* Link the curve being bootstrapped to the index if the index has
           no projection curve */
        if (flatIndexHasCurve && haveFlatDiscountCurve) {
            if (!spreadIndexHasCurve) {
                spreadIndex_ = spreadIndex_->clone(termStructureHandle_);
                spreadIndex_->unregisterWith(termStructureHandle_);
            }
        } else if (spreadIndexHasCurve && haveSpreadDiscountCurve) {
            if (!flatIndexHasCurve) {
                flatIndex_ = flatIndex_->clone(termStructureHandle_);
                flatIndex_->unregisterWith(termStructureHandle_);
            }
        } else {
            QL_FAIL("Need one leg of the cross currency basis swap to "
                "have all of its curves.");
        }

        registerWith(spotFX_);
        registerWith(flatIndex_);
        registerWith(spreadIndex_);
        registerWith(flatDiscountCurve_);
        registerWith(spreadDiscountCurve_);

        initializeDates();
    }

    void CrossCcyBasisSwapHelper::initializeDates() {

        Date settlementDate = settlementCalendar_.advance(evaluationDate_,
            settlementDays_, Days);
        Date maturityDate = settlementDate + swapTenor_;
        
        Period flatLegTenor = flatIndex_->tenor();
        Schedule flatLegSchedule = MakeSchedule()
            .from(settlementDate)
            .to(maturityDate)
            .withTenor(flatLegTenor)
            .withCalendar(settlementCalendar_)
            .withConvention(rollConvention_)
            .endOfMonth(eom_);

        Period spreadLegTenor = spreadIndex_->tenor();
        Schedule spreadLegSchedule = MakeSchedule()
            .from(settlementDate)
            .to(maturityDate)
            .withTenor(spreadLegTenor)
            .withCalendar(settlementCalendar_)
            .withConvention(rollConvention_)
            .endOfMonth(eom_);

        Real flatLegNominal = 1.0;
        Real spreadLegNominal = 1.0;
        bool spreadIsSource = true; // TODO:fix this! (spreadLegCurrency_ == spotFX_->source());
        if (spreadIsSource) {
            flatLegNominal = spotFX_->value();
        } else {
            spreadLegNominal = spotFX_->value();
        }

        /* Arbitrarily set the spread leg as the pay leg */
        swap_ = boost::shared_ptr<CrossCcyBasisSwap>(
            new CrossCcyBasisSwap(spreadLegNominal,
                spreadLegCurrency_,
                spreadLegSchedule,                  
                spreadIndex_, 0.0,
                flatLegNominal,
                flatLegCurrency_,
                flatLegSchedule,
                flatIndex_, 0.0));

        Handle<Quote> spotFX(*spotFX_);
        boost::shared_ptr<PricingEngine> engine;
        if (spreadIsSource) {
            engine.reset(new 
                CrossCcySwapEngine(spreadLegCurrency_,
                    spreadDiscountRLH_,
                    flatLegCurrency_,
                    flatDiscountRLH_,
                    spreadLegCurrency_,
                    spotFX,
                    boost::none,
                    settlementDate));
        } else {
            engine.reset(new 
                CrossCcySwapEngine(flatLegCurrency_,
                    flatDiscountRLH_,
                    spreadLegCurrency_,
                    spreadDiscountRLH_,
                    flatLegCurrency_,
                    spotFX,
                    boost::none,
                    settlementDate));
        }
        swap_->setPricingEngine(engine);

        earliestDate_ = swap_->startDate();
        latestDate_ = swap_->maturityDate();

        /* May need to adjust latestDate_ if you are projecting libor based
           on tenor length rather than from accrual date to accrual date. */
        #ifdef QL_USE_INDEXED_COUPON
        if (termStructureHandle_ == spreadIndex_->forwardingTermStructure()) {
            Size numCashflows = swap_->leg(0).size();
            if (numCashflows > 2) {
                boost::shared_ptr<FloatingRateCoupon> lastFloating =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(
                        swap_->leg(0)[numCashflows-2]);
                Date fixingValueDate =
                    spreadIndex_->valueDate(lastFloating->fixingDate());
                Date endValueDate = spreadIndex_->maturityDate(
                    fixingValueDate);
                latestDate_ = std::max(latestDate_, endValueDate);
            }
        }
        if (termStructureHandle_ == flatIndex_->forwardingTermStructure()) {
            Size numCashflows = swap_->leg(1).size();
            if (numCashflows > 2) {
                boost::shared_ptr<FloatingRateCoupon> lastFloating =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(
                        swap_->leg(1)[numCashflows-2]);
                Date fixingValueDate =
                    flatIndex_->valueDate(lastFloating->fixingDate());
                Date endValueDate = flatIndex_->maturityDate(fixingValueDate);
                latestDate_ = std::max(latestDate_, endValueDate);
            }
        }
        #endif
    }

    void CrossCcyBasisSwapHelper::setTermStructure(YieldTermStructure* t) {
        
        bool observer = false;
        boost::shared_ptr<YieldTermStructure> temp(t, no_deletion);

        termStructureHandle_.linkTo(temp, observer);

        if (flatDiscountCurve_.empty())
            flatDiscountRLH_.linkTo(temp, observer);
        else
            flatDiscountRLH_.linkTo(*flatDiscountCurve_, observer);

        if (spreadDiscountCurve_.empty())
            spreadDiscountRLH_.linkTo(temp, observer);
        else
            spreadDiscountRLH_.linkTo(*spreadDiscountCurve_, observer);

        RelativeDateRateHelper::setTermStructure(t);
    }

    Real CrossCcyBasisSwapHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_, "Term structure needs to be set");
        swap_->recalculate();
        return swap_->fairPaySpread();
    }

    void CrossCcyBasisSwapHelper::accept(AcyclicVisitor& v) {
        Visitor<CrossCcyBasisSwapHelper>* v1 =
            dynamic_cast<Visitor<CrossCcyBasisSwapHelper>*>(&v);
        if (v1)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

}
