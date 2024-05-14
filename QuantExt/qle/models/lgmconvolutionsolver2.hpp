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

/*! \file lgmconvolutionsolver2.hpp
    \brief numeric convolution solver for the LGM model using RandoMVariable

    \ingroup engines
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/models/lgmbackwardsolver.hpp>

namespace QuantExt {

//! Numerical convolution solver for the LGM model
/*! Reference: Hagan, Methodology for callable swaps and Bermudan
               exercise into swaptions
*/

class LgmConvolutionSolver2 : public LgmBackwardSolver {
public:
    LgmConvolutionSolver2(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                          const Real sx, const Size nx);
    Size gridSize() const override { return 2 * mx_ + 1; }
    RandomVariable stateGrid(const Real t) const override;
    // steps are always ignored, since we can take large steps
    RandomVariable rollback(const RandomVariable& v, const Real t1, const Real t0,
                            Size steps = Null<Size>()) const override;
    const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model() const override { return model_; }

private:
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> model_;
    int mx_, my_, nx_;
    Real h_;
    std::vector<Real> y_, w_;
};

} // namespace QuantExt
