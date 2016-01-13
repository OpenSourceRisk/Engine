/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/discountingfxforwardengine.hpp
    \brief Engine to value an FX Forward off two yield curves
*/

#ifndef quantext_discounting_fxforward_engine_hpp
#define quantext_discounting_fxforward_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/fxforward.hpp>

namespace QuantExt {
    //! Discounting FX Forward Engine
    
	/*! This class implements pricing of FX Forwards by discounting the future 
        nominal cash flows using the respective yield curves and by converting
        the base currency amount into price currency at the provided spot FX 
		rate.
		
		\ingroup engines
    */
    class DiscountingFxForwardEngine : public FxForward::engine {
      public:
		/*! \param ccy1, currency1Discountcurve
                   Currency 1 and its discount curve.
            \param ccy2, currency2Discountcurve
                   Currency 2 and its discount curve.
            \param priceCurrency
                   Results currency. Must be one of ccy1 or ccy2.
            \param spotFX
                   The market spot rate quote of priceCurrency vs. base 
				   currency where base currency is one of ccy1 or ccy2. 
				   If not an FxQuote, the quote is interpreted as the 
				   number of units of priceCurrency per unit of base.
                   In order to have the correct conventions, a 
                   properly formed FxQuote should be used.
            \param includeSettlementDateFlows, settlementDate
                   If includeSettlementDateFlows is true (false), cashflows 
				   on the settlementDate are (not) included in the NPV.
            \param npvDate
			       Discount to this date.
        */
        DiscountingFxForwardEngine(const Currency& ccy1,
			const Handle<YieldTermStructure>& currency1Discountcurve,
            const Currency& ccy2,
            const Handle<YieldTermStructure>& currency2Discountcurve,
            const Currency& priceCurrency,
            const Handle<Quote>& spotFX,  
            boost::optional<bool> includeSettlementDateFlows = boost::none, 
            const Date& settlementDate = Date(), 
            const Date& npvDate = Date());

        void calculate() const;
        
		const Handle<YieldTermStructure>& currency1Discountcurve() const {
            return currency1Discountcurve_;
        }
		
		const Handle<YieldTermStructure>& currency2Discountcurve() const {
            return currency2Discountcurve_;
        }

		const Handle<Quote>& spotFX() const {
			return spotFX_;
		}

      private:
        Currency ccy1_;
        Handle<YieldTermStructure> currency1Discountcurve_;
        Currency ccy2_;
		Handle<YieldTermStructure> currency2Discountcurve_;
		Currency priceCurrency_;
		Handle<Quote> spotFX_;
        boost::optional<bool> includeSettlementDateFlows_;
        Date settlementDate_;
		Date npvDate_;
    };

}

#endif
