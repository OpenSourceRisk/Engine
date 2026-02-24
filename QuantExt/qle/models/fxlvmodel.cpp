/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file fxlvmodel.hpp
    \brief fx lv model
    \ingroup models
*/

#include <qle/models/fxlvmodel.hpp>

namespace QuantExt {

FxLvModel::FxLvModel(const QuantLib::ext::shared_ptr<FxLvParametrization>& parametrization, const Discretization disc)
    : parametrization_(parametrization), disc_(disc) {
    QL_REQUIRE(parametrization != nullptr, "FxLvModel: parametrization is null");
}

double FxLvModel::volatility(const Time t, const Array& s) const { return parametrization_->sigma(t, s[0]); }

Array FxLvModel::marginalStep(const Time t0, const Array& x0, const Time dt, const Array& dw, const Real r_dom,
                              const Real r_for, const std::optional<Discretization> disc) const {
    Discretization effDisc = disc ? *disc : disc_;
    if (effDisc == Discretization::Euler) {

        // log-euler scheme

        Real sigma = parametrization_->sigma(t0, x0[0]);
        return x0 + (r_dom - r_for - 0.5 * sigma * sigma) * dt + sigma * std::sqrt(dt) * dw[0];

    } else if (effDisc == Discretization::PC) {

        // predictor-corrector scheme, see Kloeden et. al. 1994, or ssrn-3406231

        Real sigma = parametrization_->sigma(t0, x0[0]);
        Array p = x0 + (r_dom - r_for - 0.5 * sigma * sigma) * dt + sigma * std::sqrt(dt) * dw[0];

        Real sigma2 = parametrization_->sigma(t0 + dt, p[0]);

        Real h = x0[0] * 1.0001;
        Real dsigmadp = (parametrization_->sigma(t0, x0[0] + h) - sigma) / h;
        Real dsigma2dp = (parametrization_->sigma(t0 + dt, p[0] + h) - sigma2) / h;

        return x0 +
               (r_dom - r_for - 0.25 * (sigma * sigma + sigma2 * sigma2 + sigma * dsigmadp + sigma2 * dsigma2dp)) * dt +
               0.5 * (sigma + sigma2) * std::sqrt(dt) * dw[0];

    } else {
        QL_FAIL("FxLvModel::marginalStep(): discretization " << static_cast<int>(effDisc) << " not handled.");
    }
}

} // namespace QuantExt
