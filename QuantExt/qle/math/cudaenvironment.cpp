/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/math/cudaenvironment.hpp>
#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/timer/timer.hpp>

#include <iostream>

#ifdef ORE_ENABLE_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <curand.h>
#include <curand_mtgp32_host.h>
#include <nvrtc.h>
#endif

#define MAX_N_PLATFORMS 4U
#define MAX_N_DEVICES 8U
#define MAX_N_NAME 64U
#define MAX_BUILD_LOG 65536U
#define MAX_BUILD_LOG_LOGFILE 1024U

namespace {
std::string curandGetErrorString(curandStatus_t err) {
    switch (err) {
    case CURAND_STATUS_SUCCESS:
        return "CURAND_STATUS_SUCCESS";
    case CURAND_STATUS_VERSION_MISMATCH:
        return "CURAND_STATUS_VERSION_MISMATCH";
    case CURAND_STATUS_NOT_INITIALIZED:
        return "CURAND_STATUS_NOT_INITIALIZED";
    case CURAND_STATUS_ALLOCATION_FAILED:
        return "CURAND_STATUS_ALLOCATION_FAILED";
    case CURAND_STATUS_TYPE_ERROR:
        return "CURAND_STATUS_TYPE_ERROR";
    case CURAND_STATUS_OUT_OF_RANGE:
        return "CURAND_STATUS_OUT_OF_RANGE";
    case CURAND_STATUS_LENGTH_NOT_MULTIPLE:
        return "CURAND_STATUS_LENGTH_NOT_MULTIPLE";
    case CURAND_STATUS_DOUBLE_PRECISION_REQUIRED:
        return "CURAND_STATUS_DOUBLE_PRECISION_REQUIRED";
    case CURAND_STATUS_LAUNCH_FAILURE:
        return "CURAND_STATUS_LAUNCH_FAILURE";
    case CURAND_STATUS_PREEXISTING_FAILURE:
        return "CURAND_STATUS_PREEXISTING_FAILURE";
    case CURAND_STATUS_INITIALIZATION_FAILED:
        return "CURAND_STATUS_INITIALIZATION_FAILED";
    case CURAND_STATUS_ARCH_MISMATCH:
        return "CURAND_STATUS_ARCH_MISMATCH";
    case CURAND_STATUS_INTERNAL_ERROR:
        return "CURAND_STATUS_INTERNAL_ERROR";
    default:
        return "unknown curand error code " + std::to_string(err);
    }
}

std::map<std::string, std::string> getGPUArchitecture{{"Nvidia Geforce RTX 3080", "compute_86"},
                                                       {"Quadro T1000", "compute_75"}};
} // namespace

namespace QuantExt {

#ifdef ORE_ENABLE_CUDA

class CudaContext : public ComputeContext {
public:
    CudaContext(size_t device,
                const std::vector<std::pair<std::string, std::string>>& deviceInfo, const bool supportsDoublePrecision);
    ~CudaContext() override final;
    void init() override final;

    std::pair<std::size_t, bool> initiateCalculation(const std::size_t n, const std::size_t id = 0,
                                                     const std::size_t version = 0,
                                                     const Settings settings = {}) override final;
    void disposeCalculation(const std::size_t n) override final;
    std::size_t createInputVariable(double v) override final;
    std::size_t createInputVariable(double* v) override final;
    std::vector<std::vector<std::size_t>> createInputVariates(const std::size_t dim, const std::size_t steps) override final;
    std::size_t applyOperation(const std::size_t randomVariableOpCode,
                               const std::vector<std::size_t>& args) override final;
    void freeVariable(const std::size_t id) override final;
    void declareOutputVariable(const std::size_t id) override final;
    void finalizeCalculation(std::vector<double*>& output) override final;

    std::vector<std::pair<std::string, std::string>> deviceInfo() const override;
    bool supportsDoublePrecision() const override;
    const DebugInfo& debugInfo() const override final;

private:

    void releaseMem(double*& m, const std::string& desc);
    void releaseMem(size_t id, const std::string& desc);
    void releaseModule(CUmodule& k, const std::string& desc);
    void releaseMersenneTwisterStates(curandStateMtgp32*& rngState, const std::string& desc);
    void updateVariates();

    enum class ComputeState { idle, createInput, createVariates, calc };

    bool initialized_ = false;
    size_t device_;
    CUcontext context_;
    size_t NUM_THREADS_ = 256;
    
    std::vector<std::pair<std::string, std::string>> deviceInfo_;
    bool supportsDoublePrecision_;
    size_t maxRandomVariates_;
    double* d_randomVariables_;

