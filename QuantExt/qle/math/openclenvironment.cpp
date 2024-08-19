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

#include <qle/math/gpucodegenerator.hpp>
#include <qle/math/openclenvironment.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/timer/timer.hpp>

#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

#define ORE_OPENCL_MAX_N_DEV_INFO 1024U
#define ORE_OPENCL_MAX_N_DEV_INFO_LARGE 65536U
#define ORE_OPENCL_MAX_BUILD_LOG 65536U
#define ORE_OPENCL_MAX_BUILD_LOG_LOGFILE 1024U

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

void errorCallback(const char* errinfo, const void* private_info, size_t cb, void* user_data) {
    printf("Callback from OpenCL context: errinfo = '%s'\n", errinfo);
}

} // namespace

class OpenClContext : public ComputeContext {
public:
    OpenClContext(cl_device_id* device, cl_context* context,
                  const std::vector<std::pair<std::string, std::string>>& deviceInfo,
                  const bool supportsDoublePrecision);
    ~OpenClContext() override final;
    void init() override final;

    std::pair<std::size_t, bool> initiateCalculation(const std::size_t n, const std::size_t id = 0,
                                                     const std::size_t version = 0,
                                                     const Settings settings = {}) override final;
    void disposeCalculation(const std::size_t n) override final;
    std::size_t createInputVariable(double v) override final;
    std::size_t createInputVariable(double* v) override final;
    std::vector<std::vector<std::size_t>> createInputVariates(const std::size_t dim,
                                                              const std::size_t steps) override final;
    std::size_t applyOperation(const std::size_t randomVariableOpCode,
                               const std::vector<std::size_t>& args) override final;
    void freeVariable(const std::size_t id) override final;
    void declareOutputVariable(const std::size_t id) override final;
    void finalizeCalculation(std::vector<double*>& output) override final;

    std::vector<std::pair<std::string, std::string>> deviceInfo() const override;
    bool supportsDoublePrecision() const override;
    const DebugInfo& debugInfo() const override final;

private:
    void updateVariatesPool();
    void initGpuCodeGenerator();
    void finalizeGpuCodeGenerator();
    void copyLocalValuesToHost(std::vector<cl_event>& runWaitEvents, cl_mem valuesBuffer, double* values,
                               const std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>>& vars) const;
    void copyLocalValuesToDevice(std::vector<cl_event>& runWaitEvents, cl_mem valuesBuffer, double* values,
                                 const std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>>& vars) const;

    void runHealthChecks();
    std::string runHealthCheckProgram(const std::string& source, const std::string& kernelName);

    static void releaseMem(cl_mem& m, const std::string& desc);
    static void releaseKernel(cl_kernel& k, const std::string& desc);
    static void releaseKernel(std::vector<cl_kernel>& ks, const std::string& desc);
    static void releaseProgram(cl_program& p, const std::string& desc);
    static void releaseProgram(std::vector<cl_program>& p, const std::string& desc);

    enum class ComputeState { idle, createInput, createVariates, calc, declareOutput };

    bool initialized_ = false;
    cl_device_id* device_;   // passed from framework
    cl_context* context_;    // passed from framework
    cl_command_queue queue_; // contructed per OpenClContext

    // set once in the ctor
    std::vector<std::pair<std::string, std::string>> deviceInfo_;
    bool supportsDoublePrecision_;

    // will be accumulated over all calcs
    ComputeContext::DebugInfo debugInfo_;

    // 1a vectors, indexed by calc id

    std::vector<std::size_t> size_;
    std::vector<bool> disposed_;
    std::vector<bool> hasKernel_;
    std::vector<std::size_t> version_;
    std::vector<std::vector<cl_program>> program_;
    std::vector<std::vector<cl_kernel>> kernel_;
    std::vector<GpuCodeGenerator> gpuCodeGenerator_;
    std::vector<std::size_t> numberOfOperations_;

    // 1b variates (shared pool of mersenne twister based normal variates)

    std::size_t variatesPoolSize_ = 0; // count of single random numbers
    cl_mem variatesPool_;
    cl_mem variatesMtStateBuffer_;
    cl_program variatesProgram_;
    cl_kernel variatesKernelSeedInit_;
    cl_kernel variatesKernelTwist_;
    cl_kernel variatesKernelGenerate_;

    // 2 curent calc

    std::size_t currentId_ = 0;
    ComputeState currentState_ = ComputeState::idle;
    std::size_t nVariates_;
    std::size_t nVars_;
    Settings settings_;

    // 2a indexed by var id
    std::vector<std::size_t> inputVarOffset_;
    std::vector<bool> inputVarIsScalar_;
    std::vector<float> inputVarValues32_;
    std::vector<double> inputVarValues64_;
};

bool OpenClFramework::initialized_ = false;

boost::shared_mutex OpenClFramework::mutex_;

cl_uint OpenClFramework::nPlatforms_ = 0;

std::string OpenClFramework::platformName_[ORE_OPENCL_MAX_N_PLATFORMS];

std::string OpenClFramework::deviceName_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];

cl_uint OpenClFramework::nDevices_[ORE_OPENCL_MAX_N_PLATFORMS];

cl_device_id OpenClFramework::devices_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];

cl_context OpenClFramework::context_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];

std::vector<std::pair<std::string, std::string>> OpenClFramework::deviceInfo_[ORE_OPENCL_MAX_N_PLATFORMS]
                                                                             [ORE_OPENCL_MAX_N_DEVICES];

bool OpenClFramework::supportsDoublePrecision_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];

