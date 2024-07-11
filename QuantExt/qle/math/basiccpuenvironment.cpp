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

#include <qle/math/basiccpuenvironment.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_io.hpp>
#include <qle/math/randomvariable_opcodes.hpp>
#include <qle/math/randomvariable_ops.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

#include <ql/errors.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/models/marketmodels/browniangenerators/mtbrowniangenerator.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/timer/timer.hpp>

namespace QuantExt {

class BasicCpuContext : public ComputeContext {
public:
    BasicCpuContext();
    ~BasicCpuContext() override final;
    void init() override final;

    std::pair<std::size_t, bool> initiateCalculation(const std::size_t n, const std::size_t id = 0,
                                                     const std::size_t version = 0,
                                                     const Settings settings = {}) override final;
    void disposeCalculation(const std::size_t id) override final;
    std::size_t createInputVariable(double v) override final;
    std::size_t createInputVariable(double* v) override final;
    std::vector<std::vector<std::size_t>> createInputVariates(const std::size_t dim,
                                                              const std::size_t steps) override final;
    std::size_t applyOperation(const std::size_t randomVariableOpCode,
                               const std::vector<std::size_t>& args) override final;
    void freeVariable(const std::size_t id) override final;
    void declareOutputVariable(const std::size_t id) override final;
    void finalizeCalculation(std::vector<double*>& output) override final;

    bool supportsDoublePrecision() const override { return true; }

    const DebugInfo& debugInfo() const override final;

private:
    enum class ComputeState { idle, createInput, createVariates, calc };

    class program {
    public:
        program() {}
        void clear() {
            args_.clear();
            op_.clear();
            resultId_.clear();
        }
        std::size_t size() const { return args_.size(); }
        void add(std::size_t resultId, std::size_t op, const std::vector<std::size_t>& args) {
            args_.push_back(args);
            op_.push_back(op);
            resultId_.push_back(resultId);
        }
        const std::vector<std::size_t>& args(std::size_t i) const { return args_[i]; }
        const std::size_t op(std::size_t i) const { return op_[i]; }
        const std::size_t resultId(std::size_t i) const { return resultId_[i]; }

    private:
        std::vector<std::vector<std::size_t>> args_;
        std::vector<std::size_t> op_;
        std::vector<std::size_t> resultId_;
    };

    bool initialized_ = false;

    // will be accumulated over all calcs
    ComputeContext::DebugInfo debugInfo_;

    // 1a vectors per current calc id

    std::vector<std::size_t> size_;
    std::vector<std::size_t> version_;
    std::vector<bool> disposed_;
    std::vector<program> program_;
    std::vector<std::size_t> numberOfInputVars_;
    std::vector<std::size_t> numberOfVariates_;
    std::vector<std::size_t> numberOfVars_;
    std::vector<std::vector<std::size_t>> outputVars_;

    // 2 curent calc

    std::size_t currentId_ = 0;
    ComputeState currentState_ = ComputeState::idle;
    Settings settings_;
    bool newCalc_;

    std::vector<RandomVariable> values_;
    std::vector<std::size_t> freedVariables_;

    // shared random variates for all calcs

    std::unique_ptr<QuantLib::MersenneTwisterUniformRng> rng_;
    QuantLib::InverseCumulativeNormal icn_;
    std::vector<RandomVariable> variates_;
};

BasicCpuFramework::BasicCpuFramework() { contexts_["BasicCpu/Default/Default"] = new BasicCpuContext(); }

BasicCpuFramework::~BasicCpuFramework() {
    for (auto& [_, c] : contexts_) {
        delete c;
    }
}

BasicCpuContext::BasicCpuContext() : initialized_(false) {}

BasicCpuContext::~BasicCpuContext() {}

void BasicCpuContext::init() {

    if (initialized_) {
        return;
    }

    debugInfo_.numberOfOperations = 0;
    debugInfo_.nanoSecondsDataCopy = 0;
    debugInfo_.nanoSecondsProgramBuild = 0;
    debugInfo_.nanoSecondsCalculation = 0;

    initialized_ = true;
}

void BasicCpuContext::disposeCalculation(const std::size_t id) {
    QL_REQUIRE(!disposed_[id - 1], "BasicCpuContext::disposeCalculation(): id " << id << " was already disposed.");
    program_[id - 1].clear();
    disposed_[id - 1] = true;
}

std::pair<std::size_t, bool> BasicCpuContext::initiateCalculation(const std::size_t n, const std::size_t id,
                                                                  const std::size_t version, const Settings settings) {

    QL_REQUIRE(n > 0, "BasicCpuContext::initiateCalculation(): n must not be zero");

    newCalc_ = false;
    settings_ = settings;

    if (id == 0) {

        // initiate new calcaultion

        size_.push_back(n);
        version_.push_back(version);
        disposed_.push_back(false);
        program_.push_back(program());
        numberOfInputVars_.push_back(0);
        numberOfVariates_.push_back(0);
        numberOfVars_.push_back(0);
        outputVars_.push_back({});

        currentId_ = size_.size();
        newCalc_ = true;

    } else {

        // initiate calculation on existing id

        QL_REQUIRE(id <= size_.size(),
                   "BasicCpuContext::initiateCalculation(): id (" << id << ") invalid, got 1..." << size_.size());
        QL_REQUIRE(size_[id - 1] == n, "BasicCpuContext::initiateCalculation(): size ("
                                           << size_[id - 1] << ") for id " << id << " does not match current size ("
                                           << n << ")");
        QL_REQUIRE(!disposed_[id - 1], "BasicCpuContext::initiateCalculation(): id ("
                                           << id << ") was already disposed, it can not be used any more.");

        if (version != version_[id - 1]) {
            version_[id - 1] = version;
            program_[id - 1].clear();
            numberOfInputVars_[id - 1] = 0;
            numberOfVariates_[id - 1] = 0;
            numberOfVars_[id - 1] = 0;
            outputVars_[id - 1].clear();
            newCalc_ = true;
        }

        currentId_ = id;
    }

    // reset variables

    numberOfInputVars_[currentId_ - 1] = 0;

    values_.clear();
    if(newCalc_)
        freedVariables_.clear();

    // set state

    currentState_ = ComputeState::createInput;

    // return calc id

    return std::make_pair(currentId_, newCalc_);
}

std::size_t BasicCpuContext::createInputVariable(double v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "BasicCpuContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                    << ")");
    values_.push_back(RandomVariable(size_[currentId_ - 1], v));
    return numberOfInputVars_[currentId_ - 1]++;
}

