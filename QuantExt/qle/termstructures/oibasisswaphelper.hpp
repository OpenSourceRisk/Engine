/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/termstructures/oibasisswaphelper.hpp
    \brief Overnight Indexed Basis Swap rate helpers
*/

#ifndef quantlib_oisbasisswaphelper_hpp
#define quantlib_oisbasisswaphelper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/instruments/overnightindexedswap.hpp>
#include <qle/instruments/oibasisswap.hpp>

namespace QuantExt {

    //! Rate helper for bootstrapping over Overnight Indexed Basis Swap Spreads
    class OIBSHelper : public RelativeDateRateHelper {
      public:
        OIBSHelper(Natural settlementDays,
                   const Period& tenor, // swap maturity
                   const Handle<Quote>& oisSpread,
                   const boost::shared_ptr<OvernightIndex>& overnightIndex,
                   const boost::shared_ptr<IborIndex>& iborIndex
                   );
        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name inspectors
        //@{
        boost::shared_ptr<OvernightIndexedBasisSwap> swap() const { return swap_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
    protected:
        void initializeDates();

        Natural settlementDays_;
        Period tenor_;
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        boost::shared_ptr<IborIndex> iborIndex_;

        boost::shared_ptr<OvernightIndexedBasisSwap> swap_;
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
    };
}

#endif