    // will be accumulated over all calcs
    ComputeContext::DebugInfo debugInfo_;

    // 1a vectors per current calc id

    std::vector<std::size_t> size_;
    std::vector<bool> hasKernel_;
    std::vector<bool> disposed_;
    std::vector<std::size_t> version_;
    std::vector<CUmodule> module_;
    std::vector<CUfunction> kernel_;
    std::vector<std::size_t> nRandomVariables_;
    std::vector<std::size_t> nOperations_;
    std::vector<std::size_t> nNUM_BLOCKS_;
    std::vector<curandStateMtgp32*> mersenneTwisterStates_;
    std::vector<std::size_t> nOutputVariables_;

    std::map<std::size_t, std::vector<double*>> nOutputPtrList_;
    std::map<std::size_t, double**> nOutputPtr_;

    // 2 curent calc

    std::size_t currentId_ = 0;
    ComputeState currentState_ = ComputeState::idle;
    std::size_t nInputVars_;
    Settings settings_;

    // 2a indexed by var id
    std::vector<bool> inputVarIsScalar_;
    std::vector<double*> hostVarList_, deviceVarList_;

    // 2b collection of variable ids
    std::vector<std::size_t> freedVariables_;
    std::vector<std::size_t> outputVariables_;

    // 2c variate states
    //curandStateMtgp32* mersenneTwisterStates_

    // 2d kernel source code
    std::string source_;
};

CudaFramework::CudaFramework() {
    int nDevices;
    cudaGetDeviceCount(&nDevices);
    for (std::size_t d = 0; d < nDevices; ++d) {
        cudaDeviceProp device_prop;
        cudaGetDeviceProperties(&device_prop, 0);
        char device_name[MAX_N_NAME];
        std::vector<std::pair<std::string, std::string>> deviceInfo;
        contexts_["CUDA/DEFAULT/" + std::string(device_prop.name)] = new CudaContext(d, deviceInfo, true);
    }
}

CudaFramework::~CudaFramework() {
    for (auto& [_, c] : contexts_) {
        delete c;
    }
}

CudaContext::CudaContext(
    size_t device,
    const std::vector<std::pair<std::string, std::string>>& deviceInfo, const bool supportsDoublePrecision)
    : initialized_(false), device_(device), deviceInfo_(deviceInfo), supportsDoublePrecision_(supportsDoublePrecision) {}

CudaContext::~CudaContext() {
    if (initialized_) {
        CUresult err;
        
        for (Size i = 0; i < mersenneTwisterStates_.size(); ++i) {
            if (disposed_[i])
                continue;
            releaseMersenneTwisterStates(mersenneTwisterStates_[i], "~CudaContext()");
        }

        for (auto& outer : nOutputPtrList_) {
            for (auto& ptr : outer.second) {
                releaseMem(ptr, "~CudaContext()");
            }
        }

        for (auto& ptr : nOutputPtr_) {
            auto cudaErr = cudaFree(ptr.second);
            QL_REQUIRE(cudaErr == cudaSuccess,
                       "CudaContext::~CudaContext(): free memory for nOutputPtr_ fails: "
                           << cudaGetErrorString(cudaErr));
        }

        for (Size i = 0; i < module_.size(); ++i) {
            if (disposed_[i])
                continue;
            releaseModule(module_[i], "~CudaContext()");
        }

        err = cuCtxDestroy(context_);
        if (err != CUDA_SUCCESS) {
            const char* errorStr;
            cuGetErrorString(err, &errorStr);
            std::cerr << "CudaContext: error during cuCtxDestroy: " << errorStr << std::endl;
        }
        
    }
}

void CudaContext::releaseMem(double*& m, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(m); err != cudaSuccess) {
        std::cerr << "CudaContext: error during cudaFree at "<< description
                  << ": " << cudaGetErrorString(err) << std::endl;
    }
}

void CudaContext::releaseMem(size_t id, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(deviceVarList_[id]); err != cudaSuccess) {
        std::cerr << "CudaContext: error during releaseMem at " << description << ": " << cudaGetErrorString(err)
                  << std::endl;
    }
}

void CudaContext::releaseMersenneTwisterStates(curandStateMtgp32*& rngState, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(rngState); err != cudaSuccess) {
        std::cerr << "CudaContext: error during releaseMersenneTwisterStates at " << description << ": "
                  << cudaGetErrorString(err) << std::endl;
    }
}

