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

/*! \file irlgm1fpiecewiseconstanthullwhiteadaptor.hpp
    \brief adaptor to emulate piecewise constant Hull White parameters
    \ingroup models
*/

#ifndef quantext_piecewiseconstant_irlgm1f_hwadaptor_hpp
#define quantext_piecewiseconstant_irlgm1f_hwadaptor_hpp

#include <ql/math/comparison.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {

//! LGM 1f Piecewise Constant Hull White Adaptor
/*! \ingroup models
 */
template <class TS>
class Lgm1fPiecewiseConstantHullWhiteAdaptor : public Lgm1fParametrization<TS>,
                                               private PiecewiseConstantHelper3,
                                               private PiecewiseConstantHelper2 {
public:
    Lgm1fPiecewiseConstantHullWhiteAdaptor(const Currency& currency, const Handle<TS>& termStructure,
                                           const Array& sigmaTimes, const Array& sigma, const Array& kappaTimes,
                                           const Array& kappa, const std::string& name = std::string());
    Lgm1fPiecewiseConstantHullWhiteAdaptor(const Currency& currency, const Handle<TS>& termStructure,
                                           const std::vector<Date>& sigmaDates, const Array& sigma,
                                           const std::vector<Date>& kappaDates, const Array& kappa,
                                           const std::string& name = std::string());
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    Real alpha(const Time t) const;
    Real kappa(const Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    Real hullWhiteSigma(const Time t) const;
    const Array& parameterTimes(const Size) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;
    void update() const;

protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;

private:
    void initialize(const Array& sigma, const Array& kappa);
};

// implementation

template <class TS>
Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::Lgm1fPiecewiseConstantHullWhiteAdaptor(
    const Currency& currency, const Handle<TS>& termStructure, const Array& sigmaTimes, const Array& sigma,
    const Array& kappaTimes, const Array& kappa, const std::string& name)
    : Lgm1fParametrization<TS>(currency, termStructure, name), PiecewiseConstantHelper3(sigmaTimes, kappaTimes),
      PiecewiseConstantHelper2(kappaTimes) {
    initialize(sigma, kappa);
}

template <class TS>
Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::Lgm1fPiecewiseConstantHullWhiteAdaptor(
    const Currency& currency, const Handle<TS>& termStructure, const std::vector<Date>& sigmaDates, const Array& sigma,
    const std::vector<Date>& kappaDates, const Array& kappa, const std::string& name)
    : Lgm1fParametrization<TS>(currency, termStructure, name),
      PiecewiseConstantHelper3(sigmaDates, kappaDates, termStructure),
      PiecewiseConstantHelper2(kappaDates, termStructure) {
    initialize(sigma, kappa);
}

template <class TS>
void Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::initialize(const Array& sigma, const Array& kappa) {
    QL_REQUIRE(PiecewiseConstantHelper3::t1().size() + 1 == sigma.size(),
               "sigma size (" << sigma.size() << ") inconsistent to times size ("
                              << PiecewiseConstantHelper3::t1().size() << ")");
    QL_REQUIRE(PiecewiseConstantHelper2::t().size() + 1 == kappa.size(),
               "kappa size (" << kappa.size() << ") inconsistent to times size ("
                              << PiecewiseConstantHelper2::t().size() << ")");

    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper3::y1_->size(); ++i) {
        PiecewiseConstantHelper3::y1_->setParam(i, inverse(0, sigma[i]));
    }
    for (Size i = 0; i < PiecewiseConstantHelper3::y2_->size(); ++i) {
        PiecewiseConstantHelper3::y2_->setParam(i, inverse(1, kappa[i]));
    }
    for (Size i = 0; i < PiecewiseConstantHelper2::y_->size(); ++i) {
        PiecewiseConstantHelper2::y_->setParam(i, inverse(1, kappa[i]));
    }
    update();
}

// inline

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::direct(const Size i, const Real x) const {
    return i == 0 ? PiecewiseConstantHelper3::direct1(x) : PiecewiseConstantHelper2::direct(x);
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::inverse(const Size i, const Real y) const {
    return i == 0 ? PiecewiseConstantHelper3::inverse1(y) : PiecewiseConstantHelper2::inverse(y);
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::zeta(const Time t) const {
    return PiecewiseConstantHelper3::int_y1_sqr_exp_2_int_y2(t) / (this->scaling_ * this->scaling_);
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::alpha(const Time t) const {
    return hullWhiteSigma(t) / Hprime(t) / this->scaling_;
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::H(const Time t) const {
    return this->scaling_ * PiecewiseConstantHelper2::int_exp_m_int_y(t) + this->shift_;
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::kappa(const Time t) const {
    return PiecewiseConstantHelper2::y(t);
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::Hprime(const Time t) const {
    return this->scaling_ * PiecewiseConstantHelper2::exp_m_int_y(t);
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::Hprime2(const Time t) const {
    return -this->scaling_ * PiecewiseConstantHelper2::exp_m_int_y(t) * kappa(t);
}

template <class TS> inline Real Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::hullWhiteSigma(const Time t) const {
    return PiecewiseConstantHelper3::y1(t);
}

template <class TS> inline void Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::update() const {
    PiecewiseConstantHelper3::update();
    PiecewiseConstantHelper2::update();
}

template <class TS> inline const Array& Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::parameterTimes(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return PiecewiseConstantHelper3::t1_;
    else
        return PiecewiseConstantHelper2::t_;
}

template <class TS>
inline const boost::shared_ptr<Parameter> Lgm1fPiecewiseConstantHullWhiteAdaptor<TS>::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return PiecewiseConstantHelper3::y1_;
    else
        return PiecewiseConstantHelper2::y_;
}

// typedef

typedef Lgm1fPiecewiseConstantHullWhiteAdaptor<YieldTermStructure> IrLgm1fPiecewiseConstantHullWhiteAdaptor;

} // namespace QuantExt

#endif
