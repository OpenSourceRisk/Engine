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

/*! \file qle/math/fillemptymatrix.hpp
    \brief functions to fill a "not-completely-populated" matrix.
    \ingroup math
*/

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/matrix.hpp>

namespace QuantExt {
using namespace QuantLib;

//! function that fills a matrix
/*! This function fills a matrix that is not completely populated by linear interpolation across
 * the desired axis. */
void fillIncompleteMatrix(Matrix& mat, bool interpRows, Real blank);
} // namespace QuantExt
