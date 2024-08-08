/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file qle/math/gpucodegenerator.hpp
    \brief gpu code generator utilities
*/

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace QuantExt {

class GpuCodeGenerator {
public:
    enum class VarType { input, rn, local };

    void initialize(const std::size_t nInputVars, const std::vector<bool> inputVarIsScalar, const std::size_t nVariates,
                    const std::size_t modelSize, const bool doublePrecision);
    std::size_t applyOperation(const std::size_t randomVariableOpCode, const std::vector<std::size_t>& args);
    void freeVariable(const std::size_t id);
    void declareOutputVariable(const std::size_t id);
    void finalize();

    bool initialized() const { return initialized_; }
    bool finalized() const { return finalized_; }

    const std::string& sourceCode() const { return sourceCode_; }
    const std::vector<std::string>& kernelNames() const { return kernelNames_; }
    std::size_t nInputVars() const { return nInputVars_; }
    const std::vector<bool>& inputVarIsScalar() const { return inputVarIsScalar_; }
    std::size_t inputBufferSize() const;
    std::size_t nVariates() const { return nVariates_; }
    std::size_t nLocalVars() const { return nLocalVars_; }

    // the conditional expectation vars and output vars are both guaranteed to be local vars
    const std::vector<std::vector<std::vector<std::pair<VarType, std::size_t>>>>& conditionalExpectationVars() const {
        return conditionalExpectationVars_;
    }
    const std::vector<std::pair<VarType, std::size_t>>& outputVars() const { return outputVars_; }

private:
    struct Operation {
        std::pair<VarType, std::size_t> lhs;
        std::vector<std::pair<VarType, std::size_t>> rhs;
        std::size_t randomVariableOpCode;
    };

    class LocalVarReplacement {
    public:
        LocalVarReplacement(std::size_t id) : id_(id) {}
        void setFirstLhsUse(std::size_t u) const { firstLhsUse_ = u; }
        void setFirstRhsUse(std::size_t u) const { firstRhsUse_ = u; }
        std::size_t id() const { return id_; }
        std::optional<std::size_t> firstLhsUse() const { return firstLhsUse_; }
        std::optional<std::size_t> firstRhsUse() const { return firstRhsUse_; }

    private:
        std::size_t id_;
        mutable std::optional<std::size_t> firstLhsUse_;
        mutable std::optional<std::size_t> firstRhsUse_;
    };

    friend bool operator<(const LocalVarReplacement, const LocalVarReplacement);

    std::pair<VarType, std::size_t> getVar(const std::size_t id) const;
    std::string getVarStr(const std::pair<VarType, const std::size_t>& var) const;
    std::size_t getId(const std::pair<VarType, const std::size_t>& var) const;
    std::size_t generateResultId();

    void generateBoilerplateCode();
    void determineKernelBreakLines();
    void determineLocalVarReplacements();
    void generateKernelStartCode();
    void generateKernelEndCode();
    void generateOutputVarAssignments();
    void generateOperationCode(const Operation& op);

    bool initialized_ = false;
    bool finalized_ = false;

    // inputs
    std::size_t nInputVars_;
    std::vector<bool> inputVarIsScalar_;
    std::size_t nVariates_;
    std::size_t modelSize_;
    bool doublePrecision_;

    // global state
    std::string fpTypeStr_;
    std::string fpEpsStr_;
    std::string fpSuffix_;
    std::vector<std::size_t> inputVarOffset_;

    // state during op application
    std::size_t nLocalVars_;
    std::vector<Operation> ops_;
    std::vector<std::size_t> freedVariables_;

    // state / result of finalize()
    std::size_t currentKernelNo_;
    std::vector<std::size_t> kernelBreakLines_;
    std::string outputVarAssignments_;
    std::string sourceCode_;
    std::vector<std::string> kernelNames_;
    std::vector<std::vector<std::vector<std::pair<VarType, std::size_t>>>> conditionalExpectationVars_;
    std::vector<std::pair<VarType, std::size_t>> outputVars_;

    std::vector<std::set<LocalVarReplacement>> localVarReplacements_;
};

bool operator<(const GpuCodeGenerator::LocalVarReplacement a, const GpuCodeGenerator::LocalVarReplacement b);

} // namespace QuantExt
