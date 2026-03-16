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

#include <qle/processes/commodityschwartzstateprocess.hpp>

#include <ql/processes/eulerdiscretization.hpp>

namespace QuantExt {

CommoditySchwartzStateProcess::CommoditySchwartzStateProcess(
    const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>& parametrization,
    const CommoditySchwartzModel::Discretization discretization)
    : StochasticProcess1D(discretization == CommoditySchwartzModel::Discretization::Euler
                              ? QuantLib::ext::make_shared<EulerDiscretization>()
                              : QuantLib::ext::static_pointer_cast<StochasticProcess1D::discretization>(
                                    QuantLib::ext::make_shared<ExactDiscretization>(parametrization))),
      p_(parametrization) {}

Real CommoditySchwartzStateProcess::drift(Time t, Real x0) const {
    if (p_->driftFreeState())
        return 0.0;
    else
        return -x0 * p_->kappaParameter();
}

Real CommoditySchwartzStateProcess::diffusion(Time t, Real) const { return p_->sigma(t); }

Real CommoditySchwartzStateProcess::ExactDiscretization::drift(const StochasticProcess1D& p, Time t, Real x0,
                                                               Time dt) const {
    if (p_->driftFreeState())
        return 0.0;
    else {
        // Ornstein-Uhlenbeck expectation
        return x0 * (exp(-p_->kappaParameter() * dt) - 1.0);
    }
}

Real CommoditySchwartzStateProcess::ExactDiscretization::variance(const StochasticProcess1D& p, Time t0, Real x0,
                                                                  Time dt) const {
    if (p_->driftFreeState())
        return p_->variance(t0 + dt) - p_->variance(t0);
    else {
        // Ornstein-Uhlenbeck variance
        Real kappa = p_->kappaParameter();
        Real sigma = p_->sigmaParameter();
        return sigma * sigma * (1.0 - std::exp(-2.0 * kappa * dt)) / (2.0 * kappa);
    }
}

Real CommoditySchwartzStateProcess::ExactDiscretization::diffusion(const StochasticProcess1D& p, Time t0, Real x0,
                                                                   Time dt) const {
    return std::sqrt(variance(p, t0, x0, dt));
}

} // namespace QuantExt
