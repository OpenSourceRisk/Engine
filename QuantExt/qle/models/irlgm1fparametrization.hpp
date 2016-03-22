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

template <class TS> class Lgm1fParametrization : public Parametrization {
  public:
    Lgm1fParametrization(const Currency &currency,
                         const Handle<TS> &termStructure);
    /*! zeta must satisfy zeta(0) = 0, zeta'(t) >= 0 */
    virtual Real zeta(const Time t) const = 0;
    /*! H must be such that H' does not change its sign */
    virtual Real H(const Time t) const = 0;
    virtual Real alpha(const Time t) const;
    virtual Real kappa(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(const Time t) const;
    const Handle<TS> termStructure() const;

    /*! allows to apply a shift to H (model invariance 1) */
    Real &shift();

    /*! allows to apply a scaling to H and zeta (model invariance 2),
      note that if a non unit scaling is provided, then
      the parameterValues method returns the unscaled alpha,
      while all other methods return scaled (and shifted) values */
    Real &scaling();

  protected:
    Real shift_, scaling_;

  private:
    const Handle<TS> termStructure_;
};

// implementation

template <class TS>
Lgm1fParametrization<TS>::Lgm1fParametrization(const Currency &currency,
                                               const Handle<TS> &termStructure)
    : Parametrization(currency), shift_(0.0), scaling_(1.0),
      termStructure_(termStructure) {}

// inline

template <class TS>
inline Real Lgm1fParametrization<TS>::alpha(const Time t) const {
    return std::sqrt((zeta(tr(t)) - zeta(tl(t))) / h_) / scaling_;
}

template <class TS>
inline Real Lgm1fParametrization<TS>::Hprime(const Time t) const {
    return scaling_ * (H(tr(t)) - H(tl(t))) / h_;
}

template <class TS>
inline Real Lgm1fParametrization<TS>::Hprime2(const Time t) const {
    return scaling_ * (H(tr2(t)) - 2.0 * H(tm2(t)) + H(tl2(t))) / (h2_ * h2_);
}

template <class TS>
inline Real Lgm1fParametrization<TS>::hullWhiteSigma(const Time t) const {
    return Hprime(t) * alpha(t);
}

template <class TS>
inline Real Lgm1fParametrization<TS>::kappa(const Time t) const {
    return -Hprime2(t) / Hprime(t);
}

template <class TS>
inline const Handle<TS> Lgm1fParametrization<TS>::termStructure() const {
    return termStructure_;
}

template <class TS> inline Real &Lgm1fParametrization<TS>::shift() {
    return shift_;
}

template <class TS> inline Real &Lgm1fParametrization<TS>::scaling() {
    return scaling_;
}

// typedef

typedef Lgm1fParametrization<YieldTermStructure> IrLgm1fParametrization;

} // namespace QuantExt

#endif
