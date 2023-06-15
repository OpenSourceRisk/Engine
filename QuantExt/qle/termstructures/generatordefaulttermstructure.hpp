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

/*! \file generatordefaulttermstructure.hpp
    \brief Default curve implied from a single generator matrix
    \ingroup termstructures
*/

#pragma once

#include <ql/math/matrix.hpp>
#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Default probability term structure implied from a transition matrix
/*!
  This class uses a transition or generator matrix to imply cumulative survival probabilities depending on initial
  state.

  \ingroup termstructures
*/
class GeneratorDefaultProbabilityTermStructure : public SurvivalProbabilityStructure {
public:
    enum class MatrixType { Transition, Generator };
    //! Constructor using a single (annual!) transition matrix or a generator matrix
    GeneratorDefaultProbabilityTermStructure(MatrixType matrixType, Matrix matrix, Size initialState,
                                             const Date& referenceDate, const Calendar& cal = NullCalendar(),
                                             const DayCounter& dc = Actual365Fixed());
    Date maxDate() const override { return QuantLib::Date::maxDate(); }

    //! return the underlying annualised transition matrix
    const Matrix& transitionMatrix() const { return transitionMatrix_; }
    const Matrix& generator() const { return generator_; }

protected:
    Probability survivalProbabilityImpl(Time) const override;

private:
    Size initialState_;
    Matrix transitionMatrix_;
    Matrix generator_;
};

} // namespace QuantExt
