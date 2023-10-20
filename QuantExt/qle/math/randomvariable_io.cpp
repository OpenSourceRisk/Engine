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

#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_io.hpp>

namespace QuantExt {

namespace {

int get_index_randomvariable_output_size() {
    static int index = std::ios_base::xalloc();
    return index;
}

int get_index_randomvariable_output_pattern() {
    static int index = std::ios_base::xalloc();
    return index;
}

} // namespace

std::ostream& operator<<(std::ostream& out, const randomvariable_output_size& r) {
    out.iword(get_index_randomvariable_output_size()) = static_cast<long>(r.n());
    return out;
}

std::ostream& operator<<(std::ostream& out, const randomvariable_output_pattern& p) {
    out.iword(get_index_randomvariable_output_pattern()) = static_cast<long>(p.getPattern());
    return out;
}

namespace {

const RandomVariable& convertToRandomVariable(const RandomVariable& c) { return c; }
RandomVariable convertToRandomVariable(const Filter& c) { return RandomVariable(c); }

template <typename C> std::ostream& output(std::ostream& out, const C& c) {
    if (!c.initialised()) {
        out << "na";
    } else if (c.deterministic()) {
        out << std::boolalpha << c.at(0);
    } else {

        // get format flags

        long os = out.iword(get_index_randomvariable_output_size());
        long pt = out.iword(get_index_randomvariable_output_pattern());

        // set final format flags

        Size n = std::min<Size>(c.size(), os == 0 ? 10 : static_cast<Size>(os));
        randomvariable_output_pattern::pattern p = pt == 0 ? randomvariable_output_pattern::pattern::left
                                                           : static_cast<randomvariable_output_pattern::pattern>(pt);

        // output random variable or filter

        if (p == randomvariable_output_pattern::left) {
            out << "[";
            for (Size i = 0; i < n; ++i)
                out << c.at(i) << (i < n - 1 ? "," : "");
            if (n < c.size())
                out << "...";
            out << "]";
        } else if (p == randomvariable_output_pattern::left_middle_right) {
            out << "[";
            Size s = std::max<Size>(n / 3, 1);
            if (c.size() <= 3 * s) {
                for (Size i = 0; i < c.size(); ++i)
                    out << c.at(i) << (i < c.size() - 1 ? "," : "");
            } else {
                for (Size i = 0; i < s; ++i)
                    out << c.at(i) << ",";
                out << "...,";
                for (Size i = c.size() / 2 - s / 2; i < c.size() / 2 - s / 2 + s; ++i)
                    out << c.at(i) << ",";
                out << "...,";
                for (Size i = c.size() - s; i < c.size(); ++i)
                    out << c.at(i) << (i < c.size() - 1 ? "," : "");
                out << "]";
            }
        } else if (p == randomvariable_output_pattern::expectation) {
            out << expectation(convertToRandomVariable(c)) << (!c.deterministic() ? " (avg)" : "");
        } else {
            out << "<unknown output pattern>";
        }
    }

    return out;
}
} // namespace

std::ostream& operator<<(std::ostream& out, const Filter& f) { return output(out, f); }

std::ostream& operator<<(std::ostream& out, const RandomVariable& r) {
    output(out, r);
    if (r.time() != Null<Real>())
        out << " t=" << r.time();
    return out;
}

} // namespace QuantExt
