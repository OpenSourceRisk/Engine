/*
 Copyright (C) 2026 BNY Mellon, NVIDIA CORPORATION & AFFILIATES

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

/*! \file qle/math/gpuqrsolve.hpp
    \brief GPU-accelerated QR solve using cuSOLVER
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Process-wide opt-in flag for the GPU QR path. Default off.
void setUseGpuRegression(bool b);

//! Read the process-wide opt-in flag.
bool useGpuRegression();

//! GPU-accelerated QR solve for least squares problems
/*! This function solves the least squares problem:
    minimize ||A*x - b||_2

    using QR factorization on the GPU via NVIDIA cuSOLVER library.
    Falls back to CPU implementation if CUDA is not available or
    for small problem sizes where GPU overhead would be too high.

    \param A the coefficient matrix (m x n)
    \param b the right-hand side vector (m x 1)
    \return the solution vector x (n x 1)
*/
Array gpuQrSolve(const Matrix& A, const Array& b);

//! Check if GPU QR solve is available
bool gpuQrSolveAvailable();

} // namespace QuantExt
