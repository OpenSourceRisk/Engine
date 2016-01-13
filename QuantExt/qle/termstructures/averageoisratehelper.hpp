/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file averageoisratehelper.hpp
    \brief Rate helpers to facilitate usage of AverageOIS in bootstrapping
*/

#ifndef quantext_average_ois_rate_helper_hpp
#define quantext_average_ois_rate_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/averageois.hpp>

using namespace QuantLib;

namespace QuantExt {

    /*! Rate helper to facilitate the usage of an AverageOIS instrument in 
        bootstrapping. This instrument pays a fixed leg vs. a leg that 
        pays the arithmetic average of an ON index plus a spread. 
    */
    class AverageOISRateHelper : public RelativeDateRateHelper {
      public:
        AverageOISRateHelper(const Handle<Quote>& fixedRate,
            const Period& spotLagTenor,
            const Period& swapTenor,
            // Fixed leg
            const Period& fixedTenor,
            const DayCounter& fixedDayCounter,
            const Calendar& fixedCalendar,
            BusinessDayConvention fixedConvention,
            BusinessDayConvention fixedPaymentAdjustment,
            // ON leg
            const boost::shared_ptr<OvernightIndex>& overnightIndex,
            const Period& onTenor,
            const Handle<Quote>& onSpread,
            Natural rateCutoff,
            // Exogenous discount curve
            const Handle<YieldTermStructure>& discountCurve 
                = Handle<YieldTermStructure>());

        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name AverageOISRateHelper inspectors
        //@{
        Spread onSpread() const;
        boost::shared_ptr<AverageOIS> averageOIS() const;
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
      protected:
        void initializeDates();
        boost::shared_ptr<AverageOIS> averageOIS_;
        // Swap
        Period spotLagTenor_;
        Period swapTenor_;
        // Fixed leg
        Period fixedTenor_;
        DayCounter fixedDayCounter_;
        Calendar fixedCalendar_;
        BusinessDayConvention fixedConvention_;
        BusinessDayConvention fixedPaymentAdjustment_;
        // ON leg
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        Period onTenor_;
        Handle<Quote> onSpread_;
        Natural rateCutoff_;
        // Curves
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        Handle<YieldTermStructure> discountHandle_;
        RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    };
}

#endif
