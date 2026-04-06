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
#include <qle/math/gpuqrsolve_multistream.hpp>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>

#ifdef ORE_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cusolverDn.h>
#endif

namespace {

// Cache environment variable lookups (done once at startup)
bool envVarsInitialized = false;
bool multiStreamEnabled = false;
bool asyncMemcpyEnabled = false;
bool perOpSync = false;

std::mutex envInitMutex;

void initEnvVars() {
    std::lock_guard<std::mutex> lock(envInitMutex);
    if (envVarsInitialized)
        return;

    // Check ORE_USE_CUDA_STREAMS
    const char* streamEnv = std::getenv("ORE_USE_CUDA_STREAMS");
    if (streamEnv != nullptr) {
        std::string val(streamEnv);
        multiStreamEnabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes" || val == "YES");
    }

    // Check ORE_CUDA_ASYNC_MEMCPY
    const char* asyncEnv = std::getenv("ORE_CUDA_ASYNC_MEMCPY");
    if (asyncEnv != nullptr) {
        std::string val(asyncEnv);
        asyncMemcpyEnabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes" || val == "YES");
    }

    // Check ORE_CUDA_STREAM_SYNC_MODE
    const char* syncEnv = std::getenv("ORE_CUDA_STREAM_SYNC_MODE");
    if (syncEnv != nullptr) {
        std::string val(syncEnv);
        perOpSync = (val == "per_op");
    }

    envVarsInitialized = true;

    if (multiStreamEnabled) {
        std::cout << "[CUDA MultiStream] Multi-stream mode ENABLED" << std::endl;
        std::cout << "[CUDA MultiStream] Async memcpy: " << (asyncMemcpyEnabled ? "enabled" : "disabled") << std::endl;
        std::cout << "[CUDA MultiStream] Sync mode: " << (perOpSync ? "per_op" : "per_solve") << std::endl;
    }
}

#ifdef ORE_ENABLE_CUDA

// Thread-local CUDA resources with dedicated stream
struct ThreadCudaContext {
    cusolverDnHandle_t handle = nullptr;
    cudaStream_t stream = nullptr;
    bool initialized = false;

    // Pre-allocated device memory for common sizes (optional optimization)
    double* d_A = nullptr;
    double* d_b = nullptr;
    double* d_tau = nullptr;
    double* d_work = nullptr;
    int* d_info = nullptr;
    size_t allocatedM = 0;
    size_t allocatedN = 0;
    int allocatedLwork = 0;

    // Statistics
    QuantExt::StreamStats stats = {0, 0, 0, 0, 0.0};

    ~ThreadCudaContext() { cleanup(); }

