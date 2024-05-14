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

/*! \file ored/scripting/safestack.hpp
    \brief stack with safety checks and pop() that returns rvalue reference of top element
    \ingroup utilities
*/

#pragma once

#include <ql/errors.hpp>

#include <stack>

namespace ore {
namespace data {

template <typename T> class SafeStack {
public:
    const T& top() const { return stack_.top(); }
    T& top() {
        QL_REQUIRE(!stack_.empty(), "SafeStack::top(): empty stack");
        return stack_.top();
    }
    T pop() {
        T v(std::move(top()));
        stack_.pop();
        return v;
    }
    bool empty() const { return stack_.empty(); }
    typename std::stack<T>::size_type size() const { return stack_.size(); }
    void push(const T& t) { stack_.push(t); }
    void push(T&& t) { stack_.push(std::forward<T>(t)); }
    template <typename... Args> void emplace(Args&&... t) { stack_.emplace(std::forward<Args>(t)...); }
    void swap(const SafeStack<T>& other) { stack_.swap(other.stack_); }

private:
    std::stack<T> stack_;
};

} // namespace data
} // namespace ore
