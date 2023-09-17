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

#include <qle/math/openclenvironment.hpp>
#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/timer/timer.hpp>

#include <iostream>

#ifdef ORE_ENABLE_OPENCL
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#endif

#define MAX_N_PLATFORMS 4U
#define MAX_N_DEVICES 8U
#define MAX_N_NAME 64U
#define MAX_BUILD_LOG 65536U
#define MAX_BUILD_LOG_LOGFILE 1024U

namespace QuantExt {

#ifdef ORE_ENABLE_OPENCL
namespace {
std::string errorText(cl_int err) {
    switch (err) {
    case 0:
        return "CL_SUCCESS";
    case -1:
        return "CL_DEVICE_NOT_FOUND";
    case -2:
        return "CL_DEVICE_NOT_AVAILABLE";
    case -3:
        return "CL_COMPILER_NOT_AVAILABLE";
    case -4:
        return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5:
        return "CL_OUT_OF_RESOURCES";
    case -6:
        return "CL_OUT_OF_HOST_MEMORY";
    case -7:
        return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8:
        return "CL_MEM_COPY_OVERLAP";
    case -9:
        return "CL_IMAGE_FORMAT_MISMATCH";
    case -10:
        return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11:
        return "CL_BUILD_PROGRAM_FAILURE";
    case -12:
        return "CL_MAP_FAILURE";
    case -13:
        return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14:
        return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15:
        return "CL_COMPILE_PROGRAM_FAILURE";
    case -16:
        return "CL_LINKER_NOT_AVAILABLE";
    case -17:
        return "CL_LINK_PROGRAM_FAILURE";
    case -18:
        return "CL_DEVICE_PARTITION_FAILED";
    case -19:
        return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
    case -30:
        return "CL_INVALID_VALUE";
    case -31:
        return "CL_INVALID_DEVICE_TYPE";
    case -32:
        return "CL_INVALID_PLATFORM";
    case -33:
        return "CL_INVALID_DEVICE";
    case -34:
        return "CL_INVALID_CONTEXT";
    case -35:
        return "CL_INVALID_QUEUE_PROPERTIES";
    case -36:
        return "CL_INVALID_COMMAND_QUEUE";
    case -37:
        return "CL_INVALID_HOST_PTR";
    case -38:
        return "CL_INVALID_MEM_OBJECT";
    case -39:
        return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40:
        return "CL_INVALID_IMAGE_SIZE";
    case -41:
        return "CL_INVALID_SAMPLER";
    case -42:
        return "CL_INVALID_BINARY";
    case -43:
        return "CL_INVALID_BUILD_OPTIONS";
    case -44:
        return "CL_INVALID_PROGRAM";
    case -45:
        return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46:
        return "CL_INVALID_KERNEL_NAME";
    case -47:
        return "CL_INVALID_KERNEL_DEFINITION";
    case -48:
        return "CL_INVALID_KERNEL";
    case -49:
        return "CL_INVALID_ARG_INDEX";
    case -50:
        return "CL_INVALID_ARG_VALUE";
    case -51:
        return "CL_INVALID_ARG_SIZE";
    case -52:
        return "CL_INVALID_KERNEL_ARGS";
    case -53:
        return "CL_INVALID_WORK_DIMENSION";
    case -54:
        return "CL_INVALID_WORK_GROUP_SIZE";
    case -55:
        return "CL_INVALID_WORK_ITEM_SIZE";
    case -56:
        return "CL_INVALID_GLOBAL_OFFSET";
    case -57:
        return "CL_INVALID_EVENT_WAIT_LIST";
    case -58:
        return "CL_INVALID_EVENT";
    case -59:
        return "CL_INVALID_OPERATION";
    case -60:
        return "CL_INVALID_GL_OBJECT";
    case -61:
        return "CL_INVALID_BUFFER_SIZE";
    case -62:
        return "CL_INVALID_MIP_LEVEL";
    case -63:
        return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64:
        return "CL_INVALID_PROPERTY";
    case -65:
        return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66:
        return "CL_INVALID_COMPILER_OPTIONS";
    case -67:
        return "CL_INVALID_LINKER_OPTIONS";
    case -68:
        return "CL_INVALID_DEVICE_PARTITION_COUNT";
    default:
        return "unknown cl error code " + std::to_string(err);
    }
}
} // namespace

class OpenClContext : public ComputeContext {
public:
    OpenClContext(cl_device_id device);
    ~OpenClContext() override final;
    void init() override final;

