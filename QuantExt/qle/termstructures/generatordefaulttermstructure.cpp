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

/*! \file generatordefaultprobabilitytermstructure.cpp
    \brief Default curve implied from a single generator matrix
    \ingroup termstructures
*/

#include <qle/math/matrixfunctions.hpp>
#include <qle/models/transitionmatrix.hpp>
#include <qle/termstructures/generatordefaulttermstructure.hpp>

namespace QuantExt {

GeneratorDefaultProbabilityTermStructure::GeneratorDefaultProbabilityTermStructure(MatrixType matrixType, Matrix matrix,
                                                                                   Size initialState,
                                                                                   const Date& referenceDate,
                                                                                   const Calendar& cal,
                                                                                   const DayCounter& dc)
    : SurvivalProbabilityStructure(referenceDate, cal, dc), initialState_(initialState),
      transitionMatrix_(matrixType == MatrixType::Transition ? matrix : Matrix()),
      generator_(matrixType == MatrixType::Generator ? matrix : Matrix()) {

    QL_REQUIRE(matrix.rows() == matrix.columns(), "input matrix is not square");

    // compute/check the generator
    if (matrixType == MatrixType::Transition) {
        sanitiseTransitionMatrix(transitionMatrix_);
        checkTransitionMatrix(transitionMatrix_);
        generator_ = QuantExt::generator(transitionMatrix_);
    }
    checkGeneratorMatrix(generator_);
}

Probability GeneratorDefaultProbabilityTermStructure::survivalProbabilityImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "non-negative time required");
    Matrix Q = QuantExt::Expm(generator_ * t);
    QL_REQUIRE(initialState_ < Q.rows(),
               "initial state " << initialState_ << " out of range [0," << Q.rows() - 1 << "]");
    Size lastState = Q.columns() - 1;
    return 1.0 - Q[initialState_][lastState];
}

} // namespace QuantExt
