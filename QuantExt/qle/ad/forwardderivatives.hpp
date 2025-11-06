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

/*! \file qle/ad/forwardderivatives.hpp
    \brief forward derivatives computation
*/

#pragma once

#include <qle/ad/computationgraph.hpp>

#include <ql/shared_ptr.hpp>

namespace QuantExt {

/* Note: This formulation assumes a separate forward run to calculate the values. We could combine the calculation of
   the values and derivatives and apply the deleter to improve memory consumption */

template <class T>
void forwardDerivatives(
    const ComputationGraph& g, std::vector<T>& values, std::vector<T>& derivatives,
    const std::vector<std::function<std::vector<T>(const std::vector<const T*>&, const T*, const QuantLib::Size)>>&
        grad,
    std::function<void(T&)> deleter = {}, const std::vector<bool>& keepNodes = {},
    const std::vector<std::function<T(const std::vector<const T*>&, const QuantLib::Size)>>& fwdOps = {},
    const std::vector<std::function<std::pair<std::vector<bool>, bool>(const std::size_t)>>&
        fwdOpRequiresNodesForDerivatives = {},
    const std::vector<bool>& fwdOpKeepNodes = {}, const std::size_t conditionalExpectationOpId = 0,
    const std::function<T(const std::vector<const T*>&, const QuantLib::Size)>& conditionalExpectation = {},
    std::function<void(T&)> fwdOpPreDeleter = {}, const std::vector<bool>& fwdOpAllowsPredeletion = {}) {

    if (g.size() == 0)
        return;

    std::size_t redBlockId = 0;

    // loop over the nodes in the graph in forward order

    for (std::size_t node = 0; node < g.size(); ++node) {

        if (g.redBlockId(node) != redBlockId) {

            // delete the values in the previous red block

            auto range = g.redBlockRanges()[redBlockId - 1];
            QL_REQUIRE(range.second != ComputationGraph::nan,
                       "forwardDerivatives(): red block " << redBlockId << " was not closed.");
            for (std::size_t n = range.first; n < range.second; ++n) {
                if (g.redBlockId(n) == redBlockId && !fwdOpKeepNodes[n])
                    deleter(values[n]);
            }

            // populate the values in the current red block

            if (g.redBlockId(node) > 0) {
                auto range = g.redBlockRanges()[g.redBlockId(node) - 1];
                QL_REQUIRE(range.second != ComputationGraph::nan,
                           "forwardDerivatives(): red block " << g.redBlockId(node) << " was not closed.");
                forwardEvaluation(g, values, fwdOps, deleter, true, fwdOpRequiresNodesForDerivatives, fwdOpKeepNodes,
                                  range.first, range.second, true, fwdOpPreDeleter, fwdOpAllowsPredeletion);
            }

            // update the red block id

            redBlockId = g.redBlockId(node);
        }

        if (!g.predecessors(node).empty()) {

            // propagate the derivatives from predecessors of a node to the node

            std::vector<const T*> args(g.predecessors(node).size());
            for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                args[arg] = &values[g.predecessors(node)[arg]];
            }

            if (g.opId(node) == conditionalExpectationOpId && conditionalExpectation) {

                args[0] = &derivatives[g.predecessors(node)[0]];
                derivatives[node] = conditionalExpectation(args, node);

            } else {

                auto gr = grad[g.opId(node)](args, &values[node], node);

                for (std::size_t p = 0; p < g.predecessors(node).size(); ++p) {
                    derivatives[node] += derivatives[g.predecessors(node)[p]] * gr[p];
                }
            }

            // the check if we can delete the predecessors

            if (deleter) {
                for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                    std::size_t p = g.predecessors(node)[arg];

                    // is the node no longer needed for other target nodes?

                    if (g.maxNodeRequiringArg(p) > node)
                        continue;

                    // is the node marked as to be kept ?

                    if (!keepNodes.empty() && keepNodes[p])
                        continue;

                    // apply the deleter

                    deleter(derivatives[p]);

                } // for arg over g.predecessors
            }     // if deleter
        }         // if !g.predecessors empty
    }             // for node
}

} // namespace QuantExt
