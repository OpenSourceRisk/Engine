/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file basistwoswaphelper.hpp
    \brief Libor basis swap helper as two swaps
*/

#ifndef quantext_basis_two_swap_helper_hpp
#define quantext_basis_two_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

using namespace QuantLib;

namespace QuantExt {

    /*! Rate helper for bootstrapping using Libor tenor basis as the
     *  difference between the fixed rate on two swaps
     */
    class BasisTwoSwapHelper : public RelativeDateRateHelper {
      public:
        BasisTwoSwapHelper(const Handle<Quote>& spread,
            const Period& swapTenor,
            const Calendar& calendar,
            // Long tenor swap
            Frequency longFixedFrequency,
            BusinessDayConvention longFixedConvention,
            const DayCounter& longFixedDayCount,
            const boost::shared_ptr<IborIndex>& longIndex,
            // Short tenor swap
            Frequency shortFixedFrequency,
            BusinessDayConvention shortFixedConvention,
            const DayCounter& shortFixedDayCount,
            const boost::shared_ptr<IborIndex>& shortIndex,
            bool longMinusShort = true,
            // Discount curve
            const Handle<YieldTermStructure>& discountingCurve
                = Handle<YieldTermStructure>());

        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name BasisTwoSwapHelper inspectors
        //@{
        boost::shared_ptr<VanillaSwap> longSwap() const;
        boost::shared_ptr<VanillaSwap> shortSwap() const;
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}

      protected:
        void initializeDates();
        Period swapTenor_;
        Calendar calendar_;
        // Long tenor swap
        Frequency longFixedFrequency_;
        BusinessDayConvention longFixedConvention_;
        DayCounter longFixedDayCount_;
        boost::shared_ptr<IborIndex> longIndex_;
        // Short tenor swap
        Frequency shortFixedFrequency_;
        BusinessDayConvention shortFixedConvention_;
        DayCounter shortFixedDayCount_;
        boost::shared_ptr<IborIndex> shortIndex_;
        bool longMinusShort_;

        boost::shared_ptr<VanillaSwap> longSwap_;
        boost::shared_ptr<VanillaSwap> shortSwap_;
        
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        Handle<YieldTermStructure> discountHandle_;
        RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    };

    inline boost::shared_ptr<VanillaSwap> BasisTwoSwapHelper::
        shortSwap() const {
        return shortSwap_;
    }

    inline boost::shared_ptr<VanillaSwap> BasisTwoSwapHelper::
        longSwap() const {
        return longSwap_;
    }
}

#endif
