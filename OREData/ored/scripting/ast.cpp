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

#define ACCEPT(C) void C::accept(StAstVisitor& v) { v.visit(*this); }

ACCEPT(ASTNode);
ACCEPT(OperatorPlusNode);
ACCEPT(OperatorMinusNode);
ACCEPT(OperatorMultiplyNode);
ACCEPT(OperatorDivideNode);
ACCEPT(NegateNode);
ACCEPT(FunctionAbsNode);
ACCEPT(FunctionExpNode);
ACCEPT(FunctionLogNode);
ACCEPT(FunctionSqrtNode);
ACCEPT(FunctionNormalCdfNode);
ACCEPT(FunctionNormalPdfNode);
ACCEPT(FunctionMaxNode);
ACCEPT(FunctionMinNode);
ACCEPT(FunctionFractionNode);
ACCEPT(FunctionRoundNode);
ACCEPT(FunctionPowNode);
ACCEPT(FunctionBlackNode);
ACCEPT(FunctionDcfNode);
ACCEPT(FunctionDaysNode);
ACCEPT(FunctionPayNode);
ACCEPT(FunctionLogPayNode);
ACCEPT(FunctionNpvNode);
ACCEPT(FunctionNpvMemNode);
ACCEPT(HistFixingNode);
ACCEPT(FunctionDiscountNode);
ACCEPT(FunctionFwdCompNode);
ACCEPT(FunctionFwdAvgNode);
ACCEPT(FunctionAboveProbNode);
ACCEPT(FunctionBelowProbNode);
ACCEPT(FunctionDateIndexNode);
ACCEPT(SortNode);
ACCEPT(PermuteNode);
ACCEPT(ConstantNumberNode);
ACCEPT(VariableNode);
ACCEPT(SizeOpNode);
ACCEPT(VarEvaluationNode);
ACCEPT(AssignmentNode);
ACCEPT(RequireNode);
ACCEPT(DeclarationNumberNode);
ACCEPT(SequenceNode);
ACCEPT(ConditionEqNode);
ACCEPT(ConditionNeqNode);
ACCEPT(ConditionLtNode);
ACCEPT(ConditionLeqNode);
ACCEPT(ConditionGtNode);
ACCEPT(ConditionGeqNode);
ACCEPT(ConditionNotNode);
ACCEPT(ConditionAndNode);
ACCEPT(ConditionOrNode);
ACCEPT(IfThenElseNode);
ACCEPT(LoopNode);

} // namespace data
} // namespace ore
