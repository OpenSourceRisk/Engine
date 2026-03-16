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

/*! \file lgmconvolutionsolver.hpp
    \brief numeric convolution solver for the LGM model

    \ingroup engines
*/

#pragma once

#include <qle/models/lgm.hpp>

namespace QuantExt {

//! Numerical convolution solver for the LGM model
/*! Reference: Hagan, Methodology for callable swaps and Bermudan
               exercise into swaptions
*/

class LgmConvolutionSolver {
public:
    LgmConvolutionSolver(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                         const Real sx, const Size nx);

    /* get grid size */
    Size gridSize() const { return 2 * mx_ + 1; }

    /* get discretised states grid at time t */
    std::vector<Real> stateGrid(const Real t) const;

    /* roll back an deflated NPV array from t1 to t0 */
    template <typename ValueType = Real>
    std::vector<ValueType> rollback(const std::vector<ValueType>& v, const Real t1, const Real t0,
                                    const ValueType zero = ValueType(0.0)) const;

    /* the underlying model */
    const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model() const { return model_; }

private:
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> model_;
    int mx_, my_, nx_;
    Real h_;
    std::vector<Real> y_, w_;
};

// rollback implementation

template <typename ValueType>
std::vector<ValueType> LgmConvolutionSolver::rollback(const std::vector<ValueType>& v, const Real t1, const Real t0,
                                                      const ValueType zero) const {
    if (QuantLib::close_enough(t0, t1))
        return v;
    QL_REQUIRE(t0 < t1, "LgmConvolutionSolver::rollback(): t0 (" << t0 << ") < t1 (" << t1 << ") required.");
    Real sigma = std::sqrt(model_->parametrization()->zeta(t1));
    Real dx = sigma / static_cast<Real>(nx_);
    if (QuantLib::close_enough(t0, 0.0)) {
        // rollback from t1 to t0 = 0
        ValueType value(zero);
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
        return std::vector<ValueType>(2 * mx_ + 1, value);
    } else {
        std::vector<ValueType> value(2 * mx_ + 1, zero);
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
                value[k] +=
                    w_[i] *
                    (kk < 0 ? v[0] : (kk + 1 > 2 * mx_ ? v[2 * mx_] : (kp - kk) * v[kk + 1] + (1.0 + kk - kp) * v[kk]));
            }
        }
        return value;
    }
}

} // namespace QuantExt
