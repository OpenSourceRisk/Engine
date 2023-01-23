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

/*! \file qle/math/blockmatrixinverse.hpp
    \brief inverse of a matrix using a block formula
    \ingroup math
*/

#ifndef quantext_blockmatrixinverse_hpp
#define quantext_blockmatrixinverse_hpp

#include <ql/math/matrix.hpp>
#include <ql/math/matrixutilities/sparsematrix.hpp>

namespace QuantExt {

/* inverse of a sparse matrix per LU decomposition */
QuantLib::SparseMatrix inverse(QuantLib::SparseMatrix m);

/* Reference: https://en.wikipedia.org/wiki/Invertible_matrix#Blockwise_inversion */
QuantLib::Matrix blockMatrixInverse(const QuantLib::Matrix& A, const std::vector<QuantLib::Size>& blockIndices);
QuantLib::SparseMatrix blockMatrixInverse(const QuantLib::SparseMatrix& A, const std::vector<QuantLib::Size>& blockIndices);

/*! modified max norm of a sparse matrix, i.e. std::sqrt(row * columns) * max_i,j abs(a_i,j) */
QuantLib::Real modifiedMaxNorm(const QuantLib::SparseMatrix& A);

} // namespace QuantExt

#endif
