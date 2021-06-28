/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#pragma once

#include <ql/types.hpp>

#include <ostream>

namespace QuantExt {

struct Filter;
struct RandomVariable;

using namespace QuantLib;

std::ostream& operator<<(std::ostream& out, const Filter&);
std::ostream& operator<<(std::ostream& out, const RandomVariable& a);

class randomvariable_output_size {
public:
    randomvariable_output_size() : n_(10) {}
    explicit randomvariable_output_size(const Size n) : n_(n) {}
    Size n() const { return n_; }

private:
    Size n_;
};

class randomvariable_output_pattern {
public:
    enum pattern : long { left, left_middle_right, expectation };
    randomvariable_output_pattern() : pattern_(pattern::left) {}
    explicit randomvariable_output_pattern(const pattern p) : pattern_(p) {}
    pattern getPattern() const { return pattern_; }

private:
    pattern pattern_;
};

std::ostream& operator<<(std::ostream& out, const randomvariable_output_size&);
std::ostream& operator<<(std::ostream& out, const randomvariable_output_pattern&);

} // namespace QuantExt
