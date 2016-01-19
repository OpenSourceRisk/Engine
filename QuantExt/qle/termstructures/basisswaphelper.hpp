/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file basisswaphelper.hpp
    \brief Basis Swap helpers
*/

#ifndef quantext_basisswap_helper_hpp
#define quantext_basisswap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>
#include <qle/instruments/basisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Rate helper for bootstrapping forward curves from Basis Swap spreads
    /*
      The bootstrap affects the impliedIndex' termstructure only.
    */
    class BasisSwapHelper : public RelativeDateRateHelper {
      public:
        BasisSwapHelper(Natural settlementDays,
                        const Period& term, // swap maturity
                        const boost::shared_ptr<QuantLib::IborIndex>& impliedIndex,
                        const boost::shared_ptr<QuantLib::IborIndex>& fixedIndex,
                        const Handle<Quote>& spreadQuote,
                        bool spreadQuoteOnPayLeg,
                        const Handle<YieldTermStructure>& fixedDiscountCurve);
        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name inspectors
        //@{
        boost::shared_ptr<BasisSwap> swap() const { return swap_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
    protected:
        void initializeDates();

        Natural settlementDays_;
        Period term_;
        boost::shared_ptr<QuantLib::IborIndex> impliedIndex_;
        boost::shared_ptr<QuantLib::IborIndex> fixedIndex_;
        bool spreadQuoteOnPayLeg_;
        Handle<YieldTermStructure> fixedDiscountCurve_;

        boost::shared_ptr<QuantExt::BasisSwap> swap_;
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
    };
}

#endif
