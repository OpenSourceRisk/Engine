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
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/tuple.hpp>

namespace ore {
namespace data {

void DependencyGraph::buildDependencyGraph(const std::string& configuration,
                                           std::map<std::string, std::string>& buildErrors) {

    LOG("Build dependency graph for TodaysMarket configuration " << configuration);

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
            g[v] = {o, m.first, m.second, spec, false};
            TLOG("add vertex # " << index[v] << ": " << g[v]);
        }
    }

    // add the dependencies based on the required curve ids stored in the curve configs; notice that no dependencies
    // to FXSpots are stored in the configs, these are not needed because a complete FXTriangulation object is created
    // upfront that is passed to all curve builders which require it.

    VertexIterator v, vend, w, wend;

    for (std::tie(v, vend) = boost::vertices(g); v != vend; ++v) {
        std::map<CurveSpec::CurveType, std::set<string>> requiredIds;
        if (g[*v].curveSpec)
            requiredIds = curveConfigs_->requiredCurveIds(g[*v].curveSpec->baseType(), g[*v].curveSpec->curveConfigID());
        
        // Special case for SwapIndex - we need to add the discount dependency here
        if (g[*v].obj == MarketObject::SwapIndexCurve) 
            requiredIds[CurveSpec::CurveType::Yield].insert(g[*v].mapping);

        if (requiredIds.size() > 0) {
            for (auto const& r : requiredIds) {
                for (auto const& cId : r.second) {
                    // avoid self reference
                    if (r.first == g[*v].curveSpec->baseType() && (cId == g[*v].curveSpec->curveConfigID() || cId == g[*v].name))
                        continue;
                    bool found = false;
                    for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                        if (*w != *v && g[*w].curveSpec && r.first == g[*w].curveSpec->baseType() &&
                            (cId == g[*w].curveSpec->curveConfigID() || (g[*w].obj == MarketObject::DiscountCurve && g[*w].name == cId))) {
                           // we also handle the special case for discount curves, the dependency is of form (CurveSpec::CurveType::Yield, ccy)
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

} // TodaysMarket::buildDependencyGraph

} // namespace data
} // namespace ore