void CudaContext::releaseModule(CUmodule& k, const std::string& description) {
    CUresult err = cuModuleUnload(k);
    if (err != CUDA_SUCCESS) {
        const char* errorStr;
        cuGetErrorString(err, &errorStr);
        std::cerr << "CudaContext: error during cuModuleUnload at " << description << ": " << errorStr << std::endl;
    }
}

void CudaContext::init() {

    if (initialized_) {
        return;
    }

    debugInfo_.numberOfOperations = 0;
    debugInfo_.nanoSecondsDataCopy = 0;
    debugInfo_.nanoSecondsProgramBuild = 0;
    debugInfo_.nanoSecondsCalculation = 0;

    const char* errStr;
    maxRandomVariates_ = 0;

    // Initialize CUDA context and module
    cuInit(0);
    CUresult err = cuCtxCreate(&context_, 0, 0);
    if (err != CUDA_SUCCESS) {
        cuGetErrorString(err, &errStr);
        std::cerr << "CUDAContext::CUDAContext(): error during cuCtxCreate(): " << errStr << std::endl;
    }

    initialized_ = true;
}

//curandStateMtgp32* CudaContext::initMersenneTwisterRng(std::size_t nBlocks, std::uint32_t seed) {
//
//    // For Mersenne Twister device API, at most 200 states can be generated one time.
//    // If this passed the test, we will generate 2 separate mtStates for blocks after 200.
//    // Need to test if using same seed will generate same random numbers
//    QL_REQUIRE(nBlocks <= 200, "When using Mersenne Twister, at most 200 states can be generate now");
//    
//    cudaError_t cudaErr;
//    curandStatus_t curandErr;
//
//    // Random generator state
//    curandStateMtgp32* mtStates;
//    cudaErr = cudaMalloc(&mtStates, nBlocks * sizeof(curandStateMtgp32));
//    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::initMersenneTwisterRng(): memory allocate for mtStates_ fails: "
//                                           << cudaGetErrorString(cudaErr));
//
//    // Define MTGP32 parameters
//    mtgp32_kernel_params* kernelParams;
//    cudaErr = cudaMalloc((void**)&kernelParams, sizeof(mtgp32_kernel_params));
//    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::initMersenneTwisterRng(): memory allocate for kernelParams fails: "
//                                           << cudaGetErrorString(cudaErr));
//
//    // Initialize MTGP32 states
//    curandErr = curandMakeMTGP32Constants(mtgp32dc_params_fast_11213, kernelParams);
//    QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
//               "CudaContext::initMersenneTwisterRng(): error during curandMakeMTGP32Constants(): "
//                   << curandGetErrorString(curandErr));
//    curandErr = curandMakeMTGP32KernelState(mtStates, mtgp32dc_params_fast_11213, kernelParams,
//                                            nNUM_BLOCKS_[currentId_ - 1], seed);
//    QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
//               "CudaContext::initMersenneTwisterRng(): error during curandMakeMTGP32KernelState(): "
//                   << curandGetErrorString(curandErr));
//
//    return mtStates;
//}

void CudaContext::disposeCalculation(const std::size_t id) {
    QL_REQUIRE(!disposed_[id - 1], "CudaContext::disposeCalculation(): id " << id << " was already disposed.");
    //disposed_[id - 1] = true;
    //releaseMersenneTwisterStates(mersenneTwisterStates_[id - 1]);
    //releaseModule(module_[id - 1]);
}

std::pair<std::size_t, bool> CudaContext::initiateCalculation(const std::size_t n, const std::size_t id,
                                                              const std::size_t version, const Settings settings) {
    QL_REQUIRE(n > 0, "CudaContext::initiateCalculation(): n must not be zero");

    bool newCalc = false;
    settings_ = settings;

    if (id == 0) {

        // initiate new calcaultion

        size_.push_back(n);
        hasKernel_.push_back(false);
        disposed_.push_back(false);
        version_.push_back(version);

        CUmodule cuModule;
        module_.push_back(cuModule);

        CUfunction function;
        kernel_.push_back(function);

        nNUM_BLOCKS_.push_back((n + NUM_THREADS_ - 1) / NUM_THREADS_);

        nOutputVariables_.push_back(0);
        nRandomVariables_.push_back(0);
        nOperations_.push_back(0);

        mersenneTwisterStates_.push_back(nullptr);

        currentId_ = hasKernel_.size();
        newCalc = true;

    } else {

        // initiate calculation on existing id

        QL_REQUIRE(id <= hasKernel_.size(),
                   "CudaContext::initiateCalculation(): id (" << id << ") invalid, got 1..." << hasKernel_.size());
        QL_REQUIRE(size_[id - 1] == n, "OpenClCOntext::initiateCalculation(): size ("
                                           << size_[id - 1] << ") for id " << id << " does not match current size ("
                                           << n << ")");
        QL_REQUIRE(!disposed_[id - 1], "CudaContext::initiateCalculation(): id ("
                                           << id << ") was already disposed, it can not be used any more.");

        if (version != version_[id - 1]) {
            hasKernel_[id - 1] = false;
            version_[id - 1] = version;
            releaseModule(module_[id - 1], "initiateCalculation"); // releaseModule will also release the linked kernel
            nOutputVariables_[id - 1] = 0;
            nRandomVariables_[id - 1] = 0;
            nOperations_[id - 1] = 0;
            newCalc = true;
        }

        currentId_ = id;
    }

    nInputVars_ = 0;
    inputVarIsScalar_.clear();

    hostVarList_.clear();
    deviceVarList_.clear();

    freedVariables_.clear();
    outputVariables_.clear();

    // reset kernel source

    source_.clear();

    // set state

    currentState_ = ComputeState::createInput;

    // return calc id

    return std::make_pair(currentId_, newCalc);
}

