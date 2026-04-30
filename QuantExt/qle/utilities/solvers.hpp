/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/utilities/solvers.hpp
    \brief solver related utilities.
*/

#pragma once
#include <utility>
#include <ql/types.hpp>
#include <ql/utilities/null.hpp>
#include <ql/math/solver1d.hpp>

namespace QuantExt {

//! A simple struct to group together options used by a Solver1D instance.
struct Solver1DOptions {
    //! The maximum number of evaluations. Default used if not set.
    QuantLib::Size maxEvaluations = QuantLib::Null<QuantLib::Size>();
    //! The accuracy for the search.
    QuantLib::Real accuracy = QuantLib::Null<QuantLib::Real>();
    //! The initial guess for the search.
    QuantLib::Real initialGuess = QuantLib::Null<QuantLib::Real>();
    //! Set the minimum and maximum search.
    std::pair<QuantLib::Real, QuantLib::Real> minMax =
        std::make_pair(QuantLib::Null<QuantLib::Real>(), QuantLib::Null<QuantLib::Real>());
    //! Set the step size for the search.
    QuantLib::Real step = QuantLib::Null<QuantLib::Real>();
    //! The lower bound of the search domain. A \c Null<Real>() indicates that the bound should not be set.
    QuantLib::Real lowerBound = QuantLib::Null<QuantLib::Real>();
    //! The upper bound of the search domain. A \c Null<Real>() indicates that the bound should not be set.
    QuantLib::Real upperBound = QuantLib::Null<QuantLib::Real>();
    //! Whether to favour min / max or step.
    bool favourMinMax = true;
};

template <class SolverImpl>
std::pair<SolverImpl, bool> createSolver1D(const Solver1DOptions& options) {
    using QuantLib::Null;
    using QuantLib::Real;
    using QuantLib::Size;

    SolverImpl solver;

    // Check that enough solver options have been provided.
    Real guess = options.initialGuess;
    QL_REQUIRE(guess != Null<Real>(), "createSolver1D: need a valid initial guess for the 1D solver.");

    Real accuracy = options.accuracy;
    QL_REQUIRE(accuracy != Null<Real>(),
        "createSolver1D: need a valid accuracy for the 1D solver.");

    // Set maximum evaluations if provided. If not, defaults to MAX_FUNCTION_EVALUATIONS in Solver1D = 100.
    if (options.maxEvaluations != Null<Size>())
        solver.setMaxEvaluations(options.maxEvaluations);

    // Check and set the lower bound and upper bound
    if (options.lowerBound != Null<Real>() && options.upperBound != Null<Real>()) {
        QL_REQUIRE(options.lowerBound < options.upperBound, "createSolver1D: lowerBound (" << options.lowerBound <<
            ") should be less than upperBound (" << options.upperBound << ")");
    }

    if (options.lowerBound != Null<Real>())
        solver.setLowerBound(options.lowerBound);
    if (options.upperBound != Null<Real>())
        solver.setUpperBound(options.upperBound);

    // Check min / max.
    bool validMinMax = false;
    auto [min, max] = options.minMax;
    if (min != Null<Real>() && max != Null<Real>()) {
        QL_REQUIRE(min < max, "createSolver1D: min (" << min << ") should be less than max (" <<
            max << ") for the 1D solver.");
        validMinMax = true;
    }

    // Check step.
    bool validStep = false;
    if (options.step != Null<Real>()) {
        QL_REQUIRE(options.step > 0.0, "createSolver1D: step (" << options.step <<
            ") should be positive for the 1D solver.");
        validStep = true;
    }

    // Check at least one of min/max or step is provided.
    QL_REQUIRE(validMinMax || validStep, "createSolver1D: need a valid step size "
        "or (min, max) pair for the 1D solver.");

    // Indicate what solve method the created solver should use based on the options provided.
    bool useMinMax = (validMinMax && options.favourMinMax) || !validStep;

    return {solver, useMinMax};
}

}
