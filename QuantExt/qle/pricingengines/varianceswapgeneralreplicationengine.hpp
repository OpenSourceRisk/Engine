/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/varswapengine.hpp
    \brief equity variance swap engine
*/

#pragma once
#include <ql/instruments/varianceswap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/quote.hpp>

using namespace QuantLib;

namespace QuantExt {

class VarSwapEngine : public VarianceSwap::engine {
public:
    VarSwapEngine(
                //! Equity name (needed to look up fixings)
                const std::string& equityName,
                //! Equity Spot price
                const Handle<Quote>& equityPrice,
                //! Interest Rate in Equity Currency
                const Handle<YieldTermStructure>& yieldTS,
                //! Equity Dividend Rate
                const Handle<YieldTermStructure>& dividendTS,
                //! Equity Volatility
                const Handle<BlackVolTermStructure>& volTS,
                //! Discounting Curve (may be the same as yieldTS)
                const Handle<YieldTermStructure>& discountingTS,
                //! Number of Puts
                Size numPuts = 11,
                //! Number of Calls
                Size numCalls = 11,
                //! Default Moneyness StepSize for 1Y swap (scaled by sqrt(T))
                Real stepSize = 0.05);

    void calculate() const;
    private:
    Real calculateAccruedVariance() const;
    Real calculateFutureVariance() const;

    std::string equityName_;
    Handle<Quote> equityPrice_;
    Handle<YieldTermStructure> yieldTS_;
    Handle<YieldTermStructure> dividendTS_;
    Handle<BlackVolTermStructure> volTS_;
    Handle<YieldTermStructure> discountingTS_;
    Size numPuts_;
    Size numCalls_;
    Real stepSize_;
};

}
