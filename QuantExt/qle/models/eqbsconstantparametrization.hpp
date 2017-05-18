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

/*! \file fxbspiecewiseconstantparametrization.hpp
    \brief piecewise constant model parametrization
    \ingroup models
*/

#ifndef quantext_constant_eqbs_parametrization_hpp
#define quantext_constant_eqbs_parametrization_hpp

#include <qle/models/eqbsparametrization.hpp>

namespace QuantExt {
//! EQ Black Scholes parametrization
/*! EQ Black Scholes parametrization, with constant volatility
    \ingroup models
*/
class EqBsConstantParametrization : public EqBsParametrization {
public:
    /*! The currency refers to the equity currency, the
        spots are as of today (i.e. the discounted spot) */
    EqBsConstantParametrization(const Currency& currency, const std::string& eqName, const Handle<Quote>& eqSpotToday,
                                const Handle<Quote>& fxSpotToday, const Real sigma,
                                const Handle<YieldTermStructure>& eqIrCurveToday,
                                const Handle<YieldTermStructure>& eqDivYieldCurveToday);
    Real variance(const Time t) const;
    Real sigma(const Time t) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;

protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size i, const Real y) const;

private:
    const boost::shared_ptr<PseudoParameter> sigma_;
};

// inline

inline Real EqBsConstantParametrization::direct(const Size, const Real x) const { return x * x; }

inline Real EqBsConstantParametrization::inverse(const Size, const Real y) const { return std::sqrt(y); }

inline Real EqBsConstantParametrization::variance(const Time t) const {
    return direct(0, sigma_->params()[0]) * direct(0, sigma_->params()[0]) * t;
}

inline Real EqBsConstantParametrization::sigma(const Time) const { return direct(0, sigma_->params()[0]); }

inline const boost::shared_ptr<Parameter> EqBsConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return sigma_;
}

} // namespace QuantExt

#endif
