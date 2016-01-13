/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/crossccyswapengine.hpp
    \brief Cross currency swap engine
*/

#ifndef quantext_cross_ccy_swap_engine_hpp
#define quantext_cross_ccy_swap_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {

    //! Cross currency swap engine

    /*! This class implements an engine for pricing swaps comprising legs that 
        invovlve two currencies.

        \ingroup engines
    */
    class CrossCcySwapEngine : public CrossCcySwap::engine {
      public:
        //! \name Constructors
        //@{
        /*! \param sourceCcy, sourceDiscountCurve
                   source currency and its discount curve.
            \param targetCcy, targetDiscountCurve
                   target currency and its discount curve.
            \param npvCcy
                   currency in which the NPV is calculated. Must be one of 
                   source currency or target currency.
            \param spotFX
                   The market spot rate quote. If not an FxQuote, the quote 
                   is interpreted as the number of units of target currency 
                   per unit of source currency. If it is an FxQuote, then the 
                   quote can have either of source or target as base.
                   In order to have the correct conventions, a 
                   properly formed FxQuote should be used.
            \param includeSettlementDateFlows, settlementDate
                   If includeSettlementDateFlows is true (false), cashflows 
				   on the settlementDate are (not) included in the NPV.
            \param npvDate
			       Discount to this date.
        */
        CrossCcySwapEngine(const Currency& sourceCcy,
            const Handle<YieldTermStructure>& sourceDiscountCurve,
			const Currency& targetCcy,
			const Handle<YieldTermStructure>& targetDiscountCurve,
            const Currency& npvCcy,
			const Handle<Quote>& spotFX,
            boost::optional<bool> includeSettlementDateFlows = boost::none, 
            const Date& settlementDate = Date(), 
            const Date& npvDate = Date());
		//@}

        //! \name PricingEngine interface
        //@{
        void calculate() const;
        //@}

        //! \name Inspectors
        //@{
        const Handle<YieldTermStructure>& sourceDiscountCurve() const {
            return sourceDiscountCurve_;
        }
		const Handle<YieldTermStructure>& targetDiscountCurve() const {
            return targetDiscountCurve_;
        }
        const Currency& sourceCcy() const { return sourceCcy_; }
        const Currency& targetCcy() const { return targetCcy_; }
        const Currency& npvCcy() const { return npvCcy_; }
		const Handle<Quote>& spotFX() const { return spotFX_; }
        //@}
      
	  private:
		Currency sourceCcy_;
        Handle<YieldTermStructure> sourceDiscountCurve_;
		Currency targetCcy_;
		Handle<YieldTermStructure> targetDiscountCurve_;
		Currency npvCcy_;
		Handle<Quote> spotFX_;
        boost::optional<bool> includeSettlementDateFlows_;
        Date settlementDate_;
        Date npvDate_;
    };
}

#endif