std::size_t CudaContext::createInputVariable(double v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "CudaContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                << ")");
    double* hostVar = new double(v);
    double* dMem;
    deviceVarList_.push_back(dMem);
    inputVarIsScalar_.push_back(true);
    hostVarList_.push_back(hostVar);
    return nInputVars_++;
}

std::size_t CudaContext::createInputVariable(double* v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "CudaContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                  << ")");
    double* dMem;
    deviceVarList_.push_back(dMem);
    inputVarIsScalar_.push_back(false);
    hostVarList_.push_back(v);
    return nInputVars_++;
}

std::vector<std::vector<std::size_t>> CudaContext::createInputVariates(const std::size_t dim, const std::size_t steps) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates,
               "CudaContext::createInputVariable(): not in state createInput or createVariates ("
                   << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(currentId_ > 0, "CudaContext::createInputVariates(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::createInputVariates(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, input variates cannot be regenerated.");
    currentState_ = ComputeState::createVariates;
    // For Mersenne Twister device API, at most 200 states can be generated one time.
    // If this passed the test, we will generate 2 separate mtStates for blocks after 200.
    // Need to test if using same seed will generate same random numbers
    QL_REQUIRE(nNUM_BLOCKS_[currentId_ - 1] <= 200,
               "When using Mersenne Twister, at most 200 states can be generate now");
    cudaError_t cudaErr;
    curandStatus_t curandErr;

    // Random generator state
    curandStateMtgp32* mtStates;
    cudaErr = cudaMalloc(&mtStates, nNUM_BLOCKS_[currentId_ - 1] * sizeof(curandStateMtgp32));
    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::initMersenneTwisterRng(): memory allocate for mtStates_ fails: "
                                           << cudaGetErrorString(cudaErr));

    // Define MTGP32 parameters
    mtgp32_kernel_params* kernelParams;
    cudaErr = cudaMalloc((void**)&kernelParams, sizeof(mtgp32_kernel_params));
    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::initMersenneTwisterRng(): memory allocate for kernelParams fails: "
                                           << cudaGetErrorString(cudaErr));

    // Initialize MTGP32 states
    curandErr = curandMakeMTGP32Constants(mtgp32dc_params_fast_11213, kernelParams);
    QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
               "CudaContext::initMersenneTwisterRng(): error during curandMakeMTGP32Constants(): "
                   << curandGetErrorString(curandErr));
    curandErr = curandMakeMTGP32KernelState(mtStates, mtgp32dc_params_fast_11213, kernelParams,
                                            nNUM_BLOCKS_[currentId_ - 1], settings_.rngSeed);
    QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
               "CudaContext::initMersenneTwisterRng(): error during curandMakeMTGP32KernelState(): "
                   << curandGetErrorString(curandErr));
    mersenneTwisterStates_[currentId_ - 1] = mtStates;

    std::vector<std::vector<std::size_t>> resultIds(dim, std::vector<std::size_t>(steps));
    for (std::size_t i = 0; i < dim; ++i) {
        for (std::size_t j = 0; j < steps; ++j) {
            resultIds[i][j] = nInputVars_ + nRandomVariables_[currentId_ - 1];
            nRandomVariables_[currentId_ - 1]++;
        }
    }

    updateVariates();

    return resultIds;
}