void OpenClFramework::init() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    if (initialized_)
        return;

    initialized_ = true;

    cl_platform_id platforms[ORE_OPENCL_MAX_N_PLATFORMS];
    clGetPlatformIDs(ORE_OPENCL_MAX_N_PLATFORMS, platforms, &nPlatforms_);

    for (std::size_t p = 0; p < nPlatforms_; ++p) {
        char platformName[ORE_OPENCL_MAX_N_DEV_INFO];
        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, ORE_OPENCL_MAX_N_DEV_INFO, platformName, NULL);
        clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, ORE_OPENCL_MAX_N_DEVICES, devices_[p], &nDevices_[p]);

        platformName_[p] = std::string(platformName);

        for (std::size_t d = 0; d < nDevices_[p]; ++d) {
            char deviceName[ORE_OPENCL_MAX_N_DEV_INFO], driverVersion[ORE_OPENCL_MAX_N_DEV_INFO],
                deviceVersion[ORE_OPENCL_MAX_N_DEV_INFO], deviceExtensions[ORE_OPENCL_MAX_N_DEV_INFO_LARGE];

            clGetDeviceInfo(devices_[p][d], CL_DEVICE_NAME, ORE_OPENCL_MAX_N_DEV_INFO, &deviceName, NULL);
            clGetDeviceInfo(devices_[p][d], CL_DRIVER_VERSION, ORE_OPENCL_MAX_N_DEV_INFO, &driverVersion, NULL);
            clGetDeviceInfo(devices_[p][d], CL_DEVICE_VERSION, ORE_OPENCL_MAX_N_DEV_INFO, &deviceVersion, NULL);
            clGetDeviceInfo(devices_[p][d], CL_DEVICE_EXTENSIONS, ORE_OPENCL_MAX_N_DEV_INFO_LARGE, &deviceExtensions,
                            NULL);

            deviceInfo_[p][d].push_back(std::make_pair("device_name", std::string(deviceName)));
            deviceInfo_[p][d].push_back(std::make_pair("driver_version", std::string(driverVersion)));
            deviceInfo_[p][d].push_back(std::make_pair("device_version", std::string(deviceVersion)));
            deviceInfo_[p][d].push_back(std::make_pair("device_extensions", std::string(deviceExtensions)));

            deviceName_[p][d] = std::string(deviceName);

            supportsDoublePrecision_[p][d] = false;
#if CL_VERSION_1_2
            cl_device_fp_config doubleFpConfig;
            clGetDeviceInfo(devices_[p][d], CL_DEVICE_DOUBLE_FP_CONFIG, sizeof(cl_device_fp_config), &doubleFpConfig,
                            NULL);
            deviceInfo_[p][d].push_back(std::make_pair(
                "device_double_fp_config",
                ((doubleFpConfig & CL_FP_DENORM) ? std::string("Denorm,") : std::string()) +
                    ((doubleFpConfig & CL_FP_INF_NAN) ? std::string("InfNan,") : std::string()) +
                    ((doubleFpConfig & CL_FP_ROUND_TO_NEAREST) ? std::string("RoundNearest,") : std::string()) +
                    ((doubleFpConfig & CL_FP_ROUND_TO_ZERO) ? std::string("RoundZero,") : std::string()) +
                    ((doubleFpConfig & CL_FP_FMA) ? std::string("FMA,") : std::string()) +
                    ((doubleFpConfig & CL_FP_SOFT_FLOAT) ? std::string("SoftFloat,") : std::string())));
            supportsDoublePrecision_[p][d] = supportsDoublePrecision_[p][d] || (doubleFpConfig != 0);
#else
            deviceInfo_[p][d].push_back(std::make_pair("device_double_fp_config", "not provided before opencl 1.2"));
            supportsDoublePrecision_[p][d] =
                supportsDoublePrecision || std::string(deviceExtensions).find("cl_khr_fp64");
#endif

            // create context, this is static and will be deleted at program end (no clReleaseContext() necessary ?)

            cl_int err;
            context_[p][d] = clCreateContext(NULL, 1, &devices_[p][d], &errorCallback, NULL, &err);
            QL_REQUIRE(err == CL_SUCCESS,
                       "OpenClFramework::OpenClContext(): error during clCreateContext(): " << errorText(err));
        }
    }
}

OpenClFramework::OpenClFramework() {
    init();
    for (std::size_t p = 0; p < nPlatforms_; ++p) {
        for (std::size_t d = 0; d < nDevices_[p]; ++d) {
            contexts_["OpenCL/" + platformName_[p] + "/" + deviceName_[p][d]] =
                new OpenClContext(&devices_[p][d], &context_[p][d], deviceInfo_[p][d], supportsDoublePrecision_[p][d]);
        }
    }
}

OpenClFramework::~OpenClFramework() {
    for (auto& [_, c] : contexts_) {
        delete c;
    }
}

OpenClContext::OpenClContext(cl_device_id* device, cl_context* context,
                             const std::vector<std::pair<std::string, std::string>>& deviceInfo,
                             const bool supportsDoublePrecision)
    : initialized_(false), device_(device), context_(context), deviceInfo_(deviceInfo),
      supportsDoublePrecision_(supportsDoublePrecision) {}

OpenClContext::~OpenClContext() {
    if (initialized_) {
        if (variatesPoolSize_ > 0) {
            releaseMem(variatesPool_, "variates pool");
            releaseMem(variatesMtStateBuffer_, "variates state buffer");
            releaseKernel(variatesKernelSeedInit_, "variates seed init");
            releaseKernel(variatesKernelTwist_, "variates twist");
            releaseKernel(variatesKernelGenerate_, "variates generate");
            releaseProgram(variatesProgram_, "variates");
        }

        for (Size i = 0; i < kernel_.size(); ++i) {
            if (disposed_[i] || !hasKernel_[i])
                continue;
            releaseKernel(kernel_[i], "ore kernel");
        }

        for (Size i = 0; i < program_.size(); ++i) {
            if (disposed_[i] || !hasKernel_[i])
                continue;
            releaseProgram(program_[i], "ore program");
        }

        cl_int err;
        if (err = clReleaseCommandQueue(queue_); err != CL_SUCCESS) {
            std::cerr << "OpenClFramework: error during clReleaseCommandQueue: " + errorText(err) << std::endl;
        }
    }
}

void OpenClContext::releaseMem(cl_mem& m, const std::string& description) {
    cl_int err;
    if (err = clReleaseMemObject(m); err != CL_SUCCESS) {
        std::cerr << "OpenClContext: error during clReleaseMemObject '" << description << "': " + errorText(err)
                  << std::endl;
    }
}

