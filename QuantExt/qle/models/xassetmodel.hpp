/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file xassetmodel.hpp
    \brief cross asset model
*/

#ifndef quantext_xasset_model_hpp
#define quantext_xasset_model_hpp

#include <qle/models/parametrization.hpp>
#include <ql/math/matrix.hpp>
#include <ql/models/model.hpp>
#include <ql/stochasticprocess.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! The model is operated under the domestic LGM measure

    There are two ways of calibrating the model:

    - provide a calibrated parametrization for a component
      extracted from some external model
    - do the calibration within the XAssetModel using one
      of the calibration procedures

    The inter-parametrization correlation matrix specified
    here can not be calibrated currently, but is a fixed,
    external input.

    The model's reference date is by definition the reference
    date of the termstructure of the first parametrization,
    i.e. of the domestic yield term structure.
*/

class XAssetModel : public CalibratedModel {
  public:
    /*! Parametrizations must be given in the following order
        - IR (first parametrization defines the domestic currency)
        - FX (for all pairs domestic-ccy defined by the IR models)
        - INF (optionally, ccy must be a subset of the IR ccys)
        - CRD (optionally, ccy must be a subset of the IR ccys)
        - COM (optionally) ccy must be a subset of the IR ccys) */
    XAssetModel(const boost::shared_ptr<Parametrization> parametrizations,
                const Matrix &correlation);

    /*! allow for time dependent correlation (second constructor) ? */

    /*! inspectors */
    boost::shared_ptr<StochasticProcess> stateProcess() const;

    /*! LGM1F components */
    Real numeraire(const Size ccy, const Time t, const Real x) const;
    Real discountBond(const Size ccy, const Time t, const Time T,
                      const Real x) const;
    Real reducedDiscountBond(const Size ccy, const Time t, const Time T,
                             const Real x) const;
    Real discountBondOption(const Size ccy, Option::Type type, const Real K,
                            const Time t, const Time S, const Time T) const;

    /*! other components */

    /*! calibration procedures */
    void calibrateIrVolatilitiesIterative();
    /* ... */

  private:
    Size nIrLgm1f_;
    const IrLgm1fParametrization *irlgm1f(const Size ccy);
    std::vector<const boost::shared_ptr<Parametrization> > parametrizations_;
};

// inline

const IrLgm1fParametrization *XAssetModel::irlgm1f(const Size ccy) {
    QL_REQUIRE(ccy < nIrLgm1f_, "irlgm1f index (" << ccy << ") must be in 0..."
                                                  << (nIrLgm1f_ - 1));
    return boost::dynamic_pointer_cast<IrLgm1fParametrization>(
               parametrizations_[ccy])
        .get();
}

inline Real XAssetModel::numeraire(const Size ccy, const Time t,
                                   const Real x) const {
    const IrLgm1fParametrization *p = irlgm1f(ccy);
    Real Ht = p->H(t);
    return std::exp(Ht * x + 0.5 * Ht * Ht * p->zeta(t)) /
           p->termStructure()->discount(t);
}

inline Real XAssetModel::discountBond(const Size ccy, const Time t,
                                      const Time T, const Real x) const {
    QL_REQURE(T >= t, "T(" << T << ") >= t(" << t
                           << ") required in irlgm1f discountBond");
    const IrLgm1fParametrization *p = irlgm1f(ccy);
    Real Ht = p->H(t);
    Real HT = p->H(T);
    return p->termStructure()->discount(T) / p->termStructure()->discount(t) *
           std::exp(-(HT - ht) * x - 0.5 * (HT * HT - Ht * Ht) * p->zeta(t));
}

inline Real XAssetModel::reducedDiscountBond(const Size ccy, const Time t,
                                             const Time T, const Real x) const {
    QL_REQURE(T >= t, "T(" << T << ") >= t(" << t
                           << ") required in irlgm1f reducedDiscountBond");
    const IrLgm1fParametrization *p = irlgm1f(ccy);
    Real HT = p->H(T);
    return p->termStructure()->discount(T) *
           std::exp(-HT * x - 0.5 * HT * HT * p->zeta(t));
}

inline Real XAssetModel::discountBondOption(const Size ccy, Option::Type type,
                                            const Real K, const Time t,
                                            const Time S, const Time T) const {
    QL_REQUIRE(T > S && S >= t,
               "T(" << T << ") > S(" << S << ") >= t(" << t
                    << ") required in irlgm1f discountBondOption");
    const IrLgm1fParametrization *p = irlgm1f(ccy);
    Real w = (type == Option::Call ? 1.0 : -1.0);
    Real pS = p->termStructure()->discount(S);
    Real pT = p->termStructure()->discount(T);
    // slight generalization of Lichters, Stamm, Gallagher 11.2.1
    // with t < S only resulting in a different time at which zeta
    // has to be taken
    Real sigma = sqrt(p->zeta(t)) * (p->H(T) - p->H(S));
    Real dp = (std::log(pT / (K * pS)) / sigma + 0.5 * sigma);
    Real dm = dp - sigma;
    CumulativeNormalDistribution N;
    return w * (pT * N(w * dp) - pS * K * N(w * dm));
}

} // namespace QuantExt

#endif
