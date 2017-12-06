/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file qle/pricingengines/oiccbasisswapengine.hpp
    \brief Cross Currency Overnight Indexed Basis Swap Engine

        \ingroup engines
*/

#ifndef quantext_oiccbs_engine_hpp
#define quantext_oiccbs_engine_hpp

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <qle/instruments/oiccbasisswap.hpp>

namespace QuantExt {

//! Overnight Indexed Cross Currency Basis Swap Engine
//! \ingroup engines
class OvernightIndexedCrossCcyBasisSwapEngine : public OvernightIndexedCrossCcyBasisSwap::engine {
public:
    OvernightIndexedCrossCcyBasisSwapEngine(const Handle<YieldTermStructure>& ts1, const Currency& ccy1,
                                            const Handle<YieldTermStructure>& ts2, const Currency& ccy2,
                                            // Spot FX rate quoted as 1 ccy2 = fx ccy1,
                                            // ccy1 is price currency, ccy 2 amounts to be multiplied by fx
                                            const Handle<Quote>& fx);
    void calculate() const;
    Handle<YieldTermStructure> ts1() { return ts1_; }
    Handle<YieldTermStructure> ts2() { return ts2_; }
    Currency ccy1() { return ccy1_; }
    Currency ccy2() { return ccy2_; }
    Handle<Quote> fx() { return fx_; }

private:
    Handle<YieldTermStructure> ts1_;
    Currency ccy1_;
    Handle<YieldTermStructure> ts2_;
    Currency ccy2_;
    Handle<Quote> fx_;
};
} // namespace QuantExt

#endif
