/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file commodityschwartzparametrization.hpp
    \brief Schwartz commodity model parametrization
    \ingroup models
*/

#ifndef quantext_com_schwartz_parametrization_hpp
#define quantext_com_schwartz_parametrization_hpp

#include <ql/handle.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {
//! COM Schwartz parametrization
/*! COM parametrization for the Schwartz (1997) mean-reverting one-factor model
    with log-normal forward price dynamics and forward volatility sigma * exp(-kappa*(T-t)):
    dF(t,T) / F(t,T) = sigma * exp(-kappa * (T-t)) * dW    

    The model can be propagated in terms of an artificial spot price process of the form 
    S(t) = A(t) * exp(B(t) * X(t))
    where    
        dX(t) = -kappa * X(t) * dt + sigma * dW(t)
        X(t) - X(s) = -X(s) * (1 - exp(-kappa*(t-s)) + int_s^t sigma * exp(-kappa*(t-u)) dW(u)
        E[X(t)|s] = X(s) * exp(-kappa*(t-s))
        Var[X(t)-X(s)|s] = sigma^2 * (1 - exp(-2*kappa*(t-s))) / (2*kappa)

    The stochastic future price curve in terms of X(t) is
        F(t,T) = F(0,T) * exp( X(t) * exp(-kappa*(T-t) - 1/2 * (V(0,T) - V(t,T))
    with
        V(t,T) = sigma^2 * (1 - exp(-2*kappa*(T-t))) / (2*kappa)
    and
        Var[ln F(T,T)] = VaR[X(T)] 

    Instead of state variable X we can use 
        Y(t) = exp(kappa * t) * X(t) 
    with drift-free
        dY(t) = sigma * exp(kappa * t) * dW
        Y(t) = int_0^t sigma * exp(kappa * s) * dW(s)
        Var[Y(t)] = sigma^2 * (exp(2*kappa*t) - 1) / (2*kappa)
        Var[Y(t)-Y(s)|s] = int_s^t sigma * exp(kappa * u) * dW(u) = Var[Y(t)] - Var[Y(s)]
    The stochastic future price curve in terms of Y(t) is 
        F(t,T) = F(0,t) * exp( Y(t) * exp(-kappa*T) - 1/2 * (V(0,T) - V(t,T))

    \ingroup models
*/
class CommoditySchwartzParametrization : public Parametrization {
public:
    /*! The currency refers to the commodity currency, the
        fx spot is as of today (i.e. the discounted spot) */
    CommoditySchwartzParametrization(const Currency& currency, const std::string& name,
                                     const Handle<QuantExt::PriceTermStructure>& priceCurve,
                                     const Handle<Quote>& fxSpotToday, const Real sigma, const Real kappa,
                                     bool driftFreeState = false);

    Size numberOfParameters() const override { return 2; }
    //! State variable variance on [0, t]
    Real variance(const Time t) const;
    //! State variable Y's diffusion at time t: sigma * exp(kappa * t)
    Real sigma(const Time t) const;
    //! Inspector for the current value of model parameter sigma (direct)
    Real sigmaParameter() const;
    //! Inspector for the current value of model parameter kappa (direct)
    Real kappaParameter() const;
    //! Inspector for current value of the model parameter vector (inverse values)
    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const override;
    //! Inspector for today's price curve
    Handle<QuantExt::PriceTermStructure> priceCurve() { return priceCurve_; }
    //! Variance V(t,T) used in the computation of F(t,T)
    Real VtT(Real t, Real T);

    bool driftFreeState() const { return driftFreeState_; }
    
protected:
    Real direct(const Size i, const Real x) const override;
    Real inverse(const Size i, const Real y) const override;

private:

    const Handle<QuantExt::PriceTermStructure> priceCurve_;
    const Handle<Quote> fxSpotToday_;
    std::string comName_;
    const QuantLib::ext::shared_ptr<PseudoParameter> sigma_;
    const QuantLib::ext::shared_ptr<PseudoParameter> kappa_;
    bool driftFreeState_;
};

// inline

inline Real CommoditySchwartzParametrization::direct(const Size, const Real x) const { return x * x; }

inline Real CommoditySchwartzParametrization::inverse(const Size, const Real y) const { return std::sqrt(y); }

inline Real CommoditySchwartzParametrization::variance(const Time t) const {
    Real sig = direct(0, sigma_->params()[0]);
    Real kap = direct(0, kappa_->params()[0]);
    if (kap < QL_EPSILON)
        return sig * sig * t;
    else if (driftFreeState_)
        return sig * sig * (std::exp(2.0 * kap * t) - 1.0) / (2.0 * kap);
    else
        return sig * sig * (1.0 - std::exp(-2.0 * kap * t)) / (2.0 * kap);
}

inline Real CommoditySchwartzParametrization::sigma(const Time u) const {
    Real sig = direct(0, sigma_->params()[0]);
    Real kap = direct(0, kappa_->params()[0]);
    if (driftFreeState_)
        return sig * std::exp(kap * u);
    else
        return sig;
}

inline Real CommoditySchwartzParametrization::sigmaParameter() const {
    return direct(0, sigma_->params()[0]);
}

inline Real CommoditySchwartzParametrization::kappaParameter() const {
    return direct(0, kappa_->params()[0]);
}

inline const QuantLib::ext::shared_ptr<Parameter> CommoditySchwartzParametrization::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0 and 1");
    if (i == 0)
        return sigma_;
    else
        return kappa_;
}

} // namespace QuantExt

#endif
