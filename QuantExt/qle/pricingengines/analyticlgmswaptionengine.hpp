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

/*! \file pricingengines/analyticlgmswaptionengine.hpp
    \brief analytic engine for european swaptions in the LGM model

        \ingroup engines
*/

#ifndef quantext_analytic_lgm_swaption_engine_hpp
#define quantext_analytic_lgm_swaption_engine_hpp

#include <ql/instruments/swaption.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

//! Analytic LGM swaption engine for european exercise
/*! \ingroup swaptionengines

    All fixed coupons with start date greater or equal to the respective
    option expiry are considered to be
    part of the exercise into right.

    References:

    Hagan, Evaluating and hedging exotic swap instruments via LGM

    Lichters, Stamm, Gallagher: Modern Derivatives Pricing and Credit Exposure
    Analysis, Palgrave Macmillan, 2015, 11.2.2

    \warning Cash settled swaptions are not supported

    The basis between the given discounting curve (or - if not given - the
    model curve) and the forwarding curve attached to the underlying swap's
    ibor index is taken into account by a static correction spread for
    the underlying's fixed leg. Likewise a spread on the floating leg is
    taken into account.

    Note that we assume H' does not change its sign, but this is a general
    requirement of the LGM parametrization anyway (see the base parametrization
    class).

    \ingroup engines
*/

class AnalyticLgmSwaptionEngine : public GenericEngine<Swaption::arguments, Swaption::results> {

public:
    /*! nextCoupon is Mapping A, proRata is Mapping B
        in Lichters, Stamm, Gallagher (2015), 11.2.2 */
    enum FloatSpreadMapping { nextCoupon, proRata, simple };

    /*! Lgm model based constructor */
    AnalyticLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                              const FloatSpreadMapping floatSpreadMapping = proRata);

    /*! CrossAsset model based constructor */
    AnalyticLgmSwaptionEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, const Size ccy,
                              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                              const FloatSpreadMapping floatSpreadMapping = proRata);

    /*! parametrization based constructor, note that updates in the
        parametrization are not observed by the engine, you would
        have to call update() on the engine explicitly */
    AnalyticLgmSwaptionEngine(const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1f,
                              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                              const FloatSpreadMapping floatSpreadMapping = proRata);

    void calculate() const override;

    /* If enabled, the underlying instrument should not changed between two pricings or the cache has to be
       cleared. Furthermore it is assumed that all the market data stays constant between two pricings.
       Regarding the LGM parameters it is assumed that either H(t) or alpha(t) (or both) is constant, depending
       on the passed parameters here. enableCache() should only be called once on an instance of this class. */
    void enableCache(const bool lgm_H_constant = true, const bool lgm_alpha_constant = false);
    void clearCache();

private:
    Real flatAmount(const Size k) const;
    Real yStarHelper(const Real y) const;
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> p_;
    Handle<YieldTermStructure> c_;
    mutable FloatSpreadMapping floatSpreadMapping_;
    bool caching_, lgm_H_constant_, lgm_alpha_constant_;
    mutable Real H0_, D0_, zetaex_, S_m1, u_, w_;
    mutable std::vector<Real> S_, Hj_, Dj_;
    mutable Size j1_, k1_;
    mutable std::vector<QuantLib::ext::shared_ptr<FixedRateCoupon>> fixedLeg_;
    mutable std::vector<QuantLib::ext::shared_ptr<FloatingRateCoupon>> floatingLeg_;
    mutable Real nominal_;
};

} // namespace QuantExt

#endif
