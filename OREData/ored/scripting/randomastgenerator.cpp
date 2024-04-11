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

#include <ored/scripting/randomastgenerator.hpp>

#include <ql/errors.hpp>

#include <boost/make_shared.hpp>

#include <random>

namespace ore {
namespace data {

struct RandomASTGenerator {
    RandomASTGenerator(const Size maxSequenceLength, const Size maxDepth, const Size seed)
        : maxSequenceLength_(maxSequenceLength), maxDepth_(maxDepth), gen_(seed) {}

    void createInstructionSequence() {
        std::uniform_int_distribution<> seq(1, maxSequenceLength_);
        std::uniform_int_distribution<> instr(0, depth >= maxDepth_ ? 4 : 6);
        ++depth;
        std::vector<ASTNodePtr> args;
        for (int i = 0; i < seq(gen_); ++i) {
            switch (instr(gen_)) {
            case 0:
                createRequire();
                break;
            case 1:
                createSort();
                break;
            case 2:
                createPermute();
                break;
            case 3:
                createDeclaration();
                break;
            case 4:
                createAssignment();
                break;
            case 5:
                createIfThenElse();
                break;
            case 6:
                createLoop();
                break;
            default:
                QL_FAIL("internal error:");
            }
            args.push_back(current);
        }
        current = QuantLib::ext::make_shared<SequenceNode>(args);
        --depth;
    }

    void createRequire() {
        ++depth;
        createCondition();
        current = QuantLib::ext::make_shared<RequireNode>(std::vector<ASTNodePtr>(1, current));
        --depth;
    }

    void createSort() {
        std::uniform_int_distribution<> seq(1, 3);
        std::vector<ASTNodePtr> vars;
        for (int i = 0; i < seq(gen_); ++i) {
            vars.push_back(QuantLib::ext::make_shared<VariableNode>(createVariableName()));
        }
        current = QuantLib::ext::make_shared<SortNode>(vars);
    }

    void createPermute() {
        std::uniform_int_distribution<> seq(2, 3);
        std::vector<ASTNodePtr> vars;
        for (int i = 0; i < seq(gen_); ++i) {
            vars.push_back(QuantLib::ext::make_shared<VariableNode>(createVariableName()));
        }
        current = QuantLib::ext::make_shared<PermuteNode>(vars);
    }

    void createDeclaration() {
        std::uniform_int_distribution<> seq(1, maxSequenceLength_);
        ++depth;
        std::vector<ASTNodePtr> args;
        for (int i = 0; i < seq(gen_); ++i) {
            createVarExpr();
            args.push_back(current);
        }
        current = QuantLib::ext::make_shared<DeclarationNumberNode>(args);
        --depth;
    }

    void createIfThenElse() {
        ++depth;
        std::uniform_int_distribution<> yn(0, 1);
        std::vector<ASTNodePtr> args;
        createCondition();
        args.push_back(current);
        createInstructionSequence();
        args.push_back(current);
        if (yn(gen_)) {
            createInstructionSequence();
            args.push_back(current);
        }
        current = QuantLib::ext::make_shared<IfThenElseNode>(args);
        --depth;
    }

    void createLoop() {
        ++depth;
        std::vector<ASTNodePtr> args;
        createTerm();
        args.push_back(current);
        createTerm();
        args.push_back(current);
        createTerm();
        args.push_back(current);
        createInstructionSequence();
        args.push_back(current);
        current = QuantLib::ext::make_shared<LoopNode>(createVariableName(), args);
        --depth;
    }

    void createAssignment() {
        ++depth;
        std::vector<ASTNodePtr> args;
        createVarExpr();
        args.push_back(current);
        createTerm();
        args.push_back(current);
        current = QuantLib::ext::make_shared<AssignmentNode>(args);
        --depth;
    }

