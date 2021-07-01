/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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
