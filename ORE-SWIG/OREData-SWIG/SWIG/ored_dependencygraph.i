/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_dependencygraph_i
#define ored_dependencygraph_i

%include ored_todaysmarketparameters.i
%include ored_curveconfigurations.i
%include ored_iborfallbackconfig.i
%include ored_referencedatamanager.i

%{
#include <ored/marketdata/dependencygraph.hpp>
#include <boost/graph/topological_sort.hpp>
using ore::data::DependencyGraph;
%}

// Hide the raw C++ entry point – it takes a non-const map& output parameter
// that SWIG cannot automatically marshal. Python users call buildOrder() instead.
%ignore ore::data::DependencyGraph::buildDependencyGraph;
%ignore ore::data::DependencyGraph::dependencies;
%ignore ore::data::DependencyGraph::reducedDependencies;

%nodefaultctor ore::data::DependencyGraph;

namespace ore {
namespace data {

//! Establishes the build order of market objects and their dependencies.
class DependencyGraph {
public:
    %extend {
        //! Construct from market parameters and curve configurations.
        //! curveConfigs is accepted as non-const shared_ptr for Python convenience.
        DependencyGraph(
            const QuantLib::Date& asof,
            const ext::shared_ptr<ore::data::TodaysMarketParameters>& params,
            const ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
            const ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
                QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(
                    ore::data::IborFallbackConfig::defaultConfig()),
            const ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr) {
            return new ore::data::DependencyGraph(
                asof, params,
                ext::shared_ptr<const ore::data::CurveConfigurations>(curveConfigs),
                iborFallbackConfig, referenceData);
        }

        //! Return the build order for a given configuration as a list of
        //! "name/mapping" strings in the order curves must be constructed.
        //! Returns an empty list if the configuration is unknown or contains a cycle.
        std::vector<std::string> buildOrder(const std::string& configuration) {
            std::map<std::string, std::string> errors;
            self->buildDependencyGraph(configuration, errors);
            const auto& deps = self->dependencies();
            auto it = deps.find(configuration);
            if (it == deps.end())
                return {};
            const auto& g = it->second;
            using Vertex = ore::data::DependencyGraph::Vertex;
            std::vector<Vertex> order;
            try {
                boost::topological_sort(g, std::back_inserter(order));
            } catch (...) {
                return {};
            }
            std::vector<std::string> result;
            result.reserve(order.size());
            for (auto vit = order.rbegin(); vit != order.rend(); ++vit) {
                const auto& node = g[*vit];
                result.push_back(node.name + "/" + node.mapping);
            }
            return result;
        }

        //! Return any build errors encountered when computing the dependency graph
        //! for the given configuration. Keys are curve IDs, values are error messages.
        std::map<std::string, std::string> getBuildErrors(const std::string& configuration) {
            std::map<std::string, std::string> errors;
            self->buildDependencyGraph(configuration, errors);
            return errors;
        }
    }
};

} // namespace data
} // namespace ore

#endif