    void createCondition() {
        std::uniform_int_distribution<> dep(0, maxDepth_);
        std::uniform_int_distribution<> cond(0, 5);
        std::uniform_int_distribution<> andor(0, 1);
        ++depth;
        if (depth + dep(gen_) >= maxDepth_) {
            std::vector<ASTNodePtr> args;
            createTerm();
            args.push_back(current);
            createTerm();
            args.push_back(current);
            switch (cond(gen_)) {
            case 0:
                current = QuantLib::ext::make_shared<ConditionEqNode>(args);
                break;
            case 1:
                current = QuantLib::ext::make_shared<ConditionNeqNode>(args);
                break;
            case 2:
                current = QuantLib::ext::make_shared<ConditionGeqNode>(args);
                break;
            case 3:
                current = QuantLib::ext::make_shared<ConditionGtNode>(args);
                break;
            case 4:
                current = QuantLib::ext::make_shared<ConditionLeqNode>(args);
                break;
            case 5:
                current = QuantLib::ext::make_shared<ConditionLtNode>(args);
                break;
            default:
                QL_FAIL("internal error");
            }
        } else {
            std::vector<ASTNodePtr> args;
            createCondition();
            args.push_back(current);
            createCondition();
            args.push_back(current);
            switch (andor(gen_)) {
            case 0:
                current = QuantLib::ext::make_shared<ConditionAndNode>(args);
                break;
            case 1:
                current = QuantLib::ext::make_shared<ConditionOrNode>(args);
                break;
            default:
                QL_FAIL("internal error");
            }
        }
        --depth;
    }

    void createTerm() {
        std::uniform_int_distribution<> dep(0, maxDepth_);
        std::uniform_int_distribution<> pm(0, 3);
        ++depth;
        if (depth + dep(gen_) >= maxDepth_) {
            createFactor();
        } else {
            std::vector<ASTNodePtr> args;
            createTerm();
            args.push_back(current);
            createTerm();
            args.push_back(current);
            switch (pm(gen_)) {
            case 0:
                current = QuantLib::ext::make_shared<OperatorPlusNode>(args);
                break;
            case 1:
                current = QuantLib::ext::make_shared<OperatorMinusNode>(args);
                break;
            case 2:
                current = QuantLib::ext::make_shared<OperatorMultiplyNode>(args);
                break;
            case 3:
                current = QuantLib::ext::make_shared<OperatorDivideNode>(args);
                break;
            default:
                QL_FAIL("internal error");
            }
        }
        --depth;
    }

