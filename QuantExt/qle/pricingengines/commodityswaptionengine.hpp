/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/commodityswaptionengine.hpp
    \brief commodity swaption engine
    \ingroup engines
*/

#ifndef quantext_commodity_swaption_engine_hpp
#define quantext_commodity_swaption_engine_hpp

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/instruments/genericswaption.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Swaption Engine base class
/*! Correlation is parametrized as rho(s, t) = exp(-beta * abs(s - t)) where s and t are times to futures expiry.
    This is described in detail in the ORE+ Product Catalogue.
 */
class CommoditySwaptionBaseEngine : public GenericSwaption::engine {
public:
    CommoditySwaptionBaseEngine(const Handle<YieldTermStructure>& discountCurve,
                                const Handle<QuantLib::BlackVolTermStructure>& vol, Real beta = 0.0);

protected:
    /*! Performs checks on the underlying swap to ensure that:
        - it has two legs with a commodity fixed leg against a commodity floating leg
        - every cashflow on the commodity floating leg is either averaging or non-averaging
        Returns the index of the commodity fixed leg. Based on the checks, the commodity floating leg is the other leg.
    */
    QuantLib::Size fixedLegIndex() const;

    //! Give back the fixed leg price at the swaption expiry time
    QuantLib::Real fixedLegValue(QuantLib::Size fixedLegIndex) const;

    /*! Need a strike price when querying the volatility surface in certain calculations. We take this as the
        first fixed leg period amount divided by the first floating leg quantity.
    */
    QuantLib::Real strike(QuantLib::Size fixedLegIndex) const;

    /*! Return the correlation between two future expiry dates \p ed_1 and \p ed_2
     */
    QuantLib::Real rho(const QuantLib::Date& ed_1, const QuantLib::Date& ed_2) const;

    /*! Return `true` if floating leg is averaging, otherwise false.
     */
    bool averaging(QuantLib::Size floatLegIndex) const;

    // set by the constructor
    Handle<YieldTermStructure> discountCurve_;
    Handle<QuantLib::BlackVolTermStructure> volStructure_;
    Real beta_;
};

//! Commodity Swaption Analytical Engine
/*! Analytical pricing based on the two-moment Turnbull-Wakeman approximation similar to APO pricing.
    Reference: Iain Clark, Commodity Option Pricing, Wiley, section 2.8
    See also the documentation in the ORE+ product catalogue.
*/
class CommoditySwaptionEngine : public CommoditySwaptionBaseEngine {
public:
    CommoditySwaptionEngine(const Handle<YieldTermStructure>& discountCurve,
                            const Handle<QuantLib::BlackVolTermStructure>& vol, Real beta = 0.0)
        : CommoditySwaptionBaseEngine(discountCurve, vol, beta) {}
    void calculate() const override;

private:
    /*! Calculate the expected value of the floating leg at the swaption expiry date i.e. the expected value of the
        quantity A(t_e) from the ORE+ Product Catalogue. Quantities in the calculation are divided by the
        \p normFactor to guard against numerical blow up.
    */
    QuantLib::Real expA(QuantLib::Size floatLegIndex, QuantLib::Real normFactor) const;

    /*! Calculate the expected value of the floating leg squared at the swaption expiry date i.e. the expected value
        of the quantity A^2(t_e) from the ORE+ Product Catalogue. Quantities in the calculation are divided by the
        \p normFactor to guard against numerical blow up.
    */
    QuantLib::Real expASquared(QuantLib::Size floatLegIndex, QuantLib::Real strike, QuantLib::Real normFactor) const;

    /*! Calculate the cross terms involved in expASquared. Quantities in the calculation are divided by the
        \p normFactor to guard against numerical blow up.
    */
    QuantLib::Real crossTerms(const QuantLib::ext::shared_ptr<QuantLib::CashFlow>& cf_1,
                              const QuantLib::ext::shared_ptr<QuantLib::CashFlow>& cf_2, bool isAveraging,
                              QuantLib::Real strike, QuantLib::Real normFactor) const;

    /*! Return the maximum quantity over all cashflows on the commodity floating leg. This is used as a
        normalisation factor in the calculation of E[A(t_e)] and E[A^2(t_e)] to guard against blow up.
    */
    QuantLib::Real maxQuantity(QuantLib::Size floatLegIndex) const;
};

//! Commodity Swaption Monte Carlo Engine
/*! Monte Carlo implementation of the Swaption payoff for as documented in ORE+ Product Catalogue.
    Reference: Iain Clark, Commodity Option Pricing, Wiley, section 2.8
*/
class CommoditySwaptionMonteCarloEngine : public CommoditySwaptionBaseEngine {
public:
    CommoditySwaptionMonteCarloEngine(const Handle<YieldTermStructure>& discountCurve,
                                      const Handle<QuantLib::BlackVolTermStructure>& vol, Size samples, Real beta = 0.0,
                                      const Size seed = 42)
        : CommoditySwaptionBaseEngine(discountCurve, vol, beta), samples_(samples), seed_(seed) {}
    void calculate() const override;

private:
    //! Calculations when underlying swap references a commodity spot price
    void calculateSpot(QuantLib::Size idxFixed, QuantLib::Size idxFloat, QuantLib::Real strike) const;

    //! Calculations when underlying swap references a commodity spot price
    void calculateFuture(QuantLib::Size idxFixed, QuantLib::Size idxFloat, QuantLib::Real strike) const;

    /*! Calculate the underlying spot float leg factor value at expiry time. This quantity will be multiplied by
        a sample value on each Monte Carlo iteration to give the swap float leg value.
    */
    QuantLib::Real spotFloatLegFactor(QuantLib::Size idxFloat, QuantLib::Real discountExercise) const;

    /*! Populate the factors that we need to value the floating leg of a swap referencing a future contract given
        a Monte Carlo sample. This is to avoid recalculation of these quantities on each iteration.
    */
    void futureFloatLegFactors(QuantLib::Size idxFloat, QuantLib::Real discountExercise,
                               const std::vector<QuantLib::Date>& expiries, QuantLib::Matrix& floatLegFactors,
                               QuantLib::Array& discounts, QuantLib::Array& amounts) const;

    /*! Given the index of the floating leg \p idxFloat, populate a map where the keys are the unique expiry dates of
        the future contracts referenced in the floating leg and the values are the volatilities associated with the
        future contract. Also, the matrix \p outSqrtCorr is poulated with the square root of the correlation matrix
        between the expiries.

        An exception is thrown if the floating leg is not referencing a commodity future price.
    */
    std::map<QuantLib::Date, QuantLib::Real> futureExpiries(QuantLib::Size idxFloat, QuantLib::Matrix& outSqrtCorr,
                                                            QuantLib::Real strike) const;

    Size samples_;
    long seed_;
};

} // namespace QuantExt

#endif
