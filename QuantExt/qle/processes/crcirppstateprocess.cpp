/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <ql/math/distributions/chisquaredistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/processes/eulerdiscretization.hpp>
#include <qle/models/crcirpp.hpp>
#include <qle/processes/crcirppstateprocess.hpp>

namespace QuantExt {

CrCirppStateProcess::CrCirppStateProcess(CrCirpp* const model,
                                         CrCirppStateProcess::Discretization disc)
    : StochasticProcess(QuantLib::ext::shared_ptr<StochasticProcess::discretization>(new EulerDiscretization)), model_(model),
      discretization_(disc) {}

Size CrCirppStateProcess::size() const { return 2; }

Array CrCirppStateProcess::initialValues() const {
    Array res(size(), 0.0);
    res[0] = model_->parametrization()->y0(0); // y0
    res[1] = 1.0; // S(0,0) = 1
    return res;
}

Array CrCirppStateProcess::drift(Time t, const Array& x) const {
    QL_FAIL("not implemented");
}

Matrix CrCirppStateProcess::diffusion(Time t, const Array& x) const {
    QL_FAIL("not implemented");
}

Array CrCirppStateProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {
    Array retVal(size());
    Real kappa, theta, sigma, y0;
    kappa = model_->parametrization()->kappa(t0);
    theta = model_->parametrization()->theta(t0);
    sigma = model_->parametrization()->sigma(t0);
    y0 = model_->parametrization()->y0(t0);

    const Real sdt = std::sqrt(dt);
    switch (discretization_) {

    case BrigoAlfonsi: {
        // see D. Brigo and F. Mercurio. Interest Rate Models: Theory and Practice, 2nd
        // Edition. Springer, 2006.
        // Ensures non-negative values for \sigma^2 \leq 2*\kappa*\theta
        Real temp = (1 - kappa / 2.0 * dt);
        Real temp2 = temp * std::sqrt(x0[0]) + sigma * sdt * dw[0] / (2.0 * temp);
        retVal[0] = temp2 * temp2 + (kappa * theta - sigma * sigma / 4) * dt;
        break;
    }
    default:
        QL_FAIL("unknown discretization schema");
    }

    // second element is S(0,i)
    Real SM_ti = model_->defaultCurve()->survivalProbability(t0 + dt);
    Real SM_ti_pre = model_->defaultCurve()->survivalProbability(t0);
    Real Pcir_ti = model_->zeroBond(0, t0 + dt, y0);
    Real Pcir_ti_pre = model_->zeroBond(0, t0, y0);
    retVal[1] = x0[1] * SM_ti / SM_ti_pre * Pcir_ti_pre / Pcir_ti * exp(-x0[0] * dt);

    return retVal;
}

} // namespace QuantExt
