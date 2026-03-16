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

/*! \file irlgm1fpiecewiselinearparametrization.hpp
    \brief piecewise linear model parametrization
    \ingroup models
*/

#ifndef quantext_piecewiselinear_irlgm1f_parametrization_hpp
#define quantext_piecewiselinear_irlgm1f_parametrization_hpp

#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {
//! Lgm 1f Piecewise Linear Parametrization
/*! parametrization with piecewise linear H and zeta,
    w.r.t. zeta this is the same as piecewise constant alpha,
    w.r.t. H this is implemented with a new (helper) parameter
    h > 0, such that \f$H(t) = \int_0^t h(s) ds\f$

    \warning this class is considered experimental, it is not
             tested well and might have conceptual issues
             (e.g. kappa is zero almost everywhere); you
             might rather want to rely on the piecewise
             constant parametrization

    \ingroup models
*/

template <class TS>
class Lgm1fPiecewiseLinearParametrization : public Lgm1fParametrization<TS>, private PiecewiseConstantHelper11 {
public:
    Lgm1fPiecewiseLinearParametrization(
        const Currency& currency, const Handle<TS>& termStructure, const Array& alphaTimes, const Array& alpha,
        const Array& hTimes, const Array& h, const std::string& name = std::string(),
        const QuantLib::ext::shared_ptr<QuantLib::Constraint>& alphaConstraint = QuantLib::ext::make_shared<QuantLib::NoConstraint>(),
        const QuantLib::ext::shared_ptr<QuantLib::Constraint>& hConstraint = QuantLib::ext::make_shared<QuantLib::NoConstraint>());
    Lgm1fPiecewiseLinearParametrization(
        const Currency& currency, const Handle<TS>& termStructure, const std::vector<Date>& alphaDates,
        const Array& alpha, const std::vector<Date>& hDates, const Array& h, const std::string& name = std::string(),
        const QuantLib::ext::shared_ptr<QuantLib::Constraint>& alphaConstraint = QuantLib::ext::make_shared<QuantLib::NoConstraint>(),
        const QuantLib::ext::shared_ptr<QuantLib::Constraint>& hConstraint = QuantLib::ext::make_shared<QuantLib::NoConstraint>());

    Real zeta(const Time t) const override;
    Real H(const Time t) const override;
    Real alpha(const Time t) const override;
    Real kappa(const Time t) const override;
    Real Hprime(const Time t) const override;
    Real Hprime2(const Time t) const override;
    const Array& parameterTimes(const Size) const override;
    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const override;
    void update() const override;

protected:
    Real direct(const Size i, const Real x) const override;
    Real inverse(const Size j, const Real y) const override;

private:
    void initialize(const Array& alpha, const Array& h);
};

// implementation

template <class TS>
Lgm1fPiecewiseLinearParametrization<TS>::Lgm1fPiecewiseLinearParametrization(
    const Currency& currency, const Handle<TS>& termStructure, const Array& alphaTimes, const Array& alpha,
    const Array& hTimes, const Array& h, const std::string& name,
    const QuantLib::ext::shared_ptr<QuantLib::Constraint>& alphaConstraint,
    const QuantLib::ext::shared_ptr<QuantLib::Constraint>& hConstraint)
    : Lgm1fParametrization<TS>(currency, termStructure, name),
      PiecewiseConstantHelper11(alphaTimes, hTimes, alphaConstraint, hConstraint) {
    initialize(alpha, h);
}

template <class TS>
Lgm1fPiecewiseLinearParametrization<TS>::Lgm1fPiecewiseLinearParametrization(
    const Currency& currency, const Handle<TS>& termStructure, const std::vector<Date>& alphaDates, const Array& alpha,
    const std::vector<Date>& hDates, const Array& h, const std::string& name,
    const QuantLib::ext::shared_ptr<QuantLib::Constraint>& alphaConstraint,
    const QuantLib::ext::shared_ptr<QuantLib::Constraint>& hConstraint)
    : Lgm1fParametrization<TS>(currency, termStructure, name),
      PiecewiseConstantHelper11(alphaDates, hDates, termStructure, alphaConstraint, hConstraint) {
    initialize(alpha, h);
}

template <class TS> void Lgm1fPiecewiseLinearParametrization<TS>::initialize(const Array& alpha, const Array& h) {
    QL_REQUIRE(helper1().t().size() + 1 == alpha.size(),
               "alpha size (" << alpha.size() << ") inconsistent to times size (" << helper1().t().size() << ")");
    QL_REQUIRE(helper2().t().size() + 1 == h.size(),
               "h size (" << h.size() << ") inconsistent to times size (" << helper1().t().size() << ")");
    // store raw parameter values
    for (Size i = 0; i < helper1().p()->size(); ++i) {
        helper1().p()->setParam(i, inverse(0, alpha[i]));
    }
    for (Size i = 0; i < helper2().p()->size(); ++i) {
        helper2().p()->setParam(i, inverse(1, h[i]));
    }
    update();
}

// inline

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::direct(const Size i, const Real x) const {
    return i == 0 ? helper1().direct(x) : helper2().direct(x);
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::inverse(const Size i, const Real y) const {
    return i == 0 ? helper1().inverse(y) : helper2().inverse(y);
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::zeta(const Time t) const {
    return helper1().int_y_sqr(t) / (this->scaling_ * this->scaling_);
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::H(const Time t) const {
    return this->scaling_ * helper2().int_y_sqr(t) + this->shift_;
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::alpha(const Time t) const {
    return helper1().y(t) / this->scaling_;
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::kappa(const Time) const {
    return 0.0; // almost everywhere
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::Hprime(const Time t) const {
    return this->scaling_ * helper2().y(t);
}

template <class TS> inline Real Lgm1fPiecewiseLinearParametrization<TS>::Hprime2(const Time) const {
    return 0.0; // almost everywhere
}

template <class TS> inline void Lgm1fPiecewiseLinearParametrization<TS>::update() const {
    Lgm1fParametrization<TS>::update();
    helper1().update();
    helper2().update();
}

template <class TS> inline const Array& Lgm1fPiecewiseLinearParametrization<TS>::parameterTimes(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return helper1().t();
    else
        return helper2().t();
    ;
}

template <class TS>
inline const QuantLib::ext::shared_ptr<Parameter> Lgm1fPiecewiseLinearParametrization<TS>::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return helper1().p();
    else
        return helper2().p();
}

// typedef

typedef Lgm1fPiecewiseLinearParametrization<YieldTermStructure> IrLgm1fPiecewiseLinearParametrization;

} // namespace QuantExt

#endif
