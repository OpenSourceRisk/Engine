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

#include <ored/scripting/ast.hpp>

#include <ql/errors.hpp>

#include <sstream>

namespace ore {
namespace data {

std::string to_string(const LocationInfo& l) {
    if (l.initialised) {
        if (l.lineStart == l.lineEnd && l.columnStart <= l.columnEnd)
            return 'L' + std::to_string(l.lineStart) + ':' + std::to_string(l.columnStart) + ':' +
                   std::to_string(l.columnEnd - l.columnStart);
        else
            return 'L' + std::to_string(l.lineStart) + ':' + std::to_string(l.columnStart) + "-L" +
                   std::to_string(l.lineEnd) + ':' + std::to_string(l.columnEnd);
    } else
        return "L?";
}

ASTNode::ASTNode(const std::vector<ASTNodePtr>& args, const Size minArgs, const Size maxArgs) : args(args) {
    QL_REQUIRE(minArgs <= args.size() && (maxArgs == Null<Size>() || args.size() <= maxArgs),
               "ASTNode construction failed, got " << args.size() << " arguments, expected " << minArgs << "..."
                                                   << (maxArgs == Null<Size>() ? "inf" : std::to_string(maxArgs)));
    if (maxArgs != Null<Size>())
        this->args.resize(maxArgs, ASTNodePtr());
}

void ASTNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ASTNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        QL_FAIL("not an ASTNode visitor");
}

void OperatorPlusNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<OperatorPlusNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void OperatorMinusNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<OperatorMinusNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void OperatorMultiplyNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<OperatorMultiplyNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void OperatorDivideNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<OperatorDivideNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void NegateNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<NegateNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionAbsNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionAbsNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionExpNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionExpNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionLogNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionLogNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionSqrtNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionSqrtNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionNormalCdfNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionNormalCdfNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionNormalPdfNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionNormalPdfNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionMaxNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionMaxNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionMinNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionMinNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionPowNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionPowNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionBlackNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionBlackNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionDcfNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionDcfNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionDaysNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionDaysNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionPayNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionPayNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionLogPayNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionLogPayNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionNpvNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionNpvNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionNpvMemNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionNpvMemNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void HistFixingNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<HistFixingNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionDiscountNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionDiscountNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionFwdCompNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionFwdCompNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionFwdAvgNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionFwdAvgNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionAboveProbNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionAboveProbNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionBelowProbNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionBelowProbNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void FunctionDateIndexNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<FunctionDateIndexNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void SortNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<SortNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void PermuteNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<PermuteNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConstantNumberNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConstantNumberNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void VariableNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<VariableNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void SizeOpNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<SizeOpNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void VarEvaluationNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<VarEvaluationNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void AssignmentNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<AssignmentNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void RequireNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<RequireNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void DeclarationNumberNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<DeclarationNumberNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void SequenceNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<SequenceNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionEqNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionEqNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionNeqNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionNeqNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionLtNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionLtNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionLeqNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionLeqNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionGtNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionGtNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionGeqNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionGeqNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionNotNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionNotNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionAndNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionAndNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void ConditionOrNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<ConditionOrNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void IfThenElseNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<IfThenElseNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

void LoopNode::accept(AcyclicVisitor& v) {
    auto v1 = dynamic_cast<Visitor<LoopNode>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        ASTNode::accept(v);
}

} // namespace data
} // namespace ore
