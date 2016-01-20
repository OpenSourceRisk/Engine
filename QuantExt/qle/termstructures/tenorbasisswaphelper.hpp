/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file tenorbasisswap.hpp
    \brief Single currency tenor basis swap helper
*/

#ifndef quantext_tenor_basis_swap_helper_hpp
#define quantext_tenor_basis_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/tenorbasisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Rate helper for bootstrapping using Libor tenor basis swaps
    class TenorBasisSwapHelper : public RelativeDateRateHelper {
    public:
        TenorBasisSwapHelper(Handle<Quote> spread,
            const Period& swapTenor,
            const boost::shared_ptr<IborIndex> longIndex,
            const boost::shared_ptr<IborIndex> shortIndex,
            const Period& shortPayTenor = Period(),
            const Handle<YieldTermStructure>& discountingCurve = 
                Handle<YieldTermStructure>(),
            bool spreadOnShort = true,
            bool includeSpread = false,
            SubPeriodsCoupon::Type type = SubPeriodsCoupon::Compounding);

        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name TenorBasisSwapHelper inspectors
        //@{
        boost::shared_ptr<TenorBasisSwap> swap() const { return swap_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
      
      protected:
        void initializeDates();

        Period swapTenor_;
        boost::shared_ptr<IborIndex> longIndex_;
        boost::shared_ptr<IborIndex> shortIndex_;
        Period shortPayTenor_;
        bool spreadOnShort_;
        bool includeSpread_;
        SubPeriodsCoupon::Type type_;

        boost::shared_ptr<TenorBasisSwap> swap_;
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        Handle<YieldTermStructure> discountHandle_;
        RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    };

}

#endif
