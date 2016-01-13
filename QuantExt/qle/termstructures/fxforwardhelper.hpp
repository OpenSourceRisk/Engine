/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/termstructures/fxforwardhelper.hpp
    \brief Fx Forward helper class
*/

#ifndef quantext_fxforward_helper_hpp
#define quantext_fxforward_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/fxforward.hpp>

using namespace QuantLib;

namespace QuantExt {

    /*! Helper for bootstrapping over FX forward rate quotes.
        
        \warning If including maturities before spot (e.g. ON and TN) in a 
                 curve, the spotRate argument should contain the rate and date 
                 for exchange on that maturity.
    */
    class FxForwardHelper : public RelativeDateRateHelper {
      public:
        /*! \param spotDays
                   The number of business days to the spot date.
            \param forwardTenor
                   The FX forward rate tenor.
            \param sourceCurrency, targetCurrency
                   The FX rate is expressed as number of units of 
                   targetCurrency per unit of sourceCurrency.
            \param forwardPoints
                   The forward points quote.
            \param spotRate
                   The market spot rate quote expressed as number of units 
                   of target per unit of source. In order to have the correct 
                   conventions, a properly formed FxQuote should be used.
            \param pointsFactor
                   The number by which to multiply the points quote before 
                   adding or subtracting it from the spot quote.
            \param knownDiscountCurve, knownDiscountCurrency
                   We already have the discount curve \em knownDiscountCurve 
                   in the currency knownDiscountCurrency.
            \param advanceCal
                   The calendar(s) used to advance from valuation date to 
                   the spot date and from spot/valuation to forward exchange 
                   date.
            \param spotRelative
                   True if adding the forwardTenor to spot date to get the 
                   forward date. False if adding it to valuation date. Usually 
                   true except for ON and TN tenors.
            \param additionalSettleCal
                   Any additional calendar(s) used to adjust the spot date 
                   after advancing from the valuation date (e.g. USD).
        */
        FxForwardHelper(Natural spotDays,
            const Period& forwardTenor,
            const Currency& sourceCurrency,
            const Currency& targetCurrency,
            const Handle<Quote>& forwardPoints,
            const Handle<Quote>& spotRate,
            Real pointsFactor,
            const Handle<YieldTermStructure>& knownDiscountCurve,
            const Currency& knownDiscountCurrency,
            const Calendar& advanceCal = NullCalendar(),
            bool spotRelative = true,
            const Calendar& additionalSettleCal = NullCalendar());
        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name Inspectors
        //@{
        boost::shared_ptr<FxForward> fxForward() const { return fxForward_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
    protected:
        void initializeDates();

        Natural spotDays_;
        Period forwardTenor_;
        Currency sourceCurrency_;
        Currency targetCurrency_;
        Handle<Quote> spotRate_;
        Real pointsFactor_;
        Handle<YieldTermStructure> knownDiscountCurve_;
        Currency knownDiscountCurrency_;
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        Calendar advanceCal_;
        bool spotRelative_;
        Calendar additionalSettleCal_;

        boost::shared_ptr<Quote> fxForwardQuote_;
        boost::shared_ptr<FxForward> fxForward_;
        Money nominal_;
    };
}

#endif
