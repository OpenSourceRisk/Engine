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

#include <ql/shared_ptr.hpp>

namespace QuantExt {

template <class T>
void forwardEvaluation(const ComputationGraph& g, std::vector<T>& values,
                       const std::vector<std::function<T(const std::vector<const T*>&)>>& ops,
                       std::function<void(T&)> deleter = {}, bool keepValuesForDerivatives = true,
                       const std::vector<std::function<std::pair<std::vector<bool>, bool>(const std::size_t)>>&
                           opRequiresNodesForDerivatives = {},
                       const std::vector<bool>& keepNodes = {}, const std::size_t startNode = 0,
                       const std::size_t endNode = ComputationGraph::nan, const bool redBlockReconstruction = false,
                       std::function<void(T&)> preDeleter = {}, const std::vector<bool>& opAllowsPredeletion = {}) {

    std::vector<bool> keepNodesDerivatives;
    if (deleter && keepValuesForDerivatives)
        keepNodesDerivatives = std::vector<bool>(g.size(), false);

    // loop over the nodes in the graph in ascending order

    for (std::size_t node = startNode; node < (endNode == ComputationGraph::nan ? g.size() : endNode); ++node) {

        // if a node is computed by an op applied to predecessors ...

        if (!g.predecessors(node).empty()) {

            // evaluate the node

            std::vector<const T*> args(g.predecessors(node).size());
            for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                args[arg] = &values[g.predecessors(node)[arg]];
            }

            std::set<std::size_t> nodesToDelete;
            if (deleter) {
                for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                    std::size_t p = g.predecessors(node)[arg];

                    if (!keepNodesDerivatives.empty()) {

                        // is the node required to compute derivatives, then add it to the keep nodes vector

                        if (opRequiresNodesForDerivatives[g.opId(p)](args.size()).second ||
                            opRequiresNodesForDerivatives[g.opId(node)](args.size()).first[arg])
                            keepNodesDerivatives[p] = true;
                    }

                    // is the node no longer needed for the forward evaluation?

                    if (g.maxNodeRequiringArg(p) > node)
                        continue;

                    // is the node marked as to be kept ?

                    if ((!keepNodes.empty() && keepNodes[p]) ||
                        (!keepNodesDerivatives.empty() && keepNodesDerivatives[p] &&
                         (g.redBlockId(p) == 0 || redBlockReconstruction)))
                        continue;

                    // apply the deleter

                    nodesToDelete.insert(p);

                } // for arg over g.predecessors
            }

            if (preDeleter && !opAllowsPredeletion.empty() && opAllowsPredeletion[g.opId(node)]) {
                for(auto n: nodesToDelete)
                    preDeleter(values[n]);
            }

            values[node] = ops[g.opId(node)](args);

            QL_REQUIRE(values[node].initialised(), "forwardEvaluation(): value at active node "
                                                       << node << " is not initialized, opId = " << g.opId(node));

            if(deleter) {
                for(auto n: nodesToDelete)
                    deleter(values[n]);
            }

        }     // if !g.predecessors empty
    }         // for node
}

} // namespace QuantExt