void OpenClContext::releaseKernel(cl_kernel& k, const std::string& description) {
    cl_int err;
    if (err = clReleaseKernel(k); err != CL_SUCCESS) {
        std::cerr << "OpenClContext: error during clReleaseKernel '" << description << "': " + errorText(err)
                  << std::endl;
    }
}

void OpenClContext::releaseKernel(std::vector<cl_kernel>& ks, const std::string& description) {
    for (auto& k : ks)
        releaseKernel(k, description);
}

void OpenClContext::releaseProgram(cl_program& p, const std::string& description) {
    cl_int err;
    if (err = clReleaseProgram(p); err != CL_SUCCESS) {
        std::cerr << "OpenClContext: error during clReleaseProgram '" << description << "': " + errorText(err)
                  << std::endl;
    }
}

void OpenClContext::releaseProgram(std::vector<cl_program>& ps, const std::string& description) {
    for (auto& p : ps)
        releaseProgram(p, description);
}

std::string OpenClContext::runHealthCheckProgram(const std::string& source, const std::string& kernelName) {

    struct CleanUp {
        std::vector<cl_program> p;
        std::vector<cl_kernel> k;
        std::vector<cl_mem> m;
        ~CleanUp() {
            for (auto& pgm : p)
                OpenClContext::releaseProgram(pgm, "health check");
            for (auto& krn : k)
                OpenClContext::releaseKernel(krn, "health check");
            for (auto& mem : m)
                OpenClContext::releaseMem(mem, "health check");
        }
    } cleanup;

    const char* programPtr = source.c_str();

    cl_int err;

    cl_program program = clCreateProgramWithSource(*context_, 1, &programPtr, NULL, &err);
    if (err != CL_SUCCESS) {
        return errorText(err);
    }
    cleanup.p.push_back(program);

    err = clBuildProgram(program, 1, device_, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        return errorText(err);
    }

    cl_kernel kernel = clCreateKernel(program, kernelName.c_str(), &err);
    if (err != CL_SUCCESS) {
        return errorText(err);
    }
    cleanup.k.push_back(kernel);

    cl_mem resultBuffer = clCreateBuffer(*context_, CL_MEM_READ_WRITE, sizeof(cl_ulong), NULL, &err);
    if (err != CL_SUCCESS) {
        return errorText(err);
    }
    cleanup.m.push_back(resultBuffer);

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &resultBuffer);

    cl_event runEvent;
    constexpr std::size_t sizeOne = 1;
    err = clEnqueueNDRangeKernel(queue_, kernel, 1, NULL, &sizeOne, NULL, 0, NULL, &runEvent);
    if (err != CL_SUCCESS) {
        return errorText(err);
    }

    cl_ulong result;
    err = clEnqueueReadBuffer(queue_, resultBuffer, CL_TRUE, 0, sizeof(cl_ulong), &result, 1, &runEvent, NULL);
    if (err != CL_SUCCESS) {
        return errorText(err);
    }

    return std::to_string(result);
}

void OpenClContext::runHealthChecks() {
    deviceInfo_.push_back(std::make_pair("host_sizeof(cl_uint)", std::to_string(sizeof(cl_uint))));
    deviceInfo_.push_back(std::make_pair("host_sizeof(cl_ulong)", std::to_string(sizeof(cl_ulong))));
    deviceInfo_.push_back(std::make_pair("host_sizeof(cl_float)", std::to_string(sizeof(cl_float))));
    deviceInfo_.push_back(std::make_pair("host_sizeof(cl_double)", std::to_string(sizeof(cl_double))));

    std::string kernelGetUintSize =
        "__kernel void ore_get_uint_size(__global ulong* result) { result[0] = sizeof(uint); }";
    std::string kernelGetUlongSize =
        "__kernel void ore_get_ulong_size(__global ulong* result) { result[0] = sizeof(ulong); }";
    std::string kernelGetFloatSize =
        "__kernel void ore_get_float_size(__global ulong* result) { result[0] = sizeof(float); }";
    std::string kernelGetDoubleSize =
        "__kernel void ore_get_double_size(__global ulong* result) { result[0] = sizeof(double); }";

    deviceInfo_.push_back(
        std::make_pair("device_sizeof(uint)", runHealthCheckProgram(kernelGetUintSize, "ore_get_uint_size")));
    deviceInfo_.push_back(
        std::make_pair("device_sizeof(ulong)", runHealthCheckProgram(kernelGetUlongSize, "ore_get_ulong_size")));
    deviceInfo_.push_back(
        std::make_pair("device_sizeof(float)", runHealthCheckProgram(kernelGetFloatSize, "ore_get_float_size")));
    deviceInfo_.push_back(
        std::make_pair("device_sizeof(double)", runHealthCheckProgram(kernelGetDoubleSize, "ore_get_double_size")));
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
#if CL_VERSION_2_0
    queue_ = clCreateCommandQueueWithProperties(*context_, *device_, NULL, &err);
#else
    // deprecated in cl version 2_0
    queue_ = clCreateCommandQueue(*context_, *device_, 0, &err);
#endif
    QL_REQUIRE(err == CL_SUCCESS,
               "OpenClFramework::OpenClContext(): error during clCreateCommandQueue(): " << errorText(err));

    initialized_ = true;

    runHealthChecks();
}

void OpenClContext::disposeCalculation(const std::size_t id) {
    QL_REQUIRE(!disposed_[id - 1], "OpenClContext::disposeCalculation(): id " << id << " was already disposed.");
    disposed_[id - 1] = true;
    if (hasKernel_[id - 1]) {
        releaseKernel(kernel_[id - 1], "kernel id " + std::to_string(id) + " (during dispose())");
        releaseProgram(program_[id - 1], "program id " + std::to_string(id) + " (during dispose())");
    }
}

