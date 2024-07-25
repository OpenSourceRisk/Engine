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
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_opcodes.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>

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
#include <cusolverDn.h>
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
    void releaseMemCU(CUdeviceptr dptr, const std::string& description);
    void releaseModule(CUmodule& k, const std::string& desc);
    void releaseStream(cudaStream_t& stream, const std::string& desc);
    //void releaseMersenneTwisterStates(curandStateMtgp32*& rngState, const std::string& desc);
    void updateVariatesMTGP32();
    //void updateVariatesMTGP32_dynamic();
    void updateVariatesMT19937();
    void updateVariatesMT19937_CPU();

    size_t binom_helper(size_t n, size_t k);

    enum class ComputeState { idle, createInput, createVariates, calc };

    cudaStream_t stream_;

    bool initialized_ = false;
    size_t device_;
    size_t NUM_THREADS_ = 256;
    
    std::vector<std::pair<std::string, std::string>> deviceInfo_;
    bool supportsDoublePrecision_;
    size_t maxRandomVariates_;
    double* d_randomVariables_;
    //CUdeviceptr d_randomVariables_;

    // will be accumulated over all calcs
    ComputeContext::DebugInfo debugInfo_;

    // 1a vectors per current calc id

    std::vector<std::size_t> size_;
    std::vector<bool> hasKernel_;
    std::vector<bool> disposed_;
    std::vector<std::size_t> version_;
    std::vector<CUmodule> module_;
    std::vector<std::vector<CUfunction>> kernel_;
    std::vector<std::size_t> nRandomVariables_;
    std::vector<std::size_t> nOperations_;
    std::vector<std::size_t> nNUM_BLOCKS_;
    std::vector<std::size_t> nOutputVariables_;

    std::map<std::size_t, double*> dOutput_;

    // 2 curent calc

    std::size_t currentId_ = 0;
    ComputeState currentState_ = ComputeState::idle;
    std::size_t nInputVars_;
    Settings settings_;
    std::vector<size_t> inputVarOffset_;
    std::vector<double> inputVar_;
    size_t conditionalExpectationCount_;

    // 2a indexed by var id
    std::vector<bool> inputVarIsScalar_;

    // 2b collection of variable ids
    std::vector<std::size_t> freedVariables_;
    std::vector<std::size_t> outputVariables_;

    // 2c variate states

    // 2d kernel source code
    std::vector<std::string> source_;

    // 3 MT19937 CPU version
    // 3 MT19937_CPU
    unsigned long long* d_mt_;
    CUmodule moduleMT19937_;
    CUfunction seedInitializationKernel_;
    CUfunction ore_twistKernel_;
    CUfunction ore_generateKernel_;

    // 4a Conditional Expectation
    //std::vector<size_t> idCE_; // per calc id
    //std::vector<CUmodule> moduleCE_;  // id by idCE_
    //std::vector<CUkernel> kernelCE_;  // id by idCE_

    // 4b Conditional Expectation
    std::map<size_t, std::vector<size_t>> basisFunctionCE_; // vector per conditional expectation: store number of basis functions per conditional expectation
    std::vector<size_t> numResultIdCE_; // current calc: store number of result id used for all the operations before the conditional expectation
    std::vector<size_t> resultIdCE_; // current calc: store the number of the result id returned from the conditional expectation
};

CudaFramework::CudaFramework() {
    int nDevices;
    cuInit(0);
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
        //CUresult err;
        //cudaError_t cudaErr;
        //for (auto& ptr : dOutput_) {
        //    releaseMem(ptr.second, "~CudaContext()::");
        //}

        //releaseMem(dOutput_[0], "~CudaContext()::dOutput_[0]");
        //releaseMem(d_randomVariables_, "~CudaContext()::d_randomVariables_");
        //cudaErr = cudaFree(d_randomVariables_);
        //std::cout << "cudaFree(d_randomVariables_): " << cudaGetErrorString(cudaErr) << std::endl; 
        //releaseMemCU(d_randomVariables_, "~CudaContext()");


        for (Size i = 0; i < module_.size(); ++i) {
            if (disposed_[i])
                continue;
            releaseModule(module_[i], "~CudaContext()");
        }
        
        releaseStream(stream_, "~CudaContext()");
        //releaseMemCU(d_randomVariables_, "~CudaContext()");
        
    }
}

void CudaContext::releaseMem(double*& m, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(m); err != cudaSuccess) {
        std::cerr << "CudaContext: error during cudaFree at "<< description
                  << ": " << cudaGetErrorName(err) << std::endl;
    }
}
/*
void CudaContext::releaseMersenneTwisterStates(curandStateMtgp32*& rngState, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(rngState); err != cudaSuccess) {
        std::cerr << "CudaContext: error during releaseMersenneTwisterStates at " << description << ": "
                  << cudaGetErrorString(err) << std::endl;
    }
}
*/
void CudaContext::releaseModule(CUmodule& k, const std::string& description) {
    CUresult err = cuModuleUnload(k);
    if (err != CUDA_SUCCESS) {
        const char* errorStr;
        cuGetErrorString(err, &errorStr);
        std::cerr << "CudaContext: error during cuModuleUnload at " << description << ": " << errorStr << std::endl;
    }
   // else {
   //    std::cout << "Module released successfully!" << std::endl;
   // }
}

void CudaContext::releaseMemCU(CUdeviceptr dptr, const std::string& description) {
    CUresult err = cuMemFree(dptr);
    if (err != CUDA_SUCCESS) {
        const char* errorStr;
        cuGetErrorName(err, &errorStr);
        std::cerr << "CudaContext: error during cuMemFree at " << description << ": " << errorStr << std::endl;
    }
    // else {
    //    std::cout << "Module released successfully!" << std::endl;
    // }
}

void CudaContext::releaseStream(cudaStream_t& stream, const std::string& description) {
    cudaError_t err = cudaStreamDestroy(stream);
    if (err != CUDA_SUCCESS) {
        std::cerr << "CudaContext: error during cudaStreamDestroy at " << description << ": " << cudaGetErrorName(err)
                  << std::endl;
    }
    // else {
    //    std::cout << "Module released successfully!" << std::endl;
    // }
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
    
    //cuInit(0);

    cudaError_t cudaErr = cudaStreamCreate(&stream_);
    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::init(): cudaStreamCreate() fails: " << cudaGetErrorString(cudaErr));
    //std::cout << "stream_ = " << stream_ << std::endl;
    
    initialized_ = true;
}

void CudaContext::disposeCalculation(const std::size_t id) {
    QL_REQUIRE(!disposed_[id - 1], "CudaContext::disposeCalculation(): id " << id << " was already disposed.");
    disposed_[id - 1] = true;
    releaseModule(module_[id - 1], "disposeCalculation");
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
        std::cout << "init source_.size() = " << source_.size() << std::endl;
        source_.push_back("");
        std::cout << "after push_back init source_.size() = " << source_.size() << std::endl;
        conditionalExpectationCount_ = 0;

        CUmodule cuModule;
        module_.push_back(cuModule);

        std::vector<CUfunction> function;
        kernel_.push_back(function);

        nNUM_BLOCKS_.push_back((n + NUM_THREADS_ - 1) / NUM_THREADS_);

        nOutputVariables_.push_back(0);
        nRandomVariables_.push_back(0);
        nOperations_.push_back(0);

        currentId_ = hasKernel_.size();
        newCalc = true;

    } else {

        // initiate calculation on existing id

        QL_REQUIRE(id <= hasKernel_.size(),
                   "CudaContext::initiateCalculation(): id (" << id << ") invalid, got 1..." << hasKernel_.size());
        QL_REQUIRE(size_[id - 1] == n, "CudaContext::initiateCalculation(): size ("
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
            source_.push_back("");
            basisFunctionCE_[id].clear();
            newCalc = true;
        }

        currentId_ = id;
    }

    nInputVars_ = 0;
    inputVarIsScalar_.clear();
    inputVarOffset_.clear();
    inputVarOffset_.push_back(0);
    inputVar_.clear();

    freedVariables_.clear();
    outputVariables_.clear();

    basisFunctionCE_.clear();
    numResultIdCE_.clear();
    resultIdCE_.clear();

    // set state

    currentState_ = ComputeState::createInput;

    // return calc id

    return std::make_pair(currentId_, newCalc);
}