    std::pair<std::size_t, bool> initiateCalculation(const std::size_t n, const std::size_t id = 0,
                                                     const std::size_t version = 0,
                                                     const bool debug = false) override final;
    std::size_t createInputVariable(double v) override final;
    std::size_t createInputVariable(double* v) override final;
    std::vector<std::vector<std::size_t>> createInputVariates(const std::size_t dim, const std::size_t steps,
                                                              const std::uint32_t seed) override final;
    std::size_t applyOperation(const std::size_t randomVariableOpCode,
                               const std::vector<std::size_t>& args) override final;
    void freeVariable(const std::size_t id) override final;
    void declareOutputVariable(const std::size_t id) override final;
    void finalizeCalculation(std::vector<double*>& output) override final;

    const DebugInfo& debugInfo() const override final;

private:
    cl_mem initLinearCongruentialRng(const std::size_t n, std::uint32_t& seedUpdate);

    void releaseMem(cl_mem& m);
    void releaseKernel(cl_kernel& k);
    void releaseProgram(cl_program& p);

    enum class ComputeState { idle, createInput, createVariates, calc };

    bool initialized_ = false;
    cl_device_id device_;
    cl_context context_;
    cl_command_queue queue_;

    // will be accumulated over all calcs
    ComputeContext::DebugInfo debugInfo_;

    // 1a vectors per current calc id

    std::vector<std::size_t> size_;
    std::vector<bool> hasKernel_;
    std::vector<std::size_t> version_;
    std::vector<cl_program> program_;
    std::vector<cl_kernel> kernel_;
    std::vector<std::size_t> inputBufferSize_;
    std::vector<std::size_t> nOutputVars_;

    // 1b linear congruential rng multipliers per size

    std::map<std::size_t, cl_mem> linearCongruentialMultipliers_;
    std::map<std::size_t, std::uint32_t> seedUpdate_;

    // 2 curent calc

    std::size_t currentId_ = 0;
    ComputeState currentState_ = ComputeState::idle;
    std::size_t nVars_;
    bool debug_;

    // 2a indexed by var id
    std::vector<std::size_t> inputVarOffset_;
    std::vector<bool> inputVarIsScalar_;
    std::vector<float> inputVarValue_;
    std::vector<float*> inputVarPtr_;
    std::vector<std::vector<float>> inputVarPtrVal_;

    // 2b collection of variable ids
    std::vector<std::size_t> freedVariables_;
    std::vector<std::size_t> outputVariables_;

    // 2c variate seeds
    std::vector<std::uint32_t> variateSeed_;

    // 2d kernel ssa
    std::string currentSsa_;
};

OpenClFramework::OpenClFramework() {
    std::set<std::string> tmp;
    cl_platform_id platforms[MAX_N_PLATFORMS];
    cl_uint nPlatforms;
    clGetPlatformIDs(MAX_N_PLATFORMS, platforms, &nPlatforms);
    for (std::size_t p = 0; p < nPlatforms; ++p) {
        char platformName[MAX_N_NAME];
        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, MAX_N_NAME, platformName, NULL);
        cl_device_id devices[MAX_N_DEVICES];
        cl_uint nDevices;
        clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 3, devices, &nDevices);
        for (std::size_t d = 0; d < nDevices; ++d) {
            char deviceName[MAX_N_NAME]; //, driverVersion[MAX_N_NAME];
            clGetDeviceInfo(devices[d], CL_DEVICE_NAME, MAX_N_NAME, &deviceName, NULL);
            // clGetDeviceInfo(devices[d], CL_DRIVER_VERSION, MAX_N_NAME, &driverVersion, NULL);
            contexts_["OpenCL/" + std::string(platformName) + "/" + std::string(deviceName)] =
                new OpenClContext(devices[d]);
        }
    }
}

OpenClFramework::~OpenClFramework() {
    for (auto& [_, c] : contexts_) {
        delete c;
    }
}

OpenClContext::OpenClContext(cl_device_id device) : initialized_(false), device_(device) {}

