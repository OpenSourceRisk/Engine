/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/ast.hpp
    \brief abstract syntax tree for payoff scripting
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/value.hpp>

#include <ql/patterns/visitor.hpp>
#include <ql/types.hpp>
#include <ql/utilities/null.hpp>

#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <ql/shared_ptr.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace ore {
namespace data {

using namespace QuantLib;

struct ASTNode;
using ASTNodePtr = QuantLib::ext::shared_ptr<ASTNode>;

struct LocationInfo {
    LocationInfo() : initialised(false) {}
    LocationInfo(const Size lineStart, const Size columnStart, const Size lineEnd, const Size columnEnd)
        : initialised(true), lineStart(lineStart), columnStart(columnStart), lineEnd(lineEnd), columnEnd(columnEnd) {}
    bool initialised;
    Size lineStart, columnStart, lineEnd, columnEnd;
};

std::string to_string(const LocationInfo& l);

struct ASTNode {
    ASTNode(){};
    virtual ~ASTNode() {}
    ASTNode(const std::vector<ASTNodePtr>& args, const Size minArgs, const Size maxArgs = Null<Size>());
    virtual void accept(AcyclicVisitor&);
    LocationInfo locationInfo;
    std::vector<ASTNodePtr> args;
};

struct OperatorPlusNode : public ASTNode {
    OperatorPlusNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct OperatorMinusNode : public ASTNode {
    OperatorMinusNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct OperatorMultiplyNode : public ASTNode {
    OperatorMultiplyNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct OperatorDivideNode : public ASTNode {
    OperatorDivideNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct NegateNode : public ASTNode {
    NegateNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionAbsNode : public ASTNode {
    FunctionAbsNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionExpNode : public ASTNode {
    FunctionExpNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionLogNode : public ASTNode {
    FunctionLogNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionSqrtNode : public ASTNode {
    FunctionSqrtNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionNormalCdfNode : public ASTNode {
    FunctionNormalCdfNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionNormalPdfNode : public ASTNode {
    FunctionNormalPdfNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionMaxNode : public ASTNode {
    FunctionMaxNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionMinNode : public ASTNode {
    FunctionMinNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionPowNode : public ASTNode {
    FunctionPowNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionBlackNode : public ASTNode {
    FunctionBlackNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 6, 6) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionDcfNode : public ASTNode {
    FunctionDcfNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 3, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionDaysNode : public ASTNode {
    FunctionDaysNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 3, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionPayNode : public ASTNode {
    FunctionPayNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 4) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionLogPayNode : public ASTNode {
    FunctionLogPayNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 7) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionNpvNode : public ASTNode {
    FunctionNpvNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 5) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionNpvMemNode : public ASTNode {
    FunctionNpvMemNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 3, 6) {}
    void accept(AcyclicVisitor&) override;
};

struct HistFixingNode : public ASTNode {
    HistFixingNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionDiscountNode : public ASTNode {
    FunctionDiscountNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 3, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionFwdCompNode : public ASTNode {
    FunctionFwdCompNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 14) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionFwdAvgNode : public ASTNode {
    FunctionFwdAvgNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 14) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionAboveProbNode : public ASTNode {
    FunctionAboveProbNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 4) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionBelowProbNode : public ASTNode {
    FunctionBelowProbNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 4) {}
    void accept(AcyclicVisitor&) override;
};

struct FunctionDateIndexNode : public ASTNode {
    FunctionDateIndexNode(const boost::fusion::vector<std::string, std::string>& params,
                          const std::vector<ASTNodePtr>& args)
        : ASTNode(args, 1, 1), name(boost::fusion::at_c<0>(params)), op(boost::fusion::at_c<1>(params)) {}
    void accept(AcyclicVisitor&) override;
    std::string name, op;
};

struct SortNode : public ASTNode {
    SortNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct PermuteNode : public ASTNode {
    PermuteNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct ConstantNumberNode : public ASTNode {
    ConstantNumberNode(const double value, const std::vector<ASTNodePtr>& args = {})
        : ASTNode(args, 0, 0), value(value) {}
    void accept(AcyclicVisitor&) override;
    const double value;
};

struct VariableNode : public ASTNode {
    VariableNode(const std::string& name, const std::vector<ASTNodePtr>& args = {}) : ASTNode(args, 0, 1), name(name) {}
    void accept(AcyclicVisitor&) override;
    const std::string name;
    // cache for optimised variable reference retrieval
    bool isCached = false, isScalar = false;
    ValueType* cachedScalar = nullptr;
    std::vector<ValueType>* cachedVector = nullptr;
};

struct SizeOpNode : public ASTNode {
    SizeOpNode(const std::string& name, const std::vector<ASTNodePtr>& args = {}) : ASTNode(args, 0, 0), name(name) {}
    void accept(AcyclicVisitor&) override;
    const std::string name;
};

struct VarEvaluationNode : public ASTNode {
    VarEvaluationNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct AssignmentNode : public ASTNode {
    AssignmentNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2) {}
    void accept(AcyclicVisitor&) override;
};

struct RequireNode : public ASTNode {
    RequireNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct DeclarationNumberNode : public ASTNode {
    DeclarationNumberNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct SequenceNode : public ASTNode {
    SequenceNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1) {}
    void accept(AcyclicVisitor&) override;
};

struct ConditionEqNode : public ASTNode {
    ConditionEqNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionNeqNode : public ASTNode {
    ConditionNeqNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionLtNode : public ASTNode {
    ConditionLtNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionLeqNode : public ASTNode {
    ConditionLeqNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionGtNode : public ASTNode {
    ConditionGtNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionGeqNode : public ASTNode {
    ConditionGeqNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionNotNode : public ASTNode {
    ConditionNotNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 1, 1){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionAndNode : public ASTNode {
    ConditionAndNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct ConditionOrNode : public ASTNode {
    ConditionOrNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 2){};
    void accept(AcyclicVisitor&) override;
};

struct IfThenElseNode : public ASTNode {
    IfThenElseNode(const std::vector<ASTNodePtr>& args) : ASTNode(args, 2, 3) {}
    void accept(AcyclicVisitor&) override;
};

struct LoopNode : public ASTNode {
    LoopNode(const std::string& name, const std::vector<ASTNodePtr>& args) : ASTNode(args, 4, 4), name(name) {}
    void accept(AcyclicVisitor&) override;
    const std::string name;
};

} // namespace data
} // namespace ore
