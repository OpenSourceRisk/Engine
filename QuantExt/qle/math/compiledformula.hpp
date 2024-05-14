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

/*! \file qle/math/compiledformula.hpp
    \brief compiled formula
    \ingroup indexes
*/

#pragma once

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>
#include <ql/types.hpp>
#include <ql/utilities/null.hpp>

#include <vector>

using QuantLib::Null;
using QuantLib::Real;
using QuantLib::Size;

namespace QuantExt {

//! helper class representing a formula with variables given by an id v
class CompiledFormula {
public:
    enum Operator { none, plus, minus, multiply, divide, max, min, pow, abs, gtZero, geqZero, negate, exp, log };
    CompiledFormula() : op_(none), x_(0.0), v_(Null<Size>()) {}
    // ctor representing a formula that is a constant value x
    CompiledFormula(const Real x) : op_(none), x_(x), v_(Null<Size>()) {}
    // ctor representing a formula that is a variable with index v
    CompiledFormula(const Size v) : op_(none), x_(Null<Real>()), v_(v) {}
    CompiledFormula(const CompiledFormula& f) : op_(f.op_), x_(f.x_), v_(f.v_), args_(f.args_) {}
    CompiledFormula(CompiledFormula&& f) : op_(f.op_), x_(f.x_), v_(f.v_) { args_.swap(f.args_); }
    CompiledFormula& operator=(const CompiledFormula&);
    CompiledFormula& operator=(CompiledFormula&&);

    // evaluate given values for the variables at index 0, 1, 2, ...
    template <class I> Real operator()(I begin, I end) const;
    Real operator()(const std::vector<Real>& values) const { return operator()(values.begin(), values.end()); }

    // Operators
    CompiledFormula& operator+=(const CompiledFormula&);
    CompiledFormula& operator-=(const CompiledFormula&);
    CompiledFormula& operator*=(const CompiledFormula&);
    CompiledFormula& operator/=(const CompiledFormula&);
    //
    CompiledFormula operator-() const;
    //
    friend CompiledFormula operator+(CompiledFormula, const CompiledFormula&);
    friend CompiledFormula operator-(CompiledFormula, const CompiledFormula&);
    friend CompiledFormula operator*(CompiledFormula, const CompiledFormula&);
    friend CompiledFormula operator/(CompiledFormula, const CompiledFormula&);
    //
    friend CompiledFormula max(CompiledFormula, const CompiledFormula&);
    friend CompiledFormula min(CompiledFormula, const CompiledFormula&);
    friend CompiledFormula pow(CompiledFormula, const CompiledFormula&);
    //
    friend CompiledFormula gtZero(CompiledFormula);
    friend CompiledFormula geqZero(CompiledFormula);
    friend CompiledFormula abs(CompiledFormula);
    friend CompiledFormula exp(CompiledFormula);
    friend CompiledFormula log(CompiledFormula);

private:
    friend CompiledFormula unaryOp(CompiledFormula, Operator op);
    friend CompiledFormula binaryOp(CompiledFormula, const CompiledFormula&, Operator op);
    Operator op_;
    Real x_;
    Size v_;
    std::vector<CompiledFormula> args_;
};

// implementation

template <class I> Real CompiledFormula::operator()(I begin, I end) const {
    if (x_ != Null<Real>())
        return x_;
    if (v_ != Null<Size>()) {
        QL_REQUIRE((end - begin) > static_cast<int>(v_),
                   "CompiledFormula: need value for index " << v_ << ", given values size is " << (end - begin));
        return *(begin + v_);
    }
    switch (op_) {
    case plus:
        return args_[0](begin, end) + args_[1](begin, end);
    case minus:
        return args_[0](begin, end) - args_[1](begin, end);
    case multiply:
        return args_[0](begin, end) * args_[1](begin, end);
    case divide:
        return args_[0](begin, end) / args_[1](begin, end);
    case max:
        return std::max(args_[0](begin, end), args_[1](begin, end));
    case min:
        return std::min(args_[0](begin, end), args_[1](begin, end));
    case pow:
        return std::pow(args_[0](begin, end), args_[1](begin, end));
    case gtZero: {
        Real tmp = args_[0](begin, end);
        return tmp > 0.0 && !QuantLib::close_enough(tmp, 0.0) ? 1.0 : 0.0;
    }
    case geqZero: {
        Real tmp = args_[0](begin, end);
        return tmp > 0.0 || QuantLib::close_enough(tmp, 0.0) ? 1.0 : 0.0;
    }
    case abs:
        return std::abs(args_[0](begin, end));
    case negate:
        return -args_[0](begin, end);
    case exp:
        return std::exp(args_[0](begin, end));
    case log:
        return std::log(args_[0](begin, end));
    default:
        QL_FAIL("CompiledFormula: unknown operator");
    }
}

} // namespace QuantExt
