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

#include <ql/errors.hpp>
#include <ql/math/matrixutilities/qrdecomposition.hpp>
#include <qle/math/gpuqrsolve.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

#ifdef ORE_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cusolverDn.h>
#endif

namespace {
// Minimum problem size (rows) to use GPU acceleration
// Below this, CPU implementation is often faster due to GPU overhead
QuantLib::Size gpuMinSize = 1000;

#ifdef ORE_ENABLE_CUDA
// Thread-local cuSOLVER handle for reuse
thread_local cusolverDnHandle_t cusolverHandle = nullptr;
thread_local bool cusolverInitialized = false;

void ensureCusolverHandle() {
    if (!cusolverInitialized) {
        cusolverStatus_t status = cusolverDnCreate(&cusolverHandle);
        if (status != CUSOLVER_STATUS_SUCCESS) {
            cusolverHandle = nullptr;
            return;
        }
        cusolverInitialized = true;
    }
}

void cleanupCusolverHandle() {
    if (cusolverInitialized && cusolverHandle != nullptr) {
        cusolverDnDestroy(cusolverHandle);
        cusolverHandle = nullptr;
        cusolverInitialized = false;
    }
}

// Check for CUDA errors
bool checkCuda(cudaError_t err, const char* msg) {
    if (err != cudaSuccess) {
        std::cerr << "CUDA error at " << msg << ": " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    return true;
}

// Check for cuSOLVER errors
bool checkCusolver(cusolverStatus_t err, const char* msg) {
    if (err != CUSOLVER_STATUS_SUCCESS) {
        std::cerr << "cuSOLVER error at " << msg << ": " << err << std::endl;
        return false;
    }
    return true;
}
#endif
} // namespace

namespace QuantExt {

// Check if cuSOLVER is disabled via environment variable
bool isCusolverDisabled() {
    const char* envVar = std::getenv("ORE_DISABLE_CUSOLVER");
    if (envVar != nullptr) {
        std::string val(envVar);
        // Any non-empty value (except "0" or "false") disables cuSOLVER
        if (!val.empty() && val != "0" && val != "false" && val != "FALSE") {
            return true;
        }
    }
    return false;
}

bool gpuQrSolveAvailable() {
#ifdef ORE_ENABLE_CUDA
    // Check if disabled via environment variable
    if (isCusolverDisabled()) {
        return false;
    }
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    return (err == cudaSuccess && deviceCount > 0);
#else
    return false;
#endif
}

void setGpuQrSolveMinSize(Size minSize) { gpuMinSize = minSize; }

Size getGpuQrSolveMinSize() { return gpuMinSize; }

Array gpuQrSolve(const Matrix& A, const Array& b) {
    const Size m = A.rows();

    QL_REQUIRE(b.size() == m, "gpuQrSolve: dimensions of A and b don't match");

#ifdef ORE_ENABLE_CUDA
    const Size n = A.columns();
    // Check if GPU is available and problem is large enough
    if (gpuQrSolveAvailable() && m >= gpuMinSize) {
        ensureCusolverHandle();
        if (cusolverHandle == nullptr) {
            // Fall back to CPU if handle creation failed
            return qrSolve(A, b);
        }

        // Allocate device memory
        double* d_A = nullptr;
        double* d_b = nullptr;
        double* d_tau = nullptr;
        double* d_work = nullptr;
        int* d_info = nullptr;
        int lwork = 0;

        bool success = true;

        // A is stored row-major in QuantLib, cuSOLVER expects column-major
        // We need to transpose A for cuSOLVER
        std::vector<double> A_col_major(m * n);
        for (Size i = 0; i < m; ++i) {
            for (Size j = 0; j < n; ++j) {
                A_col_major[j * m + i] = A[i][j];
            }
        }

        // b needs to be extended to have space for the solution (n elements)
        // Since m >= n for least squares, we use m-sized buffer
        std::vector<double> b_extended(std::max(m, n));
        std::copy(b.begin(), b.end(), b_extended.begin());

        do {
            if (!checkCuda(cudaMalloc(&d_A, m * n * sizeof(double)), "cudaMalloc d_A")) {
                success = false;
                break;
            }
            if (!checkCuda(cudaMalloc(&d_b, std::max(m, n) * sizeof(double)), "cudaMalloc d_b")) {
                success = false;
                break;
            }
            if (!checkCuda(cudaMalloc(&d_tau, n * sizeof(double)), "cudaMalloc d_tau")) {
                success = false;
                break;
            }
            if (!checkCuda(cudaMalloc(&d_info, sizeof(int)), "cudaMalloc d_info")) {
                success = false;
                break;
            }

            // Copy data to device
            if (!checkCuda(cudaMemcpy(d_A, A_col_major.data(), m * n * sizeof(double), cudaMemcpyHostToDevice),
                           "cudaMemcpy d_A")) {
                success = false;
                break;
            }
            if (!checkCuda(cudaMemcpy(d_b, b_extended.data(), m * sizeof(double), cudaMemcpyHostToDevice),
                           "cudaMemcpy d_b")) {
                success = false;
                break;
            }

            // Query workspace size for QR factorization
            if (!checkCusolver(cusolverDnDgeqrf_bufferSize(cusolverHandle, m, n, d_A, m, &lwork), "geqrf_bufferSize")) {
                success = false;
                break;
            }

            if (!checkCuda(cudaMalloc(&d_work, lwork * sizeof(double)), "cudaMalloc d_work")) {
                success = false;
                break;
            }

            // Perform QR factorization: A = Q * R
            if (!checkCusolver(cusolverDnDgeqrf(cusolverHandle, m, n, d_A, m, d_tau, d_work, lwork, d_info), "geqrf")) {
                success = false;
                break;
            }

            // Check for errors in QR factorization
            int info = 0;
            if (!checkCuda(cudaMemcpy(&info, d_info, sizeof(int), cudaMemcpyDeviceToHost), "cudaMemcpy d_info")) {
                success = false;
                break;
            }
            if (info != 0) {
                success = false;
                break;
            }

            // Query workspace for ormqr (multiply by Q^T)
            int lwork_ormqr = 0;
            if (!checkCusolver(cusolverDnDormqr_bufferSize(cusolverHandle, CUBLAS_SIDE_LEFT, CUBLAS_OP_T, m, 1, n, d_A,
                                                           m, d_tau, d_b, m, &lwork_ormqr),
                               "ormqr_bufferSize")) {
                success = false;
                break;
            }

            if (lwork_ormqr > lwork) {
                cudaFree(d_work);
                lwork = lwork_ormqr;
                if (!checkCuda(cudaMalloc(&d_work, lwork * sizeof(double)), "cudaMalloc d_work realloc")) {
                    success = false;
                    break;
                }
            }

            // Apply Q^T to b: b = Q^T * b
            if (!checkCusolver(cusolverDnDormqr(cusolverHandle, CUBLAS_SIDE_LEFT, CUBLAS_OP_T, m, 1, n, d_A, m, d_tau,
                                                d_b, m, d_work, lwork, d_info),
                               "ormqr")) {
                success = false;
                break;
            }

            // Check for errors
            if (!checkCuda(cudaMemcpy(&info, d_info, sizeof(int), cudaMemcpyDeviceToHost), "cudaMemcpy d_info ormqr")) {
                success = false;
                break;
            }
            if (info != 0) {
                success = false;
                break;
            }

            // Solve R * x = Q^T * b using triangular solve
            // R is upper triangular and stored in the upper part of d_A
            // We need to use cublas for triangular solve, but cusolverDn doesn't have it directly
            // Instead, we'll do a simple back-substitution on CPU after copying R back

            // Copy R matrix (upper n x n part of A) back to host
            std::vector<double> R_col_major(n * n);
            // Copy only the upper triangular part (first n columns of the m x n matrix)
            if (!checkCuda(cudaMemcpy(R_col_major.data(), d_A, n * n * sizeof(double), cudaMemcpyDeviceToHost),
                           "cudaMemcpy R")) {
                success = false;
                break;
            }

            // Copy Q^T * b (first n elements) back to host
            std::vector<double> qtb(n);
            if (!checkCuda(cudaMemcpy(qtb.data(), d_b, n * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy qtb")) {
                success = false;
                break;
            }

            // Clean up device memory
            cudaFree(d_A);
            cudaFree(d_b);
            cudaFree(d_tau);
            cudaFree(d_work);
            cudaFree(d_info);

            // Back-substitution to solve R * x = qtb
            // R is stored column-major, so R[i][j] = R_col_major[j * n + i]
            Array x(n);
            for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
                double sum = qtb[i];
                for (Size j = i + 1; j < n; ++j) {
                    sum -= R_col_major[j * n + i] * x[j]; // R[i][j] in column-major
                }
                double R_ii = R_col_major[i * n + i]; // R[i][i]
                if (std::abs(R_ii) < 1e-14) {
                    x[i] = 0.0; // Handle near-singular case
                } else {
                    x[i] = sum / R_ii;
                }
            }

            return x;

        } while (false);

        // Cleanup on failure
        if (d_A)
            cudaFree(d_A);
        if (d_b)
            cudaFree(d_b);
        if (d_tau)
            cudaFree(d_tau);
        if (d_work)
            cudaFree(d_work);
        if (d_info)
            cudaFree(d_info);

        if (!success) {
            // Fall back to CPU implementation
            return qrSolve(A, b);
        }
    }
#endif

    // Use CPU implementation for small problems or when CUDA is not available
    return qrSolve(A, b);
}

} // namespace QuantExt
