/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file crossccybasisswaphelper.hpp
    \brief Cross currency basis swap helper
*/

#ifndef quantext_cross_ccy_basis_swap_helper_hpp
#define quantext_cross_ccy_basis_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/crossccybasisswap.hpp>

namespace QuantExt {

    /*! Rate helper for bootstrapping over cross currency basis swap spreads
        
        Assumes that you have, at a minimum, either:
        - flatIndex with attached YieldTermStructure and flatDiscountCurve
        - spreadIndex with attached YieldTermStructure and spreadDiscountCurve

        The other leg is then solved for i.e. index curve (if no 
        YieldTermStructure is attached to its index) or discount curve (if 
        its Handle is empty) or both.
    */
    class CrossCcyBasisSwapHelper : public RelativeDateRateHelper {
      public:
        CrossCcyBasisSwapHelper(const Handle<Quote>& spreadQuote,
            const Handle<Quote>& spotFX,
            Natural settlementDays,
            const Calendar& settlementCalendar,
            const Period& swapTenor,
            BusinessDayConvention rollConvention,
            const boost::shared_ptr<QuantLib::IborIndex>& flatIndex,
            const boost::shared_ptr<QuantLib::IborIndex>& spreadIndex,
            const Handle<YieldTermStructure>& flatDiscountCurve,
            const Handle<YieldTermStructure>& spreadDiscountCurve,
            bool eom = false);
        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name inspectors
        //@{
        boost::shared_ptr<CrossCcyBasisSwap> swap() const { return swap_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
    
      protected:
        void initializeDates();

        Handle<Quote> spotFX_;
        Natural settlementDays_;
        Calendar settlementCalendar_;
        Period swapTenor_;
        BusinessDayConvention rollConvention_;
        boost::shared_ptr<QuantLib::IborIndex> flatIndex_;
        boost::shared_ptr<QuantLib::IborIndex> spreadIndex_;
        Handle<YieldTermStructure> flatDiscountCurve_;
        Handle<YieldTermStructure> spreadDiscountCurve_;
        bool eom_;

        Currency flatLegCurrency_;
        Currency spreadLegCurrency_;
        boost::shared_ptr<CrossCcyBasisSwap> swap_;

        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        RelinkableHandle<YieldTermStructure> flatDiscountRLH_;
        RelinkableHandle<YieldTermStructure> spreadDiscountRLH_;
    };
}

#endif
