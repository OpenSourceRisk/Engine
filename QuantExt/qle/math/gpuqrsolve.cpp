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

#include <qle/math/gpuqrsolve.hpp>

#include <ql/errors.hpp>
#include <ql/math/matrixutilities/qrdecomposition.hpp>

#include <algorithm>
#include <atomic>
#include <iostream>

#ifdef ORE_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cusolverDn.h>
#endif

namespace {

// Process-wide opt-in flag set by the simulation analytic during input
// parsing when the user has
// `<Parameter name="amcUseGpuRegression">Y</Parameter>` in their XML
// config. Read on every entry into gpuQrSolve. Default false so a
// CUDA-built binary running an existing config that does not set the
// flag behaves exactly like a non-CUDA build.
std::atomic<bool> g_use_gpu_regression{false};

#ifdef ORE_ENABLE_CUDA

// Below this row count the per-call CUDA launch overhead exceeds the GEMM
// win for AMC-shaped problems, so the dispatcher routes back to CPU. Chosen
// empirically; AMC regression matrices are typically m >= calibrationSamples
// (1000+) so this threshold rarely fires in practice.
constexpr QuantLib::Size kGpuMinRows = 1000;

// Per-worker-thread state. RAII destructor frees device + pinned host
// buffers and tears down the cuSOLVER handle / stream when the AMC worker
// thread exits. C++ runs thread_local destructors at thread exit only for
// objects with non-trivial dtors, so we keep ALL state in this struct
// (don't pull pieces out as raw thread_local handles — those would leak).
struct ThreadCtx {
    cusolverDnHandle_t handle = nullptr;
    cudaStream_t stream = nullptr;
    bool initialized = false;
    bool init_failed = false; // sticky; once set we go straight to CPU

    // Device buffers (pooled, grow on demand).
    double* d_A = nullptr;
    double* d_b = nullptr;
    double* d_tau = nullptr;
    double* d_work = nullptr;
    int* d_info = nullptr;
    std::size_t cap_A = 0;     // doubles
    std::size_t cap_b = 0;     // doubles
    std::size_t cap_tau = 0;   // doubles
    std::size_t cap_work = 0;  // doubles

    // Pinned host buffers (cudaMallocHost). cudaMemcpyAsync from pageable
    // memory silently stages through an internal pinned buffer, which
    // serialises the copy with subsequent stream work — defeating the
    // per-thread-stream concurrency. Pinning is a measurable speedup.
    double* h_A = nullptr;
    double* h_b = nullptr;
    double* h_R = nullptr;
    double* h_qtb = nullptr;
    int* h_info = nullptr;
    std::size_t cap_hA = 0;    // doubles
    std::size_t cap_hb = 0;    // doubles
    std::size_t cap_hR = 0;    // doubles
    std::size_t cap_hqtb = 0;  // doubles

