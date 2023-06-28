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

#include <qle/ad/computationgraph.hpp>

#include <boost/shared_ptr.hpp>

namespace QuantExt {

template <class T>
void backwardDerivatives(const ComputationGraph& g, const std::vector<T>& values, std::vector<T>& derivatives,
                         const std::vector<std::function<std::vector<T>(const std::vector<const T*>&, const T*)>>& grad,
                         std::function<void(T&)> deleter = {}, const std::vector<bool>& keepNodes = {}) {
    if (g.size() == 0)
        return;

    // loop over the nodes in the graph in reverse order

    for (std::size_t node = g.size() - 1; node > 0; --node) {

        if (!g.predecessors(node).empty()) {

            // propagate the derivative at a node to its predecessors

            std::vector<const T*> args(g.predecessors(node).size());
            for (std::size_t arg = 0; arg < g.predecessors(node).size(); ++arg) {
                args[arg] = &values[g.predecessors(node)[arg]];
            }

            auto gr = grad[g.opId(node)](args, &values[node]);

            for (std::size_t p = 0; p < g.predecessors(node).size(); ++p) {
                derivatives[g.predecessors(node)[p]] += derivatives[node] * gr[p];
            }

            // then check if we can delete the node

            if (deleter) {

                // is the node marked as to be kept?

                if (!keepNodes.empty() && keepNodes[node])
                    continue;

                // apply the deleter

                deleter(derivatives[node]);
            }

        } // if !g.predecessors empty
    }     // for node
}

} // namespace QuantExt