    void cleanup() {
        if (d_A) {
            cudaFree(d_A);
            d_A = nullptr;
        }
        if (d_b) {
            cudaFree(d_b);
            d_b = nullptr;
        }
        if (d_tau) {
            cudaFree(d_tau);
            d_tau = nullptr;
        }
        if (d_work) {
            cudaFree(d_work);
            d_work = nullptr;
        }
        if (d_info) {
            cudaFree(d_info);
            d_info = nullptr;
        }
        allocatedM = allocatedN = allocatedLwork = 0;

        if (handle) {
            cusolverDnDestroy(handle);
            handle = nullptr;
        }
        if (stream) {
            cudaStreamDestroy(stream);
            stream = nullptr;
        }
        initialized = false;
    }
};

thread_local ThreadCudaContext threadCtx;

bool ensureThreadContext() {
    if (threadCtx.initialized)
        return true;

    // Create a non-blocking stream for this thread
    cudaError_t streamErr = cudaStreamCreateWithFlags(&threadCtx.stream, cudaStreamNonBlocking);
    if (streamErr != cudaSuccess) {
        std::cerr << "[CUDA MultiStream] Failed to create stream: " << cudaGetErrorString(streamErr) << std::endl;
        return false;
    }

    // Create cuSOLVER handle
    cusolverStatus_t handleErr = cusolverDnCreate(&threadCtx.handle);
    if (handleErr != CUSOLVER_STATUS_SUCCESS) {
        std::cerr << "[CUDA MultiStream] Failed to create cuSOLVER handle: " << handleErr << std::endl;
        cudaStreamDestroy(threadCtx.stream);
        threadCtx.stream = nullptr;
        return false;
    }

    // Associate the handle with this thread's stream
    cusolverStatus_t setStreamErr = cusolverDnSetStream(threadCtx.handle, threadCtx.stream);
    if (setStreamErr != CUSOLVER_STATUS_SUCCESS) {
        std::cerr << "[CUDA MultiStream] Failed to set stream on handle: " << setStreamErr << std::endl;
        cusolverDnDestroy(threadCtx.handle);
        cudaStreamDestroy(threadCtx.stream);
        threadCtx.handle = nullptr;
        threadCtx.stream = nullptr;
        return false;
    }

    threadCtx.initialized = true;
    return true;
}

// Check for CUDA errors
bool checkCudaMs(cudaError_t err, const char* msg) {
    if (err != cudaSuccess) {
        std::cerr << "[CUDA MultiStream] CUDA error at " << msg << ": " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    return true;
}

// Check for cuSOLVER errors
bool checkCusolverMs(cusolverStatus_t err, const char* msg) {
    if (err != CUSOLVER_STATUS_SUCCESS) {
        std::cerr << "[CUDA MultiStream] cuSOLVER error at " << msg << ": " << err << std::endl;
        return false;
    }
    return true;
}

// Allocate or reallocate device memory if needed
bool ensureDeviceMemory(size_t m, size_t n, int lwork) {
    bool needRealloc = false;

    // Check if we need more space
    if (m * n > threadCtx.allocatedM * threadCtx.allocatedN) {
        needRealloc = true;
    }
    if (lwork > threadCtx.allocatedLwork) {
        needRealloc = true;
    }

    if (!needRealloc && threadCtx.d_A != nullptr) {
        return true; // Existing allocation is sufficient
    }

    // Free existing allocations
    if (threadCtx.d_A)
        cudaFree(threadCtx.d_A);
    if (threadCtx.d_b)
        cudaFree(threadCtx.d_b);
    if (threadCtx.d_tau)
        cudaFree(threadCtx.d_tau);
    if (threadCtx.d_work)
        cudaFree(threadCtx.d_work);
    if (threadCtx.d_info)
        cudaFree(threadCtx.d_info);

    threadCtx.d_A = nullptr;
    threadCtx.d_b = nullptr;
    threadCtx.d_tau = nullptr;
    threadCtx.d_work = nullptr;
    threadCtx.d_info = nullptr;

    // Allocate with some headroom to reduce reallocations
    size_t allocM = std::max(m, threadCtx.allocatedM);
    size_t allocN = std::max(n, threadCtx.allocatedN);
    int allocLwork = std::max(lwork, threadCtx.allocatedLwork);

    // Add 20% headroom
    allocM = static_cast<size_t>(allocM * 1.2);
    allocN = static_cast<size_t>(allocN * 1.2);
    allocLwork = static_cast<int>(allocLwork * 1.2);

    if (!checkCudaMs(cudaMalloc(&threadCtx.d_A, allocM * allocN * sizeof(double)), "cudaMalloc d_A")) {
        return false;
    }
    if (!checkCudaMs(cudaMalloc(&threadCtx.d_b, std::max(allocM, allocN) * sizeof(double)), "cudaMalloc d_b")) {
        return false;
    }
    if (!checkCudaMs(cudaMalloc(&threadCtx.d_tau, allocN * sizeof(double)), "cudaMalloc d_tau")) {
        return false;
    }
    if (!checkCudaMs(cudaMalloc(&threadCtx.d_work, allocLwork * sizeof(double)), "cudaMalloc d_work")) {
        return false;
    }
    if (!checkCudaMs(cudaMalloc(&threadCtx.d_info, sizeof(int)), "cudaMalloc d_info")) {
        return false;
    }

    threadCtx.allocatedM = allocM;
    threadCtx.allocatedN = allocN;
    threadCtx.allocatedLwork = allocLwork;

    return true;
}

#endif // ORE_ENABLE_CUDA

} // anonymous namespace

namespace QuantExt {

bool isMultiStreamEnabled() {
    initEnvVars();
    return multiStreamEnabled;
}

bool isAsyncMemcpyEnabled() {
    initEnvVars();
    return asyncMemcpyEnabled;
}

bool gpuQrSolveMultiStreamAvailable() {
#ifdef ORE_ENABLE_CUDA
    initEnvVars();
    if (!multiStreamEnabled) {
        return false;
    }
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

void syncCurrentThreadStream() {
#ifdef ORE_ENABLE_CUDA
    if (threadCtx.initialized && threadCtx.stream != nullptr) {
        cudaStreamSynchronize(threadCtx.stream);
        threadCtx.stats.streamSyncs++;
    }
#endif
}

StreamStats getThreadStreamStats() {
#ifdef ORE_ENABLE_CUDA
    return threadCtx.stats;
#else
    return StreamStats{0, 0, 0, 0, 0.0};
#endif
}

void resetThreadStreamStats() {
#ifdef ORE_ENABLE_CUDA
    threadCtx.stats = StreamStats{0, 0, 0, 0, 0.0};
#endif
}

// Thread-local statistics for problem sizes
namespace {
thread_local size_t qrSolveCount = 0;
thread_local size_t totalRows = 0;
thread_local size_t totalCols = 0;
thread_local size_t minRows = SIZE_MAX;
thread_local size_t maxRows = 0;
thread_local size_t minCols = SIZE_MAX;
thread_local size_t maxCols = 0;
thread_local std::map<std::pair<size_t, size_t>, size_t> sizeCounts;

bool printSizeStats() {
    const char* env = std::getenv("ORE_QR_SIZE_STATS");
    return env != nullptr && std::string(env) != "0";
}
} // namespace

Array gpuQrSolveMultiStream(const Matrix& A, const Array& b) {
    const Size m = A.rows();    // number of rows
    const Size n = A.columns(); // number of columns

    QL_REQUIRE(b.size() == m, "gpuQrSolveMultiStream: dimensions of A and b don't match");

    // Track problem sizes
    static bool doStats = printSizeStats();
    if (doStats) {
        qrSolveCount++;
        totalRows += m;
        totalCols += n;
        minRows = std::min(minRows, (size_t)m);
        maxRows = std::max(maxRows, (size_t)m);
        minCols = std::min(minCols, (size_t)n);
        maxCols = std::max(maxCols, (size_t)n);
        sizeCounts[{m, n}]++;

        // Print histogram at end of each trade's processing (~280 solves)
        if (qrSolveCount % 280 == 0) {
            std::cout << "[QR Stats] After " << qrSolveCount << " solves - Size histogram:" << std::endl;
            for (const auto& [sz, cnt] : sizeCounts) {
                std::cout << "  " << sz.first << "x" << sz.second << ": " << cnt << " times" << std::endl;
            }
        }
    }

#ifdef ORE_ENABLE_CUDA
    const Size gpuMinSizeMultiStream = 1000;
    initEnvVars();
    threadCtx.stats.totalSolves++;

    // Check if multi-stream mode is enabled and GPU is available
    if (multiStreamEnabled && !isCusolverDisabled() && m >= gpuMinSizeMultiStream) {

        if (!ensureThreadContext()) {
            threadCtx.stats.cpuFallbacks++;
            return qrSolve(A, b);
        }

        // Query workspace size first (we need this before allocation)
        int lwork = 0;
        if (!checkCusolverMs(cusolverDnDgeqrf_bufferSize(threadCtx.handle, m, n, threadCtx.d_A, m, &lwork),
                             "geqrf_bufferSize")) {
            threadCtx.stats.cpuFallbacks++;
            return qrSolve(A, b);
        }

        // Also check ormqr workspace
        int lwork_ormqr = 0;
        // We need a dummy call - use conservative estimate
        lwork_ormqr = static_cast<int>(m * n); // Conservative estimate
        lwork = std::max(lwork, lwork_ormqr);

        // Ensure we have enough device memory
        if (!ensureDeviceMemory(m, n, lwork)) {
            threadCtx.stats.cpuFallbacks++;
            return qrSolve(A, b);
        }

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
        std::vector<double> b_extended(std::max(m, n));
        std::copy(b.begin(), b.end(), b_extended.begin());

        do {
            // Copy data to device - use async if enabled
            if (asyncMemcpyEnabled) {
                if (!checkCudaMs(cudaMemcpyAsync(threadCtx.d_A, A_col_major.data(), m * n * sizeof(double),
                                                 cudaMemcpyHostToDevice, threadCtx.stream),
                                 "cudaMemcpyAsync d_A")) {
                    success = false;
                    break;
                }
                if (!checkCudaMs(cudaMemcpyAsync(threadCtx.d_b, b_extended.data(), m * sizeof(double),
                                                 cudaMemcpyHostToDevice, threadCtx.stream),
                                 "cudaMemcpyAsync d_b")) {
                    success = false;
                    break;
                }
            } else {
                // Sync copy but on our stream
                if (!checkCudaMs(
                        cudaMemcpy(threadCtx.d_A, A_col_major.data(), m * n * sizeof(double), cudaMemcpyHostToDevice),
                        "cudaMemcpy d_A")) {
                    success = false;
                    break;
                }
                if (!checkCudaMs(
                        cudaMemcpy(threadCtx.d_b, b_extended.data(), m * sizeof(double), cudaMemcpyHostToDevice),
                        "cudaMemcpy d_b")) {
                    success = false;
                    break;
                }
            }

            // Perform QR factorization: A = Q * R (this runs on our stream)
            if (!checkCusolverMs(cusolverDnDgeqrf(threadCtx.handle, m, n, threadCtx.d_A, m, threadCtx.d_tau,
                                                  threadCtx.d_work, lwork, threadCtx.d_info),
                                 "geqrf")) {
                success = false;
                break;
            }

            if (perOpSync) {
                cudaStreamSynchronize(threadCtx.stream);
                threadCtx.stats.streamSyncs++;
            }

            // Check for errors in QR factorization
            int info = 0;
            if (!checkCudaMs(cudaMemcpy(&info, threadCtx.d_info, sizeof(int), cudaMemcpyDeviceToHost),
                             "cudaMemcpy d_info")) {
                success = false;
                break;
            }
            if (info != 0) {
                std::cerr << "[CUDA MultiStream] QR factorization failed with info = " << info << std::endl;
                success = false;
                break;
            }

            // Query workspace for ormqr (multiply by Q^T)
            if (!checkCusolverMs(cusolverDnDormqr_bufferSize(threadCtx.handle, CUBLAS_SIDE_LEFT, CUBLAS_OP_T, m, 1, n,
                                                             threadCtx.d_A, m, threadCtx.d_tau, threadCtx.d_b, m,
                                                             &lwork_ormqr),
                                 "ormqr_bufferSize")) {
                success = false;
                break;
            }

            // Reallocate work buffer if needed
            if (lwork_ormqr > threadCtx.allocatedLwork) {
                cudaFree(threadCtx.d_work);
                if (!checkCudaMs(cudaMalloc(&threadCtx.d_work, lwork_ormqr * sizeof(double)),
                                 "cudaMalloc d_work realloc")) {
                    success = false;
                    break;
                }
                threadCtx.allocatedLwork = lwork_ormqr;
            }

            // Apply Q^T to b: b = Q^T * b (this runs on our stream)
            if (!checkCusolverMs(cusolverDnDormqr(threadCtx.handle, CUBLAS_SIDE_LEFT, CUBLAS_OP_T, m, 1, n,
                                                  threadCtx.d_A, m, threadCtx.d_tau, threadCtx.d_b, m, threadCtx.d_work,
                                                  lwork_ormqr, threadCtx.d_info),
                                 "ormqr")) {
                success = false;
                break;
            }

            if (perOpSync) {
                cudaStreamSynchronize(threadCtx.stream);
                threadCtx.stats.streamSyncs++;
            }

            // Check for errors
            if (!checkCudaMs(cudaMemcpy(&info, threadCtx.d_info, sizeof(int), cudaMemcpyDeviceToHost),
                             "cudaMemcpy d_info ormqr")) {
                success = false;
                break;
            }
            if (info != 0) {
                std::cerr << "[CUDA MultiStream] ormqr failed with info = " << info << std::endl;
                success = false;
                break;
            }

            // Copy R matrix (upper n x n part of A) back to host
            std::vector<double> R_col_major(n * n);
            if (asyncMemcpyEnabled) {
                if (!checkCudaMs(cudaMemcpyAsync(R_col_major.data(), threadCtx.d_A, n * n * sizeof(double),
                                                 cudaMemcpyDeviceToHost, threadCtx.stream),
                                 "cudaMemcpyAsync R")) {
                    success = false;
                    break;
                }
            } else {
                if (!checkCudaMs(
                        cudaMemcpy(R_col_major.data(), threadCtx.d_A, n * n * sizeof(double), cudaMemcpyDeviceToHost),
                        "cudaMemcpy R")) {
                    success = false;
                    break;
                }
            }

            // Copy Q^T * b (first n elements) back to host
            std::vector<double> qtb(n);
            if (asyncMemcpyEnabled) {
                if (!checkCudaMs(cudaMemcpyAsync(qtb.data(), threadCtx.d_b, n * sizeof(double), cudaMemcpyDeviceToHost,
                                                 threadCtx.stream),
                                 "cudaMemcpyAsync qtb")) {
                    success = false;
                    break;
                }
                // Must sync before using the results for back-substitution
                cudaStreamSynchronize(threadCtx.stream);
                threadCtx.stats.streamSyncs++;
            } else {
                if (!checkCudaMs(cudaMemcpy(qtb.data(), threadCtx.d_b, n * sizeof(double), cudaMemcpyDeviceToHost),
                                 "cudaMemcpy qtb")) {
                    success = false;
                    break;
                }
            }

            // Back-substitution to solve R * x = qtb (CPU, very fast for small n)
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

            threadCtx.stats.gpuSolves++;
            return x;

        } while (false);

        if (!success) {
            threadCtx.stats.cpuFallbacks++;
            return qrSolve(A, b);
        }
    }
#endif

    // Fall back to original GPU implementation or CPU
    // This uses the existing gpuQrSolve which has its own logic
    return gpuQrSolve(A, b);
}

} // namespace QuantExt