    ~ThreadCtx() {
        // Best-effort cleanup. Errors during program-exit teardown are
        // swallowed — the CUDA primary context may already have been
        // reset by the runtime, in which case these calls return error
        // codes we can't act on.
        if (d_A) cudaFree(d_A);
        if (d_b) cudaFree(d_b);
        if (d_tau) cudaFree(d_tau);
        if (d_work) cudaFree(d_work);
        if (d_info) cudaFree(d_info);
        if (h_A) cudaFreeHost(h_A);
        if (h_b) cudaFreeHost(h_b);
        if (h_R) cudaFreeHost(h_R);
        if (h_qtb) cudaFreeHost(h_qtb);
        if (h_info) cudaFreeHost(h_info);
        if (handle) cusolverDnDestroy(handle);
        if (stream) cudaStreamDestroy(stream);
    }
};

thread_local ThreadCtx tctx;

bool checkCuda(cudaError_t e, const char* what) {
    if (e == cudaSuccess) return true;
    std::cerr << "[gpuQrSolve] cuda " << what << ": " << cudaGetErrorString(e) << std::endl;
    return false;
}

bool checkCusolver(cusolverStatus_t e, const char* what) {
    if (e == CUSOLVER_STATUS_SUCCESS) return true;
    std::cerr << "[gpuQrSolve] cusolver " << what << ": " << e << std::endl;
    return false;
}

// Lazily create the per-thread non-blocking stream + cuSOLVER handle and
// bind them. On any failure we mark init_failed so subsequent calls go
// straight to the CPU path without retrying.
bool ensureThreadCtx() {
    if (tctx.initialized) return true;
    if (tctx.init_failed) return false;

    if (!checkCuda(cudaStreamCreateWithFlags(&tctx.stream, cudaStreamNonBlocking), "stream create")) {
        tctx.stream = nullptr;
        tctx.init_failed = true;
        return false;
    }
    if (!checkCusolver(cusolverDnCreate(&tctx.handle), "handle create")) {
        cudaStreamDestroy(tctx.stream);
        tctx.stream = nullptr;
        tctx.handle = nullptr;
        tctx.init_failed = true;
        return false;
    }
    if (!checkCusolver(cusolverDnSetStream(tctx.handle, tctx.stream), "set stream")) {
        cusolverDnDestroy(tctx.handle);
        cudaStreamDestroy(tctx.stream);
        tctx.handle = nullptr;
        tctx.stream = nullptr;
        tctx.init_failed = true;
        return false;
    }
    tctx.initialized = true;
    return true;
}

// Grow pooled device buffer to at least `need_doubles`. 20% headroom so
// modest size drift does not retrigger malloc.
bool growDevice(double*& buf, std::size_t& cap, std::size_t need, const char* name) {
    if (cap >= need && buf != nullptr) return true;
    if (buf) cudaFree(buf);
    buf = nullptr;
    cap = static_cast<std::size_t>(need * 1.2) + 16;
    if (!checkCuda(cudaMalloc(&buf, cap * sizeof(double)), name)) {
        buf = nullptr;
        cap = 0;
        return false;
    }
    return true;
}

bool growPinned(double*& buf, std::size_t& cap, std::size_t need, const char* name) {
    if (cap >= need && buf != nullptr) return true;
    if (buf) cudaFreeHost(buf);
    buf = nullptr;
    cap = static_cast<std::size_t>(need * 1.2) + 16;
    if (!checkCuda(cudaMallocHost(reinterpret_cast<void**>(&buf), cap * sizeof(double)), name)) {
        buf = nullptr;
        cap = 0;
        return false;
    }
    return true;
}

#endif // ORE_ENABLE_CUDA

} // anonymous namespace