std::pair<std::size_t, bool> OpenClContext::initiateCalculation(const std::size_t n, const std::size_t id,
                                                                const std::size_t version, const Settings settings) {
    QL_REQUIRE(n > 0, "OpenClContext::initiateCalculation(): n must not be zero");

    bool newCalc = false;
    settings_ = settings;

    if (id == 0) {

        // initiate new calcaultion

        size_.push_back(n);
        disposed_.push_back(false);
        hasKernel_.push_back(false);
        version_.push_back(version);
        program_.push_back({});
        kernel_.push_back({});
        gpuCodeGenerator_.push_back({});
        numberOfOperations_.push_back(0);

        currentId_ = hasKernel_.size();
        newCalc = true;

    } else {

        // initiate calculation on existing id

        QL_REQUIRE(id <= hasKernel_.size(),
                   "OpenClContext::initiateCalculation(): id (" << id << ") invalid, got 1..." << hasKernel_.size());
        QL_REQUIRE(size_[id - 1] == n, "OpenClContext::initiateCalculation(): size ("
                                           << size_[id - 1] << ") for id " << id << " does not match current size ("
                                           << n << ")");
        QL_REQUIRE(!disposed_[id - 1], "OpenClContext::initiateCalculation(): id ("
                                           << id << ") was already disposed, it can not be used any more.");

        if (version != version_[id - 1]) {
            hasKernel_[id - 1] = false;
            version_[id - 1] = version;
            releaseKernel(kernel_[id - 1],
                          "kernel id " + std::to_string(id) + " (during initiateCalculation, old version: " +
                              std::to_string(version_[id - 1]) + ", new version:" + std::to_string(version) + ")");
            kernel_[id - 1].clear();
            releaseProgram(program_[id - 1],
                           "program id " + std::to_string(id) + " (during initiateCalculation, old version: " +
                               std::to_string(version_[id - 1]) + ", new version:" + std::to_string(version) + ")");
            program_[id - 1].clear();
            gpuCodeGenerator_[id - 1] = GpuCodeGenerator();
            numberOfOperations_[id - 1] = 0;
            newCalc = true;
        }

        currentId_ = id;
    }

    // reset variable info

    nVars_ = 0;
    nVariates_ = 0;

    inputVarOffset_.clear();
    inputVarIsScalar_.clear();
    inputVarValues32_.clear();
    inputVarValues64_.clear();

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
    if (settings_.useDoublePrecision) {
        inputVarValues64_.push_back(v);
    } else {
        // ensure that v falls into the single precision range
        inputVarValues32_.push_back((float)std::max(std::min(v, (double)std::numeric_limits<float>::max()),
                                                    -(double)std::numeric_limits<float>::max()));
    }
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
    for (std::size_t i = 0; i < size_[currentId_ - 1]; ++i) {
        if (settings_.useDoublePrecision) {
            inputVarValues64_.push_back(v[i]);
        } else {
            inputVarValues32_.push_back((float)std::max(std::min(v[i], (double)std::numeric_limits<float>::max()),
                                                        -(double)std::numeric_limits<float>::max()));
        }
    }
    return nVars_++;
}

