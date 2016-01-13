/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/termstructures/fxforwardhelper.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>

using namespace QuantLib;

namespace QuantExt {

    namespace {
        void no_deletion(YieldTermStructure*) {}
    }

    FxForwardHelper::FxForwardHelper(Natural spotDays,
        const Period& forwardTenor,
        const Currency& sourceCurrency,
        const Currency& targetCurrency,
        const Handle<Quote>& forwardPoints,
        const Handle<Quote>& spotRate,
        Real pointsFactor,
        const Handle<YieldTermStructure>& knownDiscountCurve,
        const Currency& knownDiscountCurrency,
        const Calendar& advanceCal,
        bool spotRelative,
        const Calendar& additionalSettleCal)
    : RelativeDateRateHelper(forwardPoints),
      spotDays_(spotDays), 
      forwardTenor_(forwardTenor),
      sourceCurrency_(sourceCurrency),
      targetCurrency_(targetCurrency),
      spotRate_(spotRate),
      pointsFactor_(pointsFactor),
      knownDiscountCurve_(knownDiscountCurve),
      knownDiscountCurrency_(knownDiscountCurrency),
      advanceCal_(advanceCal),
      spotRelative_(spotRelative),
      additionalSettleCal_(additionalSettleCal),
      nominal_(Money(1.0, sourceCurrency_)) {
        
        QL_REQUIRE(knownDiscountCurrency_ == sourceCurrency_
            || knownDiscountCurrency_ == targetCurrency_,
            "Yield curve currency must equal one of the currencies"
            " in the currency pair.");

        QL_REQUIRE((forwardTenor_ != 0*Days && spotRelative_) || (forwardTenor_
            == spotDays_*Days && !spotRelative_), "Cannot have "
            "the forward date being equal to the spot date.");

        /* The forward rate value does not matter here. This quote is just
           used to create the FxForward object with the correct dates. */ 
        boost::shared_ptr<ExchangeRate> forwardRate(new ExchangeRate(
            sourceCurrency_, targetCurrency_, 1.0));
            //TODO: fix this
            /*
        fxForwardQuote_.reset(new FxForwardQuote(forwardRate, spotDays_,
            forwardTenor_, advanceCal_, spotRelative_, additionalSettleCal_));
            */

        registerWith(knownDiscountCurve_);
        registerWith(spotRate_);
        initializeDates();
    }

    void FxForwardHelper::initializeDates() {

        fxForward_.reset(new FxForward(nominal_, *fxForwardQuote_, true));
        if (knownDiscountCurrency_ == sourceCurrency_) {
            fxForward_->setPricingEngine(
                boost::shared_ptr<DiscountingFxForwardEngine>(new
                    DiscountingFxForwardEngine(sourceCurrency_,
                        knownDiscountCurve_,
                        targetCurrency_,
                        termStructureHandle_,
                        targetCurrency_,
                        spotRate_)));
        } else {
            fxForward_->setPricingEngine(
                boost::shared_ptr<DiscountingFxForwardEngine>(new
                    DiscountingFxForwardEngine(sourceCurrency_,
                        termStructureHandle_,
                        targetCurrency_,
                        knownDiscountCurve_,
                        targetCurrency_,
                        spotRate_)));
        }

        earliestDate_ = Settings::instance().evaluationDate();
        /* For ON and TN, this will give a date that is less than spot
           but that is what was intended. */
        latestDate_ = fxForward_->maturityDate();
    }

    void FxForwardHelper::setTermStructure(YieldTermStructure* t) {
        bool observer = false;
        boost::shared_ptr<YieldTermStructure> temp(t, no_deletion);
        termStructureHandle_.linkTo(temp, observer);
        RelativeDateRateHelper::setTermStructure(t);
    }

    Real FxForwardHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != 0, "term structure not set");
        fxForward_->recalculate();
        Decimal forwardRate = fxForward_->fairForwardRate().rate();
        Real spotRate = spotRate_->value();
        return (forwardRate - spotRate) * pointsFactor_;
    }

    void FxForwardHelper::accept(AcyclicVisitor& v) {
        Visitor<FxForwardHelper>* v1 =
            dynamic_cast<Visitor<FxForwardHelper>*>(&v);
        if (v1 != 0)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

}