OpenClContext::~OpenClContext() {
    if (initialized_) {
        cl_int err;

        for (auto& [_, b] : linearCongruentialMultipliers_)
            releaseMem(b);

        for (auto& k : kernel_) {
            releaseKernel(k);
        }

        for (auto& p : program_) {
            releaseProgram(p);
        }

        if (err = clReleaseCommandQueue(queue_); err != CL_SUCCESS) {
            std::cerr << "OpenClContext: error during clReleaseCommandQueue: " + errorText(err) << std::endl;
        }

        if (err = clReleaseContext(context_); err != CL_SUCCESS) {
            std::cerr << "OpenClContext: error during clReleaseContext: " + errorText(err) << std::endl;
        }
    }
}

void OpenClContext::releaseMem(cl_mem& m) {
    cl_int err;
    if (err = clReleaseMemObject(m); err != CL_SUCCESS) {
        std::cerr << "OpenClContext: error during clReleaseMemObject: " + errorText(err) << std::endl;
    }
}

void OpenClContext::releaseKernel(cl_kernel& k) {
    cl_int err;
    if (err = clReleaseKernel(k); err != CL_SUCCESS) {
        std::cerr << "OpenClContext: error during clReleaseKernel: " + errorText(err) << std::endl;
    }
}

void OpenClContext::releaseProgram(cl_program& p) {
    cl_int err;
    if (err = clReleaseProgram(p); err != CL_SUCCESS) {
        std::cerr << "OpenClContext: error during clReleaseProgram: " + errorText(err) << std::endl;
    }
}

void OpenClContext::init() {

    if (initialized_) {
        return;
    }

    debugInfo_.numberOfOperations = 0;
    debugInfo_.nanoSecondsDataCopy = 0;
    debugInfo_.nanoSecondsProgramBuild = 0;
    debugInfo_.nanoSecondsCalculation = 0;

    cl_int err;

    // create context and command queue

    context_ = clCreateContext(NULL, 1, &device_, NULL, NULL, &err);
    QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::OpenClContext(): error during clCreateContext(): " << errorText(err));

    // deprecated in open-cl version 2.0, clCreateCommandQueueWithProperties
    queue_ = clCreateCommandQueue(context_, device_, 0, &err);
    QL_REQUIRE(err == CL_SUCCESS,
               "OpenClContext::OpenClContext(): error during clCreateCommandQueue(): " << errorText(err));

    initialized_ = true;
}

cl_mem OpenClContext::initLinearCongruentialRng(const std::size_t n, std::uint32_t& seedUpdate) {

    const std::uint32_t a = 1099087573; // same as in the boost compute lg-engine

    std::vector<std::uint32_t> linearCongruentialMultipliers(n);
    linearCongruentialMultipliers[0] = a;
    for (std::size_t i = 1; i < n; ++i) {
        linearCongruentialMultipliers[i] = a * linearCongruentialMultipliers[i - 1];
    }
    seedUpdate = linearCongruentialMultipliers.back() * a;

    cl_int err;
    cl_mem multiplierBuffer = clCreateBuffer(context_, CL_MEM_READ_WRITE, sizeof(std::uint32_t) * n, NULL, &err);
    QL_REQUIRE(err == CL_SUCCESS,
               "OpenClContext::initLinearCongruentialRng(): error during clCreateBuffer(): " << errorText(err));
    err = clEnqueueWriteBuffer(queue_, multiplierBuffer, CL_TRUE, 0, sizeof(std::uint32_t) * n,
                               &linearCongruentialMultipliers[0], 0, NULL, NULL);
    QL_REQUIRE(err == CL_SUCCESS,
               "OpenClContext::initLinearCongruentialRng(): error during clEnqueueWriteBuffer(): " << errorText(err));
    return multiplierBuffer;
}