void OpenClContext::updateVariatesPool() {
    QL_REQUIRE(nVariates_ > 0, "OpenClContext::updateVariatesPool(): internal error, got nVariates_ == 0.");

    constexpr std::size_t size_one = 1; // constant 1
    constexpr std::size_t mt_N = 624;   // mersenne twister N

    std::size_t fpSize = settings_.useDoublePrecision ? sizeof(double) : sizeof(float);

    QL_REQUIRE(!settings_.useDoublePrecision || supportsDoublePrecision(),
               "OpenClContext::updateVariatesPool(): double precision is configured for this calculation, but not "
               "supported by the device. Switch to single precision or use an appropriate device.");

    cl_event initEvent;
    if (variatesPoolSize_ == 0) {

        // build the kernels to fill the variates pool

        std::string fpTypeStr = settings_.useDoublePrecision ? "double" : "float";
        std::string fpSuffix = settings_.useDoublePrecision ? "" : "f";
        std::string fpMaxValue = settings_.useDoublePrecision ? "0x1.fffffffffffffp1023" : "0x1.fffffep127f";

        // clang-format off
        // ported from from QuantLib::InverseCumulativeNormal
        std::string sourceInvCumN = fpTypeStr + " ore_invCumN(const uint x0);\n" +
            fpTypeStr + " ore_invCumN(const uint x0) {\n"
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
            "        if (x0 == UINT_MAX) {\n"
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

        std::string kernelSourceSeedInit = "__kernel void ore_seedInitialization(const ulong s, __global ulong* mt) {\n"
            "  const ulong N = 624;\n"
            "  mt[0]= s & 0xffffffffUL;\n"
            "  for (ulong mti=1; mti<N; ++mti) {\n"
            "    mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);\n"
            "    mt[mti] &= 0xffffffffUL;\n"
            "  }\n"
            "}\n\n";

        std::string kernelSourceTwist = "__kernel void ore_twist(__global ulong* mt) {\n"
            " const ulong N = 624;\n"
            " const ulong M = 397;\n"
            " const ulong MATRIX_A = 0x9908b0dfUL;\n"
            " const ulong UPPER_MASK=0x80000000UL;\n"
            " const ulong LOWER_MASK=0x7fffffffUL;\n"
            " const ulong mag01[2]={0x0UL, MATRIX_A};\n"
            " ulong kk;\n"
            " ulong y;\n"
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
            "__kernel void ore_generate(const ulong offset, __global ulong* mt, __global " + fpTypeStr + "* output) {\n"
            "   ulong mti = get_global_id(0);\n"
            "   ulong y = mt[mti];\n"
            "   y ^= (y >> 11);\n"
            "   y ^= (y << 7) & 0x9d2c5680UL;\n"
            "   y ^= (y << 15) & 0xefc60000UL;\n"
            "   y ^= (y >> 18);\n"
            "   output[offset + mti] = ore_invCumN((uint)y);\n"
            "}\n\n";
        // clang-format on

        std::string programSource = sourceInvCumN + kernelSourceSeedInit + kernelSourceTwist + kernelSourceGenerate;

        // std::cerr << "generated variates program:\n" + programSource << std::endl;

        const char* programSourcePtr = programSource.c_str();
        cl_int err;
        variatesProgram_ = clCreateProgramWithSource(*context_, 1, &programSourcePtr, NULL, &err);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error creating program: " << errorText(err));
        err = clBuildProgram(variatesProgram_, 1, device_, NULL, NULL, NULL);
        if (err != CL_SUCCESS) {
            char buffer[ORE_OPENCL_MAX_BUILD_LOG];
            clGetProgramBuildInfo(variatesProgram_, *device_, CL_PROGRAM_BUILD_LOG,
                                  ORE_OPENCL_MAX_BUILD_LOG * sizeof(char), buffer, NULL);
            QL_FAIL("OpenClContext::updateVariatesPool(): error during program build: "
                    << errorText(err) << ": " << std::string(buffer).substr(0, ORE_OPENCL_MAX_BUILD_LOG_LOGFILE));
        }

        variatesKernelSeedInit_ = clCreateKernel(variatesProgram_, "ore_seedInitialization", &err);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error creating kernel seedInit: " << errorText(err));

        variatesKernelTwist_ = clCreateKernel(variatesProgram_, "ore_twist", &err);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error creating kernel twist: " << errorText(err));

        variatesKernelGenerate_ = clCreateKernel(variatesProgram_, "ore_generate", &err);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error creating kernel generate: " << errorText(err));

        variatesMtStateBuffer_ = clCreateBuffer(*context_, CL_MEM_READ_WRITE, sizeof(cl_ulong) * mt_N, NULL, &err);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error creating mt state buffer: " << errorText(err));

        cl_ulong tmpSeed = (cl_ulong)settings_.rngSeed;
        err = clSetKernelArg(variatesKernelSeedInit_, 0, sizeof(cl_ulong), &tmpSeed);
        err |= clSetKernelArg(variatesKernelSeedInit_, 1, sizeof(cl_mem), &variatesMtStateBuffer_);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error setting kernel args seed init: " << errorText(err));

        err = clEnqueueNDRangeKernel(queue_, variatesKernelSeedInit_, 1, NULL, &size_one, NULL, 0, NULL, &initEvent);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error running kernel seed init: " << errorText(err));
    }

    // if the variates pool is big enough, we exit early

    if (variatesPoolSize_ >= nVariates_ * size_[currentId_ - 1]) {
        if (variatesPoolSize_ == 0)
            clWaitForEvents(1, &initEvent);
        return;
    }

    // create new buffer to hold the variates and copy the current buffer contents to the new buffer

    Size alignedSize =
        624 * (nVariates_ * size_[currentId_ - 1] / 624 + (nVariates_ * size_[currentId_ - 1] % 624 == 0 ? 0 : 1));

    cl_int err;

    cl_mem oldBuffer = variatesPool_;

    struct OldBufferReleaser {
        OldBufferReleaser(cl_mem b, bool active) : b(b), active(active) {}
        ~OldBufferReleaser() {
            if (active)
                OpenClContext::releaseMem(b, "expired variates buffer");
        }
        cl_mem b;
        bool active;
    } oldBufferReleaser(oldBuffer, variatesPoolSize_ > 0);

    variatesPool_ = clCreateBuffer(*context_, CL_MEM_READ_WRITE, fpSize * alignedSize, NULL, &err);
    QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::updateVariatesPool(): error creating variates buffer with size "
                                      << fpSize * alignedSize << " bytes: " << errorText(err));
    cl_event copyEvent;
    if (variatesPoolSize_ > 0) {
        err = clEnqueueCopyBuffer(queue_, oldBuffer, variatesPool_, 0, 0, fpSize * variatesPoolSize_, 0, NULL,
                                  &copyEvent);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error copying existing variates buffer to new buffer: "
                       << errorText(err));
    }

    // fill in the new variates

    std::size_t currentPoolSize;
    cl_event generateEvent;
    bool haveGenerated = false;
    for (currentPoolSize = variatesPoolSize_; currentPoolSize < nVariates_ * size_[currentId_ - 1];
         currentPoolSize += mt_N) {
        err = clSetKernelArg(variatesKernelTwist_, 0, sizeof(cl_mem), &variatesMtStateBuffer_);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error setting args for kernel twist: " << errorText(err));
        cl_event twistEvent;
        err = clEnqueueNDRangeKernel(
            queue_, variatesKernelTwist_, 1, NULL, &size_one, NULL, variatesPoolSize_ == 0 || haveGenerated ? 1 : 0,
            variatesPoolSize_ == 0 ? &initEvent : (haveGenerated ? &generateEvent : NULL), &twistEvent);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error running kernel twist: " << errorText(err));

        err = clSetKernelArg(variatesKernelGenerate_, 0, sizeof(cl_ulong), &currentPoolSize);
        err |= clSetKernelArg(variatesKernelGenerate_, 1, sizeof(cl_mem), &variatesMtStateBuffer_);
        err |= clSetKernelArg(variatesKernelGenerate_, 2, sizeof(cl_mem), &variatesPool_);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error settings args for kernel generate: " << errorText(err));
        err = clEnqueueNDRangeKernel(queue_, variatesKernelGenerate_, 1, NULL, &mt_N, NULL, 1, &twistEvent,
                                     &generateEvent);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::updateVariatesPool(): error running kernel generate: " << errorText(err));
        haveGenerated = true;
    }

    // wait for events to finish

    std::vector<cl_event> waitList;
    if (variatesPoolSize_ > 0)
        waitList.push_back(copyEvent);
    if (haveGenerated)
        waitList.push_back(generateEvent);
    if (!waitList.empty())
        clWaitForEvents(waitList.size(), &waitList[0]);

    // update current variates pool size

    QL_REQUIRE(currentPoolSize == alignedSize, "OpenClContext::updateVariatesPool(): internal error, currentPoolSize = "
                                                   << currentPoolSize << " does not match alignedSize " << alignedSize);
    variatesPoolSize_ = currentPoolSize;
}