    void createFactor() {
        std::uniform_int_distribution<> dep(0, maxDepth_);
        std::uniform_int_distribution<> sing(0, 2);
        std::uniform_int_distribution<> fac(0, 25);
        std::uniform_int_distribution<> yn(0, 1);
        std::uniform_int_distribution<> zero123(0, 3);
        std::uniform_int_distribution<> slot(0, 5);
        std::uniform_int_distribution<> legNo(0, 5);
        ++depth;
        if (depth + dep(gen_) >= maxDepth_) {
            switch (sing(gen_)) {
            case 0:
                current = QuantLib::ext::make_shared<VariableNode>(createVariableName(), std::vector<ASTNodePtr>());
                break;
            case 1:
                current = QuantLib::ext::make_shared<ConstantNumberNode>(createConstantNumber());
                break;
            case 2:
                current = QuantLib::ext::make_shared<SizeOpNode>(createVariableName());
                break;
            default:
                QL_FAIL("internal error");
            }
        } else {
            std::vector<ASTNodePtr> args;
            switch (fac(gen_)) {
            case 0:
                createFactor();
                args.push_back(current);
                current = QuantLib::ext::make_shared<NegateNode>(args);
                break;
            case 1:
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionAbsNode>(args);
                break;
            case 2:
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionExpNode>(args);
                break;
            case 3:
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionLogNode>(args);
                break;
            case 4:
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionSqrtNode>(args);
                break;
            case 5:
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionNormalCdfNode>(args);
                break;
            case 6:
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionNormalPdfNode>(args);
                break;
            case 7:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionMaxNode>(args);
                break;
            case 8:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionMinNode>(args);
                break;
            case 9:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionPowNode>(args);
                break;
            case 10:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionBlackNode>(args);
                break;
            case 11:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionDcfNode>(args);
                break;
            case 12:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionDaysNode>(args);
                break;
            case 13:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionPayNode>(args);
                break;
            case 14: {
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                int l = legNo(gen_);
                if (l) {
                    createTerm();
                    args.push_back(current);
                    args.push_back(QuantLib::ext::make_shared<VariableNode>(createVariableName()));
                    int s = slot(gen_);
                    if (s) {
                        createTerm();
                        args.push_back(current);
                    }
                }
                current = QuantLib::ext::make_shared<FunctionLogPayNode>(args);
                break;
            }
            case 15:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                if (yn(gen_)) {
                    createCondition();
                    args.push_back(current);
                    if (yn(gen_)) {
                        createTerm();
                        args.push_back(current);
                        if (yn(gen_)) {
                            createTerm();
                            args.push_back(current);
                        }
                    }
                }
                current = QuantLib::ext::make_shared<FunctionNpvNode>(args);
                break;
            case 16:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                if (yn(gen_)) {
                    createCondition();
                    args.push_back(current);
                    if (yn(gen_)) {
                        createTerm();
                        args.push_back(current);
                        if (yn(gen_)) {
                            createTerm();
                            args.push_back(current);
                        }
                    }
                }
                current = QuantLib::ext::make_shared<FunctionNpvMemNode>(args);
                break;
            case 17:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                current = QuantLib::ext::make_shared<HistFixingNode>(args);
                break;
            case 18:
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionDiscountNode>(args);
                break;
            case 19:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                switch (zero123(gen_)) {
                case 0:
                    // 4 + 4 + 2 optional args
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                case 1:
                    // 4 +2 optional args
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                case 2:
                    // 2 optional args
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                case 3:
                    // no optional args
                    break;
                default:
                    QL_FAIL("internal error");
                }
                current = QuantLib::ext::make_shared<FunctionFwdCompNode>(args);
                break;
            case 20:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                switch (zero123(gen_)) {
                case 0:
                    // 4 + 4 + 2 optional args
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                case 1:
                    // 4 +2 optional args
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                case 2:
                    // 2 optional args
                    createTerm();
                    args.push_back(current);
                    createTerm();
                    args.push_back(current);
                case 3:
                    // no optional args
                    break;
                default:
                    QL_FAIL("internal error");
                }
                current = QuantLib::ext::make_shared<FunctionFwdAvgNode>(args);
                break;
            case 21:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionAboveProbNode>(args);
                break;
            case 22:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                createTerm();
                args.push_back(current);
                current = QuantLib::ext::make_shared<FunctionBelowProbNode>(args);
                break;
            case 23: {
                createVarExpr();
                args.push_back(current);
                boost::fusion::vector<std::string, std::string> p(createVariableName(), createVariableName());
                current = QuantLib::ext::make_shared<FunctionDateIndexNode>(p, args);
                break;
            }
            case 24:
                createVarExpr();
                args.push_back(current);
                current = QuantLib::ext::make_shared<VariableNode>(createVariableName(), args);
                break;
            case 25:
                createVarExpr();
                args.push_back(current);
                createVarExpr();
                args.push_back(current);
                if (yn(gen_)) {
                    createVarExpr();
                    args.push_back(current);
                }
                current = QuantLib::ext::make_shared<VarEvaluationNode>(args);
                break;
            default:
                QL_FAIL("internal error");
            }
        }
        --depth;
    }

    void createVarExpr() {
        std::uniform_int_distribution<> dep(0, maxDepth_);
        ++depth;
        if (depth + dep(gen_) > maxDepth_) {
            current = QuantLib::ext::make_shared<VariableNode>(createVariableName());
        } else {
            createTerm();
            current = QuantLib::ext::make_shared<VariableNode>(createVariableName(), std::vector<ASTNodePtr>{current});
        }
        --depth;
    }

    std::string createVariableName() {
        std::uniform_int_distribution<> id(1, 999); // does not matter really
        return "Var" + std::to_string(id(gen_));
    }

    double createConstantNumber() {
        std::uniform_int_distribution<> yn(0, 1);
        std::uniform_int_distribution<> whole(-999, 999);
        std::uniform_real_distribution<> real(-999, 999);
        if (yn(gen_))
            return static_cast<double>(whole(gen_));
        else
            return std::stod(std::to_string(real(gen_)));
    }

    ASTNodePtr current;
    Size depth = 0;

private:
    const Size maxSequenceLength_, maxDepth_;
    //
    std::mt19937 gen_;
};

ASTNodePtr generateRandomAST(const Size maxSequenceLength, const Size maxDepth, const Size seed) {
    RandomASTGenerator gen(maxSequenceLength, maxDepth, seed);
    gen.createInstructionSequence();
    return gen.current;
}

} // namespace data
} // namespace ore
