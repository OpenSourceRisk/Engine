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

/*! \file commodityschwartzmodel.hpp
    \brief Schwartz (1997) one-factor model of the commodity price termstructure
    \ingroup models
*/

#pragma once

#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/commoditymodel.hpp>

#include <ql/math/comparison.hpp>

namespace QuantExt {

/*! Schwartz (1997) one-factor model of the commodity price termstructure
    with two constant parameters, sigma and kappa

    Price curve dynamics (Martingale)
      dF(t,T) / F(t,T) = sigma * exp(-kappa * (T-t)) * dW

    Model-implied price curve:
        F(t,T) = F(0,T) * exp{ Y(t) * exp(-kappa*T)) - 1/2 * (V(0,T)-V(t,T)) }, 
    with    
        V(t,T) = sigma^2 * (1 - exp(-2*kappa*(T-t))) / (2*kappa),
    state variable
        Y(t) = int_0^t sigma * exp(kappa * s) * dW(s),
    zero mean and variance
        Var(Y(t)) = sigma^2 * (exp(2*kappa*t) - 1) / (2*kappa)   
*/
class CommoditySchwartzModel : public CommodityModel {
public:
    enum class Discretization { Euler, Exact };

    CommoditySchwartzModel(const boost::shared_ptr<CommoditySchwartzParametrization>& parametrization, 
            const Discretization discretization = Discretization::Euler);

    // CommodityModel interface

    const boost::shared_ptr<Parametrization> parametrizationBase() const override { return parametrization_; }

    Size n() const override { return 1; }
    Size m() const override { return 1; }

    boost::shared_ptr<StochasticProcess> stateProcess() const override { return stateProcess_; }

    QuantLib::Real forwardPrice(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
                                const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve
                                = QuantLib::Handle<QuantExt::PriceTermStructure>()) const override;

    // Schwartz model specific methods

    const boost::shared_ptr<CommoditySchwartzParametrization> parametrization() const { return parametrization_; }

    /*! observer and linked calibrated model interface */
    void update() override;
    void generateArguments() override;

private:
    boost::shared_ptr<CommoditySchwartzParametrization> parametrization_;
    Discretization discretization_;
    boost::shared_ptr<StochasticProcess> stateProcess_;
};

} // namespace QuantExt
