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

/*! \file qle/math/computeenvironment.hpp
    \brief interface to compute envs
*/

#pragma once

#include <qle/methods/multipathgeneratorbase.hpp>

#include <ql/patterns/singleton.hpp>

#include <boost/thread/shared_mutex.hpp>

#include <cstdint>
#include <set>

namespace QuantExt {

class ComputeContext;
class ComputeFramework;

class ComputeEnvironment : public QuantLib::Singleton<ComputeEnvironment> {
public:
    ComputeEnvironment();
    ~ComputeEnvironment();
    std::set<std::string> getAvailableDevices() const;
    bool hasContext() const;
    void selectContext(const std::string& deviceName);
    ComputeContext& context();
    void reset();

private:
    void releaseFrameworks();

    std::vector<ComputeFramework*> frameworks_;
    ComputeContext* currentContext_;
    std::string currentContextDeviceName_;
};

class ComputeFramework {
public:
    virtual ~ComputeFramework() {}
    virtual std::set<std::string> getAvailableDevices() const = 0;
    virtual ComputeContext* getContext(const std::string& deviceName) = 0;
};

class ComputeContext {
public:
    struct Settings {
        Settings()
            : debug(false), useDoublePrecision(false), rngSequenceType(QuantExt::SequenceType::MersenneTwister),
              rngSeed(42), regressionOrder(4) {}
        bool debug;
        bool useDoublePrecision;
        QuantExt::SequenceType rngSequenceType;
        std::size_t rngSeed;
        std::size_t regressionOrder;
    };

    struct DebugInfo {
        unsigned long numberOfOperations = 0;
        unsigned long nanoSecondsDataCopy = 0;
        unsigned long nanoSecondsProgramBuild = 0;
        unsigned long nanoSecondsCalculation = 0;
    };

    virtual ~ComputeContext() {}
    virtual void init() = 0;

    virtual std::pair<std::size_t, bool> initiateCalculation(const std::size_t n, const std::size_t id = 0,
                                                             const std::size_t version = 0,
                                                             const Settings settings = {}) = 0;
    virtual void disposeCalculation(const std::size_t id) = 0;

    virtual std::size_t createInputVariable(double v) = 0;
    virtual std::size_t createInputVariable(double* v) = 0;
    virtual std::vector<std::vector<std::size_t>> createInputVariates(const std::size_t dim,
                                                                      const std::size_t steps) = 0;

    virtual std::size_t applyOperation(const std::size_t randomVariableOpCode,
                                       const std::vector<std::size_t>& args) = 0;
    virtual void freeVariable(const std::size_t id) = 0;
    virtual void declareOutputVariable(const std::size_t id) = 0;

    virtual void finalizeCalculation(std::vector<double*>& output) = 0;

    // get device info

    virtual std::vector<std::pair<std::string, std::string>> deviceInfo() const { return {}; }
    virtual bool supportsDoublePrecision() const { return false; }

    // debug info

    virtual const DebugInfo& debugInfo() const = 0;

    // convenience methods

    void finalizeCalculation(std::vector<std::vector<double>>& output);
};

template <class T> T* createComputeFrameworkCreator() { return new T; }

class ComputeFrameworkRegistry : public QuantLib::Singleton<ComputeFrameworkRegistry, std::true_type> {
public:
    ComputeFrameworkRegistry() {}
    void add(const std::string& name, std::function<ComputeFramework*(void)> creator,
             const bool allowOverwrite = false);
    const std::vector<std::function<ComputeFramework*(void)>>& getAll() const;

private:
    mutable boost::shared_mutex mutex_;
    std::vector<std::string> names_;
    std::vector<std::function<ComputeFramework*(void)>> creators_;
};

} // namespace QuantExt
