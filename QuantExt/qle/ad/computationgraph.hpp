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

/*! \file qle/ad/computationgraph.hpp
    \brief computation graph
*/

#pragma once

#include <boost/integer.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace QuantExt {

/*! - opId = 0 should refer to "no operation" */
class ComputationGraph {
public:
    enum class VarDoesntExist { Nan, Create, Throw };
    static std::size_t nan;

    void clear();

    std::size_t size() const;
    std::size_t insert(const std::string& label = std::string());
    std::size_t insert(const std::vector<std::size_t>& predecessors, const std::size_t opId,
                       const std::string& label = std::string());
    const std::vector<std::size_t>& predecessors(const std::size_t node) const;
    std::size_t opId(const std::size_t node) const;

    std::size_t maxNodeRequiringArg(const std::size_t node) const;

    std::size_t constant(const double c);
    const std::map<double, std::size_t>& constants() const;
    bool isConstant(const std::size_t node) const;
    double constantValue(const std::size_t node) const;

    std::size_t variable(const std::string& name, const VarDoesntExist v = VarDoesntExist::Throw);
    const std::map<std::string, std::size_t>& variables() const;
    void setVariable(const std::string& name, const std::size_t node);

    void enableLabels(const bool b = true);
    const std::map<std::size_t, std::set<std::string>>& labels() const;

    void startRedBlock();
    void endRedBlock();
    std::size_t redBlockId(const std::size_t node) const;
    const std::vector<std::pair<std::size_t, std::size_t>>& redBlockRanges() const;
    const std::set<std::size_t>& redBlockDependencies() const;

private:
    std::vector<std::vector<std::size_t>> predecessors_;
    std::vector<std::size_t> opId_;
    std::vector<bool> isConstant_;
    std::vector<double> constantValue_;
    std::vector<std::size_t> maxNodeRequiringArg_;
    std::vector<std::size_t> redBlockId_;

    std::map<double, std::size_t> constants_;

    std::map<std::string, std::size_t> variables_;
    std::map<std::string, std::size_t> variableVersion_;

    bool enableLabels_ = false;
    std::map<std::size_t, std::set<std::string>> labels_;

    std::size_t currentRedBlockId_ = 0;
    std::size_t nextRedBlockId_ = 0;
    std::vector<std::pair<std::size_t, std::size_t>> redBlockRange_;
    std::set<std::size_t> redBlockDependencies_;
};

// methods to construct cg

std::size_t cg_const(ComputationGraph& g, const double value);
std::size_t cg_insert(ComputationGraph& g, const std::string& label = std::string());
std::size_t cg_var(ComputationGraph& g, const std::string& name,
                   ComputationGraph::VarDoesntExist = ComputationGraph::VarDoesntExist::Throw);
std::size_t cg_add(ComputationGraph& g, const std::size_t a, const std::size_t b,
                   const std::string& label = std::string());
std::size_t cg_add(ComputationGraph& g, const std::vector<std::size_t>& a, const std::string& label = std::string());
std::size_t cg_subtract(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label = std::string());
std::size_t cg_negative(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
std::size_t cg_mult(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label = std::string());
std::size_t cg_div(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label = std::string());
std::size_t cg_conditionalExpectation(ComputationGraph& g, const std::size_t regressand,
                                      const std::vector<std::size_t>& regressor, const std::size_t filter,
                                      const std::string& label = std::string());
std::size_t cg_indicatorEq(ComputationGraph& g, const std::size_t a, const std::size_t b,
                           const std::string& label = std::string());
std::size_t cg_indicatorGt(ComputationGraph& g, const std::size_t a, const std::size_t b,
                           const std::string& label = std::string());
std::size_t cg_indicatorGeq(ComputationGraph& g, const std::size_t a, const std::size_t b,
                            const std::string& label = std::string());
std::size_t cg_min(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label = std::string());
std::size_t cg_max(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label = std::string());
std::size_t cg_abs(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
std::size_t cg_exp(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
std::size_t cg_sqrt(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
std::size_t cg_log(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
std::size_t cg_pow(ComputationGraph& g, const std::size_t a, const std::size_t b, const std::string& label = std::string());
std::size_t cg_normalCdf(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
std::size_t cg_normalPdf(ComputationGraph& g, const std::size_t a, const std::string& label = std::string());
    
} // namespace QuantExt
