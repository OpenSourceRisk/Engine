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

#include <ored/scripting/astprinter.hpp>

#include <sstream>

namespace ore {
namespace data {

namespace {
class ASTPrinter : public AcyclicVisitor,
                   public Visitor<ASTNode>,
                   public Visitor<OperatorPlusNode>,
                   public Visitor<OperatorMinusNode>,
                   public Visitor<OperatorMultiplyNode>,
                   public Visitor<OperatorDivideNode>,
                   public Visitor<NegateNode>,
                   public Visitor<FunctionAbsNode>,
                   public Visitor<FunctionExpNode>,
                   public Visitor<FunctionLogNode>,
                   public Visitor<FunctionSqrtNode>,
                   public Visitor<FunctionNormalCdfNode>,
                   public Visitor<FunctionNormalPdfNode>,
                   public Visitor<FunctionMinNode>,
                   public Visitor<FunctionMaxNode>,
                   public Visitor<FunctionPowNode>,
                   public Visitor<FunctionBlackNode>,
                   public Visitor<FunctionDcfNode>,
                   public Visitor<FunctionDaysNode>,
                   public Visitor<FunctionPayNode>,
                   public Visitor<FunctionLogPayNode>,
                   public Visitor<FunctionNpvNode>,
                   public Visitor<FunctionNpvMemNode>,
                   public Visitor<HistFixingNode>,
                   public Visitor<FunctionDiscountNode>,
                   public Visitor<FunctionFwdCompNode>,
                   public Visitor<FunctionFwdAvgNode>,
                   public Visitor<FunctionAboveProbNode>,
                   public Visitor<FunctionBelowProbNode>,
                   public Visitor<FunctionDateIndexNode>,
                   public Visitor<SortNode>,
                   public Visitor<PermuteNode>,
                   public Visitor<ConstantNumberNode>,
                   public Visitor<VariableNode>,
                   public Visitor<SizeOpNode>,
                   public Visitor<VarEvaluationNode>,
                   public Visitor<AssignmentNode>,
                   public Visitor<RequireNode>,
                   public Visitor<DeclarationNumberNode>,
                   public Visitor<SequenceNode>,
                   public Visitor<ConditionEqNode>,
                   public Visitor<ConditionNeqNode>,
                   public Visitor<ConditionLtNode>,
                   public Visitor<ConditionLeqNode>,
                   public Visitor<ConditionGtNode>,
                   public Visitor<ConditionGeqNode>,
                   public Visitor<ConditionNotNode>,
                   public Visitor<ConditionAndNode>,
                   public Visitor<ConditionOrNode>,
                   public Visitor<IfThenElseNode>,
                   public Visitor<LoopNode> {
public:
    ASTPrinter(const bool printLocationInfo) : tabsize_(2), printLocationInfo_(printLocationInfo), indent_(0) {}
    std::string str() const { return out_.str(); }
    void visit(ASTNode& n) override { print("Unknown", n); }
    void visit(OperatorPlusNode& n) override { print("OperatorPlus", n); }
    void visit(OperatorMinusNode& n) override { print("OperatorMinus", n); }
    void visit(OperatorMultiplyNode& n) override { print("OperatorMultiply", n); }
    void visit(OperatorDivideNode& n) override { print("OperatorDivide", n); }
    void visit(NegateNode& n) override { print("Negate", n); }
    void visit(FunctionAbsNode& n) override { print("FunctionAbs", n); }
    void visit(FunctionExpNode& n) override { print("FunctionExp", n); }
    void visit(FunctionLogNode& n) override { print("FunctionLog", n); }
    void visit(FunctionSqrtNode& n) override { print("FunctionSqrt", n); }
    void visit(FunctionNormalCdfNode& n) override { print("FunctionNormalCdf", n); }
    void visit(FunctionNormalPdfNode& n) override { print("FunctionNormalPdf", n); }
    void visit(FunctionMinNode& n) override { print("FunctionMin", n); }
    void visit(FunctionMaxNode& n) override { print("FunctionMax", n); }
    void visit(FunctionPowNode& n) override { print("FunctionPow", n); }
    void visit(FunctionBlackNode& n) override { print("FunctionBlack", n); }
    void visit(FunctionDcfNode& n) override { print("FunctionDcf", n); }
    void visit(FunctionDaysNode& n) override { print("FunctionDays", n); }
    void visit(FunctionPayNode& n) override { print("FunctionPay", n); }
    void visit(FunctionLogPayNode& n) override { print("FunctionLogPay", n); }
    void visit(FunctionNpvNode& n) override { print("FunctionNpv", n); }
    void visit(FunctionNpvMemNode& n) override { print("FunctionNpvMem", n); }
    void visit(HistFixingNode& n) override { print("HistFixing", n); }
    void visit(FunctionDiscountNode& n) override { print("FunctionDiscount", n); }
    void visit(FunctionFwdCompNode& n) override { print("FunctionFwdComp", n); }
    void visit(FunctionFwdAvgNode& n) override { print("FunctionFwdAvg", n); }
    void visit(FunctionAboveProbNode& n) override { print("FunctionAboveProb", n); }
    void visit(FunctionBelowProbNode& n) override { print("FunctionBelowProb", n); }
    void visit(FunctionDateIndexNode& n) override { print("FunctionDateIndex(" + n.name + "," + n.op + ")", n); }
    void visit(SortNode& n) override { print("Sort", n); }
    void visit(PermuteNode& n) override { print("Permute", n); }
    void visit(ConstantNumberNode& n) override { print("ConstantNumber(" + std::to_string(n.value) + ")", n); }
    void visit(VariableNode& n) override { print("Variable(" + n.name + ")", n); }
    void visit(SizeOpNode& n) override { print("Size(" + n.name + ")", n); }
    void visit(VarEvaluationNode& n) override { print("VarEvaluation", n); }
    void visit(AssignmentNode& n) override { print("Assignment", n); }
    void visit(RequireNode& n) override { print("Require", n); }
    void visit(DeclarationNumberNode& n) override { print("DeclarationNumber", n); }
    void visit(SequenceNode& n) override { print("Sequence", n); }
    void visit(ConditionEqNode& n) override { print("ConditionEq", n); }
    void visit(ConditionNeqNode& n) override { print("ConditionNeq", n); }
    void visit(ConditionLtNode& n) override { print("ConditionLt", n); }
    void visit(ConditionLeqNode& n) override { print("ConditionLeq", n); }
    void visit(ConditionGtNode& n) override { print("ConditionGt", n); }
    void visit(ConditionGeqNode& n) override { print("ConditionGeq", n); }
    void visit(ConditionNotNode& n) override { print("ConditionNot", n); }
    void visit(ConditionAndNode& n) override { print("ConditionAnd", n); }
    void visit(ConditionOrNode& n) override { print("ConditionOr", n); }
    void visit(IfThenElseNode& n) override { print("IfThenElse", n); }
    void visit(LoopNode& n) override { print("Loop(" + n.name + ")", n); }

private:
    void print(const std::string& s, const ASTNode& n) {
        out_ << std::string(indent_, ' ') << s;
        if (printLocationInfo_)
            out_ << " at " + to_string(n.locationInfo);
        out_ << '\n';
        for (auto const& c : n.args) {
            indent_ += tabsize_;
            if (c == nullptr)
                out_ << std::string(indent_, ' ') << "-\n";
            else {
                c->accept(*this);
            }
            indent_ -= tabsize_;
        }
    }
    //
    const Size tabsize_;
    const bool printLocationInfo_;
    //
    Size indent_;
    std::ostringstream out_;
};
} // namespace

std::string to_string(const ASTNodePtr root, const bool printLocationInfo) {
    ASTPrinter p(printLocationInfo);
    root->accept(p);
    return p.str();
}

} // namespace data
} // namespace ore