void CudaContext::updateVariates() {
    cudaError_t cudaErr;

    if (nRandomVariables_[currentId_ - 1] > maxRandomVariates_) {
        if (maxRandomVariates_ == 0)
            releaseMem(d_randomVariables_, "updateVariates()");
        // Allocate memory for random variables
        cudaErr =
            cudaMalloc(&d_randomVariables_, nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::createInputVariates(): memory allocate for d_randomVariables_ fails: "
                       << cudaGetErrorString(cudaErr));

        std::string rngIncludeSource = "#include <curand_kernel.h>\n\n";

        std::string rngKernelName = "ore_rngKernel";

        std::string rngKernelSource = rngIncludeSource + "extern \"C\" __global__ void " + rngKernelName +
                                      "(curandStateMtgp32 *mtStates, int n,  double* randomVariables) {\n"
                                      "    int tid = blockIdx.x * blockDim.x + threadIdx.x;\n";
        for (size_t i = 0; i < nRandomVariables_[currentId_ - 1]; ++i) {
            rngKernelSource += "    double v" + std::to_string(i) + " = curand_normal_double(&mtStates[blockIdx.x]);\n";
            if (settings_.debug)
                debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
        }
        rngKernelSource += "    if (tid < n) {\n";
        for (size_t i = 0; i < nRandomVariables_[currentId_ - 1]; ++i) {
            rngKernelSource += "        randomVariables[tid + " + std::to_string(i * size_[currentId_ - 1]) + "] = v" +
                               std::to_string(i) + ";\n";
            if (settings_.debug)
                debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
        }
        rngKernelSource += "    }\n"
                           "}\n";

        std::cout << rngKernelSource << std::endl;
        // Compile source code
        nvrtcResult nvrtcErr;
        nvrtcProgram nvrtcProgram;
        CUmodule cuModule;
        CUfunction rngKernel;
        nvrtcErr = nvrtcCreateProgram(&nvrtcProgram, rngKernelSource.c_str(), "kernel.cu", 0, nullptr, nullptr);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcCreateProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        // const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    (std::string("--gpu-architecture=") + getGPUArchitecture[device_prop.name]).c_str(), " - std = c++ 17 ",
        //    nullptr};
        // const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on",
        //    "--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt", nullptr};
        // nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 5, compileOptions);
        const char* compileOptions[] = {
            "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
            "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on",
            //"--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt",
            nullptr};
        nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 4, compileOptions);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcCompileProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));

        // Retrieve the compiled PTX code
        size_t ptxSize;
        nvrtcErr = nvrtcGetPTXSize(nvrtcProgram, &ptxSize);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcGetPTXSize(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        char* ptx = new char[ptxSize];
        nvrtcErr = nvrtcGetPTX(nvrtcProgram, ptx);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcGetPTXSize(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        nvrtcErr = nvrtcDestroyProgram(&nvrtcProgram);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcDestroyProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));

        CUresult cuErr = cuModuleLoadData(&cuModule, ptx);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::finalizeCalculation(): error during cuModuleLoadData(): " << errStr << std::endl;
        }
        cuErr = cuModuleGetFunction(&rngKernel, cuModule, rngKernelName.c_str());
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::createInputVariates(): error during cuModuleGetFunction(): " << errStr
                      << std::endl;
        }
        delete[] ptx;

        // set kernel args

        void* args[] = {&mersenneTwisterStates_[currentId_ - 1], &size_[currentId_ - 1], &d_randomVariables_};

        // execute kernel
        cuErr = cuLaunchKernel(rngKernel, nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, 0, args, nullptr);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::createInputVariates(): error during cuLaunchKernel(): " << errStr << std::endl;
        }

        // releaseMersenneTwisterStates(mersenneTwisterStates_[id - 1]);
        releaseModule(cuModule, "createInputVariates");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1];
    }
}

