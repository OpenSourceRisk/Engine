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

/*! \file qle/math/gpuqrsolve_multistream.hpp
    \brief GPU-accelerated QR solve using cuSOLVER with multi-stream support

    This implementation uses per-thread CUDA streams to enable concurrent
    GPU operations when multiple CPU threads are executing QR solves.

    Environment variables:
    - ORE_USE_CUDA_STREAMS: Set to "1" to enable multi-stream mode (default: disabled)
    - ORE_CUDA_ASYNC_MEMCPY: Set to "1" to use async memory copies (default: disabled)
    - ORE_CUDA_STREAM_SYNC_MODE: Set to "per_op" for per-operation sync,
                                  "per_solve" for per-solve sync (default: per_solve)
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

namespace QuantExt {

using namespace QuantLib;

//! GPU-accelerated QR solve with multi-stream support for concurrent execution
/*! This function solves the least squares problem:
    minimize ||A*x - b||_2

    using QR factorization on the GPU via NVIDIA cuSOLVER library.
    Unlike gpuQrSolve, this version uses per-thread CUDA streams to allow
    concurrent GPU operations when called from multiple threads.

    Falls back to CPU implementation if CUDA is not available or
    for small problem sizes where GPU overhead would be too high.

    \param A the coefficient matrix (m x n)
    \param b the right-hand side vector (m x 1)
    \return the solution vector x (n x 1)
*/
Array gpuQrSolveMultiStream(const Matrix& A, const Array& b);

//! Check if multi-stream GPU QR solve is available and enabled
bool gpuQrSolveMultiStreamAvailable();

//! Check if multi-stream mode is enabled via ORE_USE_CUDA_STREAMS environment variable
bool isMultiStreamEnabled();

//! Check if async memory copies are enabled via ORE_CUDA_ASYNC_MEMCPY environment variable
bool isAsyncMemcpyEnabled();

//! Synchronize the current thread's CUDA stream
/*! Call this to ensure all GPU operations from the current thread are complete.
    Useful when you need to guarantee results are ready before proceeding.
*/
void syncCurrentThreadStream();

//! Get statistics about stream usage (for debugging/profiling)
struct StreamStats {
    size_t totalSolves;
    size_t gpuSolves;
    size_t cpuFallbacks;
    size_t streamSyncs;
    double totalGpuTimeMs;
};

//! Get current stream statistics for this thread
StreamStats getThreadStreamStats();

//! Reset stream statistics for this thread
void resetThreadStreamStats();

} // namespace QuantExt
