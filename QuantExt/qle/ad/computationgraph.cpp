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

#include <qle/ad/computationgraph.hpp>

#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <boost/math/distributions/normal.hpp>

namespace QuantExt {

std::size_t ComputationGraph::nan = std::numeric_limits<std::size_t>::max();

void ComputationGraph::clear() {
    predecessors_.clear();
    opId_.clear();
    maxNodeRequiringArg_.clear();
    redBlockId_.clear();
    isConstant_.clear();
    constantValue_.clear();
    constants_.clear();
    variables_.clear();
    variableVersion_.clear();
    labels_.clear();
}

std::size_t ComputationGraph::size() const { return predecessors_.size(); }

std::size_t ComputationGraph::insert(const std::string& label) {
    std::size_t node = predecessors_.size();
    predecessors_.push_back(std::vector<std::size_t>());
    opId_.push_back(0);
    maxNodeRequiringArg_.push_back(0);
    redBlockId_.push_back(currentRedBlockId_);
    isConstant_.push_back(false);
    constantValue_.push_back(0.0);
    if (enableLabels_ && !label.empty())
        labels_[node].insert(label);
    return node;
}

std::size_t ComputationGraph::insert(const std::vector<std::size_t>& predecessors, const std::size_t opId,
                                     const std::string& label) {
    std::size_t node = predecessors_.size();
    predecessors_.push_back(predecessors);
    opId_.push_back(opId);
    for (auto const& p : predecessors) {
        maxNodeRequiringArg_[p] = node;
    }
    maxNodeRequiringArg_.push_back(0);
    redBlockId_.push_back(currentRedBlockId_);
    if (currentRedBlockId_ != 0) {
        for (auto const& p : predecessors) {
            if (redBlockId(p) != currentRedBlockId_) {
                redBlockDependencies_.insert(p);
            }
        }
    }
    isConstant_.push_back(false);
    constantValue_.push_back(0.0);
    if (enableLabels_ && !label.empty())
        labels_[node].insert(label);
    return node;
}

const std::vector<std::size_t>& ComputationGraph::predecessors(const std::size_t node) const {
    return predecessors_[node];
}

std::size_t ComputationGraph::opId(const std::size_t node) const { return opId_[node]; }

std::size_t ComputationGraph::maxNodeRequiringArg(const std::size_t node) const { return maxNodeRequiringArg_[node]; }

std::size_t ComputationGraph::constant(const double x) {
    auto c = constants_.find(x);
    if (c != constants_.end())
        return c->second;
    else {
        std::size_t node = predecessors_.size();
        constants_.insert(std::make_pair(x, node));
        predecessors_.push_back(std::vector<std::size_t>());
        opId_.push_back(0);
        maxNodeRequiringArg_.push_back(0);
        redBlockId_.push_back(currentRedBlockId_);
        isConstant_.push_back(true);
        constantValue_.push_back(x);
        if (enableLabels_)
            labels_[node].insert(std::to_string(x));
        return node;
    }
}

const std::map<double, std::size_t>& ComputationGraph::constants() const { return constants_; }

std::size_t ComputationGraph::variable(const std::string& name, const VarDoesntExist v) {
    auto c = variables_.find(name);
    if (c != variables_.end())
        return c->second;
    else if (v == VarDoesntExist::Create) {
        std::size_t node = predecessors_.size();
        variables_.insert(std::make_pair(name, node));
        variableVersion_[name] = 0;
        if (enableLabels_)
            labels_[node].insert(name + "(v" + std::to_string(++variableVersion_[name]) + ")");
        predecessors_.push_back(std::vector<std::size_t>());
        opId_.push_back(0);
        maxNodeRequiringArg_.push_back(0);
        redBlockId_.push_back(currentRedBlockId_);
        isConstant_.push_back(false);
        constantValue_.push_back(0.0);
        return node;
    } else if (v == VarDoesntExist::Nan) {
        return nan;
    } else if (v == VarDoesntExist::Throw) {
        QL_FAIL("ComputationGraph::variable(" << name << ") not found.");
    } else {
        QL_FAIL("ComputationGraph::variable(): internal error, VarDoesntExist enum '" << static_cast<int>(v)
                                                                                      << "' not covered.");
    }
}

const std::map<std::string, std::size_t>& ComputationGraph::variables() const { return variables_; }

void ComputationGraph::setVariable(const std::string& name, const std::size_t node) {
    auto v = variables_.find(name);
    if (v != variables_.end()) {
        if (v->second != node) {
            if (enableLabels_)
                labels_[node].insert(name + "(v" + std::to_string(++variableVersion_[name]) + ")");
            v->second = node;
        }
    } else {
        variableVersion_[name] = 0;
        if (enableLabels_)
            labels_[node].insert(name + "(v" + std::to_string(++variableVersion_[name]) + ")");
        variables_[name] = node;
    }
}

void ComputationGraph::enableLabels(const bool b) { enableLabels_ = b; }

const std::map<std::size_t, std::set<std::string>>& ComputationGraph::labels() const { return labels_; }

void ComputationGraph::startRedBlock() {
    currentRedBlockId_ = ++nextRedBlockId_;
    if (!redBlockRange_.empty())
        redBlockRange_.back().second = size();
    redBlockRange_.push_back(std::make_pair(size(), nan));
}

void ComputationGraph::endRedBlock() {
    QL_REQUIRE(currentRedBlockId_ > 0, "ComputationGraph::endRedBlock(): not in an active red block.");
    currentRedBlockId_ = 0;
    redBlockRange_.back().second = size();
}

const std::vector<std::pair<std::size_t, std::size_t>>& ComputationGraph::redBlockRanges() const {
    return redBlockRange_;
}

const std::set<std::size_t>& ComputationGraph::redBlockDependencies() const { return redBlockDependencies_; }

std::size_t ComputationGraph::redBlockId(const std::size_t node) const { return redBlockId_[node]; }

bool ComputationGraph::isConstant(const std::size_t node) const { return isConstant_[node]; }

double ComputationGraph::constantValue(const std::size_t node) const { return constantValue_[node]; }

std::size_t cg_const(ComputationGraph& g, const double value) { return g.constant(value); }

std::size_t cg_insert(ComputationGraph& g, const std::string& label) { return g.insert(label); }

std::size_t cg_var(ComputationGraph& g, const std::string& name, const ComputationGraph::VarDoesntExist v) {
    return g.variable(name, v);
}

std::size_t cg_add(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, g.constantValue(a) + g.constantValue(b));
    if (g.isConstant(a) && QuantLib::close_enough(g.constantValue(a), 0.0))
        return b;
    if (g.isConstant(b) && QuantLib::close_enough(g.constantValue(b), 0.0))
        return a;
    return g.insert({a, b}, RandomVariableOpCode::Add, label);
}

