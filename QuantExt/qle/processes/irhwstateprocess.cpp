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

#include <qle/processes/irhwstateprocess.hpp>

namespace QuantExt {

Array IrHwStateProcess::initialValues() const { return Array(size(), 0.0); }

Array IrHwStateProcess::drift(Time t, const Array& s) const {
    Array ones(parametrization_->n(), 1.0);
    Array x(s.begin(), std::next(s.begin(), parametrization_->n()));
    Array drift_x = parametrization_->y(t) * ones - parametrization_->kappa(t) * x;
    if (!(evaluateBankAccount_ && measure_ == IrModel::Measure::BA))
        return drift_x;
    Array drift_int_x = x;
    Array result(2 * parametrization_->n());
    std::copy(drift_x.begin(), drift_x.end(), result.begin());
    std::copy(drift_int_x.begin(), drift_int_x.end(), std::next(result.begin(), parametrization_->n()));
    return result;
}

Matrix IrHwStateProcess::diffusion(Time t, const Array& s) const {
    Matrix res(size(), factors(), 0.0);
    for (Size i = 0; i < parametrization_->n(); ++i) {
        for (Size j = 0; j < res.columns(); ++j) {
            res(i, j) = parametrization_->sigma_x(t)(j, i);
        }
    }
    // the rest of the diffusion rows (if present for the aux state) is zero
    return res;
}

} // namespace QuantExt
