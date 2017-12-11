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

/*! \file irlgm1fparametrization.hpp
    \brief Interest Rate Linear Gaussian Markov 1 factor parametrization
    \ingroup models
*/

#ifndef quantext_irlgm1f_parametrization_hpp
#define quantext_irlgm1f_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {

//! LGM 1F Parametrization
/*! \ingroup models
 */
template <class TS> class Lgm1fParametrization : public Parametrization {
public:
    Lgm1fParametrization(const Currency& currency, const Handle<TS>& termStructure,
                         const std::string& name = std::string());
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
    Real& shift();

    /*! allows to apply a scaling to H and zeta (model invariance 2),
      note that if a non unit scaling is provided, then
      the parameterValues method returns the unscaled alpha,
      while all other methods return scaled (and shifted) values */
    Real& scaling();

    const std::string& name() const { return name_; }

protected:
    Real shift_, scaling_;

private:
    const Handle<TS> termStructure_;
    std::string name_;
};

// implementation

template <class TS>
Lgm1fParametrization<TS>::Lgm1fParametrization(const Currency& currency, const Handle<TS>& termStructure,
                                               const std::string& name)
    : Parametrization(currency), shift_(0.0), scaling_(1.0), termStructure_(termStructure) {
    name_ = name.empty() ? currency.code() : name;
}

// inline

template <class TS> inline Real Lgm1fParametrization<TS>::alpha(const Time t) const {
    return std::sqrt((zeta(tr(t)) - zeta(tl(t))) / h_) / scaling_;
}

template <class TS> inline Real Lgm1fParametrization<TS>::Hprime(const Time t) const {
    return scaling_ * (H(tr(t)) - H(tl(t))) / h_;
}

template <class TS> inline Real Lgm1fParametrization<TS>::Hprime2(const Time t) const {
    return scaling_ * (H(tr2(t)) - 2.0 * H(tm2(t)) + H(tl2(t))) / (h2_ * h2_);
}

template <class TS> inline Real Lgm1fParametrization<TS>::hullWhiteSigma(const Time t) const {
    return Hprime(t) * alpha(t);
}

template <class TS> inline Real Lgm1fParametrization<TS>::kappa(const Time t) const { return -Hprime2(t) / Hprime(t); }

template <class TS> inline const Handle<TS> Lgm1fParametrization<TS>::termStructure() const { return termStructure_; }

template <class TS> inline Real& Lgm1fParametrization<TS>::shift() { return shift_; }

template <class TS> inline Real& Lgm1fParametrization<TS>::scaling() { return scaling_; }

// typedef

typedef Lgm1fParametrization<YieldTermStructure> IrLgm1fParametrization;

} // namespace QuantExt

#endif
