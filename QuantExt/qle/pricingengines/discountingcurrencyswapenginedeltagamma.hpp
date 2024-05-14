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

/*! \file qle/pricingengines/discountingcurrencyswapenginedeltagamma.hpp
    \brief discounting currency swap engine providing analytical deltas
           and gammas for vanilla swaps

    \ingroup engines
*/

#ifndef quantext_discounting_currencyswap_engine_delta_hpp
#define quantext_discounting_currencyswap_engine_delta_hpp

#include <ql/currency.hpp>
#include <ql/handle.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/currencies/currencycomparator.hpp>
#include <qle/instruments/currencyswap.hpp>

namespace QuantExt {

//! Discounting currency swap engine providing analytical deltas and gammas
/*! This class generalizes QuantLib's DiscountingSwapEngine. It takes leg currencies into account and converts into the
   provided "npv currency", which must be one of the leg currencies. The evaluation date is the reference date of either
   of the discounting curves (which must be equal).

   The same comments as in discountingswapenginedeltagamma.hpp apply here, so read them first.

   The engine processes FX linked flows from xccy resetting swaps, but only captures the additional FX Spot Delta risk
   coming from the reset feature. Interest Deltas and Gammas coming from the FX forwarding factor P_for / P_dom are
   neglected, this factor is treated as a constant for the purpose of sensitivity calculation.

   Here, the additional results are:

   deltaDiscount    (map<Currency, vector<Real>>   ): Delta on discount curve, rebucketed, values are in currency
   deltaForward     (map<Currency, vector<Real>>   ): Delta on forward curve, rebucketed, value are in currency
   deltaFxSpot      (map<Currency, Real>           ): Delta on FX Spot (for all leg currencies, even if = npv ccy)

   gamma            (map<Currency, Matrix>         ): Gamma matrix per currency with blocks | dsc-dsc dsc-fwd |
                                                                                            | dsc-fwd fwd-fwd |
   (note that the second derivatives including the FX Spot are zero for the pure second derivative w.r.t. the FX Spot or
   given by the in currency delta values provided as the additional result deltaDiscount, deltaForward, to be
   reinterpreted as values in domestic currency)

   fxSpot           (map<Currency, Real>           ): FX Spot used for conversion to npvCurrency (for all leg ccys)
   bucketTimes      (vector<Real>                  ): Bucketing grid for deltas and gammas

   \warning: The assumption is that per currency we only have one discount and one forward curve. It is possible to have
   several, but then the computed deltas will be aggregated over all those curves.
*/

class DiscountingCurrencySwapEngineDeltaGamma : public CurrencySwap::engine {
public:
    typedef std::map<Currency, Matrix, CurrencyComparator> result_type_matrix;
    typedef std::map<Currency, std::vector<Real>, CurrencyComparator> result_type_vector;
    typedef std::map<Currency, Real, CurrencyComparator> result_type_scalar;

    /*! The FX spots must be given as units of npvCurrency per respective currency. The spots must be given w.r.t. a
      settlement date equal to the npv date (which is the reference date of the term structures).

      If applySimmExemptions = true, simple cashflows will be excluded from the additional
      results listed above (but not from the npv / leg npv results) if
      - the underlying instrument is physically settled and
      - the underlying instrument is not a resettable swap
      Notice that the SIMM adjustments for resettable swaps are _not_ applied though!
    */
    DiscountingCurrencySwapEngineDeltaGamma(const std::vector<Handle<YieldTermStructure>>& discountCurves,
                                            const std::vector<Handle<Quote>>& fxQuotes,
                                            const std::vector<Currency>& currencies, const Currency& npvCurrency,
                                            const std::vector<Time>& bucketTimes = std::vector<Time>(),
                                            const bool computeDelta = false, const bool computeGamma = false,
                                            const bool linearInZero = true, const bool applySimmExemptions = false);
    void calculate() const override;
    std::vector<Handle<YieldTermStructure>> discountCurves() { return discountCurves_; }
    std::vector<Currency> currencies() { return currencies_; }
    Currency npvCurrency() { return npvCurrency_; }

private:
    Handle<YieldTermStructure> fetchTS(Currency ccy) const;
    Handle<Quote> fetchFX(Currency ccy) const;

    const std::vector<Handle<YieldTermStructure>> discountCurves_;
    const std::vector<Handle<Quote>> fxQuotes_;
    const std::vector<Currency> currencies_;
    const Currency npvCurrency_;
    const std::vector<Time> bucketTimes_;
    const bool computeDelta_, computeGamma_, linearInZero_, applySimmExemptions_;
};
} // namespace QuantExt

#endif