std::size_t BasicCpuContext::createInputVariable(double* v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "BasicCpuContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                    << ")");
    values_.push_back(RandomVariable(size_[currentId_ - 1]));
    for (std::size_t i = 0; i < size_[currentId_ - 1]; ++i)
        values_.back().set(i, v[i]);
    return numberOfInputVars_[currentId_ - 1]++;
}

std::vector<std::vector<std::size_t>> BasicCpuContext::createInputVariates(const std::size_t dim,
                                                                           const std::size_t steps) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates,
               "BasicCpuContext::createInputVariates(): not in state createInput or createVariates ("
                   << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(currentId_ > 0, "BasicCpuContext::freeVariable(): current id is not set");
    QL_REQUIRE(newCalc_, "BasicCpuContext::createInputVariates(): id (" << currentId_ << ") in version "
                                                                        << version_[currentId_ - 1] << " is replayed.");
    currentState_ = ComputeState::createVariates;

    if (rng_ == nullptr) {
        rng_ = std::make_unique<MersenneTwisterUniformRng>(settings_.rngSeed);
    }

    if (variates_.size() < numberOfVariates_[currentId_ - 1] + dim * steps) {
        for (std::size_t i = variates_.size(); i < numberOfVariates_[currentId_ - 1] + dim * steps; ++i) {
            variates_.push_back(RandomVariable(size_[currentId_ - 1]));
            for (std::size_t j = 0; j < variates_.back().size(); ++j)
                variates_.back().set(j, icn_(rng_->nextReal()));
        }
    }

    std::vector<std::vector<std::size_t>> resultIds(dim, std::vector<std::size_t>(steps));
    for (std::size_t i = 0; i < dim; ++i) {
        for (std::size_t j = 0; j < steps; ++j) {
            resultIds[i][j] = numberOfInputVars_[currentId_ - 1] + numberOfVariates_[currentId_ - 1] + j * dim + i;
        }
    }

    numberOfVariates_[currentId_ - 1] += dim * steps;

    return resultIds;
}

std::size_t BasicCpuContext::applyOperation(const std::size_t randomVariableOpCode,
                                            const std::vector<std::size_t>& args) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates ||
                   currentState_ == ComputeState::calc,
               "BasicCpuContext::applyOperation(): not in state createInput or calc ("
                   << static_cast<int>(currentState_) << ")");
    currentState_ = ComputeState::calc;
    QL_REQUIRE(currentId_ > 0, "BasicCpuContext::applyOperation(): current id is not set");
    QL_REQUIRE(newCalc_, "BasicCpuContext::applyOperation(): id (" << currentId_ << ") in version "
                                                                   << version_[currentId_ - 1] << " is replayed.");

    // determine variable id to use for result

    std::size_t resultId;
    if (!freedVariables_.empty()) {
        resultId = freedVariables_.back();
        freedVariables_.pop_back();
    } else {
        resultId =
            numberOfInputVars_[currentId_ - 1] + numberOfVariates_[currentId_ - 1] + numberOfVars_[currentId_ - 1]++;
    }

    // store operation

    program_[currentId_ - 1].add(resultId, randomVariableOpCode, args);

    // update num of ops in debug info

    if (settings_.debug)
        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];

    // return result id

    return resultId;
}

