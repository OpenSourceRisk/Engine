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

#include <ored/scripting/asttoscriptconverter.hpp>

#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

#include <sstream>

namespace ore {
namespace data {

namespace {
class ASTToScriptConverter : public AcyclicVisitor,
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
    ASTToScriptConverter() : indent(0) {}

    void visit(ASTNode& n) override { QL_FAIL("ASTToScriptConverter(): unknown node type"); }

    void visit(OperatorPlusNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "(" + left + " + " + right + ")";
    }

    void visit(OperatorMinusNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "(" + left + " - (" + right + "))";
    }

    void visit(OperatorMultiplyNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "(" + left + " * " + right + ")";
    }

    void visit(OperatorDivideNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "(" + left + " / (" + right + "))";
    }

    void visit(NegateNode& n) override {
        n.args[0]->accept(*this);
        script = "-(" + script + ")";
    }

    void visit(FunctionAbsNode& n) override {
        n.args[0]->accept(*this);
        script = "abs(" + script + ")";
    }

    void visit(FunctionExpNode& n) override {
        n.args[0]->accept(*this);
        script = "exp(" + script + ")";
    }

    void visit(FunctionLogNode& n) override {
        n.args[0]->accept(*this);
        script = "ln(" + script + ")";
    }

    void visit(FunctionSqrtNode& n) override {
        n.args[0]->accept(*this);
        script = "sqrt(" + script + ")";
    }

    void visit(FunctionNormalCdfNode& n) override {
        n.args[0]->accept(*this);
        script = "normalCdf(" + script + ")";
    }

    void visit(FunctionNormalPdfNode& n) override {
        n.args[0]->accept(*this);
        script = "normalPdf(" + script + ")";
    }

    void visit(FunctionMinNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "min(" + left + ", " + right + ")";
    }

    void visit(FunctionMaxNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "max(" + left + ", " + right + ")";
    }

    void visit(FunctionPowNode& n) override {
        n.args[0]->accept(*this);
        auto left = script;
        n.args[1]->accept(*this);
        auto right = script;
        script = "pow(" + left + ", " + right + ")";
    }

    void visit(FunctionBlackNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        n.args[4]->accept(*this);
        auto arg5 = script;
        n.args[5]->accept(*this);
        auto arg6 = script;
        script = "black(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4 + ", " + arg5 + ", " + arg6 + ")";
    }

    void visit(FunctionDcfNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        script = "dcf(" + arg1 + ", " + arg2 + ", " + arg3 + ")";
    }

    void visit(FunctionDaysNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        script = "days(" + arg1 + ", " + arg2 + ", " + arg3 + ")";
    }

    void visit(FunctionPayNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        script = "PAY(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4 + ")";
    }

    void visit(FunctionLogPayNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        std::string arg5;
        if (n.args[4]) {
            n.args[4]->accept(*this);
            arg5 = script;
        }
        std::string arg6;
        if (n.args[5]) {
            n.args[5]->accept(*this);
            arg6 = script;
        }
        std::string arg7;
        if (n.args[6]) {
            n.args[6]->accept(*this);
            arg7 = script;
        }
        script = "LOGPAY(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4;
        if (!arg5.empty())
            script += ", " + arg5;
        if (!arg6.empty())
            script += ", " + arg6;
        if (!arg7.empty())
            script += ", " + arg7;
        script += ")";
    }

    void visit(FunctionNpvNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        std::string arg3;
        if (n.args[2]) {
            n.args[2]->accept(*this);
            arg3 = script;
        }
        std::string arg4;
        if(n.args[3]) {
            n.args[3]->accept(*this);
            arg4 = script;
        }
        std::string arg5;
        if(n.args[4]) {
            n.args[4]->accept(*this);
            arg5 = script;
        }
        script = "NPV(" + arg1 + ", " + arg2;
        if (!arg3.empty())
            script += ", " + arg3;
        if (!arg4.empty())
            script += ", " + arg4;
        if (!arg5.empty())
            script += ", " + arg5;
        script += ")";
    }

    void visit(FunctionNpvMemNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        std::string arg4;
        if (n.args[3]) {
            n.args[3]->accept(*this);
            arg4 = script;
        }
        std::string arg5;
        if (n.args[4]) {
            n.args[4]->accept(*this);
            arg5 = script;
        }
        std::string arg6;
        if (n.args[5]) {
            n.args[5]->accept(*this);
            arg6 = script;
        }
        script = "NPVMEM(" + arg1 + ", " + arg2 + ", " + arg3;
        if (!arg4.empty())
            script += ", " + arg4;
        if (!arg5.empty())
            script += ", " + arg5;
        if (!arg6.empty())
            script += ", " + arg6;
        script += ")";
    }

    void visit(HistFixingNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = "HISTFIXING(" + arg1 + ", " + arg2 + ")";
    }

