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

#ifndef quantext_constant_fxbs_parametrization_hpp
#define quantext_constant_fxbs_parametrization_hpp

#include <qle/models/fxbsparametrization.hpp>

namespace QuantExt {
//! FX Black Scholes parametrization
/*! FX Black Scholes parametrization, with constant volatility
    \ingroup models
*/
class FxBsConstantParametrization : public FxBsParametrization {
public:
    /*! The currency refers to the foreign currency, the
        spot is as of today (i.e. the discounted spot) */
    FxBsConstantParametrization(const Currency& currency, const Handle<Quote>& fxSpotToday, const Real sigma);
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

inline Real FxBsConstantParametrization::direct(const Size, const Real x) const { return x * x; }

inline Real FxBsConstantParametrization::inverse(const Size, const Real y) const { return std::sqrt(y); }

inline Real FxBsConstantParametrization::variance(const Time t) const {
    return direct(0, sigma_->params()[0]) * direct(0, sigma_->params()[0]) * t;
}

inline Real FxBsConstantParametrization::sigma(const Time) const { return direct(0, sigma_->params()[0]); }

inline const boost::shared_ptr<Parameter> FxBsConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return sigma_;
}

} // namespace QuantExt

#endif
