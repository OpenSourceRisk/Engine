/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/math/covariancesalvage.hpp
    \brief methods to make a symmetric matrix positive semidefinite
    \ingroup math
*/

#pragma once

#include <ql/math/matrixutilities/pseudosqrt.hpp>

namespace QuantExt {

using QuantLib::Matrix;
using QuantLib::SalvagingAlgorithm;

/*! Interface, salvage(m) should return (p,s) with
    p = the salvaged (i.e. positive semidefinite) version of m
    s = a square root of p, if provided, otherwise an empty Matrix

    An implementation of this class represents a method to make a given covariance matrix m positive
    semidefinite. This includes an implementation  that just returns the input matrix unchanged, e.g. for
    cases where it is known in advance / for theoretical reasons that the matrix m is positive semidefinite.

    If the method produces a square root of the output matrix as a side product, this should be returned
    in addition since many use cases that require a salvaged covariance matrix also require a square root
    of this matrix e.g. for generating correlated normal random variates. A repeated computation of the
    square root can be avoided this way. The returned square root may but is not required to be the
    unique symmetric positive semidefinite square root of the salvaged covariance matrix p.

    If the method does not provide a square root, an empty matrix Matrix() should be returned instead,
    in which case the caller is responsible to compute a square root if required. */
struct CovarianceSalvage {
    virtual ~CovarianceSalvage() {}
    virtual std::pair<Matrix, Matrix> salvage(const Matrix& m) const = 0;
};

//! Implementation that does not change the input matrix
struct NoCovarianceSalvage : public CovarianceSalvage {
    std::pair<Matrix, Matrix> salvage(const Matrix& m) const override { return std::make_pair(m, Matrix()); }
};

//! Implementation that uses the spectral method
struct SpectralCovarianceSalvage : public CovarianceSalvage {
    std::pair<Matrix, Matrix> salvage(const Matrix& m) const override {
        auto L = pseudoSqrt(m, SalvagingAlgorithm::Spectral);
        return std::make_pair(L * transpose(L), L);
    }
};

} // namespace QuantExt
