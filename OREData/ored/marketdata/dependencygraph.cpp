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

/*! \file marketdata/dependencygraph.cpp
    \brief DependencyGraph class to establish build order of marketObjects and its dependency
    \ingroup marketdata
*/

#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/dependencygraph.hpp>
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/tuple.hpp>

namespace ore {
namespace data {

namespace {
struct CycleInserter {
    explicit CycleInserter(std::vector<set<DependencyGraph::Node>>& cycles) : cycles_(cycles) {}
    template <typename Path, typename Graph> void cycle(const Path& p, const Graph& g) {
        cycles_.push_back({});
        typename Path::const_iterator i, end = p.end();
        for (i = p.begin(); i != end; ++i) {
            cycles_.back().insert(g[*i]);
        }
    }
    std::vector<std::set<DependencyGraph::Node>>& cycles_;
};
} // namespace

void DependencyGraph::buildDependencyGraph(const std::string& configuration,
                                           std::map<std::string, std::string>& buildErrors) {

    DLOG("Build dependency graph for TodaysMarket configuration " << configuration);

    Graph& g = dependencies_[configuration];
    IndexMap index = boost::get(boost::vertex_index, g);

    // add the vertices

    std::set<MarketObject> t = getMarketObjectTypes(); // from todaysmarket parameter

    for (auto const& o : t) {
        auto mapping = params_->mapping(o, configuration);
        for (auto const& m : mapping) {
            Vertex v = boost::add_vertex(g);
            QuantLib::ext::shared_ptr<CurveSpec> spec;
            // swap index curves pass the id
            if (o == MarketObject::SwapIndexCurve)
                spec = QuantLib::ext::make_shared<SwapIndexCurveSpec>(m.first);
            else
                spec = parseCurveSpec(m.second);
            // add the vertex to the dependency graph
            g[v] = {index[v], o, m.first, m.second, spec, false};
            TLOG("add vertex # " << index[v] << ": " << g[v]);
        }
    }

    // declare vertex and edge iterators which we use below

    VertexIterator v, vend, w, wend;
    EdgeIterator e, eend;

    // add the dependencies based on the required curve ids stored in the curve configs; notice that no dependencies
    // to FXSpots are stored in the configs, these are not needed because a complete FXTriangulation object is created
    // upfront that is passed to all curve builders which require it.

    for (std::tie(v, vend) = boost::vertices(g); v != vend; ++v) {
        std::map<CurveSpec::CurveType, std::set<string>> requiredIds;
        if (g[*v].curveSpec)
            requiredIds =
                curveConfigs_->requiredCurveIds(g[*v].curveSpec->baseType(), g[*v].curveSpec->curveConfigID());

        // Special case for SwapIndex - we need to add the discount dependency here
        if (g[*v].obj == MarketObject::SwapIndexCurve)
            requiredIds[CurveSpec::CurveType::Yield].insert(g[*v].mapping);

        if (requiredIds.size() > 0) {
            for (auto const& r : requiredIds) {
                for (auto const& cId : r.second) {
                    // avoid self reference
                    if (r.first == g[*v].curveSpec->baseType() &&
                        (cId == g[*v].curveSpec->curveConfigID() || cId == g[*v].name))
                        continue;
                    if (r.first == CurveSpec::CurveType::FX)
                        continue; // FXSpots are dealt with in advance
                    bool found = false;
                    for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                        if (*w != *v && g[*w].curveSpec && r.first == g[*w].curveSpec->baseType() &&
                            (cId == g[*w].curveSpec->curveConfigID() || g[*w].name == cId)) {
                            // we also handle the special case for discount curves, the dependency is of form
                            // (CurveSpec::CurveType::Yield, ccy)
                            g.add_edge(*v, *w);
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                            found = true;
                            // it is enough to insert one dependency
                            break;
                        }
                    }
                    if (!found)
                        buildErrors[g[*v].mapping] = "did not find required curve id " + cId + " of type " +
                                                     ore::data::to_string(r.first) + " (required from " +
                                                     ore::data::to_string(g[*v]) +
                                                     ") in dependency graph for configuration " + configuration;
                }
            }
        }
    }

    DLOG("Dependency graph built with " << boost::num_vertices(g) << " vertices, " << boost::num_edges(g) << " edges.");

    // identify cycles and store them

    DLOG("Searching for cycles.");

    std::vector<std::set<Node>> cycles;
    boost::tiernan_all_cycles(g, CycleInserter(cycles));

    DLOG("Identified " << cycles.size() << " cycles in dependency graph.");

    for (std::size_t j = 0; j < cycles.size(); ++j) {
        std::ostringstream out;
        for (auto const& d : cycles[j]) {
            out << d << ",";
        }
        TLOG("cycle #" << j << ": " << out.str());
    }

