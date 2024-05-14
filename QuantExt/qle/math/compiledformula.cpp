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

#include <qle/math/compiledformula.hpp>

namespace QuantExt {

CompiledFormula& CompiledFormula::operator=(const CompiledFormula& f) {
    op_ = f.op_;
    x_ = f.x_;
    v_ = f.v_;
    args_ = f.args_;
    return *this;
}

CompiledFormula& CompiledFormula::operator=(CompiledFormula&& f) {
    op_ = f.op_;
    x_ = f.x_;
    v_ = f.v_;
    args_.swap(f.args_);
    return *this;
}

//

CompiledFormula& CompiledFormula::operator+=(const CompiledFormula& y) {
    std::vector<CompiledFormula> newArgs;
    newArgs.push_back(*this);
    newArgs.push_back(y);
    op_ = plus;
    x_ = Null<Real>();
    v_ = Null<Size>();
    args_.swap(newArgs);
    return *this;
}

CompiledFormula& CompiledFormula::operator-=(const CompiledFormula& y) {
    std::vector<CompiledFormula> newArgs;
    newArgs.push_back(*this);
    newArgs.push_back(y);
    op_ = minus;
    x_ = Null<Real>();
    v_ = Null<Size>();
    args_.swap(newArgs);
    return *this;
}

CompiledFormula& CompiledFormula::operator*=(const CompiledFormula& y) {
    std::vector<CompiledFormula> newArgs;
    newArgs.push_back(*this);
    newArgs.push_back(y);
    op_ = multiply;
    x_ = Null<Real>();
    v_ = Null<Size>();
    args_.swap(newArgs);
    return *this;
}

CompiledFormula& CompiledFormula::operator/=(const CompiledFormula& y) {
    std::vector<CompiledFormula> newArgs;
    newArgs.push_back(*this);
    newArgs.push_back(y);
    op_ = divide;
    x_ = Null<Real>();
    v_ = Null<Size>();
    args_.swap(newArgs);
    return *this;
}

//

CompiledFormula CompiledFormula::operator-() const {
    CompiledFormula r;
    r.args_ = std::vector<CompiledFormula>(1, *this);
    r.op_ = negate;
    r.x_ = Null<Real>();
    r.v_ = Null<Size>();
    return r;
}

//

CompiledFormula operator+(CompiledFormula x, const CompiledFormula& y) {
    x += y;
    return x;
}

CompiledFormula operator-(CompiledFormula x, const CompiledFormula& y) {
    x -= y;
    return x;
}

CompiledFormula operator*(CompiledFormula x, const CompiledFormula& y) {
    x *= y;
    return x;
}

CompiledFormula operator/(CompiledFormula x, const CompiledFormula& y) {
    x /= y;
    return x;
}

CompiledFormula unaryOp(CompiledFormula x, CompiledFormula::Operator op) {
    std::vector<CompiledFormula> newArgs;
    newArgs.push_back(x);
    x.op_ = op;
    x.x_ = Null<Real>();
    x.v_ = Null<Size>();
    x.args_.swap(newArgs);
    return x;
}

CompiledFormula binaryOp(CompiledFormula x, const CompiledFormula& y, CompiledFormula::Operator op) {
    std::vector<CompiledFormula> newArgs;
    newArgs.push_back(x);
    newArgs.push_back(y);
    x.op_ = op;
    x.x_ = Null<Real>();
    x.v_ = Null<Size>();
    x.args_.swap(newArgs);
    return x;
}

//

// cppcheck-suppress passedByValue
CompiledFormula gtZero(CompiledFormula x) { return unaryOp(x, CompiledFormula::gtZero); }

// cppcheck-suppress passedByValue
CompiledFormula geqZero(CompiledFormula x) { return unaryOp(x, CompiledFormula::geqZero); }

// cppcheck-suppress passedByValue
CompiledFormula abs(CompiledFormula x) { return unaryOp(x, CompiledFormula::abs); }

// cppcheck-suppress passedByValue
CompiledFormula exp(CompiledFormula x) { return unaryOp(x, CompiledFormula::exp); }

// cppcheck-suppress passedByValue
CompiledFormula log(CompiledFormula x) { return unaryOp(x, CompiledFormula::log); }

//

// cppcheck-suppress passedByValue
CompiledFormula max(CompiledFormula x, const CompiledFormula& y) { return binaryOp(x, y, CompiledFormula::max); }

// cppcheck-suppress passedByValue
CompiledFormula min(CompiledFormula x, const CompiledFormula& y) { return binaryOp(x, y, CompiledFormula::min); }

// cppcheck-suppress passedByValue
CompiledFormula pow(CompiledFormula x, const CompiledFormula& y) { return binaryOp(x, y, CompiledFormula::pow); }

} // namespace QuantExt