std::size_t CudaContext::createInputVariable(double v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "CudaContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                << ")");
    inputVarIsScalar_.push_back(true);
    inputVar_.push_back(v);
    inputVarOffset_.push_back(inputVarOffset_.back() + 1);
    return nInputVars_++;
}

std::size_t CudaContext::createInputVariable(double* v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "CudaContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                  << ")");
    inputVarIsScalar_.push_back(false);
    inputVar_.reserve(inputVar_.size() + size_[currentId_ - 1]);
    inputVar_.insert(inputVar_.end(), v, v + size_[currentId_ - 1]);
    inputVarOffset_.push_back(inputVarOffset_.back() + size_[currentId_ - 1]);
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

    std::vector<std::vector<std::size_t>> resultIds(dim, std::vector<std::size_t>(steps));
    for (std::size_t i = 0; i < dim; ++i) {
        for (std::size_t j = 0; j < steps; ++j) {
            resultIds[i][j] = nInputVars_ + nRandomVariables_[currentId_ - 1];
            nRandomVariables_[currentId_ - 1]++;
        }
    }

    updateVariatesMTGP32();
    //updateVariatesMTGP32_dynamic();
    //updateVariatesMT19937();
    //updateVariatesMT19937_CPU();

    return resultIds;
}

void CudaContext::updateVariatesMTGP32() {

    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] > maxRandomVariates_) {

        cudaError_t cudaErr;
        curandStatus_t curandErr;
        CUresult cuErr;

        if (maxRandomVariates_ > 0)
            releaseMem(d_randomVariables_, "updateVariates()");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];
        //std::cout << "maxRandomVariates_ = " << maxRandomVariates_ << std::endl;

        // Allocate memory for random variables
        cudaErr = cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::updateVariatesMTGP32(): memory allocate for d_randomVariables_ fails: "
                       << cudaGetErrorString(cudaErr));
        /*
        cuErr = cuMemAlloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        if (cuErr != CUDA_SUCCESS) {
            const char* errorStr;
            cuGetErrorString(cuErr, &errorStr);
            std::cerr << "CudaContext: error during cuMemFree at " << "updateVariatesMTGP32()" << ": " << errorStr <<
        std::endl;
        };*/

        // Random number generator setup
        curandGenerator_t generator;
        curandErr = curandCreateGenerator(&generator, CURAND_RNG_PSEUDO_MTGP32);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMTGP32(): error during curandCreateGenerator(): "
                       << curandGetErrorString(curandErr));
        curandErr = curandSetPseudoRandomGeneratorSeed(generator, settings_.rngSeed);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMTGP32(): error during curandSetPseudoRandomGeneratorSeed(): "
                       << curandGetErrorString(curandErr));

        // Set stream
        curandErr = curandSetStream(generator, stream_);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMTGP32(): error during curandSetStream(): "
                       << curandGetErrorString(curandErr));

        // Generator random numbers
        curandErr =
            curandGenerateNormalDouble(generator, d_randomVariables_, maxRandomVariates_, 0.0, 1.0);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMTGP32(): error during curandGenerateNormalDouble(): "
                       << curandGetErrorString(curandErr));

        // Release generator
        curandErr = curandDestroyGenerator(generator);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMTGP32(): error during curandDestroyGenerator(): "
                       << curandGetErrorString(curandErr));
    }
}
/*
void CudaContext::updateVariatesMTGP32_dynamic() {

    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] > maxRandomVariates_) {

        cudaError_t cudaErr;
        curandStatus_t curandErr;
        CUresult cuErr;

        if (maxRandomVariates_ > 0)
            releaseMem(d_randomVariables_, "updateVariates()");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];

        // Allocate memory for random variables
        cudaErr = cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::createInputVariates(): memory allocate for d_randomVariables_ fails: "
                       << cudaGetErrorString(cudaErr));

        // For Mersenne Twister device API, at most 200 states can be generated one time.
        // If this passed the test, we will generate 2 separate mtStates for blocks after 200.
        // Need to test if using same seed will generate same random numbers
        QL_REQUIRE(nNUM_BLOCKS_[currentId_ - 1] <= 200,
                   "When using Mersenne Twister, at most 200 states can be generate now");

        // Random generator state
        curandStateMtgp32* mtStates;
        cudaErr = cudaMalloc(&mtStates, nNUM_BLOCKS_[currentId_ - 1] * sizeof(curandStateMtgp32));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::initMersenneTwisterRng(): memory allocate for mtStates_ fails: "
                       << cudaGetErrorString(cudaErr));

        // Define MTGP32 parameters
        mtgp32_kernel_params* kernelParams;
        cudaErr = cudaMalloc((void**)&kernelParams, sizeof(mtgp32_kernel_params));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::initMersenneTwisterRng(): memory allocate for kernelParams fails: "
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

        cuErr = cuModuleLoadData(&cuModule, ptx);
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

        void* args[] = {&mtStates, &size_[currentId_ - 1], &d_randomVariables_};

        // execute kernel
        cuErr = cuLaunchKernel(rngKernel, nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args, nullptr);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::createInputVariates(): error during cuLaunchKernel(): " << errStr << std::endl;
        }

        releaseMersenneTwisterStates(mtStates, "updateVariatesMTGP32");
        releaseModule(cuModule, "updateVariatesMTGP32");
    }
*/
void CudaContext::updateVariatesMT19937() {

    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1]> maxRandomVariates_) {

        cudaError_t cudaErr;
        curandStatus_t curandErr;
        CUresult cuErr;

        if (maxRandomVariates_ > 0)
            releaseMem(d_randomVariables_, "updateVariates()");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];

        // Allocate memory for random variables
        cudaErr = cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::createInputVariates(): memory allocate for d_randomVariables_ fails: "
                       << cudaGetErrorString(cudaErr));
        /*
        cuErr = cuMemAlloc(&d_randomVariables_,
                           nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double));
        if (cuErr != CUDA_SUCCESS) {
            const char* errorStr;
            cuGetErrorString(cuErr, &errorStr);
            std::cerr << "CudaContext: error during cuMemFree at " << "updateVariatesMTGP32()" << ": " << errorStr <<
        std::endl;
        };*/

        // Random number generator setup
        curandGenerator_t generator;
        curandErr = curandCreateGenerator(&generator, CURAND_RNG_PSEUDO_MT19937);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMT19937(): error during curandCreateGenerator(): "
                       << curandGetErrorString(curandErr));
        curandErr = curandSetPseudoRandomGeneratorSeed(generator, settings_.rngSeed);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMT19937(): error during curandSetPseudoRandomGeneratorSeed(): "
                       << curandGetErrorString(curandErr));

        // Set stream
        curandErr = curandSetStream(generator, stream_);
        QL_REQUIRE(
            curandErr == CURAND_STATUS_SUCCESS,
            "CudaContext::updateVariatesMT19937(): error during curandSetStream(): " << curandGetErrorString(curandErr));

        // Generator random numbers
        curandErr = curandGenerateNormalDouble(generator, d_randomVariables_, maxRandomVariates_, 0.0, 1.0);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMT19937(): error during curandGenerateNormalDouble(): "
                       << curandGetErrorString(curandErr));

        // Release generator
        curandErr = curandDestroyGenerator(generator);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::updateVariatesMT19937(): error during curandDestroyGenerator(): "
                       << curandGetErrorString(curandErr));
        /*
        // temp code
        
        // Copy random numbers back to host
        double* h_randomVariables = new double[nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1]];
        cudaErr = cudaMemcpyAsync(h_randomVariables, d_randomVariables_,
                                  nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1],
                                  cudaMemcpyDeviceToHost, stream_);
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::updateVariatesMT19937(): memory copy for d_randomVariables_ to host fails: " << cudaGetErrorString(cudaErr));

        */
    }
}

