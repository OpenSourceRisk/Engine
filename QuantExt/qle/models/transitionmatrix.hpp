/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file models/transitionmatrix.hpp
    \brief utility functions for transition matrices and generators
    \ingroup models
*/

#pragma once

#include <ql/math/matrix.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! cap / floor elements at 1 / 0, adjust diagonal elements so that row sums are 1, or if that is not possible, divide
  the row elements by the row sum */
void sanitiseTransitionMatrix(Matrix& m);

//! check if the matrix is a transition matrix, i.e. row sums are 1 and entries are non-negative
void checkTransitionMatrix(const Matrix& t);

//! check if the matrix is a generator matirx, i.e. row sums are 0 and non-diagonal elements are non-negative
void checkGeneratorMatrix(const Matrix& g);

/*! build generator from transition matrix
 cf. Alexander Kreinin and Marina Sidelnikova, "Regularization Algorithms for Transition Matrices", Algorithm QOG */
Matrix generator(const Matrix& t, const Real horizon = 1.0);

//! compute N(0,1) credit state boundaries
template <class I> std::vector<Real> creditStateBoundaries(const I& begin, const I& end);

} // namespace QuantExt
