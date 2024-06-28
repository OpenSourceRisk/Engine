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

#include <qle/models/hwparametrization.hpp>
#include <qle/models/irmodel.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/commoditymodel.hpp>

#include <ql/math/comparison.hpp>

namespace QuantExt {

/*! Schwartz (1997) one-factor model of the commodity price termstructure
    with two constant parameters, sigma and kappa

    Price curve dynamics (Martingale)
      dF(t,T) / F(t,T) = sigma * exp(-kappa * (T-t)) * dW

    Model-implied price curve:
        F(t,T) = F(0,T) * exp{ X(t) * exp(-kappa*(T-t))) - 1/2 * (V(0,T)-V(t,T)) }, 
    with    
        V(t,T) = sigma^2 * (1 - exp(-2*kappa*(T-t))) / (2*kappa),
    and
        dX(t) = -kappa * X(t) * dt + sigma * dW(t), X(0) = 0

    In terms of drift-free state variable Y(t) = exp(kappa*t) * X(t):
        F(t,T) = F(0,T) * exp{ Y(t) * exp(-kappa*T)) - 1/2 * (V(0,T)-V(t,T)) }, 
        dY(t) = sigma * exp(kappa * t) dW(t), Y(0) = 0
*/
class CommoditySchwartzModel : public CommodityModel {
public:
    enum class Discretization { Euler, Exact };

    CommoditySchwartzModel(const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>& parametrization, 
            const Discretization discretization = Discretization::Euler);

    // CommodityModel interface

    const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const override { return parametrization_; }

    Handle<PriceTermStructure> termStructure() const override { return parametrization_->priceCurve(); }

    const Currency& currency() const override { return parametrization_->currency(); }

    Size n() const override { return 1; }
    Size m() const override { return 1; }

    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override { return stateProcess_; }

    QuantLib::Real forwardPrice(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
                                const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve
                                = QuantLib::Handle<QuantExt::PriceTermStructure>()) const override;

    //! Schwartz model specific methods
    const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> parametrization() const { return parametrization_; }

    //! observer and linked calibrated model interface
    void update() override;
    void generateArguments() override;

private:
    QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> parametrization_;
    Discretization discretization_;
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess_;
};

} // namespace QuantExt