std::size_t CudaContext::applyOperation(const std::size_t randomVariableOpCode,
                                          const std::vector<std::size_t>& args) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates ||
                   currentState_ == ComputeState::calc,
               "CudaContext::applyOperation(): not in state createInput or calc (" << static_cast<int>(currentState_)
                                                                                     << ")");
    currentState_ = ComputeState::calc;
    QL_REQUIRE(currentId_ > 0, "CudaContext::applyOperation(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::applyOperation(): id (" << currentId_ << ") in version "
                                                                                    << version_[currentId_ - 1]
                                                                                    << " has a kernel already.");

    // determine variable id to use for result

    std::size_t resultId;
    bool resultIdNeedsDeclaration;
    if (!freedVariables_.empty()) {
        resultId = freedVariables_.back();
        freedVariables_.pop_back();
        resultIdNeedsDeclaration = false;
    } else {
        resultId = nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1];
        nOperations_[currentId_ - 1]++;
       resultIdNeedsDeclaration = true;
    }

    // determine arg variable names
    QL_REQUIRE(args.size() == 1 || args.size() == 2,
               "CudaContext::applyOperation() args.size() must be 1 or 2, got" << args.size());
    std::vector<std::string> argStr(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] < inputVarIsScalar_.size())
            argStr[i] =
                "input[" + std::to_string(args[i]) + "][" + std::string(inputVarIsScalar_[args[i]] ? "0" : "tid") + "]";
        else
            argStr[i] = "v" + std::to_string(args[i]);
    }

    // generate source code
    if (resultIdNeedsDeclaration)
        source_ += "        double v" + std::to_string(resultId) + " = ";
    else
        source_ += "        v" + std::to_string(resultId) + " = ";

    switch (randomVariableOpCode) {
    case RandomVariableOpCode::None: {
        source_ += argStr[0] + ";\n";
    }
    case RandomVariableOpCode::Add: {
        source_ += argStr[0] + " + " + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::Subtract: {
        source_ += argStr[0] + " - " + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::Negative: {
        source_ += "-" + argStr[0] + ";\n";
        break;
    }
    case RandomVariableOpCode::Mult: {
        source_ += argStr[0] + " * " + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::Div: {
        source_ += argStr[0] + " / " + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::IndicatorEq: {
        source_ += "ore_indicatorEq(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::IndicatorGt: {
        source_ += "ore_indicatorGt(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::IndicatorGeq: {
        source_ += "ore_indicatorGeq(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::Min: {
        source_ += "fmin(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::Max: {
        source_ += "fmax(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::Abs: {
        source_ += "fabs(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Exp: {
        source_ += "exp(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Sqrt: {
        source_ += "sqrt(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Log: {
        source_ += "log(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Pow: {
        source_ += "pow(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::NormalCdf: {
        source_ += "normcdf(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::NormalPdf: {
        source_ += "normpdf(" + argStr[0] + ");\n";
        break;
    }
    default: {
        QL_FAIL("CudaContext::executeKernel(): no implementation for op code "
                << randomVariableOpCode << " (" << getRandomVariableOpLabels()[randomVariableOpCode] << ") provided.");
    }
    }

    // update num of ops in debug info
    if (settings_.debug)
        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];

    // return result id
    return resultId;
}

void CudaContext::freeVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ == ComputeState::calc,
               "CudaContext::free(): not in state calc (" << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::freeVariable(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, free variable cannot be called.");

    // we do not free input variables, only variables that were added during the calc

    if (id < nInputVars_)
        return;

    freedVariables_.push_back(id);
}

void CudaContext::declareOutputVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ != ComputeState::idle, "CudaContext::declareOutputVariable(): state is idle");
    QL_REQUIRE(currentId_ > 0, "CudaContext::declareOutputVariable(): current id not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::declareOutputVariable(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, output variables cannot be redeclared.");
    //nOutputVariables_[currentId_ - 1].push_back(id);
    outputVariables_.push_back(id);
}