void CudaContext::updateVariatesMT19937_CPU() {
    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] > maxRandomVariates_) {

        cudaError_t cudaErr;
        curandStatus_t curandErr;
        CUresult cuErr;

        double* d_randomVariables_old;

        if (maxRandomVariates_ > 0) {

            cudaErr = cudaMalloc(&d_randomVariables_old, maxRandomVariates_ * sizeof(double));
            QL_REQUIRE(cudaErr == cudaSuccess,
                       "CudaContext::updateVariatesMT19937_CPU(): memory allocate for d_randomVariables_old fails: "
                           << cudaGetErrorString(cudaErr));
            cudaErr = cudaMemcpyAsync(d_randomVariables_old, d_randomVariables_, maxRandomVariates_ * sizeof(double),
                                      cudaMemcpyDeviceToDevice, stream_);
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::updateVariatesMT19937_CPU(): memory copy from "
                                               "d_randomVariables_old to d_randomVariables_ fails: "
                                                   << cudaGetErrorString(cudaErr));
            releaseMem(d_randomVariables_, "updateVariatesMT19937_CPU()");
        } else {
            // set kernel args
            cudaErr = cudaMalloc((void**)&d_mt_, sizeof(unsigned long long) * 624);
            QL_REQUIRE(cudaErr == cudaSuccess,
                       "CudaContext::updateVariatesMT19937_CPU(): memory allocate for d_mt_ fails: "
                           << cudaGetErrorString(cudaErr));
        }
        size_t previousVariates = maxRandomVariates_;
        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];

        if (previousVariates == 0) {
            // build the kernels to fill the variates pool

            std::string fpTypeStr = settings_.useDoublePrecision ? "double" : "float";
            std::string fpSuffix = settings_.useDoublePrecision ? "" : "f";
            std::string fpMaxValue = settings_.useDoublePrecision ? "0x1.fffffffffffffp1023" : "0x1.fffffep127f";

            // clang-format off
            // ported from from QuantLib::InverseCumulativeNormal
            std::string sourceInvCumN = "__device__ " + fpTypeStr + " ore_invCumN(const unsigned int x0) {\n"
                "    const " + fpTypeStr + " a1_ = -3.969683028665376e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a2_ = 2.209460984245205e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a3_ = -2.759285104469687e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a4_ = 1.383577518672690e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a5_ = -3.066479806614716e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a6_ = 2.506628277459239e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b1_ = -5.447609879822406e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b2_ = 1.615858368580409e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b3_ = -1.556989798598866e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b4_ = 6.680131188771972e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b5_ = -1.328068155288572e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c1_ = -7.784894002430293e-03" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c2_ = -3.223964580411365e-01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c3_ = -2.400758277161838e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c4_ = -2.549732539343734e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c5_ = 4.374664141464968e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c6_ = 2.938163982698783e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d1_ = 7.784695709041462e-03" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d2_ = 3.224671290700398e-01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d3_ = 2.445134137142996e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d4_ = 3.754408661907416e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " x_low_ = 0.02425" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " x_high_ = 1.0" + fpSuffix + " - x_low_;\n"
                "    const " + fpTypeStr + " x = ((" + fpTypeStr + ")x0 + 0.5" + fpSuffix + ") / 4294967296.0"+ fpSuffix + ";\n"
                "    if (x < x_low_ || x_high_ < x) {\n"
                "        if (x0 == 1U <<32 - 1) {\n"
                "          return " + fpMaxValue + ";\n"
                "        } else if(x0 == 0) {\n"
                "          return -" + fpMaxValue + ";\n"
                "        }\n"
                "        " + fpTypeStr + " z;\n"
                "        if (x < x_low_) {\n"
                "            z = sqrt(-2.0" + fpSuffix + " * log(x));\n"
                "            z = (((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /\n"
                "                ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0" + fpSuffix + ");\n"
                "        } else {\n"
                "            z = sqrt(-2.0f * log(1.0f - x));\n"
                "            z = -(((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /\n"
                "                ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0" + fpSuffix + ");\n"
                "        }\n"
                "        return z;\n"
                "    } else {\n"
                "        " + fpTypeStr + " z = x - 0.5" + fpSuffix + ";\n"
                "        " + fpTypeStr + " r = z * z;\n"
                "        z = (((((a1_ * r + a2_) * r + a3_) * r + a4_) * r + a5_) * r + a6_) * z /\n"
                "            (((((b1_ * r + b2_) * r + b3_) * r + b4_) * r + b5_) * r + 1.0" + fpSuffix +");\n"
                "        return z;\n"
                "    }\n"
                "}\n\n";

            // from QuantLib::MersenneTwisterUniformRng

            std::string kernelSourceSeedInit = "extern \"C\" __global__ void ore_seedInitialization(const unsigned long long s, unsigned long long* mt) {\n"
                "  const unsigned long long N = 624;\n"
                "  mt[0]= s & 0xffffffffUL;\n"
                "  for (unsigned long long mti=1; mti<N; ++mti) {\n"
                "    mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);\n"
                "    mt[mti] &= 0xffffffffUL;\n"
                "  }\n"
                "}\n\n";

            std::string kernelSourceTwist = "extern \"C\" __global__ void ore_twist(unsigned long long* mt) {\n"
                " const unsigned long long N = 624;\n"
                " const unsigned long long M = 397;\n"
                " const unsigned long long MATRIX_A = 0x9908b0dfUL;\n"
                " const unsigned long long UPPER_MASK=0x80000000UL;\n"
                " const unsigned long long LOWER_MASK=0x7fffffffUL;\n"
                " const unsigned long long mag01[2]={0x0UL, MATRIX_A};\n"
                " unsigned long long kk;\n"
                " unsigned long long y;\n"
                " for (kk=0;kk<N-M;++kk) {\n"
                "     y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);\n"
                "     mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];\n"
                " }\n"
                " for (;kk<N-1;kk++) {\n"
                "     y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);\n"
                "     mt[kk] = mt[(kk+M)-N] ^ (y >> 1) ^ mag01[y & 0x1UL];\n"
                " }\n"
                " y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);\n"
                " mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];\n"
                "}\n\n";

            std::string kernelSourceGenerate =
                "extern \"C\" __global__ void ore_generate(const unsigned long long offset, unsigned long long* mt, " + fpTypeStr + "* output) {\n"
                "   unsigned long long mti = threadIdx.x + blockIdx.x * blockDim.x;\n"
                "   unsigned long long y = mt[mti];\n"
                "   y ^= (y >> 11);\n"
                "   y ^= (y << 7) & 0x9d2c5680UL;\n"
                "   y ^= (y << 15) & 0xefc60000UL;\n"
                "   y ^= (y >> 18);\n"
                "   output[offset + mti] = ore_invCumN((unsigned int)y);\n"
                "}\n\n";

            std::string rngKernelSource = sourceInvCumN + kernelSourceSeedInit + kernelSourceTwist + kernelSourceGenerate;

            //std::cout << rngKernelSource << std::endl;

            // Compile source code
            nvrtcResult nvrtcErr;
            nvrtcProgram nvrtcProgram;
            nvrtcErr = nvrtcCreateProgram(&nvrtcProgram, rngKernelSource.c_str(), "kernel.cu", 0, nullptr, nullptr);
            QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcCreateProgram(): "
                                                      << nvrtcGetErrorString(nvrtcErr));

            const char* compileOptions[] = {
                //"--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
                "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on",
                //"--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt",
                nullptr};
            nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 3, compileOptions);
      
   /*         size_t logSize;
            nvrtcGetProgramLogSize(nvrtcProgram, &logSize);
            std::vector<char> log(logSize);
            nvrtcGetProgramLog(nvrtcProgram, log.data());
            std::cout << log.data() << std::endl;*/

            QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcCompileProgram(): "
                                                      << nvrtcGetErrorString(nvrtcErr));

            // Retrieve the compiled PTX code
            size_t ptxSize;
            nvrtcErr = nvrtcGetPTXSize(nvrtcProgram, &ptxSize);
            QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcGetPTXSize(): "
                                                      << nvrtcGetErrorString(nvrtcErr));
            char* ptx = new char[ptxSize];
            nvrtcErr = nvrtcGetPTX(nvrtcProgram, ptx);
            QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcGetPTXSize(): "
                                                      << nvrtcGetErrorString(nvrtcErr));
            nvrtcErr = nvrtcDestroyProgram(&nvrtcProgram);
            QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcDestroyProgram(): "
                                                      << nvrtcGetErrorString(nvrtcErr));

            cuErr = cuModuleLoadData(&moduleMT19937_, ptx);
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleLoadData(): " << errStr << std::endl;
            }
            cuErr = cuModuleGetFunction(&seedInitializationKernel_, moduleMT19937_, "ore_seedInitialization");
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleGetFunction(): " << errStr
                          << std::endl;
            }
            cuErr = cuModuleGetFunction(&ore_twistKernel_, moduleMT19937_, "ore_twist");
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleGetFunction(): " << errStr
                          << std::endl;
            }
            cuErr = cuModuleGetFunction(&ore_generateKernel_, moduleMT19937_, "ore_generate");
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleGetFunction(): " << errStr
                          << std::endl;
            }

            delete[] ptx;
            // execute seed initialization kernel
            void* args[] = {&settings_.rngSeed, &d_mt_};
            cuErr = cuLaunchKernel(seedInitializationKernel_, 1, 1, 1, 1, 1, 1, 0, stream_, args, nullptr);
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuLaunchKernel(): " << errStr << std::endl;
            }
        }
        maxRandomVariates_ = 624 * (maxRandomVariates_ / 624 +
                              (maxRandomVariates_ % 624 == 0 ? 0 : 1));

        // Allocate memory for random variables
        cudaErr = cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::updateVariatesMT19937_CPU(): memory allocate for d_randomVariables_ fails: "
                       << cudaGetErrorString(cudaErr));
        if (previousVariates > 0) {
            cudaErr = cudaMemcpyAsync(d_randomVariables_, d_randomVariables_old, previousVariates * sizeof(double),
                                 cudaMemcpyDeviceToDevice, stream_);
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::updateVariatesMT19937_CPU(): memory copy from "
                                               "d_randomVariables_old to d_randomVariables_ fails: "
                                                   << cudaGetErrorString(cudaErr));
            releaseMem(d_randomVariables_old, "updateVaraitesMT19937_CPU");
        }

        for (size_t currentVariates = previousVariates; currentVariates < maxRandomVariates_; currentVariates += 624) {
            void* args_twist[] = {&d_mt_};
            // execute seed initialization kernel
            cuErr = cuLaunchKernel(ore_twistKernel_, 1, 1, 1, 1, 1, 1, 0, stream_, args_twist, nullptr);
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuLaunchKernel(): " << errStr << std::endl;
            }

            void* args_generate[] = {&currentVariates, &d_mt_, &d_randomVariables_};
            cuErr = cuLaunchKernel(ore_generateKernel_, 1, 1, 1, 624, 1, 1, 0, stream_, args_generate, nullptr);
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuLaunchKernel(): " << errStr << std::endl;
            }
        }
        //std::cout << "max = " << maxRandomVariates_ << std::endl;
        // copy back to host
        //std::vector<double> temp_output(maxRandomVariates_);
        //cudaMemcpyAsync(temp_output.data(), d_randomVariables_, maxRandomVariates_ * sizeof(double), cudaMemcpyDeviceToHost, stream_);
        cudaStreamSynchronize(stream_);
        //for (size_t i = 0; i < 20; i++) {
        //    std::cout << "rng[" << i << "] = " << temp_output[i] << std::endl;
        //}
        
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
    std::cout <<"here" << std::endl;
    std::size_t resultId;
    bool resultIdNeedsDeclaration;
    if (!freedVariables_.empty()) {
        resultId = freedVariables_.back();
        freedVariables_.pop_back();
        resultIdNeedsDeclaration = false;
    } else {
        resultId = nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1];
        ++nOperations_[currentId_ - 1];
        resultIdNeedsDeclaration = true;
    }

    std::vector<std::string> argStr(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] < inputVarIsScalar_.size()) {
            argStr[i] = "input[" + std::to_string(inputVarOffset_[args[i]]) +
                        (inputVarIsScalar_[args[i]] ? "" : " + tid") + "]";
            //source_ += "printf(\"" + argStr[i] + " = %.5f\\n\", " + argStr[i] + ");";
        }
        else if (args[i] < inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]){
            argStr[i] = "randomVariables[tid + " + std::to_string((args[i] - inputVarIsScalar_.size())  * size_[currentId_ - 1]) + "]";
        }
        else{
            if (numResultIdCE_.empty()){
                argStr[i] = "v" + std::to_string(args[i]);
            }
            else {
                if (args[i] < nInputVars_ + nRandomVariables_[currentId_ - 1] + numResultIdCE_.back()){
                    std::cout << "args[i] = " << args[i] << std::endl;
                    std::cout << "nInputVars_ = " << nInputVars_ << std::endl;
                    std::cout << "inputVarIsScalar.size() = " << inputVarIsScalar_.size() << std::endl; 
                    std::cout << "nRandomVariables_[currentId_ - 1] = " << nRandomVariables_[currentId_ - 1] << std::endl;
                    argStr[i] = "values[tid + " + std::to_string(size_[currentId_ - 1] * (args[i] - nInputVars_ - nRandomVariables_[currentId_ - 1])) + "]";
                } else
                    argStr[i] = "v" + std::to_string(args[i]);
            }
            
        }
    }
      std::cout <<"here1" << std::endl;
    if (randomVariableOpCode != RandomVariableOpCode::ConditionalExpectation) {
        // determine arg variable names
        QL_REQUIRE(args.size() == 1 || args.size() == 2,
                   "CudaContext::applyOperation() args.size() must be 1 or 2, got" << args.size());

        // generate source code
        if (resultIdNeedsDeclaration)
            source_[conditionalExpectationCount_] += "        double v" + std::to_string(resultId) + " = ";
        else
            source_[conditionalExpectationCount_] += "        v" + std::to_string(resultId) + " = ";

        switch (randomVariableOpCode) {
            case RandomVariableOpCode::None: {
                break;
            }
            case RandomVariableOpCode::Add: {
                source_[conditionalExpectationCount_] += argStr[0] + " + " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::Subtract: {
                source_[conditionalExpectationCount_] += argStr[0] + " - " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::Negative: {
                source_[conditionalExpectationCount_] += "-" + argStr[0] + ";\n";
                break;
            }
            case RandomVariableOpCode::Mult: {
                source_[conditionalExpectationCount_] += argStr[0] + " * " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::Div: {
                source_[conditionalExpectationCount_] += argStr[0] + " / " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::IndicatorEq: {
                source_[conditionalExpectationCount_] += "ore_indicatorEq(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::IndicatorGt: {
                source_[conditionalExpectationCount_] += "ore_indicatorGt(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::IndicatorGeq: {
                source_[conditionalExpectationCount_] += "ore_indicatorGeq(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::Min: {
                source_[conditionalExpectationCount_] += "fmin(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::Max: {
                source_[conditionalExpectationCount_] += "fmax(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::Abs: {
                source_[conditionalExpectationCount_] += "fabs(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Exp: {
                source_[conditionalExpectationCount_] += "exp(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Sqrt: {
                source_[conditionalExpectationCount_] += "sqrt(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Log: {
                source_[conditionalExpectationCount_] += "log(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Pow: {
                source_[conditionalExpectationCount_] += "pow(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::NormalCdf: {
                source_[conditionalExpectationCount_] += "normcdf(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::NormalPdf: {
                source_[conditionalExpectationCount_] += "normpdf(" + argStr[0] + ");\n";
                break;
            }
            default: {
                QL_FAIL("CudaContext::executeKernel(): no implementation for op code "
                        << randomVariableOpCode << " (" << getRandomVariableOpLabels()[randomVariableOpCode] << ") provided.");
            }
        }
    }
    // Conditional Expectation
    else {
          std::cout <<"here2" << std::endl;
        QL_REQUIRE(args.size() >= 2,
                   "CudaContext::applyOperation() ConditionalExpectation args.size() must be > 2, got" << args.size());
        if (args.size() == 2){
            // Expectation
        } else {
            std::cout <<"here3" << std::endl;
            // Conditional Expectation
            // calculate order
            size_t order = settings_.regressionOrder;
            size_t regressorSize = args.size() - 2;
            while (RandomVariableLsmBasisSystem::size(regressorSize, order) > static_cast<Real>(size_[currentId_ - 1]) && order > 1) {
                --order;
            }
            size_t basisFunctionSize = binom_helper(order + regressorSize, order);
            basisFunctionCE_[currentId_].push_back(basisFunctionSize);
            std::cout <<"here4" << std::endl;
            // string name
            std::vector<std::string> strA(basisFunctionSize);
            std::vector<std::string> strA_unfilter(basisFunctionSize);
            for (size_t i = 0; i < basisFunctionSize; i++){
                // row-major
                //strA[i] = "A[tid * " + std::to_string(basisFunctionSize)  + " + " + std::to_string(i) + "]";
                //strA_unfilter[i] = "A_unfilter" + std::to_string(source_.size() - 1) + "[tid * " + std::to_string(basisFunctionSize)  + " + " + std::to_string(i) + "]";
                // column-major
                strA[i] = "A[tid + " + std::to_string(size_[currentId_ - 1] * i) + "]";
                strA_unfilter[i] = "A_unfilter" + std::to_string(source_.size() - 1) + "[tid + " + std::to_string(size_[currentId_ - 1] * i) + "]";
            }
            std::cout <<"here5" << std::endl;
            // calculate basis function, write to A_unfilter
            //degree 0
            source_[conditionalExpectationCount_] += "      " + strA_unfilter[0] + " = 1.0;\n";
            std::cout <<"degree0" << std::endl;
            // degree 1
            for (size_t i = 2; i < args.size(); ++i) {
                source_[conditionalExpectationCount_] += "      " + strA_unfilter[i-1] + " = " + argStr[i] + ";\n";
            }
            std::cout <<"degree1" << std::endl;
            // degree 2+
            if (order >= 2) {
                std::vector<size_t> start_point(regressorSize);
                size_t currentPosition = regressorSize + 1;
                size_t newStartPosition = 0;
                for (size_t n = 0; n < regressorSize; n++){
                    start_point[n] = n + 1;
                }
                for (size_t d = 2; d <= order; ++d) {
                    for (size_t n = 0; n < regressorSize; ++n){
                        newStartPosition = currentPosition;
                        for (size_t sp = start_point[n]; sp <= start_point.back();++sp){
                            source_[conditionalExpectationCount_] += "      " + strA_unfilter[currentPosition] + " = " + strA_unfilter[n + 1] + " * " + strA_unfilter[sp] + ";\n";
                            currentPosition++;
                        }
                        start_point[n] = newStartPosition;
                    }
                    std::cout <<"degree2+" << std::endl;
                }
            }
            // apply filter, write to A
            source_[conditionalExpectationCount_] += "      if (ore_closeEnough(" + argStr[1] + ", 1.0)) {\n";
            for (size_t i = 0; i < basisFunctionSize; ++i) {
                source_[conditionalExpectationCount_] += "          " + strA[i] + " = " + strA_unfilter[i] + ";\n";
            }
            source_[conditionalExpectationCount_] += "      } else {\n";
            for (size_t i = 0; i < basisFunctionSize; ++i) {
                source_[conditionalExpectationCount_] += "          " + strA[i] + " = 0.0;\n";
            }
            source_[conditionalExpectationCount_] += "      }\n";
            std::cout <<"here6" << std::endl;
            // calculate vector b
            source_[conditionalExpectationCount_] += "      if (ore_closeEnough(" + argStr[1] + ", 1.0)) {\n"
                                                     "          b[tid] = " + argStr[0] + ";\n"
                                                     "      } else {\n"
                                                     "          b[tid] = 0.0;\n"
                                                     "      }\n";
            source_.push_back("");

            //debug code
            //source_[conditionalExpectationCount_] += "      if (tid == 0) printf(\"A[0] = %.8f\\n\", A[0]);\n"
            //    "      if (tid == 0) printf(\"A[1] = %.8f\\n\", A[1]);\n"
            //    "      if (tid == 0) printf(\"A[2] = %.8f\\n\", A[2]);\n";

            // record the values at the end of this kernel
            conditionalExpectationCount_++;
            numResultIdCE_.push_back(nOperations_[currentId_ - 1]);
            resultIdCE_.push_back(resultId);
            freedVariables_.clear(); // test
            std::cout <<"here7" << std::endl;
            // Calculate the new y in the next kerenl
            //source_[conditionalExpectationCount_] += "if (tid == 0) printf(\"run output kerenl\\n\");\n";
            source_[conditionalExpectationCount_] += "      double v" + std::to_string(resultId) + " = ";
            for (size_t i = 0; i < basisFunctionSize; ++i) {
                source_[conditionalExpectationCount_] += strA_unfilter[i] + " * X[" + std::to_string(i) + "] + ";
            }
            source_[conditionalExpectationCount_].erase(source_[conditionalExpectationCount_].end()-3, source_[conditionalExpectationCount_].end());
            source_[conditionalExpectationCount_] += ";\n";
            //for (size_t i = 0; i < basisFunctionSize; ++i) {
            //    source_[conditionalExpectationCount_] += "if (tid == 0) printf(\"X[" + std::to_string(i) + "] = %.8f\\n\", X[" + std::to_string(i) +"]);\n";
            //}
            std::cout <<"here end" << std::endl;
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

    // we do not free input variables, random numbers and any result ID assigned in the previous kernel, 
    // only variables that were added during the current kernel can be freed.
    //std::cout << "freeVariable::id = " << id << std::endl;
    //std::cout << "freeVariable::(numResultIdCE_.back() + inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]) = " << (numResultIdCE_.back() + inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]) << std::endl;
    if (id < inputVarIsScalar_.size()||(numResultIdCE_.empty()? false: (id <= (numResultIdCE_.back() + inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]))))
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
    // allocate and copy memory for input to device

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }
    double* input;
    size_t inputSize = inputVarOffset_.back() * sizeof(double);
    cudaErr = cudaMalloc(&input, inputSize);
    QL_REQUIRE(cudaErr == cudaSuccess,
               "CudaContext::finalizeCalculation(): memory allocate for input fails: " << cudaGetErrorString(cudaErr));
    double* h_input;
    cudaErr = cudaMallocHost(&h_input, inputSize);
    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for h_input fails: "
                                           << cudaGetErrorString(cudaErr));
    std::copy(inputVar_.begin(), inputVar_.end(), h_input);

    //cudaErr = cudaMemcpyAsync(input, h_input, inputSize, cudaMemcpyHostToDevice, stream_);
    cudaErr = cudaMemcpy(input, h_input, inputSize, cudaMemcpyHostToDevice);
    QL_REQUIRE(cudaErr == cudaSuccess,
               "CudaContext::finalizeCalculation(): memory copy for input fails: " << cudaGetErrorString(cudaErr));
    
    // Allocate memory for output
    if (dOutput_.count(nOutputVariables_[currentId_ - 1]) == 0) {
        double* d_output;
        cudaErr = cudaMalloc(&d_output, nOutputVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for d_output fails: "
                                               << cudaGetErrorString(cudaErr));
        dOutput_[nOutputVariables_[currentId_ - 1]] = d_output;
    }
    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // Allocate memory for values (conditional expectation)
    double* values;
    cudaErr = cudaMalloc(&values, sizeof(double) * size_[currentId_ - 1] * nOperations_[currentId_ - 1]);
    QL_REQUIRE(cudaErr == cudaSuccess,
               "CudaContext::finalizeCalculation(): memory allocate for values fails: " << cudaGetErrorString(cudaErr));

    // build kernel if necessary

    if (!hasKernel_[currentId_ - 1]) {

        const std::string includeSource =
            "__device__ bool ore_closeEnough(const double x, const double y) {\n"
            "    double tol = 42.0 * 0x1.0p-52;\n"
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
            "__device__ double normpdf(const double x) { return exp(-0.5 * x * x) / sqrt(2.0 * 3.1415926535897932384626); }\n\n";

        std::vector<std::string> kernelName(source_.size());
        std::string kernelSource;
        std::cout << "source_.size() = " << source_.size() << std::endl;
        std::cout << "conditionalExpectationCount_ = " << conditionalExpectationCount_ << std::endl;
        if (source_.size() == 1) {
            // No conditional expectation
            kernelName[0] =
            "ore_kernel_" + std::to_string(currentId_) + "_" + std::to_string(version_[currentId_ - 1]);

            kernelSource = includeSource + "extern \"C\" __global__ void " + kernelName[0] +
                                       "(const double* input, double* output, const double* randomVariables) {\n"
                                       "    int tid = blockIdx.x * blockDim.x + threadIdx.x;\n"
                                       "    if (tid < " + std::to_string(size_[currentId_ - 1]) + ") {\n";
            //for (size_t id = 0; id < nRandomVariables_[currentId_ - 1]; ++id) {
            //    // original version
            //    kernelSource += "       double v" + std::to_string(id + nInputVars_) + " = randomVariables[tid + " +
            //                   std::to_string(id * size_[currentId_ - 1]) + "];\n";
            //    //kernelSource += "printf(\"v" + std::to_string(id + nInputVars_) + " = %.5f\\n\", v" +
            //    //                std::to_string(id + nInputVars_) + ");\n";
            //    if (settings_.debug)
            //        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
            //}
            kernelSource += source_[0];

            size_t ii = 0;
            for (auto const& out : outputVariables_) {
                if (out < nInputVars_)
                    kernelSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = input[" +
                                    std::to_string(out) + (inputVarIsScalar_[out] ? "" : " + tid") + "];\n";
                else
                    kernelSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" +
                                    std::to_string(out) + ";\n";
                //kernelSource += "       printf(\"output[tid + " + std::to_string(ii) + "] = %.5f \\n\", output[tid + " +
                //                std::to_string(ii) + "]);\n";
                ++ii;
            }
            kernelSource += "   }\n"
                            "}\n";
        } else {
            kernelSource += includeSource;
            // Have conditional expectation
            for (size_t s = 0; s < source_.size(); ++s) {
                kernelName[s] = "ore_kernel_" + std::to_string(currentId_) + "_" + std::to_string(version_[currentId_ - 1]) + "_" + std::to_string(s);
                std::string thisKernel = "extern \"C\" __global__ void " + kernelName[s] + 
                                         "(const double* input, const double* randomVariables, double* values" + 
                                         ((s == source_.size() - 1)?", double* output":", double* A, double* b, double* A_unfilter" + std::to_string(s)) +
                                         ((s == 0)?"":", const double* A_unfilter" + std::to_string(s-1) + ", const double* X") + ") {\n"
                                         "  int tid = blockIdx.x * blockDim.x + threadIdx.x;\n"
                                         "    if (tid < " + std::to_string(size_[currentId_ - 1]) + ") {\n";
                //for (size_t id = 0; id < nRandomVariables_[currentId_ - 1]; ++id) {
                //    // original version
                //    thisKernel += "       double v" + std::to_string(id + nInputVars_) + " = randomVariables[tid + " +
                //                   std::to_string(id * size_[currentId_ - 1]) + "];\n";
                //    //kernelSource += "printf(\"v" + std::to_string(id + nInputVars_) + " = %.5f\\n\", v" +
                //    //                std::to_string(id + nInputVars_) + ");\n";

                //    if (settings_.debug)
                //        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
                //}
                thisKernel += source_[s];

                // output (If last kernel, add output. If not, add values)
                if (s == source_.size() - 1){
                    size_t ii = 0;
                    for (auto const& out : outputVariables_) {
                        if (out < inputVarIsScalar_.size())
                            thisKernel += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = input[" +
                                            std::to_string(out) + (inputVarIsScalar_[out] ? "" : " + tid") + "];\n";
                        else if (out < inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1])
                            thisKernel += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = randomVariables[tid + " +
                                            std::to_string((out - inputVarIsScalar_.size()) * size_[currentId_ - 1]) + "];\n";
                        else if (out == resultIdCE_.back())
                            thisKernel += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" +
                                            std::to_string(out) + ";\n"; 
                        else if (out < inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1] + numResultIdCE_.back()){
                            std::cout << "out = " << out << std::endl;
                            std::cout << "inputVarIsScalar_.size() = " << inputVarIsScalar_.size() << std::endl;
                            std::cout << "nRandomVariables_[currentId_ - 1] = " << nRandomVariables_[currentId_ - 1] << std::endl;
                            std::cout << "numResultIdCE_.back() = " << numResultIdCE_.back() << std::endl;
                            std::cout << "numResultIdCE_.size() = " << numResultIdCE_.size() << std::endl;
                            thisKernel += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = values[tid + " +
                                            std::to_string((out - inputVarIsScalar_.size() - nRandomVariables_[currentId_ - 1]) * size_[currentId_ - 1]) + "];\n";
                        } else
                            thisKernel += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" +
                                            std::to_string(out) + ";\n";
                        //kernelSource += "       printf(\"output[tid + " + std::to_string(ii) + "] = %.5f \\n\", output[tid + " +
                        //                std::to_string(ii) + "]);\n";
                        ++ii;
                    }
                }
                else {
                    if (s != 0)
                        thisKernel += "       values[tid + " + std::to_string((resultIdCE_[s-1] - inputVarIsScalar_.size() - nRandomVariables_[currentId_ - 1]) * size_[currentId_ - 1]) + "] = v" + 
                                        std::to_string(resultIdCE_[s-1]) + ";\n";
                    for (size_t ii=numResultIdCE_[s-1]; ii < numResultIdCE_[s]; ++ii) {
                        std::cout << "ii = " << ii << std::endl;
                        std::cout << "ii + nInputVars_ + nRandomVariables_[currentId_ - 1] = " << ii + nInputVars_ + nRandomVariables_[currentId_ - 1] << std::endl;
                        std::cout << "resultIdCE_[s-1] = " << resultIdCE_[s-1] << std::endl;
                        std::cout << "resultIdCE_[s] = " << resultIdCE_[s] << std::endl;
                        // the resultId for the last conditional expectation is assigned here, but is not needed in the global kernel
                        if (ii + nInputVars_ + nRandomVariables_[currentId_ - 1] != resultIdCE_[s]){
                            thisKernel += "       values[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" + 
                                        std::to_string(ii + nInputVars_ + nRandomVariables_[currentId_ - 1]) + ";\n";
                        }
                    }
                }
                thisKernel += "   }\n"
                                "}\n";

                kernelSource += thisKernel;

            }
        }

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
        //cudaDeviceProp device_prop;
        //cudaGetDeviceProperties(&device_prop, device_);
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
            "--split-compile=0",
            "-std=c++17",
            //"-dopt=on",
            //"--time=time.txt",
            nullptr};
        nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 3, compileOptions);
        std::cout << "CudaContext::finalizeCalculation(): error during nvrtcCompileProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr) << std::endl;

        size_t logSize;
        nvrtcGetProgramLogSize(nvrtcProgram, &logSize);
        char* log = new char[logSize];
        nvrtcGetProgramLog(nvrtcProgram, log);
        std::cerr << "NVRTC compilation log:\n" << log << std::endl;
        delete[] log;

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

        CUresult cuErr = cuModuleLoadData(&module_[currentId_ - 1], ptx);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr <<  "CudaContext::finalizeCalculation(): error during cuModuleLoadData(): " << errStr << std::endl;
        }
        
        for (size_t i = 0; i < source_.size(); ++i) {
            CUfunction k;
            std::cout << kernelName[i] << std::endl;
            cuErr = cuModuleGetFunction(&k, module_[currentId_ - 1], kernelName[i].c_str());
            const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::finalizeCalculation(): error during cuModuleGetFunction(): " << errStr << std::endl;
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::finalizeCalculation(): error during cuModuleGetFunction(): " << errStr << std::endl;
            }
            kernel_[currentId_-1].push_back(k);
        }
        delete[] ptx;

        hasKernel_[currentId_ - 1] = true;
        
        if (settings_.debug) {
            debugInfo_.nanoSecondsProgramBuild += timer.elapsed().wall - timerBase;
        }
    }

    if (kernel_[currentId_ - 1].size() == 1){

        // no conditional expectation

        // set kernel args
        void* args[] = {&input,
                        &dOutput_[nOutputVariables_[currentId_ - 1]],
                        &d_randomVariables_};

        // execute kernel
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }
        cuErr = cuLaunchKernel(kernel_[currentId_ - 1][0], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args,
                               nullptr);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        }
    
    } else{
        // conditional expectation
        cusolverStatus_t cusolverErr;
        cusolverDnHandle_t cusolverHandle;
        cusolverErr = cusolverDnCreate(&cusolverHandle);
        cusolverErr = cusolverDnSetStream(cusolverHandle, stream_);
  
        // first kernel
        // Allocate memory for A, b, A_filter
        double* A;
        cudaErr = cudaMalloc(&A, basisFunctionCE_[currentId_][0] * size_[currentId_ - 1] * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A fails: "
                                            << cudaGetErrorString(cudaErr));
        double* b;
        cudaErr = cudaMalloc(&b, size_[currentId_ - 1] * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for b fails: "
                                            << cudaGetErrorString(cudaErr));
        std::vector<double*> A_unfilter(kernel_[currentId_ - 1].size() - 1);
        std::cout << "A_unfilter.size() = " << A_unfilter.size() << std::endl;
        cudaErr = cudaMalloc(&A_unfilter[0], basisFunctionCE_[currentId_][0] * size_[currentId_ - 1] * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A_filter[0] fails: "
                                            << cudaGetErrorString(cudaErr));
        // set kernel args
        void* args0[] = {&input, &d_randomVariables_, &values, &A, &b, &A_unfilter[0]};

        // execute kernel
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }
        cuErr = cuLaunchKernel(kernel_[currentId_ - 1][0], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args0,
                                nullptr);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        }

        //temp check result
        cudaStreamSynchronize(stream_);
        double* hA;
        //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        cudaErr = cudaMallocHost(&hA, basisFunctionCE_[currentId_][0] * size_[currentId_ - 1] * sizeof(double));
        std::cout << "cudaErr = " << cudaErr << std::endl;
        cudaErr = cudaMemcpy(hA, A, basisFunctionCE_[currentId_][0] * size_[currentId_ - 1] * sizeof(double), cudaMemcpyDeviceToHost);
        std::cout << "cudaErr = " << cudaErr << std::endl;
        for (size_t i = 0; i < 10; ++i) {
            std::cout << "cpu:A["<< i <<"]: " << hA[i] << std::endl;
        }

        // linear regression
        double* X;
        cudaErr = cudaMalloc(&X, basisFunctionCE_[currentId_][0] * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for X fails: "
                                            << cudaGetErrorString(cudaErr));

        size_t d_workspace;
        cusolverErr = cusolverDnDDgels_bufferSize(cusolverHandle, size_[currentId_ - 1], basisFunctionCE_[currentId_][0], 1, A, size_[currentId_ - 1], b, size_[currentId_ - 1], X, basisFunctionCE_[currentId_][0], NULL, &d_workspace);
        std::cout << "cusolverErr = " << cusolverErr << std::endl;
        double* d_work;
        cudaErr = cudaMalloc((void**)&d_work, d_workspace);
        int niters = 0;
        int* d_info;
        cudaErr = cudaMalloc((void**)&d_info, sizeof(size_t));
        cusolverErr = cusolverDnDDgels(cusolverHandle, size_[currentId_ - 1], basisFunctionCE_[currentId_][0], 1, A, size_[currentId_ - 1], b, size_[currentId_ - 1], X, basisFunctionCE_[currentId_][0], d_work, d_workspace, &niters, d_info);
        std::cout << "cusolverErr = " << cusolverErr << std::endl;

        //temp check result
        int h_info = 0;
        //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        cudaMemcpy(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost);
        std::cout << "cusolver info: " << h_info << std::endl;

        //temp check result
        //cudaStreamSynchronize(stream_);
        //double* hX;
        ////cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        //cudaErr = cudaMallocHost(&hX, basisFunctionCE_[currentId_][0] * sizeof(double));
        //std::cout << "cudaErr = " << cudaErr << std::endl;
        //cudaErr = cudaMemcpy(hX, X, basisFunctionCE_[currentId_][0] * sizeof(double), cudaMemcpyDeviceToHost);
        //std::cout << "cudaErr = " << cudaErr << std::endl;
        //for (size_t i = 0; i < 5; ++i) {
        //    std::cout << "X["<< i <<"]: " << hX[i] << std::endl;
        //}

        // clear memory
        releaseMem(A, "CudaContext::finalizeCalculation::A");
        releaseMem(b, "CudaContext::finalizeCalculation::b");
        releaseMem(d_work, "CudaContext::finalizeCalculation::d_work");

        // second to penultimate kernel
        for (size_t k = 1; k < kernel_[currentId_ - 1].size() - 1; ++k) {
            std::cout << "for loop called " << k << std::endl;
            // Allocate memory for A, b, A_filter
            cudaErr = cudaMalloc(&A, basisFunctionCE_[currentId_][k] * size_[currentId_ - 1] * sizeof(double));
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A fails: "
                                                << cudaGetErrorString(cudaErr));
            cudaErr = cudaMalloc(&b, size_[currentId_ - 1] * sizeof(double));
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for b fails: "
                                                << cudaGetErrorString(cudaErr));
            cudaErr = cudaMalloc(&A_unfilter[k], basisFunctionCE_[currentId_][k] * size_[currentId_ - 1] * sizeof(double));
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A_unfilter[k] fails: "
                                                << cudaGetErrorString(cudaErr));
            
            // set kernel args
            void* args[] = {&input, &d_randomVariables_, &values, &A, &b, &A_unfilter[k], &A_unfilter[k-1], &X};

            // execute kernel
            if (settings_.debug) {
                timerBase = timer.elapsed().wall;
            }
            cuErr = cuLaunchKernel(kernel_[currentId_ - 1][k], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args,
                                   nullptr);
            if (cuErr != CUDA_SUCCESS) {
                const char* errStr;
                cuGetErrorString(cuErr, &errStr);
                std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
            }

            // clear memory
            releaseMem(A_unfilter[k-1], "CudaContext::finalizeCalculation::A_unfilter[k-1]");
            releaseMem(X, "CudaContext::finalizeCalculation::X");

            // linear regression
            cudaErr = cudaMalloc(&X, basisFunctionCE_[currentId_][k] * sizeof(double));
            QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for X fails: "
                                                << cudaGetErrorString(cudaErr));

            cusolverErr = cusolverDnDDgels_bufferSize(cusolverHandle, size_[currentId_ - 1], basisFunctionCE_[currentId_][k], 1, A, size_[currentId_ - 1], b, size_[currentId_ - 1], X, basisFunctionCE_[currentId_][k], NULL, &d_workspace);
            std::cout << "cusolverErr = " << cusolverErr << std::endl;
            cudaErr = cudaMalloc((void**)&d_work, d_workspace);
            int niters = 0;
            cudaErr = cudaMalloc((void**)&d_info, sizeof(size_t));
            cusolverErr = cusolverDnDDgels(cusolverHandle, size_[currentId_ - 1], basisFunctionCE_[currentId_][k], 1, A, size_[currentId_ - 1], b, size_[currentId_ - 1], X, basisFunctionCE_[currentId_][k], d_work, d_workspace, &niters, d_info);
            std::cout << "cusolverErr = " << cusolverErr << std::endl;

            //temp check result
            int h_info = 0;
            //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
            cudaMemcpy(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost);
            std::cout << "cusolver info: " << h_info << std::endl;

            // temp output
            double* hX2;
            //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
            cudaErr = cudaMallocHost(&hX2, basisFunctionCE_[currentId_][k] * sizeof(double));
            std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
            cudaErr = cudaMemcpy(hX2, X, basisFunctionCE_[currentId_][k] * sizeof(double), cudaMemcpyDeviceToHost);
            std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
            for (size_t i = 0; i < 5; ++i) {
                std::cout << "hX2["<< i <<"]: " << hX2[i] << std::endl;
            }


            // clear memory
            releaseMem(A, "CudaContext::finalizeCalculation::A");
            releaseMem(b, "CudaContext::finalizeCalculation::b");
            releaseMem(d_work, "CudaContext::finalizeCalculation::d_work");
        }

        cudaStreamSynchronize(stream_);
        double* hX1;
        //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        cudaErr = cudaMallocHost(&hX1, basisFunctionCE_[currentId_].back() * sizeof(double));
        std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        cudaErr = cudaMemcpy(hX1, X, basisFunctionCE_[currentId_].back() * sizeof(double), cudaMemcpyDeviceToHost);
        std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        for (size_t i = 0; i < 5; ++i) {
            std::cout << "hX1["<< i <<"]: " << hX1[i] << std::endl;
        }
        // last kernel
        // set kernel args
        void* argsFinal[] = {&input, &d_randomVariables_, &values, &dOutput_[nOutputVariables_[currentId_ - 1]], &A_unfilter.back(), &X};
        //void* argsTmp[] = {&input, &d_randomVariables_, &values, &output, &A_unfilter.back(), &X};
        //void* args0[] = {&input, &d_randomVariables_, &values, &A, &b, &A_unfilter[0]};
        // execute kernel
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }
        cuErr = cuLaunchKernel(kernel_[currentId_ - 1].back(), nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, argsFinal,
                                nullptr);
        const char* errStr;
            cuGetErrorString(cuErr, &errStr);
        std::cout << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        }
        double* hX3;
        //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        cudaErr = cudaMallocHost(&hX3, basisFunctionCE_[currentId_].back() * sizeof(double));
        std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        cudaErr = cudaMemcpy(hX3, X, basisFunctionCE_[currentId_].back() * sizeof(double), cudaMemcpyDeviceToHost);
        std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        for (size_t i = 0; i < 5; ++i) {
            std::cout << "hX3["<< i <<"]: " << hX3[i] << std::endl;
        }
        cudaStreamSynchronize(stream_);

        // clear memory
        releaseMem(A_unfilter.back(), "CudaContext::finalizeCalculation::A_unfilter.back()");
        releaseMem(X, "CudaContext::finalizeCalculation::X");
        releaseMem(values, "CudaContext::finalizeCalculation::values");
        releaseMem(d_work, "CudaContext::finalizeCalculation::d_work");
    }

     if (settings_.debug) {
        debugInfo_.nanoSecondsCalculation += timer.elapsed().wall - timerBase;
    }

    // copy the results

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }
    double* h_output;
    cudaErr = cudaMallocHost(&h_output, nOutputVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double));
    QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for h_output fails: "
                                           << cudaGetErrorString(cudaErr));
    //cudaErr = cudaMemcpyAsync(h_output, dOutput_[nOutputVariables_[currentId_ - 1]],
    //                          sizeof(double) * size_[currentId_ - 1] * nOutputVariables_[currentId_ - 1],
    //                          cudaMemcpyDeviceToHost, stream_);
    cudaErr = cudaMemcpy(h_output, dOutput_[nOutputVariables_[currentId_ - 1]],
                              sizeof(double) * size_[currentId_ - 1] * nOutputVariables_[currentId_ - 1],
                              cudaMemcpyDeviceToHost);
    QL_REQUIRE(cudaErr == cudaSuccess,
               "CudaContext::finalizeCalculation(): memory copy from device to host for h_output fails: " << cudaGetErrorString(cudaErr));
    cudaStreamSynchronize(stream_);
    for (size_t i = 0; i < nOutputVariables_[currentId_ - 1]; ++i) {
        std::copy(h_output + i * size_[currentId_ - 1],
                  h_output + (i + 1) * size_[currentId_ - 1], output[i]);
    }

    // clear memory
    releaseMem(input, "finalizeCalculation()");
    //delete[] h_output;
    cudaFreeHost(h_input);
    cudaFreeHost(h_output);
    
    // clear source
    source_.clear();

    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }
    //std::cout << "================================================" << std::endl;
}

const ComputeContext::DebugInfo& CudaContext::debugInfo() const { return debugInfo_; }
std::vector<std::pair<std::string, std::string>> CudaContext::deviceInfo() const { return deviceInfo_; }
bool CudaContext::supportsDoublePrecision() const { return supportsDoublePrecision_; }

size_t CudaContext::binom_helper(size_t n, size_t k) {
    return (k==1||k==n-1)?n:(k+k<n)?(binom_helper(n-1,k-1) * n)/k:(binom_helper(n-1,k)*n)/(n-k);
}

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