std::vector<std::vector<std::size_t>> OpenClContext::createInputVariates(const std::size_t dim,
                                                                         const std::size_t steps) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates,
               "OpenClContext::createInputVariates(): not in state createInput or createVariates ("
                   << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(currentId_ > 0, "OpenClContext::applyOperation(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "OpenClContext::createInputVariates(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, input variates can not be regenerated.");
    currentState_ = ComputeState::createVariates;
    std::vector<std::vector<std::size_t>> resultIds(dim, std::vector<std::size_t>(steps));
    for (std::size_t j = 0; j < steps; ++j) {
        for (std::size_t i = 0; i < dim; ++i) {
            resultIds[i][j] = nVars_++;
        }
    }
    nVariates_ += dim * steps;
    updateVariatesPool();
    return resultIds;
}

void OpenClContext::initGpuCodeGenerator() {
    if (!gpuCodeGenerator_[currentId_ - 1].initialized())
        gpuCodeGenerator_[currentId_ - 1].initialize(inputVarIsScalar_.size(), inputVarIsScalar_, nVariates_,
                                                     size_[currentId_ - 1], settings_.useDoublePrecision);
}

void OpenClContext::finalizeGpuCodeGenerator() {
    if (!gpuCodeGenerator_[currentId_ - 1].finalized())
        gpuCodeGenerator_[currentId_ - 1].finalize();
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
    initGpuCodeGenerator();

    if (settings_.debug)
        numberOfOperations_[currentId_ - 1] += size_[currentId_ - 1];

    return gpuCodeGenerator_[currentId_ - 1].applyOperation(randomVariableOpCode, args);
}

void OpenClContext::freeVariable(const std::size_t id) {
    QL_REQUIRE(currentId_ > 0, "OpenClContext::freeVariable(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "OpenClContext::freeVariable(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, variables can not be freed.");
    initGpuCodeGenerator();
    gpuCodeGenerator_[currentId_ - 1].freeVariable(id);
}

void OpenClContext::declareOutputVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ != ComputeState::idle, "OpenClContext::declareOutputVariable(): state is idle");
    QL_REQUIRE(currentId_ > 0, "OpenClContext::declareOutputVariable(): current id not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "OpenClContext::declareOutputVariable(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, output variables can not be declared.");
    currentState_ = ComputeState::declareOutput;
    initGpuCodeGenerator();
    gpuCodeGenerator_[currentId_ - 1].declareOutputVariable(id);
}

void OpenClContext::finalizeCalculation(std::vector<double*>& output) {

    // clean up tasks on exit

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

    initGpuCodeGenerator();
    finalizeGpuCodeGenerator();

    // perform some checks

    QL_REQUIRE(currentId_ > 0, "OpenClContext::finalizeCalculation(): current id is not set");
    QL_REQUIRE(output.size() == gpuCodeGenerator_[currentId_ - 1].outputVars().size(),
               "OpenClContext::finalizeCalculation(): output size ("
                   << output.size() << ") inconsistent to kernel output size ("
                   << gpuCodeGenerator_[currentId_ - 1].outputVars().size() << ")");
    QL_REQUIRE(!settings_.useDoublePrecision || supportsDoublePrecision(),
               "OpenClContext::finalizeCalculation(): double precision is configured for this calculation, but not "
               "supported by the device. Switch to single precision or use an appropriate device.");

    // create input and values buffers on the device

    boost::timer::cpu_timer timer;
    boost::timer::nanosecond_type timerBase;

    std::size_t fpSize = settings_.useDoublePrecision ? sizeof(double) : sizeof(float);

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }

    std::size_t inputBufferSize = gpuCodeGenerator_[currentId_ - 1].inputBufferSize();
    cl_int err;
    cl_mem inputBuffer;
    if (inputBufferSize > 0) {
        inputBuffer = clCreateBuffer(*context_, CL_MEM_READ_WRITE, fpSize * inputBufferSize, NULL, &err);
        guard.mem.push_back(inputBuffer);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::finalizeCalculation(): creating input buffer of size "
                                          << inputBufferSize << " fails: " << errorText(err));
    }

    cl_mem valuesBuffer;
    if (gpuCodeGenerator_[currentId_ - 1].nBufferedLocalVars() > 0) {
        // std::cerr << "creating values buffer "
        //           << gpuCodeGenerator_[currentId_ - 1].nBufferedLocalVars() * size_[currentId_ - 1] * fpSize / 1024.0
        //           /
        //                  1024.0
        //           << " MB" << std::endl;
        valuesBuffer = clCreateBuffer(
            *context_, CL_MEM_READ_WRITE,
            fpSize * gpuCodeGenerator_[currentId_ - 1].nBufferedLocalVars() * size_[currentId_ - 1], NULL, &err);
        guard.mem.push_back(valuesBuffer);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::finalizeCalculation(): creating values buffer of size "
                       << gpuCodeGenerator_[currentId_ - 1].nBufferedLocalVars() * size_[currentId_ - 1]
                       << " fails: " << errorText(err));
    }

    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // build kernel if this is a new calc id

    if (!hasKernel_[currentId_ - 1]) {
        // std::cout << "generated kernel code: " << gpuCodeGenerator_[currentId_ - 1].sourceCode() << std::endl;
        // std::cerr << "generated kernel code: " << gpuCodeGenerator_[currentId_ - 1].sourceCodeSize() / 1024.0 / 1024.0
        //           << "MB, " << gpuCodeGenerator_[currentId_ - 1].kernelNames().size() << " parts" << std::endl;

        cl_int err;
        std::size_t part = 0;
        for (auto const& s : gpuCodeGenerator_[currentId_ - 1].sourceCode()) {
            const char* kernelSourcePtr = s.c_str();
            program_[currentId_ - 1].push_back(clCreateProgramWithSource(*context_, 1, &kernelSourcePtr, NULL, &err));
            QL_REQUIRE(err == CL_SUCCESS,
                       "OpenClContext::finalizeCalculation(): error during clCreateProgramWithSource() for part "
                           << part << ": " << errorText(err));
            err = clBuildProgram(program_[currentId_ - 1][part], 1, device_, NULL, NULL, NULL);
            if (err != CL_SUCCESS) {
                char buffer[ORE_OPENCL_MAX_BUILD_LOG];
                clGetProgramBuildInfo(program_[currentId_ - 1][part], *device_, CL_PROGRAM_BUILD_LOG,
                                      ORE_OPENCL_MAX_BUILD_LOG * sizeof(char), buffer, NULL);
                QL_FAIL("OpenClContext::finalizeCalculation(): error during program build for kernel '"
                        << gpuCodeGenerator_[currentId_ - 1].kernelNames()[part] << "': " << errorText(err) << ": "
                        << std::string(buffer).substr(0, ORE_OPENCL_MAX_BUILD_LOG_LOGFILE));
            }
            ++part;
        }

        part = 0;
        for (auto const& kernelName : gpuCodeGenerator_[currentId_ - 1].kernelNames()) {
            kernel_[currentId_ - 1].push_back(clCreateKernel(program_[currentId_ - 1][part], kernelName.c_str(), &err));
            QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::finalizeCalculation(): error during clCreateKernel() ("
                                              << kernelName << "): " << errorText(err));
            ++part;
        }

        hasKernel_[currentId_ - 1] = true;

        if (settings_.debug) {
            debugInfo_.nanoSecondsProgramBuild += timer.elapsed().wall - timerBase;
        }
    }

    // write input data to input buffer (asynchronously)

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }

    cl_event inputBufferEvent;
    if (inputBufferSize > 0) {
        err = clEnqueueWriteBuffer(queue_, inputBuffer, CL_FALSE, 0, fpSize * inputBufferSize,
                                   settings_.useDoublePrecision ? (void*)&inputVarValues64_[0]
                                                                : (void*)&inputVarValues32_[0],
                                   0, NULL, &inputBufferEvent);
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::finalizeCalculation(): writing to input buffer fails: " << errorText(err));
    }

    if (settings_.debug) {
        err = clFinish(queue_);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::clFinish(): error in debug mode: " << errorText(err));
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // create the value buffer on the host

    std::vector<cl_event> runWaitEvents;
    if (inputBufferSize > 0)
        runWaitEvents.push_back(inputBufferEvent);

    std::vector<double> values(gpuCodeGenerator_[currentId_ - 1].nBufferedLocalVars() * size_[currentId_ - 1]);
    std::set<std::pair<GpuCodeGenerator::VarType, std::size_t>> varsAlreadyOnHost;

    for (std::size_t part = 0; part < gpuCodeGenerator_[currentId_ - 1].kernelNames().size(); ++part) {

        // set kernel args

        std::size_t kidx = 0;
        err = 0;
        if (inputBufferSize > 0) {
            err |= clSetKernelArg(kernel_[currentId_ - 1][part], kidx++, sizeof(cl_mem), &inputBuffer);
        }
        if (gpuCodeGenerator_[currentId_ - 1].nVariates() > 0) {
            err |= clSetKernelArg(kernel_[currentId_ - 1][part], kidx++, sizeof(cl_mem), &variatesPool_);
        }
        if (gpuCodeGenerator_[currentId_ - 1].nBufferedLocalVars() > 0) {
            err |= clSetKernelArg(kernel_[currentId_ - 1][part], kidx++, sizeof(cl_mem), &valuesBuffer);
        }
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::finalizeCalculation(): set kernel args fails: " << errorText(err));

        // execute kernel

        if (settings_.debug) {
            err = clFinish(queue_);
            timerBase = timer.elapsed().wall;
        }

        cl_event runEvent;
        err = clEnqueueNDRangeKernel(queue_, kernel_[currentId_ - 1][part], 1, NULL, &size_[currentId_ - 1], NULL,
                                     runWaitEvents.size(), runWaitEvents.empty() ? NULL : &runWaitEvents[0], &runEvent);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::finalizeCalculation(): enqueue kernel fails: " << errorText(err));
        runWaitEvents.push_back(runEvent);

        /* calculate conditional expectations, this is the variant where we do this on the host */

        if (!gpuCodeGenerator_[currentId_ - 1].conditionalExpectationVars()[part].empty()) {

            std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>> tmp;
            for (auto const& l : gpuCodeGenerator_[currentId_ - 1].conditionalExpectationVars()[part])
                tmp.insert(tmp.end(), l.begin(), l.end());
            copyLocalValuesToHost(runWaitEvents, valuesBuffer, &values[0], tmp);

            std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>> updatedVars;
            for (auto const& v : gpuCodeGenerator_[currentId_ - 1].conditionalExpectationVars()[part]) {

                // calculate conditional expectation value

                QL_REQUIRE(v.size() >= 3,
                           "OpenClContext::finalizeCalculation(): expected at least 3 varIds (2 args and 1 result) for "
                           "conditional expectation, got "
                               << v.size());

                RandomVariable ce;
                RandomVariable regressand(size_[currentId_ - 1],
                                          &values[gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v[1].second) *
                                                  size_[currentId_ - 1]]);
                if (v.size() < 4) {
                    // no regressor given -> take plain expectation
                    ce = expectation(regressand);
                } else {
                    Filter filter = close_enough(
                        RandomVariable(size_[currentId_ - 1],
                                       &values[gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v[2].second) *
                                               size_[currentId_ - 1]]),
                        RandomVariable(size_[currentId_ - 1], 1.0));
                    std::vector<RandomVariable> regressor(v.size() - 3);
                    for (std::size_t i = 3; i < v.size(); ++i) {
                        regressor[i - 3] =
                            RandomVariable(size_[currentId_ - 1],
                                           &values[gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v[i].second) *
                                                   size_[currentId_ - 1]]);
                    }

                    ce = conditionalExpectation(regressand, vec2vecptr(regressor),
                                                multiPathBasisSystem(regressor.size(), settings_.regressionOrder,
                                                                     QuantLib::LsmBasisSystem::Monomial,
                                                                     regressand.size()),
                                                filter);
                }

                // overwrite the value

                ce.expand();
                std::copy(ce.data(), ce.data() + ce.size(),
                          &values[gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v[0].second) *
                                  size_[currentId_ - 1]]);

                updatedVars.push_back(v[0]);
            }

            if (part < gpuCodeGenerator_[currentId_ - 1].kernelNames().size() - 1) {
                copyLocalValuesToDevice(runWaitEvents, valuesBuffer, &values[0], updatedVars);
            } else {
                varsAlreadyOnHost.insert(updatedVars.begin(), updatedVars.end());
            }

        } // if conditional expectation to be calculated

        if (settings_.debug) {
            err = clFinish(queue_);
            QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::clFinish(): error in debug mode: " << errorText(err));
            debugInfo_.nanoSecondsCalculation += timer.elapsed().wall - timerBase;
        }

    } // for part (execute kernel part)

    if (settings_.debug) {
        debugInfo_.numberOfOperations += numberOfOperations_[currentId_ - 1];
    }

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }

    // populate the output

    if (!output.empty()) {

        std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>> outputVarsMinusVarsAlreadyOnHost;
        for (auto const& o : gpuCodeGenerator_[currentId_ - 1].outputVars()) {
            if (varsAlreadyOnHost.find(o) == varsAlreadyOnHost.end())
                outputVarsMinusVarsAlreadyOnHost.push_back(o);
        }

        copyLocalValuesToHost(runWaitEvents, valuesBuffer, &values[0], outputVarsMinusVarsAlreadyOnHost);

        for (std::size_t i = 0; i < output.size(); ++i) {
            std::size_t offset = gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(
                                     gpuCodeGenerator_[currentId_ - 1].outputVars()[i].second) *
                                 size_[currentId_ - 1];
            std::copy(&values[offset], &values[offset + size_[currentId_ - 1]], output[i]);
        }
    }

    if (settings_.debug) {
        err = clFinish(queue_);
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::clFinish(): error in debug mode: " << errorText(err));
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }
}