void CudaContext::finalizeCalculation(std::vector<double*>& output) {
    struct exitGuard {
        exitGuard() {}
        ~exitGuard() {
            *currentState = ComputeState::idle;
            for (auto& m : mem)
                cudaFree(m);
        }
        ComputeState* currentState;
        std::vector<double*> mem;
    } guard;

    guard.currentState = &currentState_;
    QL_REQUIRE(currentId_ > 0, "CudaContext::finalizeCalculation(): current id is not set");

    if (!hasKernel_[currentId_ - 1])
        nOutputVariables_[currentId_ - 1] = outputVariables_.size();

    boost::timer::cpu_timer timer;
    boost::timer::nanosecond_type timerBase;
    CUresult cuErr;
    cudaError_t cudaErr;

    size_t size_scalar = sizeof(double);
    size_t size_vector = sizeof(double) * size_[currentId_ - 1];

    // build kernel if necessary

    if (!hasKernel_[currentId_ - 1]) {

        const std::string includeSource =
            "__constant__ double PI = 3.1415926535897932384626;\n"
            "__constant__ double tol = 42.0 * 1.1920929e-07;\n\n"
            "__device__ bool ore_closeEnough(const double x, const double y) {\n"
            "    double diff = fabs(x - y);\n"
            "    if (x == 0.0 || y == 0.0)\n"
            "        return diff < tol * tol;\n"
            "    return diff <= tol * fabs(x) || diff <= tol * fabs(y);\n"
            "}\n\n"
            "__device__ double ore_indicatorEq(const double x, const double y) { return ore_closeEnough(x, y) ? 1.0 : "
            "0.0; }\n\n"
            "__device__ double ore_indicatorGt(const double x, const double y) { return x > y && !ore_closeEnough(x, "
            "y); }\n\n"
            "__device__ double ore_indicatorGeq(const double x, const double y) { return x > y || ore_closeEnough(x, "
            "y); }\n\n"
            "__device__ double normpdf(const double x) { return exp(-0.5 * x * x) / sqrt(2.0 * PI); }\n\n";

        std::string kernelName =
            "ore_kernel_" + std::to_string(currentId_) + "_" + std::to_string(version_[currentId_ - 1]);

        std::string kernelSource = includeSource + "extern \"C\" __global__ void " + kernelName +
                                   "(double** input, double** output, int n,  double* randomVariables) {\n"
                                   "    int tid = blockIdx.x * blockDim.x + threadIdx.x;\n";

        kernelSource += "    if (tid < n) {\n";
        for (size_t id = 0; id < nRandomVariables_[currentId_ - 1]; ++id) {
            kernelSource += "       double v" + std::to_string(id + nInputVars_) + " = randomVariables[tid + " +
                           std::to_string(id * size_[currentId_ - 1]) + "];\n";
            if (settings_.debug)
                debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
        }
        kernelSource += source_;
        size_t ii = 0;
        for (auto const& out : outputVariables_) {
            kernelSource += "       output[" + std::to_string(ii) + "][tid] = v" + std::to_string(out) + ";\n";
            //kernelSource += "       printf(\"output[" + std::to_string(ii) + "][%d] = %.5f \\n\", tid, output[" +
            //                std::to_string(ii) + "][tid]);\n";
            ++ii;
        }
        kernelSource += "   }\n"
                        "}\n";
        std::cout << kernelSource << std::endl;
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }

        // Compile source code
        nvrtcResult nvrtcErr;
        nvrtcProgram nvrtcProgram;
        nvrtcErr = nvrtcCreateProgram(&nvrtcProgram, kernelSource.c_str(), "kernel.cu", 0, nullptr, nullptr);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcCreateProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        cudaDeviceProp device_prop;
        cudaGetDeviceProperties(&device_prop, device_);
        //const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    (std::string("--gpu-architecture=") + getGPUArchitecture[device_prop.name]).c_str(), " - std = c++ 17 ", nullptr};
        //const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on", "--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt", nullptr};
        //nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 5, compileOptions);
        const char* compileOptions[] = {
            //"--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
            "--gpu-architecture=compute_75",
            "-std=c++17",
            "-dopt=on",
            //"--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt",
            nullptr};
        nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 3, compileOptions);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcCompileProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));

        // Retrieve the compiled PTX code
        size_t ptxSize;
        nvrtcErr = nvrtcGetPTXSize(nvrtcProgram, &ptxSize);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcGetPTXSize(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        char* ptx = new char[ptxSize];
        nvrtcErr = nvrtcGetPTX(nvrtcProgram, ptx);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcGetPTXSize(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        nvrtcErr = nvrtcDestroyProgram(&nvrtcProgram);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcDestroyProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));

        //releaseProgram(program_[currentId_ - 1]);
        CUresult cuErr = cuModuleLoadData(&module_[currentId_ - 1], ptx);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr <<  "CudaContext::finalizeCalculation(): error during cuModuleLoadData(): " << errStr << std::endl;
        }
        cuErr = cuModuleGetFunction(&kernel_[currentId_ - 1], module_[currentId_ - 1], kernelName.c_str());
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::finalizeCalculation(): error during cuModuleGetFunction(): " << errStr << std::endl;
        }
        delete[] ptx;

        hasKernel_[currentId_ - 1] = true;
        
        if (settings_.debug) {
            debugInfo_.nanoSecondsProgramBuild += timer.elapsed().wall - timerBase;
        }
    }

    // allocate and copy memory for input to device

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }

    double** input;
    cudaErr = cudaMalloc(&input, (nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1]) *
                                     sizeof(double*));
    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for deviceVarList_ fails: "
                                           << cudaGetErrorString(cudaErr));
    size_t size_var;

    for (size_t i = 0; i < hostVarList_.size(); i++) {
        size_var = inputVarIsScalar_[i] ? size_scalar : size_vector;
        // Allocate memory in device
        cudaErr = cudaMalloc(&deviceVarList_[i], size_var);
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for deviceVarList_["
                                               << i << "] fails: " << cudaGetErrorString(cudaErr));
        // Copy vector from host to device
        if (hostVarList_[i] != nullptr) {
            cudaErr = cudaMemcpyAsync(deviceVarList_[i], hostVarList_[i], size_var, cudaMemcpyHostToDevice);
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory copy for deviceVarList_["
                                                   << i << "] fails: " << cudaGetErrorString(cudaErr));
        }
        cudaErr = cudaMemcpyAsync(&input[i], &deviceVarList_[i], sizeof(double*), cudaMemcpyHostToDevice);
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory copy for &deviceVarList_["
                                               << i << "] fails: " << cudaGetErrorString(cudaErr));
    }

    // Allocate memory for output
    if (nOutputPtr_.count(nOutputVariables_[currentId_ - 1]) == 0) {
        double** d_output;
        cudaErr = cudaMalloc(&d_output, nOutputVariables_[currentId_ - 1] * sizeof(double*));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::finalizeCalculation(): memory allocate for deviceVarList_ fails: "
                       << cudaGetErrorString(cudaErr));
        std::vector<double*> outputVarList;
        for (size_t i = 0; i < nOutputVariables_[currentId_ - 1]; ++i) {
            double* dMem;
            outputVarList.push_back(dMem);
            // Allocate memory in device
            cudaErr = cudaMalloc(&outputVarList[i], size_vector);
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for outputVarList["
                                                   << i << "] fails: " << cudaGetErrorString(cudaErr));
            // Copy vector from host to device
            cudaErr = cudaMemcpyAsync(&d_output[i], &outputVarList[i], sizeof(double*), cudaMemcpyHostToDevice);
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory copy for &outputVarList["
                                                   << i << "] fails: " << cudaGetErrorString(cudaErr));
        }
        nOutputPtr_[nOutputVariables_[currentId_ - 1]] = d_output;
        nOutputPtrList_[nOutputVariables_[currentId_ - 1]] = outputVarList;
    }
    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // set kernel args

    void* args[] = {&input,
                    &nOutputPtr_[nOutputVariables_[currentId_ - 1]],
                    &size_[currentId_ - 1],
                    &d_randomVariables_};

    // execute kernel

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }
    cuErr = cuLaunchKernel(kernel_[currentId_ - 1], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, 0, args,
                           nullptr);
    if (cuErr != CUDA_SUCCESS) {
        const char* errStr;
        cuGetErrorString(cuErr, &errStr);
        std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
    }

     if (settings_.debug) {
        debugInfo_.nanoSecondsCalculation += timer.elapsed().wall - timerBase;
    }

    // copy the results

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }

    cudaDeviceSynchronize();
    for (size_t i = 0; i < nOutputVariables_[currentId_ - 1]; ++i) {
        cudaErr = cudaMemcpyAsync(output[i], nOutputPtrList_[nOutputVariables_[currentId_ - 1]][i],
                                  sizeof(double) * size_[currentId_ - 1],
                                  cudaMemcpyDeviceToHost);
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::finalizeCalculation(): memory copy from device to host for outputVarList["
                       << i << "] fails: " << cudaGetErrorString(cudaErr));
    }

    // clear memory
    for (size_t i = 0; i < hostVarList_.size(); ++i) {
        if (inputVarIsScalar_[i])
            delete hostVarList_[i];
    }

    for (auto ptr : deviceVarList_) {
        releaseMem(ptr, "finalizeCalculation");
    }
    cudaErr = cudaFree(input);
    QL_REQUIRE(cudaErr == cudaSuccess,
               "CudaContext::finalizeCalculation(): free memory for input fails: " << cudaGetErrorString(cudaErr));

    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }
    //std::cout << "================================================" << std::endl;
}

const ComputeContext::DebugInfo& CudaContext::debugInfo() const { return debugInfo_; }
std::vector<std::pair<std::string, std::string>> CudaContext::deviceInfo() const { return deviceInfo_; }
bool CudaContext::supportsDoublePrecision() const { return supportsDoublePrecision_; }

#endif

#ifndef ORE_ENABLE_CUDA
CudaFramework::CudaFramework() {}
CudaFramework::~CudaFramework() {}
#endif

std::set<std::string> CudaFramework::getAvailableDevices() const {
    std::set<std::string> tmp;
    for (auto const& [name, _] : contexts_)
        tmp.insert(name);
    return tmp;
}

ComputeContext* CudaFramework::getContext(const std::string& deviceName) {
    auto c = contexts_.find(deviceName);
    if (c != contexts_.end()) {
        return c->second;
    }
    QL_FAIL("CudaFrameWork::getContext(): device '"
            << deviceName << "' not found. Available devices: " << boost::join(getAvailableDevices(), ","));
}
}; // namespace QuantExt
