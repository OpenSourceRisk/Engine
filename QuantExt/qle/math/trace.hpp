/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file trace.hpp
    \brief trace of a quadratic matrix
    \ingroup math
*/

#ifndef quantext_trace_hpp
#define quantext_trace_hpp

#include <ql/math/matrix.hpp>

using QuantLib::Size;
using QuantLib::Real;
using QuantLib::Matrix;

namespace QuantExt {

inline Real Trace(const Matrix& m) {
    QL_REQUIRE(m.rows() == m.columns(), "Trace: input matrix must be quadratic");
    Real t = 0.0;
    for (Size i = 0; i < m.rows(); ++i)
        t += m[i][i];
    return t;
}

} // namespace QuantExt

#endif
