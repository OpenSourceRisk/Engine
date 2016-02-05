/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file fxbspiecewiseconstantparametrization.hpp
    \brief piecewise constant model parametrization
*/

#ifndef quantext_constant_fxbs_parametrization_hpp
#define quantext_constant_fxbs_parametrization_hpp

#include <qle/models/fxbsparametrization.hpp>

namespace QuantExt {

/*! FX Black Scholes parametrization, with constant volatility */
class FxBsConstantParametrization : public FxBsParametrization {
  public:
    /*! The currency refers to the foreign currency, the
        spot is as of today (i.e. the discounted spot) */
    FxBsConstantParametrization(const Currency &currency,
                                const Handle<Quote> &fxSpotToday,
                                const Real sigma);
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

inline Real FxBsConstantParametrization::direct(const Size,
                                                const Real x) const {
    return x * x;
}

inline Real FxBsConstantParametrization::inverse(const Size,
                                                 const Real y) const {
    return std::sqrt(y);
}

inline Real FxBsConstantParametrization::variance(const Time t) const {
    return direct(0, sigma_->params()[0]) * direct(0, sigma_->params()[0]) * t;
}

inline Real FxBsConstantParametrization::sigma(const Time t) const {
    return direct(0, sigma_->params()[0]);
}

inline const boost::shared_ptr<Parameter>
FxBsConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return sigma_;
}

} // namespace QuantExt

#endif