std::size_t cg_add(ComputationGraph& g, const std::vector<std::size_t>& a, const std::string& label) {
    if (a.size() == 2)
        return cg_add(g, a[0], a[1], label);
    return g.insert(a, RandomVariableOpCode::Add, label);
}

std::size_t cg_subtract(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (a == b)
        return cg_const(g, 0.0);
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, g.constantValue(a) - g.constantValue(b));
    if (g.isConstant(a) && QuantLib::close_enough(g.constantValue(a), 0.0))
        return cg_negative(g, b);
    if (g.isConstant(b) && QuantLib::close_enough(g.constantValue(b), 0.0))
        return a;
    return g.insert({a, b}, RandomVariableOpCode::Subtract, label);
}

std::size_t cg_negative(ComputationGraph& g, const std::size_t a, const std::string& label) {
    if (g.isConstant(a))
        return cg_const(g, -g.constantValue(a));
    return g.insert({a}, RandomVariableOpCode::Negative, label);
}

std::size_t cg_mult(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, g.constantValue(a) * g.constantValue(b));
    if (g.isConstant(a) && QuantLib::close_enough(g.constantValue(a), 1.0))
        return b;
    if (g.isConstant(b) && QuantLib::close_enough(g.constantValue(b), 1.0))
        return a;
    if ((g.isConstant(a) && QuantLib::close_enough(g.constantValue(a), 0.0)) ||
        (g.isConstant(b) && QuantLib::close_enough(g.constantValue(b), 0.0)))
        return cg_const(g, 0.0);
    return g.insert({a, b}, RandomVariableOpCode::Mult, label);
}

