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

/*! \file qle/ad/external_randomvariable_ops.hpp
    \brief ops for external randomvariables
*/

#pragma once

#include <qle/math/randomvariable_ops.hpp>

#include <ostream>

namespace QuantExt {

class ExternalRandomVariable {
public:
    ExternalRandomVariable() {}
    explicit ExternalRandomVariable(std::size_t id);
    explicit ExternalRandomVariable(double v);
    ExternalRandomVariable(const std::size_t randomVariableOpCode,
                           const std::vector<const ExternalRandomVariable*>& args);
    void clear();
    void free();
    bool initialised() const { return initialized_; }
    bool freed() const { return freed_; }
    void declareAsOutput() const;
    std::size_t id() const;

    static std::function<void(ExternalRandomVariable&)> preDeleter;
    static std::function<void(ExternalRandomVariable&)> deleter;

private:
    bool initialized_ = false;
    bool freed_ = false;
    double v_;
    std::size_t id_;
};

// is the given random variable deterministic and zero?
inline bool isDeterministicAndZero(const ExternalRandomVariable& x) {
    return false;
}

using ExternalRandomVariableOp =
    std::function<ExternalRandomVariable(const std::vector<const ExternalRandomVariable*>&)>;

std::vector<ExternalRandomVariableOp> getExternalRandomVariableOps();

using ExternalRandomVariableGrad = std::function<std::vector<ExternalRandomVariable>(
    const std::vector<const ExternalRandomVariable*>&, const ExternalRandomVariable*)>;

std::vector<ExternalRandomVariableGrad> getExternalRandomVariableGradients();

std::ostream& operator<<(std::ostream& out, const ExternalRandomVariable& a);

} // namespace QuantExt