std::pair<std::size_t, bool> OpenClContext::initiateCalculation(const std::size_t n, const std::size_t id,
                                                                const std::size_t version, const bool debug) {

    QL_REQUIRE(n > 0, "OpenClContext::initiateCalculation(): n must not be zero");

    bool newCalc = false;
    debug_ = debug;

    if (id == 0) {

        // initiate new calcaultion

        size_.push_back(n);
        hasKernel_.push_back(false);
        version_.push_back(version);
        program_.push_back(cl_program());
        kernel_.push_back(cl_kernel());
        inputBufferSize_.push_back(0);
        nOutputVars_.push_back(0);

        if (auto l = linearCongruentialMultipliers_.find(n); l == linearCongruentialMultipliers_.end())
            linearCongruentialMultipliers_[n] = initLinearCongruentialRng(n, seedUpdate_[n]);

        currentId_ = hasKernel_.size();
        newCalc = true;

    } else {

        // initiate calculation on existing id

        QL_REQUIRE(id <= hasKernel_.size(),
                   "OpenClContext::initiateCalculation(): id (" << id << ") invalid, got 1..." << hasKernel_.size());
        QL_REQUIRE(size_[id - 1] == n, "OpenClCOntext::initiateCalculation(): size ("
                                           << size_[id - 1] << ") for id " << id << " does not match current size ("
                                           << n << ")");

        if (version != version_[id - 1]) {
            hasKernel_[id - 1] = false;
            version_[id - 1] = version;
            releaseKernel(kernel_[id - 1]);
            releaseProgram(program_[id - 1]);
            newCalc = true;
        }

        currentId_ = id;
    }

    // reset variable info

    nVars_ = 0;

    inputVarOffset_.clear();
    inputVarIsScalar_.clear();
    inputVarValue_.clear();
    inputVarPtr_.clear();
    inputVarPtrVal_.clear();

    freedVariables_.clear();
    outputVariables_.clear();

    variateSeed_.clear();

    // reset ssa

    currentSsa_.clear();

    // set state

    currentState_ = ComputeState::createInput;

    // return calc id

    return std::make_pair(currentId_, newCalc);
}

std::size_t OpenClContext::createInputVariable(double v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "OpenClContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                  << ")");
    std::size_t nextOffset = 0;
    if (!inputVarOffset_.empty()) {
        nextOffset = inputVarOffset_.back() + (inputVarIsScalar_.back() ? 1 : size_[currentId_ - 1]);
    }
    inputVarOffset_.push_back(nextOffset);
    inputVarIsScalar_.push_back(true);
    inputVarValue_.push_back((float)v);
    inputVarPtr_.push_back(nullptr);
    inputVarPtrVal_.push_back({});
    return nVars_++;
}

std::size_t OpenClContext::createInputVariable(double* v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "OpenClContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                  << ")");
    std::size_t nextOffset = 0;
    if (!inputVarOffset_.empty()) {
        nextOffset = inputVarOffset_.back() + (inputVarIsScalar_.back() ? 1 : size_[currentId_ - 1]);
    }
    inputVarOffset_.push_back(nextOffset);
    inputVarIsScalar_.push_back(false);
    inputVarValue_.push_back(0.0f);
    inputVarPtrVal_.push_back(std::vector<float>(size_[currentId_-1]));
    std::copy(v, v + size_[currentId_ - 1], inputVarPtrVal_.back().begin());
    inputVarPtr_.push_back(&inputVarPtrVal_.back()[0]);
    return nVars_++;
}

std::vector<std::vector<std::size_t>> OpenClContext::createInputVariates(const std::size_t dim, const std::size_t steps,
                                                                         const std::uint32_t seed) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates,
               "OpenClContext::createInputVariable(): not in state createInput or createVariates ("
                   << static_cast<int>(currentState_) << ")");
    currentState_ = ComputeState::createVariates;
    std::vector<std::vector<std::size_t>> resultIds(dim, std::vector<std::size_t>(steps));
    std::uint32_t currentSeed = seed;
    for (std::size_t i = 0; i < dim; ++i) {
        for (std::size_t j = 0; j < steps; ++j) {
            variateSeed_.push_back(currentSeed);
            currentSeed *= seedUpdate_[size_[currentId_ - 1]];
            resultIds[i][j] = nVars_++;
        }
    }
    return resultIds;
}

