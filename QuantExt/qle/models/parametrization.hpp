/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file parametrization.hpp
    \brief base class for model parametrizations
*/

#ifndef quantext_model_parametrization_hpp
#define quantext_model_parametrization_hpp

#include <ql/currency.hpp>
#include <ql/math/array.hpp>
#include <qle/models/pseudoparameter.hpp>

using namespace QuantLib;

namespace QuantExt {

class Parametrization {
  public:
    Parametrization(const Currency &currency);

    virtual const Currency currency() const;

    virtual const Array &parameterTimes(const Size) const;

    /*! these are the actual (real) parameter values in contrast
        to the raw values which are stored in Parameter::params_
        and on which the optimization is done; there might be
        a transformation between real and raw values in
        order to implement a constraint (this is generally
        preferable to using a hard constraint) */
    virtual Disposable<Array> parameterValues(const Size) const;

    /*! the parameter storing the raw values */
    virtual const boost::shared_ptr<Parameter> parameter(const Size) const;

    /*! this method should be called when input parameters
        linked via references or pointers change in order
        to ensure consistent results */
    virtual void update() const;

  protected:
    /*! step size for numerical differentiation */
    const Real h_, h2_;
    /*! adjusted central difference scheme */
    Time tr(const Time t) const;
    Time tl(const Time t) const;
    Time tr2(const Time t) const;
    Time tm2(const Time t) const;
    Time tl2(const Time t) const;
    /*! transformations between raw and real parameters */
    virtual Real direct(const Size, const Real x) const;
    virtual Real inverse(const Size, const Real y) const;

  private:
    Currency currency_;
    const Array emptyTimes_;
    const boost::shared_ptr<Parameter> emptyParameter_;
};

// inline

inline void Parametrization::update() const {}

inline Time Parametrization::tr(const Time t) const {
    return t > 0.5 * h_ ? t + 0.5 * h_ : h_;
}

inline Time Parametrization::tl(const Time t) const {
    return std::max(t - 0.5 * h_, 0.0);
}

inline Time Parametrization::tr2(const Time t) const {
    return t > h2_ ? t + h2_ : 2.0 * h2_;
}

inline Time Parametrization::tm2(const Time t) const {
    return t > h2_ ? t : h2_;
}

inline Time Parametrization::tl2(const Time t) const {
    return std::max(t - h2_, 0.0);
}

inline Real Parametrization::direct(const Size, const Real x) const {
    return x;
}

inline Real Parametrization::inverse(const Size, const Real y) const {
    return y;
}

inline const Currency Parametrization::currency() const { return currency_; }

inline const Array &Parametrization::parameterTimes(const Size) const {
    return emptyTimes_;
}

inline const boost::shared_ptr<Parameter>
Parametrization::parameter(const Size) const {
    return emptyParameter_;
}

inline Disposable<Array> Parametrization::parameterValues(const Size i) const {
    const Array &tmp = parameter(i)->params();
    Array res(tmp.size());
    for (Size i = 0; i < res.size(); ++i) {
        res[i] = direct(i, tmp[i]);
    }
    return res;
}

} // namespace QuantExt

#endif
