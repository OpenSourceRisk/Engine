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

#include <qle/models/lgmconvolutionsolver2.hpp>

#include <ql/math/distributions/normaldistribution.hpp>

namespace QuantExt {

LgmConvolutionSolver2::LgmConvolutionSolver2(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy,
                                             const Size ny, const Real sx, const Size nx)
    : model_(model), nx_(static_cast<int>(nx)) {

    // precompute weights

    // number of x, y grid points > 0
    mx_ = static_cast<int>(floor(sx * static_cast<Real>(nx)) + 0.5);
    my_ = static_cast<int>(floor(sy * static_cast<Real>(ny)) + 0.5);

    // y-grid spacing
    h_ = 1.0 / static_cast<Real>(ny);

    // weights for convolution in the rollback step
    CumulativeNormalDistribution N;
    NormalDistribution G;

    y_.resize(2 * my_ + 1); // x-coordinate / standard deviation of x
    w_.resize(2 * my_ + 1); // probability weight around y-grid point i
    for (int i = 0; i <= 2 * my_; i++) {
        y_[i] = h_ * (i - my_);
        if (i == 0 || i == 2 * my_)
            w_[i] = (1. + y_[0] / h_) * N(y_[0] + h_) - y_[0] / h_ * N(y_[0]) + (G(y_[0] + h_) - G(y_[0])) / h_;
        else
            w_[i] = (1. + y_[i] / h_) * N(y_[i] + h_) - 2. * y_[i] / h_ * N(y_[i]) -
                    (1. - y_[i] / h_) * N(y_[i] - h_) // opposite sign in the paper
                    + (G(y_[i] + h_) - 2. * G(y_[i]) + G(y_[i] - h_)) / h_;
        // w_[i] might be negative due to numerical errors
        if (w_[i] < 0.0) {
            QL_REQUIRE(w_[i] > -1.0E-10, "LgmConvolutionSolver: negative w (" << w_[i] << ") at i=" << i);
            w_[i] = 0.0;
        }
    }
}

RandomVariable LgmConvolutionSolver2::stateGrid(const Real t) const {
    if (QuantLib::close_enough(t, 0.0))
        return RandomVariable(2 * mx_ + 1, 0.0);
    RandomVariable x(2 * mx_ + 1);
    Real dx = std::sqrt(model_->parametrization()->zeta(t)) / static_cast<Real>(nx_);
    for (int k = 0; k <= 2 * mx_; ++k) {
        x.set(k, dx * (k - mx_));
    }
    return x;
}

RandomVariable LgmConvolutionSolver2::rollback(const RandomVariable& v, const Real t1, const Real t0, Size) const {
    if (QuantLib::close_enough(t0, t1) || v.deterministic())
        return v;
    QL_REQUIRE(t0 < t1, "LgmConvolutionSolver2::rollback(): t0 (" << t0 << ") < t1 (" << t1 << ") required.");
    Real sigma = std::sqrt(model_->parametrization()->zeta(t1));
    Real dx = sigma / static_cast<Real>(nx_);
    if (QuantLib::close_enough(t0, 0.0)) {
        // rollback from t1 to t0 = 0
        Real value = 0.0;
        for (int i = 0; i <= 2 * my_; i++) {
            // Map y index to x index, not integer in general
            Real kp = y_[i] * sigma / dx + mx_;
            // Adjacent integer x index <= k
            int kk = int(floor(kp));
            // Get value at kp by linear interpolation on
            // kk <= kp <= kk + 1 with flat extrapolation
            value +=
                w_[i] *
                (kk < 0 ? v[0] : (kk + 1 > 2 * mx_ ? v[2 * mx_] : (kp - kk) * v[kk + 1] + (1.0 + kk - kp) * v[kk]));
        }
        return RandomVariable(2 * mx_ + 1, value);
    } else {
        RandomVariable value(2 * mx_ + 1, 0.0);
        value.expand();
        // rollback from t1 to t0 > 0
        Real std = std::sqrt(model_->parametrization()->zeta(t1) - model_->parametrization()->zeta(t0));
        Real dx2 = std::sqrt(model_->parametrization()->zeta(t0)) / static_cast<Real>(nx_);
        for (int k = 0; k <= 2 * mx_; k++) {
            for (int i = 0; i <= 2 * my_; i++) {
                // Map y index to x index, not integer in generalTo
                Real kp = (dx2 * (k - mx_) + y_[i] * std) / dx + mx_;
                // Adjacent integer x index <= k
                int kk = int(floor(kp));
                // Get value at kp by linear interpolation on
                // kk <= kp <= kk + 1 with flat extrapolation
                value.set(k, value[k] + w_[i] * (kk < 0 ? v[0]
                                                        : (kk + 1 > 2 * mx_
                                                               ? v[2 * mx_]
                                                               : (kp - kk) * v[kk + 1] + (1.0 + kk - kp) * v[kk])));
            }
        }
        return value;
    }
}

} // namespace QuantExt