std::size_t OpenClContext::applyOperation(const std::size_t randomVariableOpCode,
                                          const std::vector<std::size_t>& args) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates ||
                   currentState_ == ComputeState::calc,
               "OpenClContext::applyOperation(): not in state createInput or calc (" << static_cast<int>(currentState_)
                                                                                     << ")");
    currentState_ = ComputeState::calc;
    QL_REQUIRE(currentId_ > 0, "OpenClContext::applyOperation(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "OpenClContext::applyOperation(): id (" << currentId_ << ") in version "
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
        resultId = nVars_++;
        resultIdNeedsDeclaration = true;
    }

    // determine arg variable names

    std::vector<std::string> argStr(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] < inputVarOffset_.size()) {
            argStr[i] = "input[" + std::to_string(inputVarOffset_[args[i]]) + "UL" +
                        (inputVarIsScalar_[args[i]] ? "]" : " + i]");
        } else {
            // variable is an (intermediate) result
            argStr[i] = "v" + std::to_string(args[i]);
        }
    }

    // generate ssa entry

    std::string ssaLine =
        (resultIdNeedsDeclaration ? "float " : "") + std::string("v") + std::to_string(resultId) + " = ";

    switch (randomVariableOpCode) {
    case RandomVariableOpCode::None: {
        ssaLine += argStr[0];
        break;
    }
    case RandomVariableOpCode::Add: {
        ssaLine += argStr[0] + " + " + argStr[1] + ";";
        break;
    }
    case RandomVariableOpCode::Subtract: {
        ssaLine += argStr[0] + " - " + argStr[1] + ";";
        break;
    }
    case RandomVariableOpCode::Negative: {
        ssaLine += "-" + argStr[0] + ";";
        break;
    }
    case RandomVariableOpCode::Mult: {
        ssaLine += argStr[0] + " * " + argStr[1] + ";";
        break;
    }
    case RandomVariableOpCode::Div: {
        ssaLine += argStr[0] + " / " + argStr[1] + ";";
        break;
    }
    case RandomVariableOpCode::IndicatorEq: {
        ssaLine += "ore_indicatorEq(" + argStr[0] + "," + argStr[1] + ");";
        break;
    }
    case RandomVariableOpCode::IndicatorGt: {
        ssaLine += "ore_indicatorGt(" + argStr[0] + "," + argStr[1] + ");";
        break;
    }
    case RandomVariableOpCode::IndicatorGeq: {
        ssaLine += "ore_indicatorGeq(" + argStr[0] + "," + argStr[1] + ");";
        break;
    }
    case RandomVariableOpCode::Min: {
        ssaLine += "fmin(" + argStr[0] + "," + argStr[1] + ");";
        break;
    }
    case RandomVariableOpCode::Max: {
        ssaLine += "fmax(" + argStr[0] + "," + argStr[1] + ");";
        break;
    }
    case RandomVariableOpCode::Abs: {
        ssaLine += "fabs(" + argStr[0] + ");";
        break;
    }
    case RandomVariableOpCode::Exp: {
        ssaLine += "exp(" + argStr[0] + ");";
        break;
    }
    case RandomVariableOpCode::Sqrt: {
        ssaLine += "sqrt(" + argStr[0] + ");";
        break;
    }
    case RandomVariableOpCode::Log: {
        ssaLine += "log(" + argStr[0] + ");";
        break;
    }
    case RandomVariableOpCode::Pow: {
        ssaLine += "pow(" + argStr[0] + "," + argStr[1] + ");";
        break;
    }
    default: {
        QL_FAIL("OpenClContext::executeKernel(): no implementation for op code "
                << randomVariableOpCode << " (" << getRandomVariableOpLabels()[randomVariableOpCode] << ") provided.");
    }
    }

    // add entry to global ssa

    currentSsa_ += "  " + ssaLine + "\n";

    // update num of ops in debug info

    if (debug_)
        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];

    // return result id

    return resultId;
}

void OpenClContext::freeVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ == ComputeState::calc,
               "OpenClContext::free(): not in state calc (" << static_cast<int>(currentState_) << ")");

    // we do not free input variables, only variables that were added during the calc

    if (id < inputVarOffset_.size())
        return;

    freedVariables_.push_back(id);
}

void OpenClContext::declareOutputVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ != ComputeState::idle, "OpenClContext::declareOutputVariable(): state is idle");
    QL_REQUIRE(currentId_ > 0, "OpenClContext::declareOutputVariable(): current id not set");
    outputVariables_.push_back(id);
    nOutputVars_[currentId_ - 1]++;
}