void OpenClContext::copyLocalValuesToHost(
    std::vector<cl_event>& runWaitEvents, cl_mem valuesBuffer, double* values,
    const std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>>& vars) const {

    if (vars.empty())
        return;

    std::vector<cl_event> readEvents;
    std::size_t fpSize = settings_.useDoublePrecision ? sizeof(double) : sizeof(float);

    std::vector<float> valuesFloat;
    if (!settings_.useDoublePrecision) {
        valuesFloat.resize(vars.size() * size_[currentId_ - 1]);
    }

    std::size_t counter = 0;
    for (auto const& v : vars) {
        readEvents.push_back(cl_event());
        std::size_t bid = gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v.second);
        cl_int err = clEnqueueReadBuffer(
            queue_, valuesBuffer, CL_FALSE, fpSize * bid * size_[currentId_ - 1], fpSize * size_[currentId_ - 1],
            settings_.useDoublePrecision ? (void*)&values[bid * size_[currentId_ - 1]]
                                         : (void*)&valuesFloat[counter * size_[currentId_ - 1]],
            runWaitEvents.size(), runWaitEvents.empty() ? NULL : &runWaitEvents[0], &readEvents.back());
        QL_REQUIRE(err == CL_SUCCESS, "OpenClContext::copyLocalValuesToHost() fails: " << errorText(err));
        ++counter;
    }

    cl_int err = clWaitForEvents(readEvents.size(), &readEvents[0]);
    QL_REQUIRE(
        err == CL_SUCCESS,
        "OpenClContext::copyLocalValuesToHost() fails: wait for read values buffer event fails: " << errorText(err));

    if (!settings_.useDoublePrecision) {
        std::size_t counter = 0;
        for (auto const& v : vars) {
            std::size_t bid = gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v.second);
            std::copy(&valuesFloat[counter * size_[currentId_ - 1]],
                      &valuesFloat[(counter + 1) * size_[currentId_ - 1]], &values[bid * size_[currentId_ - 1]]);
            ++counter;
        }
    }
}

