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

/*! \file marketdata/dependencygraph.hpp
    \brief DependencyGraph class to establish build order of marketObjects and its dependency
    \ingroup marketdata
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/graph_traits.hpp>
#include <ql/shared_ptr.hpp>

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/tiernan_all_cycles.hpp>

#include <map>

namespace ore {
namespace data {

namespace {

//! Helper class to output cycles in the dependency graph
template <class OutputStream> struct CyclePrinter {
    CyclePrinter(OutputStream& os) : os_(os) {}
    template <typename Path, typename Graph> void cycle(const Path& p, const Graph& g) {
        typename Path::const_iterator i, end = p.end();
        for (i = p.begin(); i != end; ++i) {
            os_ << g[*i] << " ";
        }
        os_ << "*** ";
    }
    OutputStream& os_;
};

//! Helper functions returning a string describing all circles in a graph
template <typename Graph> string getCycles(const Graph& g) {
    std::ostringstream cycles;
    CyclePrinter<std::ostringstream> cyclePrinter(cycles);
    boost::tiernan_all_cycles(g, cyclePrinter);
    return cycles.str();
}

//! Helper class to find the dependent nodes from a given start node and a topological order for them
template <typename Vertex> struct DfsVisitor : public boost::default_dfs_visitor {
    DfsVisitor(std::vector<Vertex>& order, bool& foundCycle) : order_(order), foundCycle_(foundCycle) {}
    template <typename Graph> void finish_vertex(Vertex u, const Graph& g) { order_.push_back(u); }
    template <typename Edge, typename Graph> void back_edge(Edge e, const Graph& g) { foundCycle_ = true; }
    std::vector<Vertex>& order_;
    bool& foundCycle_;
};

} // namespace

class DependencyGraph {

public:
    DependencyGraph(
        //! The asof date of the T0 market instance
        const Date& asof,
        //! Description of the market composition
        const QuantLib::ext::shared_ptr<TodaysMarketParameters>& params,
        //! Description of curve compositions
        const QuantLib::ext::shared_ptr<const CurveConfigurations>& curveConfigs,
        //! Ibor fallback config
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig())
        : asof_(asof), params_(params), curveConfigs_(curveConfigs),
          iborFallbackConfig_(iborFallbackConfig){};

    // data structure for a vertex in the graph
    struct Node {
        MarketObject obj;                       // the market object to build
        std::string name;                       // the LHS of the todays market mapping
        std::string mapping;                    // the RHS of the todays market mapping
        QuantLib::ext::shared_ptr<CurveSpec> curveSpec; // the parsed curve spec, if applicable, null otherwise
        bool built;                             // true if we have built this node
    };

    // some typedefs for graph related data types
    using Graph = boost::directed_graph<Node>;
    using IndexMap = boost::property_map<Graph, boost::vertex_index_t>::type;
    using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
    using VertexIterator = boost::graph_traits<Graph>::vertex_iterator;

    // build a graph whose vertices represent the market objects to build (DiscountCurve, IndexCurve, EquityVol, ...)
    // and an edge from x to y means that x must be built before y, since y depends on it. */
    void buildDependencyGraph(const std::string& configuration, std::map<std::string, std::string>& buildErrors);

    std::map<std::string, Graph> dependencies() { return dependencies_; }

private:
    friend std::ostream& operator<<(std::ostream& o, const Node& n);

    // the dependency graphs for each configuration
    std::map<std::string, Graph> dependencies_;

    const Date asof_;
    const QuantLib::ext::shared_ptr<TodaysMarketParameters> params_;
    const QuantLib::ext::shared_ptr<const CurveConfigurations> curveConfigs_;
    const IborFallbackConfig iborFallbackConfig_;
};

} // namespace data
} // namespace ore
