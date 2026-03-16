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

/*! \file parametrization.hpp
    \brief base class for model parametrizations
    \ingroup models
*/

#ifndef quantext_model_parametrization_hpp
#define quantext_model_parametrization_hpp

#include <ql/currency.hpp>
#include <ql/math/array.hpp>
#include <qle/models/pseudoparameter.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Parametrization
/*! Base class for classes representing model parameters. There is a disctinction between "actual" and "raw"
    parameters. The "actual" parameter value is the true value of the parameter, e.g. 0.20 to represent a black scholes
    volatility of 20%. The "raw" parameter is derived from the actual parameter by applying a transformation

    actual value = direct( raw value )
    raw value    = inverse( actual value )

    The idea behind that is that the optimization during a model calibration can be performed as an unconstrained
    optimization which usually works more stable and is faster than a constrained optimization. For example, to ensure
    a positive black volatility one can use the transformation

    direct ( x ) = x * x

    To ensure a valid correlation one can use the transformation

    direct ( x ) = (atan( x ) + pi / 2) / pi

    and so forth. To implement you own transformation you can overwrite the direct() and inverse() methods. The default
    implementation of these methods represents the trivial transformation (identity, i.e. direct( x ) = x ).

    \ingroup models
 */
class Parametrization {
public:
    Parametrization(const Currency& currency, const std::string& name = "");
    virtual ~Parametrization() {}

    /*! the currency associated to this parametrization */
    virtual const Currency& currency() const;

    /*! the times associated to parameter i */
    virtual const Array& parameterTimes(const Size) const;

    /*! the number of parameters in this parametrization */
    virtual Size numberOfParameters() const { return 0; }

    /*! the actual parameter values */
    virtual Array parameterValues(const Size) const;

    /*! the parameter storing the raw parameter values */
    virtual const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const;

    /*! this method should be called when input parameters
        linked via references or pointers change in order
        to ensure consistent results */
    virtual void update() const;

    /*! return a name (inflation index, equity name, credit name, etc.) */
    const std::string& name() const { return name_; }

    /*! transformations between raw and actual parameters */
    virtual Real direct(const Size, const Real x) const;
    virtual Real inverse(const Size, const Real y) const;

protected:
    /*! step size for numerical differentiation */
    const Real h_, h2_;
    /*! adjusted central difference scheme */
    Time tr(const Time t) const;
    Time tl(const Time t) const;
    Time tr2(const Time t) const;
    Time tm2(const Time t) const;
    Time tl2(const Time t) const;

private:
    Currency currency_;
    std::string name_;
    const Array emptyTimes_;
    const QuantLib::ext::shared_ptr<Parameter> emptyParameter_;
};

// inline

inline void Parametrization::update() const {}

inline Time Parametrization::tr(const Time t) const { return t > 0.5 * h_ ? t + 0.5 * h_ : h_; }

inline Time Parametrization::tl(const Time t) const { return std::max(t - 0.5 * h_, 0.0); }

inline Time Parametrization::tr2(const Time t) const { return t > h2_ ? t + h2_ : 2.0 * h2_; }

inline Time Parametrization::tm2(const Time t) const { return t > h2_ ? t : h2_; }

inline Time Parametrization::tl2(const Time t) const { return std::max(t - h2_, 0.0); }

inline Real Parametrization::direct(const Size, const Real x) const { return x; }

inline Real Parametrization::inverse(const Size, const Real y) const { return y; }

inline const Currency& Parametrization::currency() const { return currency_; }

inline const Array& Parametrization::parameterTimes(const Size) const { return emptyTimes_; }

inline const QuantLib::ext::shared_ptr<Parameter> Parametrization::parameter(const Size) const { return emptyParameter_; }

inline Array Parametrization::parameterValues(const Size i) const {
    const Array& tmp = parameter(i)->params();
    Array res(tmp.size());
    for (Size ii = 0; ii < res.size(); ++ii) {
        res[ii] = direct(i, tmp[ii]);
    }
    return res;
}

} // namespace QuantExt

#endif