std::size_t cg_div(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (a == b)
        return cg_const(g, 1.0);
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, g.constantValue(a) / g.constantValue(b));
    if (g.isConstant(b) && QuantLib::close_enough(g.constantValue(b), 1.0))
        return a;
    if (g.isConstant(a) && QuantLib::close_enough(g.constantValue(a), 0.0))
        return cg_const(g, 0.0);
    return g.insert({a, b}, RandomVariableOpCode::Div, label);
}

std::size_t cg_conditionalExpectation(ComputationGraph& g, const std::size_t regressand,
                                      const std::vector<std::size_t>& regressor, const std::size_t filter,
                                      const std::string& label) {
    if (g.isConstant(regressand))
        return regressand;
    std::vector<size_t> args;
    args.push_back(regressand);
    args.push_back(filter);
    args.insert(args.end(), regressor.begin(), regressor.end());
    return g.insert(args, RandomVariableOpCode::ConditionalExpectation, label);
}

std::size_t cg_indicatorEq(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, QuantLib::close_enough(g.constantValue(a), g.constantValue(b)) ? 1.0 : 0.0);
    return g.insert({a, b}, RandomVariableOpCode::IndicatorEq, label);
}

std::size_t cg_indicatorGt(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, g.constantValue(a) > g.constantValue(b) &&
                                   !QuantLib::close_enough(g.constantValue(a), g.constantValue(b))
                               ? 1.0
                               : 0.0);
    return g.insert({a, b}, RandomVariableOpCode::IndicatorGt, label);
}

std::size_t cg_indicatorGeq(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, g.constantValue(a) > g.constantValue(b) ||
                                   QuantLib::close_enough(g.constantValue(a), g.constantValue(b))
                               ? 1.0
                               : 0.0);
    return g.insert({a, b}, RandomVariableOpCode::IndicatorGeq, label);
}

std::size_t cg_min(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, std::min(g.constantValue(a), g.constantValue(b)));
    return g.insert({a, b}, RandomVariableOpCode::Min, label);
}

std::size_t cg_max(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, std::max(g.constantValue(a), g.constantValue(b)));
    return g.insert({a, b}, RandomVariableOpCode::Max, label);
}

std::size_t cg_abs(ComputationGraph& g, const std::size_t a, const std::string& label) {
    if (g.isConstant(a))
        return cg_const(g, std::abs(g.constantValue(a)));
    return g.insert({a}, RandomVariableOpCode::Abs, label);
}

std::size_t cg_exp(ComputationGraph& g, const std::size_t a, const std::string& label) {
    if (g.isConstant(a))
        return cg_const(g, std::exp(g.constantValue(a)));
    return g.insert({a}, RandomVariableOpCode::Exp, label);
}

std::size_t cg_sqrt(ComputationGraph& g, const std::size_t a, const std::string& label) {
    if (g.isConstant(a))
        return cg_const(g, std::sqrt(g.constantValue(a)));
    return g.insert({a}, RandomVariableOpCode::Sqrt, label);
}

std::size_t cg_log(ComputationGraph& g, const std::size_t a, const std::string& label) {
    if (g.isConstant(a))
        return cg_const(g, std::log(g.constantValue(a)));
    return g.insert({a}, RandomVariableOpCode::Log, label);
}

std::size_t cg_pow(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label) {
    if (g.isConstant(a) && g.isConstant(b))
        return cg_const(g, std::pow(g.constantValue(a), g.constantValue(b)));
    return g.insert({a, b}, RandomVariableOpCode::Pow, label);
}

std::size_t cg_normalCdf(ComputationGraph& g, const std::size_t a, const std::string& label) {
    static const boost::math::normal_distribution<double> n;
    if (g.isConstant(a))
        return cg_const(g, boost::math::cdf(n, g.constantValue(a)));
    return g.insert({a}, RandomVariableOpCode::NormalCdf, label);
}

std::size_t cg_normalPdf(ComputationGraph& g, const std::size_t a, const std::string& label) {
    static const boost::math::normal_distribution<double> n;
    if (g.isConstant(a))
        return cg_const(g, boost::math::pdf(n, g.constantValue(a)));
    return g.insert({a}, RandomVariableOpCode::NormalPdf, label);
}

} // namespace QuantExt
