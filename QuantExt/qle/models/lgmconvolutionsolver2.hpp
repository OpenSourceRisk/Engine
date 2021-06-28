/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
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
