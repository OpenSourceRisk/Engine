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

/*! \file lgmbackwardsolver.hpp
    \brief interface for LGM1F backward solver

    \ingroup engines
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/models/lgm.hpp>

namespace QuantExt {

//! Interface for LGM1F backward solver
class LgmBackwardSolver {
public:
    virtual ~LgmBackwardSolver() {}

    /* get grid size */
    virtual Size gridSize() const = 0;

    /* get discretised states grid at time t */
    virtual RandomVariable stateGrid(const Real t) const = 0;

    /* roll back an deflated NPV array from t1 to t0 using the given number of steps or,
       if the number of steps is not given, an appropriate number of steps which will
       generally depend on the numerical method that is used. */
    virtual RandomVariable rollback(const RandomVariable& v, const Real t1, const Real t0,
                                    Size steps = Null<Size>()) const = 0;

    /* the underlying model */
    virtual const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model() const = 0;
};

} // namespace QuantExt
