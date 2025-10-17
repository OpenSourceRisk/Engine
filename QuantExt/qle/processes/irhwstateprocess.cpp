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
    Matrix y;
    if (cacheNotReady_drift_) {
        Matrix y = parametrization_->y(t);
        if (timeStepsToCache_drift_ > 0) {
            cache_drift_.push_back(y);
            if (cache_drift_.size() == timeStepsToCache_drift_)
                cacheNotReady_drift_ = false;
        }
    } else {
        y = cache_drift_[timeStepCache_drift_++];
        if (timeStepCache_drift_ == timeStepsToCache_drift_)
            timeStepCache_drift_ = 0;
    }

    Array ones(parametrization_->n(), 1.0);
    Array x(s.begin(), std::next(s.begin(), parametrization_->n()));
    Array drift_x = y * ones - parametrization_->kappa(t) * x;
    if (!(evaluateBankAccount_ && measure_ == IrModel::Measure::BA))
        return drift_x;
    Array drift_int_x = x;
    Array result(2 * parametrization_->n());
    std::copy(drift_x.begin(), drift_x.end(), result.begin());
    std::copy(drift_int_x.begin(), drift_int_x.end(), std::next(result.begin(), parametrization_->n()));
    return result;
}

Matrix IrHwStateProcess::diffusion(Time t, const Array& s) const {
    if (cacheNotReady_diffusion_) {

        Matrix res(size(), factors(), 0.0);
        for (Size i = 0; i < parametrization_->n(); ++i) {
            for (Size j = 0; j < res.columns(); ++j) {
                res(i, j) = parametrization_->sigma_x(t)(j, i);
            }
        }
        // note: the rest of the diffusion rows (if present for the aux state) is zero

        if (timeStepsToCache_diffusion_ > 0) {
            cache_diffusion_.push_back(res);
            if (cache_diffusion_.size() == timeStepsToCache_diffusion_)
                cacheNotReady_diffusion_ = false;
        }
        return res;

    } else {

        Matrix res = cache_diffusion_[timeStepCache_diffusion_++];
        if (timeStepCache_diffusion_ == timeStepsToCache_diffusion_)
            timeStepCache_diffusion_ = 0;
        return res;
    }
}

void IrHwStateProcess::resetCache(const Size timeSteps) const {
    cacheNotReady_drift_ = cacheNotReady_diffusion_ = true;
    timeStepsToCache_drift_ = timeStepsToCache_diffusion_ = timeSteps;
    cache_drift_.clear();
    cache_diffusion_.clear();
    parametrization_->update();
}

} // namespace QuantExt