    cycles_[configuration] = cycles;

    // join overlapping cycles

    DLOG("Joining overlapping cycles (if any).");

    std::vector<std::set<std::size_t>> cyclesIndices;

    for (std::size_t i = 0; i < cycles.size(); ++i) {
        cyclesIndices.push_back({});
        std::for_each(cycles[i].begin(), cycles[i].end(),
                      [&cyclesIndices](const Node& n) { cyclesIndices.back().insert(n.index); });
    }

    bool updated;
    do {
        updated = false;
        for (std::size_t i = 0; i < cycles.size() && !updated; ++i) {
            for (std::size_t j = i + 1; j < cycles.size() && !updated; ++j) {
                std::set<std::size_t> tmp;
                std::set_intersection(cyclesIndices[i].begin(), cyclesIndices[i].end(), cyclesIndices[j].begin(),
                                      cyclesIndices[j].end(), std::inserter(tmp, tmp.end()));
                if (!tmp.empty()) {
                    cycles[i].insert(cycles[j].begin(), cycles[j].end());
                    cyclesIndices[i].insert(cyclesIndices[j].begin(), cyclesIndices[j].end());
                    cycles.erase(std::next(cycles.begin(), i));
                    cyclesIndices.erase(std::next(cyclesIndices.begin(), i));
                    updated = true;
                    TLOG("joining overlapping cycles with temp indices " << i << ", " << j);
                }
            }
        }
    } while (updated);

    DLOG("Number of cycles after joining overlapping cycles: " << cycles.size());

    ReducedGraph& rg = reducedDependencies_[configuration];
    ReducedIndexMap rindex = boost::get(boost::vertex_index, rg);

    // build the reduced graph

    DLOG("Build the reduced dependency graph where cycles are replaced by a single reduced node containing the set of nodes in the cylce.");

    std::map<Vertex, ReducedVertex> vertexMap;

    std::vector<bool> haveCycleVertex(cycles.size(), false);
    std::vector<ReducedVertex> cycleReducedVertices(cycles.size());

    for (std::tie(v, vend) = boost::vertices(g); v != vend; ++v) {
        std::size_t nodeIndex = index[*v];
        auto c = std::find_if(cyclesIndices.begin(), cyclesIndices.end(),
                              [nodeIndex](const std::set<std::size_t>& s) { return s.find(nodeIndex) != s.end(); });
        if (c == cyclesIndices.end()) {
            ReducedVertex rv = boost::add_vertex(rg);
            vertexMap[*v] = rv;
            rg[rv] = ReducedNode{std::set<Node>{g[*v]}};
            TLOG("add vertex in reduced graph #" << index[rv] << ": " << rg[rv]);
        } else {
            std::size_t cycleIndex = std::distance(cyclesIndices.begin(), c);
            if (haveCycleVertex[cycleIndex]) {
                vertexMap[*v] = cycleReducedVertices[cycleIndex];
            } else {
                ReducedVertex rv = boost::add_vertex(rg);
                cycleReducedVertices[cycleIndex] = rv;
                haveCycleVertex[cycleIndex] = true;
                vertexMap[*v] = rv;
                rg[rv] = ReducedNode{cycles[cycleIndex]};
                TLOG("add vertex in reduced graph #" << index[rv] << ": " << rg[rv]);
            }
        }
    }

    for (boost::tie(e, eend) = boost::edges(g); e != eend; ++e) {
        ReducedVertex reducedSource = vertexMap.at(boost::source(*e, g));
        ReducedVertex reducedTarget = vertexMap.at(boost::target(*e, g));
        if (reducedSource == reducedTarget)
            continue;
        rg.add_edge(reducedSource, reducedTarget);
        TLOG("add edge in reduced graph from vertex #"
             << rindex[reducedSource] << " " << rg[reducedSource] << " to #"
             << rindex[reducedTarget] << " " << rg[reducedTarget]);
    }

    DLOG("Reduced dependency graph built with " << boost::num_vertices(rg) << " vertices, " << boost::num_edges(rg) << " edges.");


} // TodaysMarket::buildDependencyGraph

std::ostream& operator<<(std::ostream& o, const DependencyGraph::Node& n) {
    return o << n.obj << "(" << n.name << "," << n.mapping << ")";
}

std::ostream& operator<<(std::ostream& o, const DependencyGraph::ReducedNode& n) {
    o << "[";
    if (n.nodes.size() > 1)
        o << "cycle: ";
    for (std::size_t i = 0; i < n.nodes.size(); ++i) {
        o << *std::next(n.nodes.begin(), i);
        if (i < n.nodes.size() - 1)
            o << "#";
    }
    return o << "]";
}

bool operator<(const DependencyGraph::Node& x, const DependencyGraph::Node& y) { return x.index < y.index; }

} // namespace data
} // namespace ore
