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

#include <qle/models/parametrization.hpp>

#include <ql/experimental/math/piecewiseintegral.hpp>
#include <ql/handle.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <map>

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

    /*! \f[ \int_0^t alpha^2(u) H^n(u) du \f]*/
    Real zetan(const Size n, const Time t, const QuantLib::ext::shared_ptr<Integrator>& integrator);

    /*! allows to apply a shift to H (model invariance 1) */
    Real& shift();

    /*! allows to apply a scaling to H and zeta (model invariance 2),
      note that if a non unit scaling is provided, then
      the parameterValues method returns the unscaled alpha,
      while all other methods return scaled (and shifted) values */
    Real& scaling();

    Size numberOfParameters() const override { return 2; }

    void update() const override;

protected:
    Real shift_, scaling_;

private:
    const Handle<TS> termStructure_;
    mutable std::map<std::pair<Size, Real>, Real> zetan_cached_;
};

// implementation

template <class TS>
Lgm1fParametrization<TS>::Lgm1fParametrization(const Currency& currency, const Handle<TS>& termStructure,
                                               const std::string& name)
    : Parametrization(currency, name.empty() ? currency.code() : name), shift_(0.0), scaling_(1.0),
      termStructure_(termStructure) {}

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

template <class TS>
inline Real Lgm1fParametrization<TS>::zetan(const Size n, const Time t,
                                            const QuantLib::ext::shared_ptr<Integrator>& integrator) {
    auto z = zetan_cached_.find(std::make_pair(n, t));
    if (z == zetan_cached_.end()) {
        std::vector<Real> times;
        for (Size i = 0; i < numberOfParameters(); ++i)
            times.insert(times.end(), parameterTimes(i).begin(), parameterTimes(i).end());
        PiecewiseIntegral pwint(integrator, times, true);
        Real v = pwint([this, n](Real s) { return std::pow(this->alpha(s), 2) * std::pow(this->H(s), n); }, 0.0, t);
        zetan_cached_[std::make_pair(n, t)] = v;
        return v;
    } else {
        return z->second;
    }
}

template <class TS> inline void Lgm1fParametrization<TS>::update() const {
    Parametrization::update();
    zetan_cached_.clear();
}

// typedef

typedef Lgm1fParametrization<YieldTermStructure> IrLgm1fParametrization;

} // namespace QuantExt

#endif