namespace QuantExt {

void setUseGpuRegression(bool b) { g_use_gpu_regression.store(b, std::memory_order_relaxed); }
bool useGpuRegression() { return g_use_gpu_regression.load(std::memory_order_relaxed); }

bool gpuQrSolveAvailable() {
#ifdef ORE_ENABLE_CUDA
    if (!useGpuRegression()) return false;
    int n = 0;
    return cudaGetDeviceCount(&n) == cudaSuccess && n > 0;
#else
    return false;
#endif
}

Array gpuQrSolve(const Matrix& A, const Array& b) {
    const Size m = A.rows();
    QL_REQUIRE(b.size() == m, "gpuQrSolve: dim mismatch (A.rows=" << m << ", b.size=" << b.size() << ")");

#ifdef ORE_ENABLE_CUDA
    const Size n = A.columns();

    if (!gpuQrSolveAvailable() || m < kGpuMinRows || !ensureThreadCtx()) {
        return qrSolve(A, b);
    }

    // Pinned host staging. If pinning fails we fall through to CPU rather
    // than degrade silently to pageable async memcpy (which serialises and
    // wipes out the per-thread-stream win).
    if (!growPinned(tctx.h_A, tctx.cap_hA, m * n, "alloc h_A")
        || !growPinned(tctx.h_b, tctx.cap_hb, std::max(m, n), "alloc h_b")
        || !growPinned(tctx.h_R, tctx.cap_hR, n * n, "alloc h_R")
        || !growPinned(tctx.h_qtb, tctx.cap_hqtb, n, "alloc h_qtb")) {
        return qrSolve(A, b);
    }
    if (tctx.h_info == nullptr) {
        if (!checkCuda(cudaMallocHost(reinterpret_cast<void**>(&tctx.h_info), sizeof(int)), "alloc h_info")) {
            return qrSolve(A, b);
        }
    }

    // Device buffers. d_b is sized max(m, n) so the in-place ormqr write
    // and the back-sub read share storage even when n exceeds m (which it
    // shouldn't for valid AMC inputs but we don't want to UB on it).
    if (!growDevice(tctx.d_A, tctx.cap_A, m * n, "alloc d_A")
        || !growDevice(tctx.d_b, tctx.cap_b, std::max(m, n), "alloc d_b")
        || !growDevice(tctx.d_tau, tctx.cap_tau, n, "alloc d_tau")) {
        return qrSolve(A, b);
    }
    if (tctx.d_info == nullptr) {
        if (!checkCuda(cudaMalloc(&tctx.d_info, sizeof(int)), "alloc d_info")) {
            return qrSolve(A, b);
        }
    }

    // QuantLib::Matrix is row-major; cuSOLVER wants column-major. Transpose
    // while copying into the pinned staging buffer.
    for (Size i = 0; i < m; ++i)
        for (Size j = 0; j < n; ++j)
            tctx.h_A[j * m + i] = A[i][j];
    std::copy(b.begin(), b.end(), tctx.h_b);

    bool ok = false;
    do {
        // All device I/O is on tctx.stream (non-blocking). We sync once at
        // the end. cudaMemcpyAsync from pinned memory is truly async; from
        // pageable it would silently stage and serialise.
        if (!checkCuda(cudaMemcpyAsync(tctx.d_A, tctx.h_A, m * n * sizeof(double),
                                     cudaMemcpyHostToDevice, tctx.stream), "memcpy A")) break;
        if (!checkCuda(cudaMemcpyAsync(tctx.d_b, tctx.h_b, m * sizeof(double),
                                     cudaMemcpyHostToDevice, tctx.stream), "memcpy b")) break;

        // Workspace size for geqrf (host-side query).
        int lwork_geqrf = 0;
        if (!checkCusolver(cusolverDnDgeqrf_bufferSize(tctx.handle, m, n, tctx.d_A, m, &lwork_geqrf),
                     "geqrf bufsize")) break;
        // Workspace size for ormqr — query before any allocation so we
        // size d_work for the larger of the two and avoid mid-flight
        // realloc (which would force a stream sync).
        int lwork_ormqr = 0;
        if (!checkCusolver(cusolverDnDormqr_bufferSize(tctx.handle, CUBLAS_SIDE_LEFT, CUBLAS_OP_T,
                                                  m, 1, n, tctx.d_A, m, tctx.d_tau, tctx.d_b, m,
                                                  &lwork_ormqr), "ormqr bufsize")) break;
        const int lwork = std::max(lwork_geqrf, lwork_ormqr);
        if (!growDevice(tctx.d_work, tctx.cap_work, static_cast<std::size_t>(lwork), "alloc d_work")) break;

        if (!checkCusolver(cusolverDnDgeqrf(tctx.handle, m, n, tctx.d_A, m, tctx.d_tau,
                                       tctx.d_work, lwork, tctx.d_info), "geqrf")) break;

        // Apply Q^T to b in-place: d_b <- Q^T * d_b.
        if (!checkCusolver(cusolverDnDormqr(tctx.handle, CUBLAS_SIDE_LEFT, CUBLAS_OP_T,
                                       m, 1, n, tctx.d_A, m, tctx.d_tau, tctx.d_b, m,
                                       tctx.d_work, lwork, tctx.d_info), "ormqr")) break;

        // R is the upper n x n block of d_A (column-major, stride m). A flat
        // cudaMemcpy of n*n*8 bytes would read only column 0 — the long-
        // standing "R-copy pitch" bug. cudaMemcpy2DAsync with src pitch =
        // m*sizeof(double) selects the right block.
        if (!checkCuda(cudaMemcpy2DAsync(tctx.h_R, n * sizeof(double),
                                        tctx.d_A, m * sizeof(double),
                                        n * sizeof(double), n,
                                        cudaMemcpyDeviceToHost, tctx.stream), "memcpy2D R")) break;
        if (!checkCuda(cudaMemcpyAsync(tctx.h_qtb, tctx.d_b, n * sizeof(double),
                                     cudaMemcpyDeviceToHost, tctx.stream), "memcpy qtb")) break;
        if (!checkCuda(cudaMemcpyAsync(tctx.h_info, tctx.d_info, sizeof(int),
                                     cudaMemcpyDeviceToHost, tctx.stream), "memcpy info")) break;

        if (!checkCuda(cudaStreamSynchronize(tctx.stream), "stream sync")) break;

        if (*tctx.h_info != 0) {
            std::cerr << "[gpuQrSolve] cusolver info=" << *tctx.h_info << " — falling back" << std::endl;
            break;
        }

        ok = true;
    } while (false);

    if (!ok) return qrSolve(A, b);

    // Back-substitute R x = Q^T b on the host. n is small (basis dim ~10),
    // a trsm kernel + round-trip costs more than this loop.
    Array x(n);
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        double s = tctx.h_qtb[i];
        for (Size j = i + 1; j < n; ++j)
            s -= tctx.h_R[j * n + i] * x[j];
        const double Rii = tctx.h_R[i * n + i];
        x[i] = std::abs(Rii) < 1e-14 ? 0.0 : s / Rii;
    }
    return x;
#else
    return qrSolve(A, b);
#endif
}

} // namespace QuantExt
