/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/formulaparser.hpp
    \brief generic formula parser
    \ingroup portfolio
*/

#pragma once

#include <qle/math/compiledformula.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/spirit/include/qi.hpp>

#include <stack>
#include <string>

namespace ore {
namespace data {

/*! evaluate arithmetic expression, T must provide operators
  T+T, T-T, -T, T*T, T/T,
  abs(T), exp(T), gtZero(T), geqZero(T), log(T), max(T,T), min(T,T), pow(T,T)
  variables are written as "{variable}"
*/
template <class T> T parseFormula(const std::string& text, const std::function<T(std::string)>& variableMapping = {});

/*! parse formula and store it as a CompiledFormula instance, the variables vector contains the label
  of the variables for each index */
QuantExt::CompiledFormula parseFormula(const std::string& text, std::vector<std::string>& variables);

/* the formula parser provides a few non-standard operators which are not defined for T = double, for convenience we
 provide them here; */
inline double gtZero(const double x) { return x > 0.0 && !QuantLib::close_enough(x, 0.0) ? 1.0 : 0.0; }
inline double geqZero(const double x) { return x > 0.0 || QuantLib::close_enough(x, 0.0) ? 1.0 : 0.0; }
inline double max(const double x, const double y) { return std::max(x, y); }
inline double min(const double x, const double y) { return std::min(x, y); }

using std::abs;

// implementation
template <class T> T parseFormula(const std::string& text, const std::function<T(std::string)>& variableMapping) {

    namespace fusion = boost::fusion;
    namespace phoenix = boost::phoenix;
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    typedef std::string::const_iterator Iterator;

    using ascii::alnum;
    using ascii::char_;
    using ascii::space;
    using qi::double_;
    using qi::lexeme;
    using qi::lit;
    using namespace qi::labels;

    qi::rule<Iterator, std::string(), ascii::space_type> variable;
    qi::rule<Iterator, ascii::space_type> factor;
    qi::rule<Iterator, ascii::space_type> term;
    qi::rule<Iterator, ascii::space_type> expression;

    std::stack<T> evalStack;

    auto pushConstant = [&evalStack](const double x) { evalStack.push(T(x)); };
    auto pushVariable = [&variableMapping, &evalStack, &text](const std::string& s) {
        QL_REQUIRE(variableMapping, "parseFormula(" << text << "): could not resolve variable \"" << s
                                                    << "\", because no variable mapping is given");
        evalStack.push(variableMapping(s));
    };

    struct doUnaryOp {
        doUnaryOp(std::stack<T>& evalStack, const std::function<T(T)>& op) : evalStack_(evalStack), op_(op) {}
        void operator()() const {
            QL_REQUIRE(evalStack_.size() > 0, "parseFormula(): internal error, empty stack for unary operation");
            T x = evalStack_.top();
            evalStack_.pop();
            T y = op_(x);
            evalStack_.push(y);
        }
        std::stack<T>& evalStack_;
        const std::function<T(T)> op_;
    };

    struct doBinaryOp {
        doBinaryOp(std::stack<T>& evalStack, const std::function<T(T, T)>& op) : evalStack_(evalStack), op_(op) {}
        void operator()() const {
            QL_REQUIRE(evalStack_.size() > 1,
                       "parseFormula(): internal error, stack size too small for binary operation");
            T rhs = evalStack_.top();
            evalStack_.pop();
            T lhs = evalStack_.top();
            evalStack_.pop();
            evalStack_.push(op_(lhs, rhs));
        }
        std::stack<T>& evalStack_;
        const std::function<T(T, T)> op_;
    };

    auto doNegate = doUnaryOp(evalStack, [](const T& x) { return -x; });
    auto doAbs = doUnaryOp(evalStack, [](const T& x) { return abs(x); });
    auto doGtZero = doUnaryOp(evalStack, [](const T& x) { return gtZero(x); });
    auto doGeqZero = doUnaryOp(evalStack, [](const T& x) { return geqZero(x); });
    auto doExp = doUnaryOp(evalStack, [](const T& x) { return exp(x); });
    auto doLog = doUnaryOp(evalStack, [](const T& x) { return log(x); });
    //
    auto doMultiply = doBinaryOp(evalStack, [](const T& x, const T& y) { return x * y; });
    auto doDivide = doBinaryOp(evalStack, [](const T& x, const T& y) { return x / y; });
    auto doPlus = doBinaryOp(evalStack, [](const T& x, const T& y) { return x + y; });
    auto doMinus = doBinaryOp(evalStack, [](const T& x, const T& y) { return x - y; });
    auto doMax = doBinaryOp(evalStack, [](const T& x, const T& y) { return max(x, y); });
    auto doMin = doBinaryOp(evalStack, [](const T& x, const T& y) { return min(x, y); });
    auto doPow = doBinaryOp(evalStack, [](const T& x, const T& y) { return pow(x, y); });

    // clang-format off

    variable      = '{' >> lexeme[+(char_ - '}') [_val += qi::labels::_1] ] >> '}';
    factor        =    double_    [ pushConstant ]
                     | variable   [ pushVariable ]
                     | '(' >> expression >> ')'
                     | '-' >> factor [ doNegate ]
                     | lit("abs(") >> expression [ doAbs ] >> ')'
                     | lit("exp(") >> expression [ doExp ] >> ')'
                     | lit("gtZero(") >> expression [ doGtZero ] >> ')'
                     | lit("geqZero(") >> expression [ doGeqZero ] >> ')'
                     | lit("log(") >> expression [ doLog ] >> ')'
                     | lit("max(") >> expression >> ',' >> expression [ doMax ] >> ')'
                     | lit("min(") >> expression >> ',' >> expression [ doMin ] >> ')'
                     | lit("pow(") >> expression >> ',' >> expression [ doPow ] >> ')';
    term          =     factor >> *(   ('*' >> factor) [ doMultiply ]
                                     | ('/' >> factor) [ doDivide ] );
    expression    =     term >> *(     ('+' >> term) [ doPlus ]
                                     | ('-' >> term) [ doMinus ]);

    // clang-format on

    std::string::const_iterator iter = text.begin();
    std::string::const_iterator end = text.end();
    bool r = phrase_parse(iter, end, expression, space);

    if (r && iter == end) {
        return evalStack.top();
    } else {
        std::string::const_iterator some = iter + 30;
        std::string context(iter, (some > end) ? end : some);
        QL_FAIL("parseFormula(" << text << "): parsing failed, stopped at \"" + context + "...\"");
    }
    return evalStack.top();
}

} // namespace data
} // namespace ore
