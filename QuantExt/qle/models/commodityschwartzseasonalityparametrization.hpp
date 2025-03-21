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

#ifndef quantext_com_schwartz_seasonality_parametrization_hpp
#define quantext_com_schwartz_seasonality_parametrization_hpp

#include <ql/handle.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
//#include <qle/models/parametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>

namespace QuantExt {
//! COM Schwartz parametrization
/*! COM parametrization for the Schwartz (1997) mean-reverting one-factor model
    with log-normal forward price dynamics and forward volatility sigma * exp(-kappa*(T-t)):
    dF(t,T) / F(t,T) = sigma * exp(a(T)) * exp(-kappa * (T-t)) * dW    

    The model can be propagated in terms of an artificial spot price process of the form 
    S(t) = A(t) * exp(B(t) * X(t))
    where    
        dX(t) = -kappa * X(t) * dt + sigma * dW(t)
        X(t) - X(s) = -X(s) * (1 - exp(-kappa*(t-s)) + int_s^t sigma * exp(-kappa*(t-u)) dW(u)
        E[X(t)|s] = X(s) * exp(-kappa*(t-s))
        Var[X(t)-X(s)|s] = sigma^2 * (1 - exp(-2*kappa*(t-s))) / (2*kappa)

    The stochastic future price curve in terms of X(t) is
        F(t,T) = F(0,T) * exp( X(t) * exp(a(T) - kappa*(T-t)) - 1/2 * (V(0,T) - V(t,T))
    with
        V(t,T) = sigma^2 * exp(a(T)) * (1 - exp(-2*kappa*(T-t))) / (2*kappa)
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
        F(t,T) = F(0,t) * exp( Y(t) * exp(a(T) -kappa*T) - 1/2 * (V(0,T) - V(t,T))

    \ingroup models
*/
class CommoditySchwartzSeasonalityParametrization : public CommoditySchwartzParametrization,
                                                    private PiecewiseConstantHelper1 {
public:
    /*! The currency refers to the commodity currency, the
        fx spot is as of today (i.e. the discounted spot) */
        CommoditySchwartzSeasonalityParametrization(const Currency& currency, const std::string& name,
                                    const Handle<QuantExt::PriceTermStructure>& priceCurve,
                                    const Handle<Quote>& fxSpotToday, const Real sigma, const Real kappa, 
                                    const Array& aTimes, const Array& a, 
                                    const QuantLib::ext::shared_ptr<QuantLib::Constraint>& aConstraint = QuantLib::ext::make_shared<QuantLib::NoConstraint>(),
                                    bool driftFreeState = false);

    Size numberOfParameters() const override { return 3; }
    //! Inspector for the current value of model parameter m(T) (direct)
    Real m(const QuantLib::Time t) const;
    //! Inspector for current value of the model parameter vector (inverse values)
    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const override;
    //! Variance V(t,T) used in the computation of F(t,T)
    Real VtT(Real t, Real T) override;

    //bool driftFreeState() const { return driftFreeState_; }
    void update() const override;
protected:
    Real direct(const Size i, const Real x) const override;
    Real inverse(const Size i, const Real y) const override;

private:
    void initialize(const Array& a);
};

// inline
inline void CommoditySchwartzSeasonalityParametrization::initialize(const Array& a) {
    QL_REQUIRE(PiecewiseConstantHelper1::t().size() + 1 == a.size(),
               "a size (" << a.size() << ") inconsistent to times size ("
                              << PiecewiseConstantHelper1::t().size() << ")");
    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper1::y_->size(); ++i) {
        PiecewiseConstantHelper1::y_->setParam(i, inverse(0, a[i]));
    }
    update();
}

inline void CommoditySchwartzSeasonalityParametrization::update() const {
    Parametrization::update();
    PiecewiseConstantHelper1::update();
}

inline Real CommoditySchwartzSeasonalityParametrization::direct(const Size i, const Real x) const { 
    if (i==0 || i==1)
        return x * x; 
    else
        return PiecewiseConstantHelper1::direct(x);
}

inline Real CommoditySchwartzSeasonalityParametrization::inverse(const Size i, const Real y) const {   
    if (i==0 || i==1)
        return std::sqrt(y); 
    else
        return PiecewiseConstantHelper1::inverse(y);
}

inline Real CommoditySchwartzSeasonalityParametrization::m(const QuantLib::Time t) const{
    return std::exp(PiecewiseConstantHelper1::y(t));
}

inline const QuantLib::ext::shared_ptr<Parameter> CommoditySchwartzSeasonalityParametrization::parameter(const Size i) const {
    QL_REQUIRE(i < 3, "parameter " << i << " does not exist, only have 0, 1 and 2");
    if (i == 0)
        return sigma_;
    else if (i == 1)
        return kappa_;
    else
        return PiecewiseConstantHelper1::y_;
}

} // namespace QuantExt

#endif
