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
};

}
