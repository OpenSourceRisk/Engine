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

#include <ored/scripting/grammar.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <sstream>

namespace ore {
namespace data {

template <typename NodeType, typename... AddArgs> struct createASTNode {
    createASTNode(std::stack<ASTNodePtr>& evalStack, const int nArgs, const bool mergeLocation = false)
        : evalStack(evalStack), nArgs(nArgs), mergeLocation(mergeLocation) {}
    void operator()(AddArgs... addArgs) const {
        std::vector<ASTNodePtr> arg;
        int n = nArgs;
        while (n) {
            QL_REQUIRE(!evalStack.empty(), "internal error (empty stack)");
            arg.insert(arg.begin(), evalStack.top());
            evalStack.pop();
            --n;
        }
        auto node = QuantLib::ext::make_shared<NodeType>(addArgs..., arg);
        if (mergeLocation && !arg.empty()) {
            auto const& loc1 = arg.front()->locationInfo;
            auto const& loc2 = arg.back()->locationInfo;
            node->locationInfo = LocationInfo(loc1.lineStart, loc1.columnStart, loc2.lineEnd, loc2.columnEnd);
        }
        evalStack.push(node);
    }
    std::stack<ASTNodePtr>& evalStack;
    const int nArgs;
    const bool mergeLocation;
};

template <typename NodeType, typename... AddArgs> struct collapseASTNode {
    collapseASTNode(std::stack<ASTNodePtr>& evalStack, const int nArgs) : evalStack(evalStack), nArgs(nArgs) {}
    void operator()(AddArgs... addArgs) const {
        std::vector<ASTNodePtr> arg;
        int n = nArgs;
        while (n) {
            QL_REQUIRE(!evalStack.empty(), "internal error (empty stack)");
            arg.insert(arg.begin(), evalStack.top());
            evalStack.pop();
            --n;
        }
        QL_REQUIRE(!evalStack.empty(), "internal error (empty stack)");
        ASTNodePtr previous = evalStack.top();
        evalStack.pop();
        arg.insert(arg.begin(), previous->args.begin(), previous->args.end());
        auto node = QuantLib::ext::make_shared<NodeType>(addArgs..., arg);
        node->locationInfo = previous->locationInfo;
        evalStack.push(node);
    }
    std::stack<ASTNodePtr>& evalStack;
    const int nArgs;
};

ScriptGrammar::ScriptGrammar(ScriptGrammarIterator first)
    : ScriptGrammar::base_type(instructionseq, "OREPayoffScript"), hasError(false), errorWhat(""),
      annotate(evalStack, first) {

    // clang-format off

    keyword %= qi::lexeme[ ( qi::lit("IF")        | qi::lit("THEN")      | qi::lit("ELSE")       | qi::lit("END")     | qi::lit("FOR")       | qi::lit("IN")   | qi::lit("DO")
                           | qi::lit("NUMBER")    | qi::lit("REQUIRE")   | qi::lit("OR")         | qi::lit("AND")     | qi::lit("abs")       | qi::lit("exp")  | qi::lit("ln")
                           | qi::lit("sqrt")      | qi::lit("normalCdf") | qi::lit("normalPdf")  | qi::lit("max")     | qi::lit("min")       | qi::lit("pow")  | qi::lit("black")
                           | qi::lit("dcf")       | qi::lit("days")      | qi::lit("PAY")        | qi::lit("NPVMEM")  | qi::lit("DISCOUNT")  | qi::lit("SIZE") | qi::lit("SORT")
                           | qi::lit("PERMUTE")   | qi::lit("LOGPAY")    | qi::lit("HISTFIXING") | qi::lit("FWDCOMP") | qi::lit("FWDAVG")    | qi::lit("ABOVEPROB")
			   | qi::lit("BELOWPROB") | qi::lit("NPV")    | qi::lit("DATEINDEX")
                           ) >> !(qi::alnum | qi::char_('_')) ];

    varname %= qi::lexeme[ (qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_')) ] - keyword;

    varexpr = ((varname >> '[') > term > ']')             [ createASTNode<VariableNode, std::string>(evalStack, 1) ]
              | varname                                   [ createASTNode<VariableNode, std::string>(evalStack, 0) ];

    instructionseq =   instruction   [ createASTNode<SequenceNode>(evalStack, 1) ]
                     > *(instruction [ collapseASTNode<SequenceNode>(evalStack, 1) ]);

    instruction = (assignment | require | declaration | ifthenelse | loop | sort | permute) > ";";

    declaration = "NUMBER" > varexpr [ createASTNode<DeclarationNumberNode>(evalStack, 1) ]
                           > *(',' > varexpr [ collapseASTNode<DeclarationNumberNode>(evalStack, 1) ]);

    ifthenelse = "IF" > condition > "THEN" > instructionseq >
                       ( qi::lit("END")                              [ createASTNode<IfThenElseNode>(evalStack, 2)]
                       | ("ELSE" > instructionseq > "END")           [ createASTNode<IfThenElseNode>(evalStack, 3)]);

    loop =
         ("FOR" > varname > "IN" > '(' > term > ',' > term > ',' > term > ')'
         > "DO" > instructionseq > "END")                           [ createASTNode<LoopNode, std::string>(evalStack, 4) ];

    assignment = varexpr > '=' > term                               [ createASTNode<AssignmentNode>(evalStack, 2) ];

    require = "REQUIRE" > condition                                 [ createASTNode<RequireNode>(evalStack, 1) ];

    sort = "SORT" > qi::lit('(') > (varexpr >
                            ( qi::lit(')')                             [ createASTNode<SortNode>(evalStack, 1) ]
                            | (qi::lit(',') > varexpr >
                              ( qi::lit(')')                            [ createASTNode<SortNode>(evalStack, 2) ]
                              | (qi::lit(',') > varexpr > qi::lit(')')) [ createASTNode<SortNode>(evalStack, 3) ]))));

    permute = "PERMUTE" > qi::lit('(') > (varexpr >
                            ( qi::lit(',') > varexpr >
                              ( qi::lit(')')                            [ createASTNode<PermuteNode>(evalStack, 2) ]
                              | (qi::lit(',') > varexpr > qi::lit(')')) [ createASTNode<PermuteNode>(evalStack, 3) ])));

    condition = condition2 > *(("OR" > condition2)   [ createASTNode<ConditionOrNode>(evalStack, 2, true)  ] );
    condition2 = condition3 > *(("AND" > condition3) [ createASTNode<ConditionAndNode>(evalStack, 2, true) ] );
    condition3 = ('{' > condition > '}')
        | (term > ( ("==" > term)    [ createASTNode<ConditionEqNode>(evalStack, 2)  ]
          | ("!=" > term)    [ createASTNode<ConditionNeqNode>(evalStack, 2) ]
          | (">=" > term)    [ createASTNode<ConditionGeqNode>(evalStack, 2) ]
          | (">"  > term)    [ createASTNode<ConditionGtNode>(evalStack, 2)  ]
          | ("<=" > term)    [ createASTNode<ConditionLeqNode>(evalStack, 2) ]
          | ("<"  > term)    [ createASTNode<ConditionLtNode>(evalStack, 2)  ] ));

    term     = product > *( ('+' > product) [ createASTNode<OperatorPlusNode>(evalStack, 2,true)      ]
                          | ('-' > product) [ createASTNode<OperatorMinusNode>(evalStack, 2, true)    ] );
    product  = factor > *(  ('*' > factor)  [ createASTNode<OperatorMultiplyNode>(evalStack, 2, true) ]
                          | ('/' > factor)  [ createASTNode<OperatorDivideNode>(evalStack, 2, true)   ] );
    factor   =   ('(' > term > ')')
        | (varexpr > -( '(' > varexpr >
                        ( (qi::lit(')')           [ createASTNode<VarEvaluationNode>(evalStack, 2)          ]
                        | (',' > varexpr > ')')   [ createASTNode<VarEvaluationNode>(evalStack, 3)          ] ))))
        | qi::double_                             [ createASTNode<ConstantNumberNode, double>(evalStack, 0) ]
        | ('-' > factor)                          [ createASTNode<NegateNode>(evalStack, 1)                 ]
        | (qi::lit("abs") > "(" > term > ')')                   [ createASTNode<FunctionAbsNode>(evalStack, 1)            ]
        | (qi::lit("exp") > "(" > term > ')')                   [ createASTNode<FunctionExpNode>(evalStack, 1)            ]
        | (qi::lit("ln") > "(" > term > ')')                    [ createASTNode<FunctionLogNode>(evalStack, 1)            ]
        | (qi::lit("sqrt") > "(" > term > ')')                  [ createASTNode<FunctionSqrtNode>(evalStack, 1)           ]
        | (qi::lit("normalCdf") > "(" > term > ')')             [ createASTNode<FunctionNormalCdfNode>(evalStack, 1)      ]
        | (qi::lit("normalPdf") > "(" > term > ')')             [ createASTNode<FunctionNormalPdfNode>(evalStack, 1)      ]
        | (qi::lit("max") > "(" > term > ',' > term > ')')      [ createASTNode<FunctionMaxNode>(evalStack, 2)            ]
        | (qi::lit("min") > "(" > term > ',' > term > ')')      [ createASTNode<FunctionMinNode>(evalStack, 2)            ]
        | (qi::lit("pow") > "(" > term > ',' > term > ')')      [ createASTNode<FunctionPowNode>(evalStack, 2)            ]
        | (qi::lit("black") > "(" > term > ',' > term > ',' > term > ',' > term > ',' > term > ',' > term > ')')
                                                         [ createASTNode<FunctionBlackNode>(evalStack, 6)          ]
        | (qi::lit("dcf") > "(" > varexpr > ',' > varexpr > ',' > varexpr > ')')
                                                         [ createASTNode<FunctionDcfNode>(evalStack, 3)            ]
        | (qi::lit("days") > "(" > varexpr > ',' > varexpr > ',' > varexpr > ')')
                                                         [ createASTNode<FunctionDaysNode>(evalStack, 3)           ]
        | (qi::lit("PAY") > "(" > term > ',' > term > ',' > term > ',' > term > ')')
                                                         [ createASTNode<FunctionPayNode>(evalStack, 4)            ]
        | (qi::lit("LOGPAY") > "(" > term > ',' > term > ',' > term > ',' > term >
           ( qi::lit(')')                                [ createASTNode<FunctionLogPayNode>(evalStack, 4)         ]
           | (qi::lit(',') > term > ',' > varexpr >
              ( qi::lit(')')                             [ createASTNode<FunctionLogPayNode>(evalStack, 6)         ]
              | (qi::lit(',') > term > ")")              [ createASTNode<FunctionLogPayNode>(evalStack, 7)         ]))))
        | (qi::lit("NPVMEM") > "(" > term > ',' > term > ',' > term >
                           ( qi::lit(')')                          [ createASTNode<FunctionNpvMemNode>(evalStack, 3)          ]
                             | (qi::lit(',') > condition >
                               ( qi::lit(')')                      [ createASTNode<FunctionNpvMemNode>(evalStack, 4)          ]
                                 | (qi::lit(',') > term >
                                   ( qi::lit(')')                  [ createASTNode<FunctionNpvMemNode>(evalStack, 5)          ]
                                     | (qi::lit(',') > term > ')') [ createASTNode<FunctionNpvMemNode>(evalStack, 6)          ]))))))
        | (qi::lit("NPV") > "(" > term > ',' > term >
                           ( qi::lit(')')                          [ createASTNode<FunctionNpvNode>(evalStack, 2)          ]
                             | (qi::lit(',') > condition >
                               ( qi::lit(')')                      [ createASTNode<FunctionNpvNode>(evalStack, 3)          ]
                                 | (qi::lit(',') > term >
                                   ( qi::lit(')')                  [ createASTNode<FunctionNpvNode>(evalStack, 4)          ]
                                     | (qi::lit(',') > term > ')')  [ createASTNode<FunctionNpvNode>(evalStack, 5)         ]))))))
        | (qi::lit("DISCOUNT") > "(" > term > ',' > term > ',' > term > ')')
                                                         [ createASTNode<FunctionDiscountNode>(evalStack, 3)       ]
        | (qi::lit("SIZE") > "(" > varname > ')')        [ createASTNode<SizeOpNode, std::string>(evalStack, 0)    ]
        | (qi::lit("HISTFIXING") > "(" > varexpr > ',' > varexpr > ')')
                                                         [ createASTNode<HistFixingNode>(evalStack, 2)             ]
        | (qi::lit("FWDCOMP") > "(" > varexpr > ',' > varexpr > ',' > varexpr > ',' > varexpr >
           ( qi::lit(')')                                                                   [ createASTNode<FunctionFwdCompNode>(evalStack, 4)  ]
             | (qi::lit(',') > term > ',' > term >
		( qi::lit(')')                                                              [ createASTNode<FunctionFwdCompNode>(evalStack, 6)  ]
                  | (qi::lit(',') > term > ',' > term > ',' > term > ',' > term >
		     ( qi::lit(')')                                                         [ createASTNode<FunctionFwdCompNode>(evalStack, 10) ]
                       | (qi::lit(',') > term > ',' > term > ',' > term > ',' > term > ')') [ createASTNode<FunctionFwdCompNode>(evalStack, 14) ]))))))
        | (qi::lit("FWDAVG") > "(" > varexpr > ',' > varexpr > ',' > varexpr > ',' > varexpr >
           ( qi::lit(')')                                                                   [ createASTNode<FunctionFwdAvgNode>(evalStack, 4)  ]
             | (qi::lit(',') > term > ',' > term >
		( qi::lit(')')                                                              [ createASTNode<FunctionFwdAvgNode>(evalStack, 6)  ]
                  | (qi::lit(',') > term > ',' > term > ',' > term > ',' > term >
		     ( qi::lit(')')                                                         [ createASTNode<FunctionFwdAvgNode>(evalStack, 10) ]
                       | (qi::lit(',') > term > ',' > term > ',' > term > ',' > term > ')') [ createASTNode<FunctionFwdAvgNode>(evalStack, 14) ]))))))
        | (qi::lit("ABOVEPROB") > "(" > varexpr > ',' > varexpr > ',' > varexpr > ',' > term > ")")
                                                         [ createASTNode<FunctionAboveProbNode>(evalStack, 4)      ]
        | (qi::lit("BELOWPROB") > "(" > varexpr > ',' > varexpr > ',' > varexpr > ',' > term > ")")
                                                         [ createASTNode<FunctionBelowProbNode>(evalStack, 4)      ]
        | (qi::lit("DATEINDEX") > "(" > varexpr > ',' > varname > ',' > varname > ")")
             [ createASTNode<FunctionDateIndexNode, boost::fusion::vector<std::string, std::string>>(evalStack, 1) ];

    // clang-format on

    // name rules
    varname.name("VarName");
    varexpr.name("VarExpr");
    instructionseq.name("InstructionSequence");
    instruction.name("Instruction");
    declaration.name("Declaration");
    ifthenelse.name("IfThenElse");
    loop.name("Loop");
    assignment.name("Assignment");
    require.name("Require");
    sort.name("Sort");
    sort.name("Permute");
    condition.name("Condition");
    condition2.name("Condition2");
    condition3.name("Condition3");
    term.name("Term");
    product.name("Product");
    factor.name("Factor");

    // ast node annotation
    auto set_location_info = phoenix::bind(&ASTNodeAnnotation::operator(), &annotate, qi::_1, qi::_3);
    qi::on_success(varexpr, set_location_info);
    qi::on_success(instructionseq, set_location_info);
    qi::on_success(instruction, set_location_info);
    qi::on_success(declaration, set_location_info);
    qi::on_success(ifthenelse, set_location_info);
    qi::on_success(loop, set_location_info);
    qi::on_success(assignment, set_location_info);
    qi::on_success(require, set_location_info);
    qi::on_success(sort, set_location_info);
    qi::on_success(permute, set_location_info);
    qi::on_success(condition, set_location_info);
    qi::on_success(condition2, set_location_info);
    qi::on_success(condition3, set_location_info);
    qi::on_success(term, set_location_info);
    qi::on_success(product, set_location_info);
    qi::on_success(factor, set_location_info);

    // error handling
    auto errorHandler =
        (phoenix::ref(hasError) = true, phoenix::ref(errorBegin) = qi::_1, phoenix::ref(errorEnd) = qi::_2,
         phoenix::ref(errorPos) = qi::_3, phoenix::ref(errorWhat) = qi::_4);
    qi::on_error<qi::fail>(instructionseq, errorHandler);
}

void ASTNodeAnnotation::operator()(const ScriptGrammarIterator f, const ScriptGrammarIterator l) const {
    QL_REQUIRE(!evalStack_.empty(), "eval stack is empty");
    ASTNodePtr& n = evalStack_.top();
    n->locationInfo.initialised = true;
    n->locationInfo.lineStart = boost::spirit::get_line(f);
    n->locationInfo.columnStart = boost::spirit::get_column(first_, f);
    n->locationInfo.lineEnd = boost::spirit::get_line(l);
    n->locationInfo.columnEnd = boost::spirit::get_column(first_, l);
}

} // namespace data
} // namespace end