void OpenClContext::finalizeCalculation(std::vector<double*>& output) {
    struct exitGuard {
        exitGuard() {}
        ~exitGuard() {
            *currentState = ComputeState::idle;
            for (auto& m : mem)
                clReleaseMemObject(m);
        }
        ComputeState* currentState;
        std::vector<cl_mem> mem;
    } guard;

    guard.currentState = &currentState_;

    QL_REQUIRE(currentId_ > 0, "OpenClContext::finalizeCalculation(): current id is not set");
    QL_REQUIRE(output.size() == nOutputVars_[currentId_ - 1],
               "OpenClContext::finalizeCalculation(): output size ("
                   << output.size() << ") inconsistent to kernel output size (" << nOutputVars_[currentId_ - 1] << ")");

    boost::timer::cpu_timer timer;
    boost::timer::nanosecond_type timerBase;

    // create input and output buffers

    if (debug_) {
        timerBase = timer.elapsed().wall;
    }

    std::size_t inputBufferSize = 0;
    if (!inputVarOffset_.empty())
        inputBufferSize = inputVarOffset_.back() + (inputVarIsScalar_.back() ? 1 : size_[currentId_ - 1]);
    cl_int err;
    cl_mem inputBuffer;
    if (inputBufferSize > 0) {
        inputBuffer = clCreateBuffer(context_, CL_MEM_READ_WRITE, sizeof(float) * inputBufferSize, NULL, &err);
        guard.mem.push_back(inputBuffer);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::finalizeCalculation(): creating input buffer fails: " << errorText(err));
    }

    std::size_t outputBufferSize = nOutputVars_[currentId_ - 1] * size_[currentId_ - 1];
    cl_mem outputBuffer;
    if (outputBufferSize > 0) {
        outputBuffer = clCreateBuffer(context_, CL_MEM_READ_WRITE, sizeof(float) * outputBufferSize, NULL, &err);
        guard.mem.push_back(outputBuffer);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::finalizeCalculation(): creating output buffer fails: " << errorText(err));
    }

    if (debug_) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // build kernel if necessary

    if (!hasKernel_[currentId_ - 1]) {

        const std::string includeSource =
            "bool ore_closeEnough(const float x, const float y) {\n"
            "    const float tol = 42.0f * 1.1920929e-07f;\n"
            "    float diff = fabs(x - y);\n"
            "    if (x == 0.0f || y == 0.0f)\n"
            "        return diff < tol * tol;\n"
            "    return diff <= tol * fabs(x) || diff <= tol * fabs(y);\n"
            "}\n"
            "\n"
            "float ore_indicatorEq(const float x, const float y) { return ore_closeEnough(x, y) ? 1.0f : 0.0f; }\n\n"
            "float ore_indicatorGt(const float x, const float y) { return x > y && !ore_closeEnough(x, y); }\n\n"
            "float ore_indicatorGeq(const float x, const float y) { return x > y || ore_closeEnough(x, y); }\n\n"
            "float ore_invCumN(const uint x0) {\n"
            "    const float a1_ = -3.969683028665376e+01f;\n"
            "    const float a2_ = 2.209460984245205e+02f;\n"
            "    const float a3_ = -2.759285104469687e+02f;\n"
            "    const float a4_ = 1.383577518672690e+02f;\n"
            "    const float a5_ = -3.066479806614716e+01f;\n"
            "    const float a6_ = 2.506628277459239e+00f;\n"
            "    const float b1_ = -5.447609879822406e+01f;\n"
            "    const float b2_ = 1.615858368580409e+02f;\n"
            "    const float b3_ = -1.556989798598866e+02f;\n"
            "    const float b4_ = 6.680131188771972e+01f;\n"
            "    const float b5_ = -1.328068155288572e+01f;\n"
            "    const float c1_ = -7.784894002430293e-03f;\n"
            "    const float c2_ = -3.223964580411365e-01f;\n"
            "    const float c3_ = -2.400758277161838e+00f;\n"
            "    const float c4_ = -2.549732539343734e+00f;\n"
            "    const float c5_ = 4.374664141464968e+00f;\n"
            "    const float c6_ = 2.938163982698783e+00f;\n"
            "    const float d1_ = 7.784695709041462e-03f;\n"
            "    const float d2_ = 3.224671290700398e-01f;\n"
            "    const float d3_ = 2.445134137142996e+00f;\n"
            "    const float d4_ = 3.754408661907416e+00f;\n"
            "    const float x_low_ = 0.02425f;\n"
            "    const float x_high_ = 1.0f - x_low_;\n"
            "    const float x = x0 / (float)UINT_MAX;\n"
            "    if (x < x_low_ || x_high_ < x) {\n"
            "        if (x0 == UINT_MAX) {\n"
            "          return 0x1.fffffep127f;\n"
            "        } else if(x0 == 0) {\n"
            "          return -0x1.fffffep127f;\n"
            "        }\n"
            "        float z;\n"
            "        if (x < x_low_) {\n"
            "            z = sqrt(-2.0f * log(x));\n"
            "            z = (((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /\n"
            "                ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0f);\n"
            "        } else {\n"
            "            z = sqrt(-2.0f * log(1.0f - x));\n"
            "            z = -(((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /\n"
            "                ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0f);\n"
            "        }\n"
            "        return z;\n"
            "    } else {\n"
            "        float z = x - 0.5f;\n"
            "        float r = z * z;\n"
            "        z = (((((a1_ * r + a2_) * r + a3_) * r + a4_) * r + a5_) * r + a6_) * z /\n"
            "            (((((b1_ * r + b2_) * r + b3_) * r + b4_) * r + b5_) * r + 1.0f);\n"
            "        return z;\n"
            "    }\n"
            "}\n\n";

        std::string kernelName =
            "ore_kernel_" + std::to_string(currentId_) + "_" + std::to_string(version_[currentId_ - 1]);

        std::string kernelSource = includeSource + "__kernel void " + kernelName +
                                   "(\n"
                                   "   __global uint* lcrng_mult" +
                                   (inputBufferSize > 0 ? ",\n   __global float* input" : "") +
                                   (outputBufferSize > 0 ? ",\n   __global float* output" : "") +
                                   ") {\n"
                                   "unsigned int i = get_global_id(0);\n"
                                   "if(i < " +
                                   std::to_string(size_[currentId_ - 1]) + "U) {\n";

        for (std::size_t i = 0; i < variateSeed_.size(); ++i) {
            kernelSource += "  float v" + std::to_string(i + inputVarOffset_.size()) + " = ore_invCumN(" +
                            std::to_string(variateSeed_[i]) + "U * lcrng_mult[i]);\n";
            if (debug_)
                debugInfo_.numberOfOperations += 23 * size_[currentId_ - 1];
        }

        kernelSource += currentSsa_;

        for (std::size_t i = 0; i < nOutputVars_[currentId_ - 1]; ++i) {
            std::size_t offset = i * size_[currentId_ - 1];
            std::string ssaLine =
                "  output[" + std::to_string(offset) + "UL + i] = v" + std::to_string(outputVariables_[i]) + ";";
            kernelSource += ssaLine + "\n";
        }

        kernelSource += "  }\n"
                        "}\n";

        // std::cerr << "generated kernel: \n" + kernelSource + "\n";

        if (debug_) {
            timerBase = timer.elapsed().wall;
        }

        cl_int err;
        const char* kernelSourcePtr = kernelSource.c_str();
        program_[currentId_ - 1] = clCreateProgramWithSource(context_, 1, &kernelSourcePtr, NULL, &err);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::finalizeCalculation(): error during clCreateProgramWithSource(): "
                                          << errorText(err));
        err = clBuildProgram(program_[currentId_ - 1], 1, &device_, NULL, NULL, NULL);
        if (err != CL_SUCCESS) {
            char buffer[MAX_BUILD_LOG];
            clGetProgramBuildInfo(program_[currentId_ - 1], device_, CL_PROGRAM_BUILD_LOG, MAX_BUILD_LOG * sizeof(char),
                                  buffer, NULL);
            QL_FAIL("OpenClContext::finalizeCalculation(): error during program build for kernel '"
                    << kernelName << "': " << errorText(err) << ": "
                    << std::string(buffer).substr(MAX_BUILD_LOG_LOGFILE));
        }
        kernel_[currentId_ - 1] = clCreateKernel(program_[currentId_ - 1], kernelName.c_str(), &err);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::finalizeCalculation(): error during clCreateKernel(): " << errorText(err));

        hasKernel_[currentId_ - 1] = true;
        inputBufferSize_[currentId_ - 1] = inputBufferSize;

        if (debug_) {
            debugInfo_.nanoSecondsProgramBuild += timer.elapsed().wall - timerBase;
        }
    } else {
        QL_REQUIRE(inputBufferSize == inputBufferSize_[currentId_ - 1],
                   "OpenClContext::finalizeCalculation(): input buffer size ("
                       << inputBufferSize << ") inconsistent to kernel input buffer size ("
                       << inputBufferSize_[currentId_ - 1] << ")");
    }

    // write input data to input buffer (asynchronously)

    if (debug_) {
        timerBase = timer.elapsed().wall;
    }

    std::vector<cl_event> inputBufferEvents;
    if (inputBufferSize > 0) {
        for (std::size_t i = 0; i < inputVarOffset_.size(); ++i) {
            inputBufferEvents.push_back(cl_event());
            err = clEnqueueWriteBuffer(queue_, inputBuffer, CL_FALSE, sizeof(float) * inputVarOffset_[i],
                                       sizeof(float) * (inputVarIsScalar_[i] ? 1 : size_[currentId_ - 1]),
                                       inputVarIsScalar_[i] ? &inputVarValue_[i] : inputVarPtr_[i], 0, NULL,
                                       &inputBufferEvents.back());
            QL_REQUIRE(err == CL_SUCCESS,
                       "OpenClContext::finalizeCalculation(): writing to input buffer fails: " << errorText(err));
        }
    }

    if (debug_) {
        err = clFinish(queue_);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::clFinish(): error in debug mode: " << errorText(err));
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // set kernel args

    std::size_t kidx = 0;
    err = clSetKernelArg(kernel_[currentId_ - 1], kidx++, sizeof(cl_mem),
                         &linearCongruentialMultipliers_.at(size_[currentId_ - 1]));
    if (inputBufferSize > 0) {
        err |= clSetKernelArg(kernel_[currentId_ - 1], kidx++, sizeof(cl_mem), &inputBuffer);
    }
    if (outputBufferSize > 0) {
        err |= clSetKernelArg(kernel_[currentId_ - 1], kidx++, sizeof(cl_mem), &outputBuffer);
    }
    QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::finalizeCalculation(): set kernel args fails: " << errorText(err));

    // execute kernel

    if (debug_) {
        timerBase = timer.elapsed().wall;
    }

    cl_event runEvent;
    err = clEnqueueNDRangeKernel(queue_, kernel_[currentId_ - 1], 1, NULL, &size_[currentId_ - 1], NULL,
                                 inputBufferEvents.size(), inputBufferEvents.empty() ? nullptr : &inputBufferEvents[0],
                                 &runEvent);
    QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::finalizeCalculation(): enqueue kernel fails: " << errorText(err));

    if (debug_) {
        err = clFinish(queue_);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::clFinish(): error in debug mode: " << errorText(err));
        debugInfo_.nanoSecondsCalculation += timer.elapsed().wall - timerBase;
    }

    // copy the results (asynchronously)

    if (debug_) {
        timerBase = timer.elapsed().wall;
    }

    std::vector<cl_event> outputBufferEvents;
    if (outputBufferSize > 0) {
        std::vector<std::vector<float>> outputFloat(output.size(), std::vector<float>(size_[currentId_ - 1]));
        for (std::size_t i = 0; i < output.size(); ++i) {
            outputBufferEvents.push_back(cl_event());
            err = clEnqueueReadBuffer(queue_, outputBuffer, CL_FALSE, i * size_[currentId_ - 1],
                                      sizeof(float) * size_[currentId_ - 1], &outputFloat[i][0], 1, &runEvent,
                                      &outputBufferEvents.back());
            QL_REQUIRE(err == CL_SUCCESS,
                       "OpenClContext::finalizeCalculation(): writing to output buffer fails: " << errorText(err));
        }
        // copy from float to double
        for (std::size_t i = 0; i < output.size(); ++i) {
            std::copy(outputFloat[i].begin(), outputFloat[i].end(), output[i]);
        }
        err = clWaitForEvents(outputBufferEvents.size(), outputBufferEvents.empty() ? nullptr : &outputBufferEvents[0]);
        QL_REQUIRE(
            err == CL_SUCCESS,
            "OpenClContext::finalizeCalculation(): wait for output buffer events to finish fails: " << errorText(err));
    }

    if (debug_) {
        err = clFinish(queue_);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::clFinish(): error in debug mode: " << errorText(err));
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }
}

const ComputeContext::DebugInfo& OpenClContext::debugInfo() const { return debugInfo_; }

#endif

#ifndef ORE_ENABLE_OPENCL
OpenClFramework::OpenClFramework() {}
OpenClFramework::~OpenClFramework() {}
#endif

std::set<std::string> OpenClFramework::getAvailableDevices() const {
    std::set<std::string> tmp;
    for (auto const& [name, _] : contexts_)
        tmp.insert(name);
    return tmp;
}

ComputeContext* OpenClFramework::getContext(const std::string& deviceName) {
    auto c = contexts_.find(deviceName);
    if (c != contexts_.end()) {
        return c->second;
    }
    QL_FAIL("OpenClFrameWork::getContext(): device '"
            << deviceName << "' not found. Available devices: " << boost::join(getAvailableDevices(), ","));
}
}; // namespace QuantExt
