/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file irlgm1fpiecewiselinearparametrization.hpp
    \brief piecewise linear model parametrization
*/

#ifndef quantext_piecewiselinear_irlgm1f_parametrization_hpp
#define quantext_piecewiselinear_irlgm1f_parametrization_hpp

#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {

/*! parametrization with piecewise linear H and zeta,
    w.r.t. zeta this is the same as piecewise constant alpha,
    w.r.t. H this is implemented with a new (helper) parameter
    h > 0, such that H(t) = \int_0^t h(s) ds

    \warning this class is considered experimental, it is not
             tested well and might have conceptual issues
             (e.g. kappa is zero almost everywhere); you
             might rather want to rely on the piecewise
             constant parametrization
*/
class IrLgm1fPiecewiseLinearParametrization
    : public IrLgm1fParametrization,
      private PiecewiseConstantHelper11 {
  public:
    IrLgm1fPiecewiseLinearParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure,
        const Array &alphaTimes, const Array &alpha, const Array &hTimes,
        const Array &h);
    IrLgm1fPiecewiseLinearParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure,
        const std::vector<Date> &alphaDates, const Array &alpha,
        const std::vector<Date> &hDates, const Array &h);
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    Real alpha(const Time t) const;
    Real kappa(Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    const Array &parameterTimes(const Size) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;
    void update() const;

  protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;

  private:
    void initialize(const Array &alpha, const Array &h);
};

// inline

inline Real IrLgm1fPiecewiseLinearParametrization::direct(const Size i,
                                                          const Real x) const {
    return i == 0 ? helper1().direct(x) : helper2().direct(x);
}

inline Real IrLgm1fPiecewiseLinearParametrization::inverse(const Size i,
                                                           const Real y) const {
    return i == 0 ? helper1().inverse(y) : helper2().inverse(y);
}

inline Real IrLgm1fPiecewiseLinearParametrization::zeta(const Time t) const {
    return helper1().int_y_sqr(t) / (scaling_ * scaling_);
}

inline Real IrLgm1fPiecewiseLinearParametrization::H(const Time t) const {
    return scaling_ * helper2().int_y_sqr(t) + shift_;
}

inline Real IrLgm1fPiecewiseLinearParametrization::alpha(const Time t) const {
    return helper1().y(t) / scaling_;
}

inline Real IrLgm1fPiecewiseLinearParametrization::kappa(const Time) const {
    return 0.0; // almost everywhere
}

inline Real IrLgm1fPiecewiseLinearParametrization::Hprime(const Time t) const {
    return scaling_ * helper2().y(t);
}

inline Real IrLgm1fPiecewiseLinearParametrization::Hprime2(const Time) const {
    return 0.0; // almost everywhere
}

inline void IrLgm1fPiecewiseLinearParametrization::update() const {
    helper1().update();
    helper2().update();
}

inline const Array &
IrLgm1fPiecewiseLinearParametrization::parameterTimes(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return helper1().t();
    else
        return helper2().t();
    ;
}

inline const boost::shared_ptr<Parameter>
IrLgm1fPiecewiseLinearParametrization::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return helper1().p();
    else
        return helper2().p();
}

} // namespace QuantExt

#endif