void OpenClContext::copyLocalValuesToDevice(
    std::vector<cl_event>& runWaitEvents, cl_mem valuesBuffer, double* values,
    const std::vector<std::pair<GpuCodeGenerator::VarType, std::size_t>>& vars) const {

    if (vars.empty())
        return;

    std::size_t fpSize = settings_.useDoublePrecision ? sizeof(double) : sizeof(float);

    std::vector<float> valuesFloat;
    if (!settings_.useDoublePrecision) {
        valuesFloat.resize(vars.size() * size_[currentId_ - 1]);
        std::size_t counter = 0;
        for (auto const& v : vars) {
            std::size_t bid = gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v.second);
            std::copy(&values[bid * size_[currentId_ - 1]], &values[(bid + 1) * size_[currentId_ - 1]],
                      &valuesFloat[counter * size_[currentId_ - 1]]);
            ++counter;
        }
    }

    std::size_t counter = 0;
    for (auto const& v : vars) {
        std::size_t bid = gpuCodeGenerator_[currentId_ - 1].bufferedLocalVarMap(v.second);
        runWaitEvents.push_back(cl_event());
        cl_int err = clEnqueueWriteBuffer(
            queue_, valuesBuffer, CL_FALSE, fpSize * bid * size_[currentId_ - 1], fpSize * size_[currentId_ - 1],
            settings_.useDoublePrecision ? (void*)&values[bid * size_[currentId_ - 1]]
                                         : (void*)&valuesFloat[counter * size_[currentId_ - 1]],
            0, NULL, &runWaitEvents.back());
        QL_REQUIRE(err == CL_SUCCESS,
                   "OpenClContext::copyLocalValuesToDevice(): write values buffer fails: " << errorText(err));
        ++counter;
    }
}

const ComputeContext::DebugInfo& OpenClContext::debugInfo() const { return debugInfo_; }

std::vector<std::pair<std::string, std::string>> OpenClContext::deviceInfo() const { return deviceInfo_; }
bool OpenClContext::supportsDoublePrecision() const { return supportsDoublePrecision_; }

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
