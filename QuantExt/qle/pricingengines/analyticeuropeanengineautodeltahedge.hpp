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

/*! \file qle/pricingengines/analyticeuropeanengineautodeltahedge.hpp
    \brief Pricing engine for Equity Auto Delta Hedged Options
    \ingroup engines
*/

#ifndef quantext_analytic_european_engine_auto_delta_hedge_hpp
#define quantext_analytic_european_engine_auto_delta_hedge_hpp

#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/instruments/equityautodeltahedgedoption.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Analytic pricing engine for EquityAutoDeltaHedgedOption
/*! Prices the auto delta-hedged option structure by splitting the PV into:
    - Realized past hedge P&L (observation start to today): computed from historical
      fixings and discrete delta accumulation.
    - Future hedge P&L (today to expiry): BS option at hedging vol with contractual forward.
    - Market option payoff PV: BS option at market vol with market forward curves.
    - Premium: deterministic cashflow discounted from expiry.

    \ingroup engines
*/
class AnalyticEuropeanEngineAutoDeltaHedge : public EquityAutoDeltaHedgedOption::engine {
public:
    AnalyticEuropeanEngineAutoDeltaHedge(
        const Handle<Quote>& spot,
        const Handle<YieldTermStructure>& discountCurve,
        const Handle<YieldTermStructure>& dividendCurve,
        const Handle<YieldTermStructure>& forecastCurve,
        const Handle<BlackVolTermStructure>& marketVol,
        const Handle<QuantExt::EquityIndex2>& equityIndex);

    void calculate() const override;

private:
    Handle<Quote> spot_;
    Handle<YieldTermStructure> discountCurve_;
    Handle<YieldTermStructure> dividendCurve_;
    Handle<YieldTermStructure> forecastCurve_;
    Handle<BlackVolTermStructure> marketVol_;
    Handle<QuantExt::EquityIndex2> equityIndex_;
};

} // namespace QuantExt

#endif
