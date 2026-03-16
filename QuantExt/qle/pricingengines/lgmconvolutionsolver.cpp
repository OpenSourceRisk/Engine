/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/lgmconvolutionsolver.hpp>

#include <ql/math/distributions/normaldistribution.hpp>

namespace QuantExt {

//! Numerical convolution solver for the LGM model
/*! Reference: Hagan, Methodology for callable swaps and Bermudan
               exercise into swaptions
*/

LgmConvolutionSolver::LgmConvolutionSolver(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy,
                                           const Size ny, const Real sx, const Size nx)
    : model_(model), nx_(nx) {

    // precompute weights

    // number of x, y grid points > 0
    mx_ = static_cast<Size>(floor(sx * static_cast<Real>(nx)) + 0.5);
    my_ = static_cast<Size>(floor(sy * static_cast<Real>(ny)) + 0.5);

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

std::vector<Real> LgmConvolutionSolver::stateGrid(const Real t) const {
    if (close_enough(t, 0.0))
        return std::vector<Real>(2 * mx_ + 1, 0.0);
    std::vector<Real> x(2 * mx_ + 1);
    Real dx = std::sqrt(model_->parametrization()->zeta(t)) / static_cast<Real>(nx_);
    for (int k = 0; k <= 2 * mx_; ++k) {
        x[k] = dx * (k - mx_);
    }
    return x;
}

} // namespace QuantExt
