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
#include <qle/models/lgm.hpp>

namespace QuantExt {

//! Numerical convolution solver for the LGM model
/*! Reference: Hagan, Methodology for callable swaps and Bermudan
               exercise into swaptions
*/

class LgmConvolutionSolver2 {
public:
    LgmConvolutionSolver2(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                          const Real sx, const Size nx);

    /* get grid size */
    Size gridSize() const { return 2 * mx_ + 1; }

    /* get discretised states grid at time t */
    RandomVariable stateGrid(const Real t) const;

    /* roll back an deflated NPV array from t1 to t0 */
    RandomVariable rollback(const RandomVariable& v, const Real t1, const Real t0) const;

    /* the underlying model */
    const boost::shared_ptr<LinearGaussMarkovModel>& model() const { return model_; }

private:
    boost::shared_ptr<LinearGaussMarkovModel> model_;
    int mx_, my_, nx_;
    Real h_;
    std::vector<Real> y_, w_;
};

} // namespace QuantExt
