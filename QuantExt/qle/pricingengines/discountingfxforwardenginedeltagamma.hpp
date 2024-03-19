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

/*! \file qle/pricingengines/discountingfxforwardenginedeltagamma.hpp
    \brief Engine to value an FX Forward off two yield curves

    \ingroup engines
*/

#ifndef quantext_discounting_fxforward_engine_delta_gamma_hpp
#define quantext_discounting_fxforward_engine_delta_gamma_hpp

#include <ql/math/matrix.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/fxforward.hpp>

namespace QuantExt {

//! Discounting FX Forward Engine providing analytical deltas and gammas

/*! This class generalises QuantExt's DiscountingFxForwardEngine, in analogy to the
    DiscountingCurrencySwapEngineDeltaGamma. Additional results are

   deltaDiscount    (map<Currency, vector<Real>>   ): Delta on discount curve, rebucketed, values are in currency
   deltaFxSpot      (map<Currency, Real>           ): Delta on FX Spot (for ccy != npvCcy)

   gamma            (map<Currency, Matrix>         ): Gamma matrix per currency between discount curve tenor points

   Note that the second derivatives including the FX Spot are zero for the pure second derivative w.r.t. the FX Spot or
   given by the in currency delta values provided as the additional result deltaDiscount to be
   reinterpreted as values in domestic currency)

   fxSpot           (map<Currency, Real>           ): FX Spot used for conversion to npvCurrency (for ccy != npvCcy)
   bucketTimes      (vector<Real>                  ): Bucketing grid for deltas and gammas

   npvDom, npvFor                                   : NPV of domestic flow resp. foreign flow (in dom resp. for ccy)

   \ingroup engines
*/
class DiscountingFxForwardEngineDeltaGamma : public FxForward::engine {
public:
    /*! \param domCcy, domCurve
               Currency 1 and its discount curve.
        \param forCcy, forCurve
               Currency 2 and its discount curve.
        \param spotFx
               The market spot rate quote, given as units of domCcy
               for one unit of forCcy. The spot rate must be given
               w.r.t. a settlement equal to the npv date.
        \param bucketTimes
               Bucketing grid for deltas and gammas
        \param computeDelta, computeGamma
               Switch to enable/disable delta and gamma calculation
        \param linearInZero
               Interpolation used in the delta/gamma rebucketing to the desired time grid
        \param includeSettlementDateFlows, settlementDate
               If includeSettlementDateFlows is true (false), cashflows
               on the settlementDate are (not) included in the NPV.
               If not given the settlement date is set to the npv date.
        \param npvDate
               The date w.r.t. which the npv should be computed.
        \param applySimmExemptions
               If true, physically settled flows are ignored in sensi calculation, i.e. in he additional results
               above, including npvDom and npvFor
    */
    DiscountingFxForwardEngineDeltaGamma(const Currency& domCcy, const Handle<YieldTermStructure>& domCurve,
                                         const Currency& forCcy, const Handle<YieldTermStructure>& forCurve,
                                         const Handle<Quote>& spotFx,
                                         const std::vector<Time>& bucketTimes = std::vector<Time>(),
                                         const bool computeDelta = false, const bool computeGamma = false,
                                         const bool linearInZero = true,
                                         boost::optional<bool> includeSettlementDateFlows = boost::none,
                                         const Date& settlementDate = Date(), const Date& npvDate = Date(),
                                         const bool applySimmExemptions = false);

    void calculate() const override;

    const Handle<YieldTermStructure>& domCurve() const { return domCurve_; }

    const Handle<YieldTermStructure>& forCurve() const { return forCurve_; }

    const Currency& domCcy() const { return domCcy_; }
    const Currency& forCcy() const { return forCcy_; }

    const Handle<Quote>& spotFx() const { return spotFx_; }

private:
    Currency domCcy_, forCcy_;
    Handle<YieldTermStructure> domCurve_, forCurve_;
    Handle<Quote> spotFx_;
    const std::vector<Time> bucketTimes_;
    const bool computeDelta_, computeGamma_, linearInZero_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_;
    Date npvDate_;
    bool applySimmExemptions_;
};
} // namespace QuantExt

#endif