void BasicCpuContext::freeVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ == ComputeState::calc,
               "BasicCpuContext::free(): not in state calc (" << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(currentId_ > 0, "BasicCpuContext::freeVariable(): current id is not set");
    QL_REQUIRE(newCalc_, "BasicCpuContext::freeVariable(): id (" << currentId_ << ") in version "
                                                                 << version_[currentId_ - 1] << " is replayed.");

    // we do not free variates, since they are shared

    if (id >= numberOfInputVars_[currentId_ - 1] &&
        id < numberOfInputVars_[currentId_ - 1] + numberOfVariates_[currentId_ - 1])
        return;

    freedVariables_.push_back(id);
}

void BasicCpuContext::declareOutputVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ != ComputeState::idle, "BasicCpuContext::declareOutputVariable(): state is idle");
    QL_REQUIRE(currentId_ > 0, "BasicCpuContext::declareOutputVariable(): current id not set");
    QL_REQUIRE(newCalc_, "BasicCpuContext::declareOutputVariable(): id ("
                             << currentId_ << ") in version " << version_[currentId_ - 1] << " is replayed.");
    outputVars_[currentId_ - 1].push_back(id);
}

void BasicCpuContext::finalizeCalculation(std::vector<double*>& output) {
    struct exitGuard {
        exitGuard() {}
        ~exitGuard() { *currentState = ComputeState::idle; }
        ComputeState* currentState;
    } guard;

    guard.currentState = &currentState_;

    QL_REQUIRE(currentId_ > 0, "BasicCpuContext::finalizeCalculation(): current id is not set");
    QL_REQUIRE(output.size() == outputVars_[currentId_ - 1].size(),
               "BasicCpuContext::finalizeCalculation(): output size ("
                   << output.size() << ") inconsistent to kernel output size (" << outputVars_[currentId_ - 1].size()
                   << ")");

    const auto& p = program_[currentId_ - 1];

    auto ops = getRandomVariableOps(size_[currentId_ - 1], settings_.regressionOrder);

    // resize values vector to required size

    values_.resize(numberOfInputVars_[currentId_ - 1] + numberOfVars_[currentId_ - 1]);

    // execute calculation

    for (Size i = 0; i < program_[currentId_ - 1].size(); ++i) {
        std::vector<const RandomVariable*> args(p.args(i).size());
        for (Size j = 0; j < p.args(i).size(); ++j) {
            if (p.args(i)[j] < numberOfInputVars_[currentId_ - 1])
                args[j] = &values_[p.args(i)[j]];
            else if (p.args(i)[j] < numberOfInputVars_[currentId_ - 1] + numberOfVariates_[currentId_ - 1])
                args[j] = &variates_[p.args(i)[j] - numberOfInputVars_[currentId_ - 1]];
            else
                args[j] = &values_[p.args(i)[j] - numberOfVariates_[currentId_ - 1]];
        }
        if (p.resultId(i) < numberOfInputVars_[currentId_ - 1])
            values_[p.resultId(i)] = ops[p.op(i)](args);
        else if (p.resultId(i) >= numberOfInputVars_[currentId_ - 1] + numberOfVariates_[currentId_ - 1])
            values_[p.resultId(i) - numberOfVariates_[currentId_ - 1]] = ops[p.op(i)](args);
        else {
            QL_FAIL("BasiCpuContext::finalizeCalculation(): internal error, result id "
                    << p.resultId(i) << " does not fall into values array.");
        }
    }

    // fill output

    for (Size i = 0; i < outputVars_[currentId_ - 1].size(); ++i) {
        std::size_t id = outputVars_[currentId_ - 1][i];
        RandomVariable* v;
        if (id < numberOfInputVars_[currentId_ - 1])
            v = &values_[id];
        else if (id < numberOfInputVars_[currentId_ - 1] + numberOfVariates_[currentId_ - 1]) {
            v = &variates_[id - numberOfInputVars_[currentId_ - 1]];
        }
        else
            v = &values_[id - numberOfVariates_[currentId_ - 1]];
        for (Size j = 0; j < size_[currentId_ - 1]; ++j) {
            output[i][j] = v->operator[](j);
        }
    }
}

const ComputeContext::DebugInfo& BasicCpuContext::debugInfo() const { return debugInfo_; }

std::set<std::string> BasicCpuFramework::getAvailableDevices() const { return {"BasicCpu/Default/Default"}; }

ComputeContext* BasicCpuFramework::getContext(const std::string& deviceName) {
    QL_REQUIRE(deviceName == "BasicCpu/Default/Default",
               "BasicCpuFramework::getContext(): device '"
                   << deviceName << "' not supported. Available device is 'BasicCpu/Default/Default'.");
    return contexts_[deviceName];
}

}; // namespace QuantExt
