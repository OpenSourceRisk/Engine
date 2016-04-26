/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/oiccbasisswapengine.hpp
    \brief Cross Currency Overnight Indexed Basis Swap Engine
*/

#ifndef quantext_oiccbs_engine_hpp
#define quantext_oiccbs_engine_hpp

#include <qle/instruments/oiccbasisswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

namespace QuantLib {

    //! Overnight Indexed Cross Currency Basis Swap Engine
    /*!
      \ingroup engines
    */
    class OvernightIndexedCrossCcyBasisSwapEngine
        : public OvernightIndexedCrossCcyBasisSwap::engine {
    public:
        OvernightIndexedCrossCcyBasisSwapEngine(
                const Handle<YieldTermStructure>& ts1,
                const Currency& ccy1,
                const Handle<YieldTermStructure>& ts2,
                const Currency& ccy2,
                // Spot FX rate quoted as 1 ccy2 = fx ccy1,
                // ccy1 is price currency, ccy 2 amounts to be multiplied by fx
                const Handle<Quote>& fx);
                
        void calculate() const;
        Handle<YieldTermStructure> ts1() { return ts1_; }
        Handle<YieldTermStructure> ts2() { return ts2_; }
        Currency ccy1() { return ccy1_; }
        Currency ccy2() { return ccy2_; }
        Handle<Quote> fx() {return fx_;}

      private:
        Handle<YieldTermStructure> ts1_;
        Currency ccy1_;
        Handle<YieldTermStructure> ts2_;
        Currency ccy2_;
        Handle<Quote> fx_;
    };
}

#endif
