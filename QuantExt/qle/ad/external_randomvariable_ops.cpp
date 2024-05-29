/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/ad/external_randomvariable_ops.hpp>

#include <qle/math/computeenvironment.hpp>

namespace QuantExt {

ExternalRandomVariable::ExternalRandomVariable(std::size_t id) : initialized_(true), id_(id) {}

ExternalRandomVariable::ExternalRandomVariable(double v) : initialized_(true), v_(v) {
    id_ = ComputeEnvironment::instance().context().createInputVariable(v);
}

ExternalRandomVariable::ExternalRandomVariable(const std::size_t randomVariableOpCode,
                                               const std::vector<const ExternalRandomVariable*>& args) {
    std::vector<std::size_t> argIds(args.size());
    std::transform(args.begin(), args.end(), argIds.begin(), [](const ExternalRandomVariable* v) {
        QL_REQUIRE(v->initialised(),
                   "ExternalRandomVariable is not initialized, but used as an argument (internal error).");
        return v->id();
    });
    id_ = ComputeEnvironment::instance().context().applyOperation(randomVariableOpCode, argIds);
    initialized_ = true;
}

void ExternalRandomVariable::clear() {
    if (initialized_) {
        free();
        initialized_ = false;
    }
}

void ExternalRandomVariable::free() {
    if (initialized_ && !freed_) {
        ComputeEnvironment::instance().context().freeVariable(id_);
        freed_ = true;
    }
}

void ExternalRandomVariable::declareAsOutput() const {
    QL_REQUIRE(initialized_, "ExternalRandomVariable::declareAsOutput(): not initialized");
    ComputeEnvironment::instance().context().declareOutputVariable(id_);
}

std::size_t ExternalRandomVariable::id() const {
    QL_REQUIRE(initialized_, "ExternalRandomVariable::id(): not initialized");
    return id_;
}

std::function<void(ExternalRandomVariable&)> ExternalRandomVariable::preDeleter =
    std::function<void(ExternalRandomVariable&)>([](ExternalRandomVariable& x) { x.free(); });

std::function<void(ExternalRandomVariable&)> ExternalRandomVariable::deleter =
    std::function<void(ExternalRandomVariable&)>([](ExternalRandomVariable& x) { x.clear(); });

std::vector<ExternalRandomVariableOp> getExternalRandomVariableOps() {
    std::vector<ExternalRandomVariableOp> ops;

    // None = 0
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) -> ExternalRandomVariable {
        QL_FAIL("ExternRandomVariable does not support op None");
    });

    // Add = 1
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Add, args);
    });

    // Subtract = 2
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Subtract, args);
    });

    // Negative = 3
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Negative, args);
    });

    // Mult = 4
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Mult, args);
    });

    // Div = 5
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Div, args);
    });

    // ConditionalExpectation = 6
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::ConditionalExpectation, args);
    });

    // IndicatorEq = 7
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::IndicatorEq, args);
    });

    // IndicatorGt = 8
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::IndicatorGt, args);
    });

    // IndicatorGeq = 9
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::IndicatorGeq, args);
    });

    // Min = 10
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Min, args);
    });

    // Max = 11
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Max, args);
    });

    // Abs = 12
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Abs, args);
    });

    // Exp = 13
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Exp, args);
    });

    // Sqrt = 14
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Sqrt, args);
    });

    // Log = 15
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Log, args);
    });

    // Pow = 16
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::Pow, args);
    });

    // NormalCdf = 17
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::NormalCdf, args);
    });

    // NormalPdf = 18
    ops.push_back([](const std::vector<const ExternalRandomVariable*>& args) {
        return ExternalRandomVariable(RandomVariableOpCode::NormalPdf, args);
    });

    return ops;
}

std::vector<ExternalRandomVariableGrad> getExternalRandomVariableGradients() { return {}; }

std::ostream& operator<<(std::ostream& out, const ExternalRandomVariable& r) {
    out << (r.freed() ? "F" : ".") << (r.initialised() ? "I" : ".");
    return out;
}

} // namespace QuantExt
