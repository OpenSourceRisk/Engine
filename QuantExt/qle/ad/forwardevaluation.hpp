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

/*! \file qle/ad/forwardevaluation.hpp
    \brief forward evaluation
*/

#pragma once

#include <qle/ad/computationgraph.hpp>

#include <boost/shared_ptr.hpp>

namespace QuantExt {

template <class T>
void forwardEvaluation(const ComputationGraph& g, std::vector<T>& values,
                       const std::vector<std::function<T(const std::vector<const T*>&)>>& ops,
                       std::function<void(T&)> deleter = {}, bool keepValuesForDerivatives = true,
                       const std::vector<std::function<std::pair<std::vector<bool>, bool>(const std::size_t)>>&
                           opRequiresNodesForDerivatives = {},
                       const std::vector<bool>& keepNodes = {}) {

    std::vector<bool> keepNodesInternal =
        !keepNodes.empty()
            ? keepNodes
            : (deleter && keepValuesForDerivatives ? std::vector<bool>(g.size(), false) : std::vector<bool>());

    // loop over the nodes in the graph in ascending order

    for (std::size_t node = 0; node < g.size(); ++node) {

        // if a node is computed by an op applied to predecessors ...

        if (!g.predecessors(node).empty()) {

            // evaluate the node

            std::vector<const T*> args(g.predecessors(node).size());
            for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                args[arg] = &values[g.predecessors(node)[arg]];
            }
            values[node] = ops[g.opId(node)](args);

            // then check if we can delete the predecessors

            if (deleter) {
                for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                    std::size_t p = g.predecessors(node)[arg];

                    if (keepValuesForDerivatives) {

                        // is the node required to compute derivatives, then add it to the keep nodes vector

                        if (opRequiresNodesForDerivatives[g.opId(p)](args.size()).second ||
                            opRequiresNodesForDerivatives[g.opId(node)](args.size()).first[arg])
                            keepNodesInternal[p] = true;
                    }

                    // is the node no longer needed for the forward evaluation?

                    if (g.maxNodeRequiringArg(p) > node)
                        continue;

                    // is the node marked as to be kept ?

                    if (!keepNodesInternal.empty() && keepNodesInternal[p])
                        continue;

                    // apply the deleter

                    deleter(values[p]);

                } // for arg over g.predecessors
            }     // if deleter
        }         // if !g.predecessors empty
    }             // for node
}

} // namespace QuantExt
