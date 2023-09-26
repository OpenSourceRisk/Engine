/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/ad/ssaform.hpp>

#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_io.hpp>

#include <sstream>

namespace QuantExt {

namespace {
std::string getLabel(const ComputationGraph& g, const std::size_t i, const bool includeLabels = true) {
    auto l = g.labels().find(i);
    std::string label = "v_" + std::to_string(i);
    if (l != g.labels().end() && includeLabels) {
        for (auto tmp = l->second.begin(); tmp != l->second.end(); ++tmp) {
            label = label + (tmp == l->second.begin() ? "[" : "") + *tmp +
                    (tmp != std::next(l->second.end(), -1) ? "," : "]");
        }
    }
    return label;
}
} // namespace

template <class T>
std::string ssaForm(const ComputationGraph& g, const std::vector<std::string>& opCodeLabels,
                    const std::vector<T>& values) {

    std::ostringstream os;

    for (std::size_t i = 0; i < g.size(); ++i) {

        os << i << ": " << getLabel(g, i);

        if (!g.predecessors(i).empty()) {
            os << " = ";
            os << (opCodeLabels.size() > g.opId(i) ? opCodeLabels[g.opId(i)] : "???") << "(";
            for (std::size_t j = 0; j < g.predecessors(i).size(); ++j) {
                auto p = g.predecessors(i)[j];
                os << getLabel(g, p, false) << (j < g.predecessors(i).size() - 1 ? ", " : "");
            }
            os << ")";
        }

        if (values.size() > i) {
            os << "  ->  " << values[i];
        }

        os << "\n";
    }

    return os.str();
}

template std::string ssaForm(const ComputationGraph& g, const std::vector<std::string>& opCodeLabels,
                             const std::vector<double>& values);
template std::string ssaForm(const ComputationGraph& g, const std::vector<std::string>& opCodeLabels,
                             const std::vector<RandomVariable>& values);

} // namespace QuantExt
