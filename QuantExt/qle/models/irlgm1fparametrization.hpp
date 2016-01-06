/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file irlgm1fparametrization.hpp
    \brief Interest Rate Linear Gaussian Markov 1 factor parametrization
*/

#ifndef quantext_irlgm1f_parametrization_hpp
#define quantext_irlgm1f_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

class IrLgm1fParametrization : public Parametrization {
  public:
    IrLgm1fParametrization(const Currency &currency,
                           const Handle<YieldTermStructure> &termStructure);
    /*! zeta must satisfy zeta(0) = 0, zeta'(t) >= 0 */
    virtual Real zeta(const Time t) const = 0;
    /*! H must satisfy H(0) = 0, H'(0) = 1, H'(t) > 0 */
    virtual Real H(const Time t) const = 0;
    virtual Real alpha(const Time t) const;
    virtual Real kappa(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(const Time t) const;
    const Handle<YieldTermStructure> termStructure() const;

  private:
    const Handle<YieldTermStructure> termStructure_;
};

// inline

inline Real IrLgm1fParametrization::alpha(const Time t) const {
    return std::sqrt((zeta(tr(t)) - zeta(tl(t))) / h_);
}

inline Real IrLgm1fParametrization::Hprime(const Time t) const {
    return (H(tr(t)) - H(tl(t))) / h_;
}

inline Real IrLgm1fParametrization::Hprime2(const Time t) const {
    return (H(tr2(t)) - 2.0 * H(tm2(t)) + H(tl2(t))) / (h2_ * h2_);
}

inline Real IrLgm1fParametrization::hullWhiteSigma(const Time t) const {
    return Hprime(t) * alpha(t);
}

inline Real IrLgm1fParametrization::kappa(const Time t) const {
    return -Hprime2(t) / Hprime(t);
}

inline const Handle<YieldTermStructure>
IrLgm1fParametrization::termStructure() const {
    return termStructure_;
}

} // namespace QuantExt

#endif