    void visit(FunctionDiscountNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        script = "DISCOUNT(" + arg1 + ", " + arg2 + ", " + arg3 + ")";
    }

    void visit(FunctionFwdCompNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        std::string arg5;
        if (n.args[4]) {
            n.args[4]->accept(*this);
            arg5 = script;
        }
        std::string arg6;
        if (n.args[5]) {
            n.args[5]->accept(*this);
            arg6 = script;
        }
        std::string arg7;
        if (n.args[6]) {
            n.args[6]->accept(*this);
            arg7 = script;
        }
        std::string arg8;
        if (n.args[7]) {
            n.args[7]->accept(*this);
            arg8 = script;
        }
        std::string arg9;
        if (n.args[8]) {
            n.args[8]->accept(*this);
            arg9 = script;
        }
        std::string arg10;
        if (n.args[9]) {
            n.args[9]->accept(*this);
            arg10 = script;
        }
        std::string arg11;
        if (n.args[10]) {
            n.args[10]->accept(*this);
            arg11 = script;
        }
        std::string arg12;
        if (n.args[11]) {
            n.args[11]->accept(*this);
            arg12 = script;
        }
        std::string arg13;
        if (n.args[12]) {
            n.args[12]->accept(*this);
            arg13 = script;
        }
        std::string arg14;
        if (n.args[13]) {
            n.args[13]->accept(*this);
            arg14 = script;
        }
        script = "FWDCOMP(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4;
        if (arg5.empty())
            script += ")";
        else {
            script += ", " + arg5;
            if (arg6.empty())
                script += ")";
            else {
                script += ", " + arg6;
                if (arg7.empty())
                    script += ")";
                else {
                    script += ", " + arg7;
                    if (arg8.empty())
                        script += ")";
                    else {
                        script += ", " + arg8;
                        if (arg9.empty())
                            script += ")";
                        else {
                            script += ", " + arg9;
                            if (arg10.empty())
                                script += ")";
                            else {
                                script += ", " + arg10;
                                if (arg11.empty())
                                    script += ")";
                                else {
                                    script += ", " + arg11;
                                    if (arg12.empty())
                                        script += ")";
                                    else {
                                        script += ", " + arg12;
                                        if (arg13.empty())
                                            script += ")";
                                        else {
                                            script += ", " + arg13;
                                            if (arg14.empty())
                                                script += ")";
                                            else {
                                                script += ", " + arg14 + ")";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void visit(FunctionFwdAvgNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        std::string arg5;
        if (n.args[4]) {
            n.args[4]->accept(*this);
            arg5 = script;
        }
        std::string arg6;
        if (n.args[5]) {
            n.args[5]->accept(*this);
            arg6 = script;
        }
        std::string arg7;
        if (n.args[6]) {
            n.args[6]->accept(*this);
            arg7 = script;
        }
        std::string arg8;
        if (n.args[7]) {
            n.args[7]->accept(*this);
            arg8 = script;
        }
        std::string arg9;
        if (n.args[8]) {
            n.args[8]->accept(*this);
            arg9 = script;
        }
        std::string arg10;
        if (n.args[9]) {
            n.args[9]->accept(*this);
            arg10 = script;
        }
        std::string arg11;
        if (n.args[10]) {
            n.args[10]->accept(*this);
            arg11 = script;
        }
        std::string arg12;
        if (n.args[11]) {
            n.args[11]->accept(*this);
            arg12 = script;
        }
        std::string arg13;
        if (n.args[12]) {
            n.args[12]->accept(*this);
            arg13 = script;
        }
        std::string arg14;
        if (n.args[13]) {
            n.args[13]->accept(*this);
            arg14 = script;
        }
        script = "FWDAVG(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4;
        if (arg5.empty())
            script += ")";
        else {
            script += ", " + arg5;
            if (arg6.empty())
                script += ")";
            else {
                script += ", " + arg6;
                if (arg7.empty())
                    script += ")";
                else {
                    script += ", " + arg7;
                    if (arg8.empty())
                        script += ")";
                    else {
                        script += ", " + arg8;
                        if (arg9.empty())
                            script += ")";
                        else {
                            script += ", " + arg9;
                            if (arg10.empty())
                                script += ")";
                            else {
                                script += ", " + arg10;
                                if (arg11.empty())
                                    script += ")";
                                else {
                                    script += ", " + arg11;
                                    if (arg12.empty())
                                        script += ")";
                                    else {
                                        script += ", " + arg12;
                                        if (arg13.empty())
                                            script += ")";
                                        else {
                                            script += ", " + arg13;
                                            if (arg14.empty())
                                                script += ")";
                                            else {
                                                script += ", " + arg14 + ")";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void visit(FunctionAboveProbNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        script = "ABOVEPROB(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4 + ")";
    }

    void visit(FunctionBelowProbNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        script = "BELOWPROB(" + arg1 + ", " + arg2 + ", " + arg3 + ", " + arg4 + ")";
    }

    void visit(FunctionDateIndexNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        script = "DATEINDEX(" + arg1 + ", " + n.name + "," + n.op + ")";
    }

    void visit(SortNode& n) override {
        auto v1 = QuantLib::ext::dynamic_pointer_cast<VariableNode>(n.args[0]);
        auto v2 = QuantLib::ext::dynamic_pointer_cast<VariableNode>(n.args[1]);
        auto v3 = QuantLib::ext::dynamic_pointer_cast<VariableNode>(n.args[2]);
        script = "SORT ( " + (v1 ? v1->name : "") + (v2 ? "," + v2->name : "") + (v3 ? "," + v3->name : "") + " )";
    }

    void visit(PermuteNode& n) override {
        auto v1 = QuantLib::ext::dynamic_pointer_cast<VariableNode>(n.args[0]);
        auto v2 = QuantLib::ext::dynamic_pointer_cast<VariableNode>(n.args[1]);
        auto v3 = QuantLib::ext::dynamic_pointer_cast<VariableNode>(n.args[2]);
        script = "PERMUTE ( " + (v1 ? v1->name : "") + (v2 ? "," + v2->name : "") + (v3 ? "," + v3->name : "") + " )";
    }

    void visit(ConstantNumberNode& n) override { script = std::to_string(n.value); }

    void visit(VariableNode& n) override {
        std::string arg1;
        if (n.args[0]) {
            n.args[0]->accept(*this);
            arg1 = script;
        }
        script = n.name + (arg1.empty() ? "" : "[" + arg1 + "]");
    }

    void visit(SizeOpNode& n) override { script = "SIZE(" + n.name + ")"; }

    void visit(VarEvaluationNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        std::string arg3;
        if (n.args[2]) {
            n.args[2]->accept(*this);
            arg3 = script;
        }
        script = arg1 + "(" + arg2 + (arg3.empty() ? ")" : ", " + arg3 + ")");
    }

    void visit(AssignmentNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = std::string(indent, ' ') + arg1 + " = " + arg2;
    }

    void visit(RequireNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        script = std::string(indent, ' ') + "REQUIRE " + arg1;
    }

    void visit(DeclarationNumberNode& n) override {
        std::string tmp = "NUMBER ";
        for (Size i = 0; i < n.args.size(); ++i) {
            n.args[i]->accept(*this);
            tmp += script + (i < n.args.size() - 1 ? ", " : "");
        }
        script = std::string(indent, ' ') + tmp;
    }

    void visit(SequenceNode& n) override {
        std::string tmp;
        for (Size i = 0; i < n.args.size(); ++i) {
            n.args[i]->accept(*this);
            tmp += script + ";\n";
        }
        script = tmp;
    }

    void visit(ConditionEqNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = arg1 + " == " + arg2;
    }

    void visit(ConditionNeqNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = arg1 + " != " + arg2;
    }

    void visit(ConditionLtNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = arg1 + " < " + arg2;
    }

    void visit(ConditionLeqNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = arg1 + " <= " + arg2;
    }

    void visit(ConditionGtNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = arg1 + " > " + arg2;
    }

    void visit(ConditionGeqNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = arg1 + " >= " + arg2;
    }

    void visit(ConditionNotNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        script = "NOT(" + arg1 + ")";
    }

    void visit(ConditionAndNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = "{" + arg1 + " AND " + arg2 + "}";
    }

    void visit(ConditionOrNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        script = "{" + arg1 + " OR " + arg2 + "}";
    }

    void visit(IfThenElseNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        indent += tabsize;
        n.args[1]->accept(*this);
        auto arg2 = script;
        std::string arg3;
        if (n.args[2]) {
            n.args[2]->accept(*this);
            arg3 = script;
        }
        indent -= tabsize;
        script = std::string(indent, ' ') + "IF " + arg1 + " THEN\n" + arg2 + std::string(indent, ' ') +
                 (arg3.empty() ? "END" : "ELSE\n" + arg3 + std::string(indent, ' ') + "END");
    }

    void visit(LoopNode& n) override {
        n.args[0]->accept(*this);
        auto arg1 = script;
        n.args[1]->accept(*this);
        auto arg2 = script;
        indent += tabsize;
        n.args[2]->accept(*this);
        auto arg3 = script;
        n.args[3]->accept(*this);
        auto arg4 = script;
        indent -= tabsize;
        script = std::string(indent, ' ') + "FOR " + n.name + " IN (" + arg1 + ", " + arg2 + ", " + arg3 + ") DO\n" +
                 arg4 + std::string(indent, ' ') + "END";
    }

    std::string script;
    Size indent;
    static constexpr Size tabsize = 2;
};
} // namespace

std::string to_script(const ASTNodePtr root) {
    ASTToScriptConverter p;
    root->accept(p);
    return p.script;
}

} // namespace data
} // namespace ore
