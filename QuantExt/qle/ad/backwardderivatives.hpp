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

/*! \file qle/ad/backwardderivatives.hpp
    \brief backward derivatives computation
*/

#pragma once

#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/errors.hpp>
#include <ql/shared_ptr.hpp>

namespace QuantExt {

/* A possible future optimization: while building the graph mark nodes as "independent variables" and all nodes that
 * depend on them directly or indirectly as "active". Derivatives are only guaranteed to be computed for independent
 * variables. When pushing derivatives backwards we do not need to push from or to non-active variables. */

template <class T>
void backwardDerivatives(
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

    std::size_t lastReconstructedRedBlockId = 0;

    // loop over the nodes in the graph in reverse order

    for (std::size_t node = g.size() - 1; node > 0; --node) {

        if (deleter && g.redBlockId(node) > 0 && lastReconstructedRedBlockId > 0 &&
            g.redBlockId(node) != lastReconstructedRedBlockId) {

            // delete the values in the last reconstructured red block

            auto range = g.redBlockRanges()[lastReconstructedRedBlockId];
            QL_REQUIRE(range.second != ComputationGraph::nan,
                       "backwardDerivatives(): red block " << lastReconstructedRedBlockId << " was not closed.");
            for (std::size_t n = range.first; n < range.second; ++n) {
                if (!fwdOpKeepNodes[n])
                    deleter(values[n]);
            }

            lastReconstructedRedBlockId = 0;
        }

        if (!g.predecessors(node).empty() && !isDeterministicAndZero(derivatives[node])) {

            if(g.redBlockId(node) > 0 && g.redBlockId(node) != lastReconstructedRedBlockId) {

                // check if any of the values entering the gradient calculation below is uninitialized
                // and if yes, reconstruct the current red block

                bool reconstructionRequired = false;
                auto req = fwdOpRequiresNodesForDerivatives[g.opId(node)](g.predecessors(node).size());
                if (!values[node].initialised() && req.second) {
                    reconstructionRequired = true;
                } else {
                    for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                        if (req.first[arg] && !values[g.predecessors(node)[arg]].initialised())
                            reconstructionRequired = true;
                    }
                }

                if (reconstructionRequired) {

                    // reconstruct the current red block

                    auto range = g.redBlockRanges()[g.redBlockId(node) - 1];
                    QL_REQUIRE(range.second != ComputationGraph::nan,
                               "backwardDerivatives(): red block " << g.redBlockId(node) << " was not closed.");
                    forwardEvaluation(g, values, fwdOps, deleter, true, fwdOpRequiresNodesForDerivatives,
                                      fwdOpKeepNodes, range.first, range.second, true, fwdOpPreDeleter,
                                      fwdOpAllowsPredeletion, true);

                    lastReconstructedRedBlockId = g.redBlockId(node);
                }
            }

            // propagate the derivative at a node to its predecessors

            std::vector<const T*> args(g.predecessors(node).size());
            for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                args[arg] = &values[g.predecessors(node)[arg]];
            }

            QL_REQUIRE(derivatives[node].initialised(),
                       "backwardDerivatives(): derivative at active node " << node << " is not initialized.");

            if (g.opId(node) == conditionalExpectationOpId && conditionalExpectation) {

                // expected stochastic automatic differentiaion, Fries, 2017
                args[0] = &derivatives[node];
                derivatives[g.predecessors(node)[0]] += conditionalExpectation(args, node);

            } else {

                auto gr = grad[g.opId(node)](args, &values[node], node);

                for (std::size_t p = 0; p < g.predecessors(node).size(); ++p) {
                    QL_REQUIRE(derivatives[g.predecessors(node)[p]].initialised(),
                               "backwardDerivatives: derivative at node "
                                   << g.predecessors(node)[p] << " not initialized, which is an active predecessor of "
                                   << node);
                    QL_REQUIRE(gr[p].initialised(),
                               "backwardDerivatives: gradient at node "
                                   << node << " (opId " << g.opId(node) << ") not initialized at component " << p
                                   << " but required to push to predecessor " << g.predecessors(node)[p]);
                    // isfinite() is relatively expensive to evaluate, therefore we disable this check by default
                    // QL_REQUIRE(gr[p].isfinite(),
                    //            "backwardDerivatives: gradient at node "
                    //                << node << " (opId " << g.opId(node) << ") is not finite at component " << p
                    //                << " but required to push to predecessor " << g.predecessors(node)[p]);

                    derivatives[g.predecessors(node)[p]] += derivatives[node] * gr[p];
                }
            }
        }

        // then check if we can delete the node

        if (deleter) {

            // is the node marked as to be kept?

            if (!keepNodes.empty() && keepNodes[node])
                continue;

            // apply the deleter

            deleter(derivatives[node]);
        }

    } // for node
}

} // namespace QuantExt
