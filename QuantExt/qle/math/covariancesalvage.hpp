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
    p = the salvaged version of m
    s = a square root of m, if provided, otherwise an empty Matrix */
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
