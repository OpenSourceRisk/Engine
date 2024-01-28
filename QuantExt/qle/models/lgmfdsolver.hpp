/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file lgmfdsolverx.hpp
    \brief numeric fd solver for LGM model

    \ingroup models
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/models/lgm.hpp>

#include <ql/methods/finitedifferences/meshers/fdmmesher.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>

namespace QuantExt {

//! Numerical FD solver for the LGM model
class LgmFdSolver {
public:
    LgmFdSolver(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real maxTime,
                const Size stateGridPoints = 64, const Size timeStepsPerYear = 24, const Real mesherEpsilon = 1E-4);

    /* get grid size */
    Size gridSize() const;

    /* get discretised states grid */
    RandomVariable stateGrid() const;

    /* roll back an deflated NPV array from t1 to t0 */
    RandomVariable rollback(const RandomVariable& v, const Real t1, const Real t0) const;

    /* the underlying model */
    const boost::shared_ptr<LinearGaussMarkovModel>& model() const;

private:
    boost::shared_ptr<LinearGaussMarkovModel> model_;
    Real maxTime_;
    Size stateGridPoints_;
    Size timeStepsPerYear_;
    Real mesherEpsilon_;

    mutable boost::shared_ptr<FdmMesher> mesher_;              // the mesher for the FD solver
    mutable boost::shared_ptr<FdmLinearOpComposite> operator_; // the operator
    mutable boost::shared_ptr<FdmBackwardSolver> solver_;      // the sovler

    RandomVariable mesherLocations_;
};

} // namespace QuantExt
