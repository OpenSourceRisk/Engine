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
#include <ored/utilities/indexparser.hpp>
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
    IndexMap index = QuantLib::ext::get(boost::vertex_index, g);
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

    // add the vertices
    std::set<MarketObject> t = getMarketObjectTypes(); // from todaysmarket parameter

    for (auto const& o : t) {
        auto mapping = params_->mapping(o, configuration);
        for (auto const& m : mapping) {
            Vertex v = boost::add_vertex(g);
            QuantLib::ext::shared_ptr<CurveSpec> spec;
            // swap index curves do not have a spec
            if (o != MarketObject::SwapIndexCurve)
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
        if (g[*v].curveSpec) {
            for (auto const& r :
                 curveConfigs_->requiredCurveIds(g[*v].curveSpec->baseType(), g[*v].curveSpec->curveConfigID())) {
                for (auto const& cId : r.second) {
                    // avoid self reference
                    if (r.first == g[*v].curveSpec->baseType() && cId == g[*v].curveSpec->curveConfigID())
                        continue;
                    bool found = false;
                    for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                        if (*w != *v && g[*w].curveSpec && r.first == g[*w].curveSpec->baseType() &&
                            cId == g[*w].curveSpec->curveConfigID()) {
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

    // add additional dependencies that are not captured in the curve config dependencies
    // it is a bit unfortunate that we have to handle these exceptions here, we should rather strive to have all
    // dependencies in the curve configurations

    for (std::tie(v, vend) = boost::vertices(g); v != vend; ++v) {

        // 1 CapFloorVolatility depends on underlying index curve(s)

        if (g[*v].obj == MarketObject::CapFloorVol &&
            curveConfigs_->hasCapFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())) {
            std::set<string> indices;
            if(!curveConfigs_->capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->index().empty())
            indices.insert(curveConfigs_->capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->index());
            if(!curveConfigs_->capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->proxySourceIndex().empty())
            indices.insert(curveConfigs_->capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->proxySourceIndex());
            if(!curveConfigs_->capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->proxyTargetIndex().empty())
            indices.insert(curveConfigs_->capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->proxyTargetIndex());
            for (auto const& ind : indices) {
                bool found = false;
                for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                    if (*w != *v && g[*w].obj == MarketObject::IndexCurve && g[*w].name == ind) {
                        g.add_edge(*v, *w);
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                        found = true;
                        // there should be only one dependency, in any case it is enough to insert one
                        break;
                    }
                }
                if (!found)
                    buildErrors[g[*v].mapping] = "did not find required ibor index " + ind + " (required from " +
                                                 ore::data::to_string(g[*v]) +
                                                 ") in dependency graph for configuration " + configuration;
            }
        }

        // 2 Correlation depends on underlying swap indices (if CMS Spread Correlations are calibrated to prices)

        if (g[*v].obj == MarketObject::Correlation &&
            curveConfigs_->hasCorrelationCurveConfig(g[*v].curveSpec->curveConfigID())) {
            auto config = curveConfigs_->correlationCurveConfig(g[*v].curveSpec->curveConfigID());
            if (config->correlationType() == CorrelationCurveConfig::CorrelationType::CMSSpread &&
                config->quoteType() == MarketDatum::QuoteType::PRICE) {
                bool found1 = config->index1().empty(), found2 = config->index2().empty();
                for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                    if (*w != *v) {
                        if (g[*w].name == config->index1()) {
                            g.add_edge(*v, *w);
                            found1 = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                        if (g[*w].name == config->index1()) {
                            g.add_edge(*v, *w);
                            found2 = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                    }
                    // there should be only one dependency, in any case it is enough to insert one
                    if (found1 && found2)
                        break;
                }
                if (!found1)
                    buildErrors[g[*v].mapping] = "did not find required swap index " + config->index1() +
                                                 " (required from " + ore::data::to_string(g[*v]) +
                                                 ") in dependency graph for configuration " + configuration;
                if (!found2)
                    buildErrors[g[*v].mapping] = "did not find required swap index " + config->index2() +
                                                 " (required from " + ore::data::to_string(g[*v]) +
                                                 ") in dependency graph for configuration " + configuration;
            }
        }

        // 3 SwaptionVolatility depends on underlying swap indices

        if (g[*v].obj == MarketObject::SwaptionVol &&
            curveConfigs_->hasSwaptionVolCurveConfig(g[*v].curveSpec->curveConfigID())) {
            auto config = curveConfigs_->swaptionVolCurveConfig(g[*v].curveSpec->curveConfigID());
            std::set<std::string> indexBases;
            if (!config->shortSwapIndexBase().empty())
                indexBases.insert(config->shortSwapIndexBase());
            if (!config->swapIndexBase().empty())
                indexBases.insert(config->swapIndexBase());
            if (!config->proxySourceShortSwapIndexBase().empty())
                indexBases.insert(config->proxySourceShortSwapIndexBase());
            if (!config->proxySourceSwapIndexBase().empty())
                indexBases.insert(config->proxySourceSwapIndexBase());
            if (!config->proxyTargetShortSwapIndexBase().empty())
                indexBases.insert(config->proxyTargetShortSwapIndexBase());
            if (!config->proxyTargetSwapIndexBase().empty())
                indexBases.insert(config->proxyTargetSwapIndexBase());
            for (auto const& indexBase : indexBases) {
                bool found = false;
                for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                    if (*w != *v) {
                        if (g[*w].name == indexBase) {
                            g.add_edge(*v, *w);
                            found = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                    }
                    // there should be only one dependency, in any case it is enough to insert one
                    if (found)
                        break;
                }
                if (!found)
                    buildErrors[g[*v].mapping] = "did not find required swap index " + indexBase + " (required from " +
                                                 ore::data::to_string(g[*v]) +
                                                 ") in dependency graph for configuration " + configuration;
            }
        }

        // 4 Swap Indices depend on underlying ibor and discount indices

        if (g[*v].obj == MarketObject::SwapIndexCurve) {
            bool foundIbor = false, foundDiscount = false;
            std::string swapIndex = g[*v].name;
            auto swapCon = QuantLib::ext::dynamic_pointer_cast<data::SwapIndexConvention>(conventions->get(swapIndex));
            QL_REQUIRE(swapCon, "Did not find SwapIndexConvention for " << swapIndex);
            auto con = QuantLib::ext::dynamic_pointer_cast<data::IRSwapConvention>(conventions->get(swapCon->conventions()));
            auto conOisComp =
                QuantLib::ext::dynamic_pointer_cast<data::OisConvention>(conventions->get(swapCon->conventions()));
            auto conOisAvg =
                QuantLib::ext::dynamic_pointer_cast<data::AverageOisConvention>(conventions->get(swapCon->conventions()));
            std::string indexName;
            if(con)
                indexName = con->indexName();
            else if(conOisComp)
                indexName = conOisComp->indexName();
            else if(conOisAvg)
                indexName = conOisAvg->indexName();
            else {
                QL_FAIL("DependencyGraph: internal errors, expected IRSwapConvention, OisConvention, "
                        "AverageOisConvention for '"
                        << swapCon->conventions() << "' from conventions for swap index '" << swapIndex << "'");
            }
            std::string discountIndex = g[*v].mapping;
            if (isGenericIborIndex(indexName))
                foundIbor = true;
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v) {
                    if (g[*w].obj == MarketObject::IndexCurve || g[*w].obj == MarketObject::YieldCurve) {
                        if (g[*w].name == discountIndex) {
                            g.add_edge(*v, *w);
                            foundDiscount = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                        if (g[*w].name == indexName) {
                            g.add_edge(*v, *w);
                            foundIbor = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                    }
                }
                // there should be only one dependency, in any case it is enough to insert one
                if (foundDiscount && foundIbor)
                    break;
            }
            if (!foundIbor)
                buildErrors[g[*v].mapping] = "did not find required ibor/ois index " + indexName + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
            if (!foundDiscount)
                buildErrors[g[*v].mapping] = "did not find required discount index " + discountIndex +
                                             " (required from " + ore::data::to_string(g[*v]) +
                                             ") in dependency graph for configuration " + configuration;
        }

        if (g[*v].obj == MarketObject::YieldCurve || g[*v].obj == MarketObject::DiscountCurve) {
            if (curveConfigs_->hasYieldCurveConfig(g[*v].curveSpec->curveConfigID())) {
                QuantLib::ext::shared_ptr<YieldCurveConfig> config =
                    curveConfigs_->yieldCurveConfig(g[*v].curveSpec->curveConfigID());
                vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments = config->curveSegments();
                for (auto s : segments) {
                    if (auto ccSegment = QuantLib::ext::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(s)) {
                        if (ccSegment->foreignDiscountCurveID() == "") {
                            // find the foreign ccy
                            std::string ccy = config->currency();
                            std::string spot = ccSegment->spotRateID();
                            std::string foreignCcy;
                            auto spotDatum = parseMarketDatum(Date(), spot, Real());
                            if (auto fxq = QuantLib::ext::dynamic_pointer_cast<FXSpotQuote>(spotDatum))
                                foreignCcy = ccy == fxq->unitCcy() ? fxq->ccy() : fxq->unitCcy();

                            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                                if (*w != *v) {
                                    if (g[*w].obj == MarketObject::DiscountCurve && g[*w].name == foreignCcy) {
                                        g.add_edge(*v, *w);
                                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #"
                                                                      << index[*w] << " " << g[*w]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 5 Equity Vol depends on spot, discount, div

        if (g[*v].obj == MarketObject::EquityVol) {
            bool foundDiscount = false, foundEqCurve = false;
            std::string eqName = g[*v].name;
            auto eqVolSpec = QuantLib::ext::dynamic_pointer_cast<EquityVolatilityCurveSpec>(g[*v].curveSpec);
            QL_REQUIRE(eqVolSpec, "could not cast to EquityVolatilityCurveSpec");
            std::string ccy = eqVolSpec->ccy();
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v) {
                    if (g[*w].obj == MarketObject::DiscountCurve && g[*w].name == ccy) {
                        g.add_edge(*v, *w);
                        foundDiscount = true;
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                    }
                    if (g[*w].obj == MarketObject::EquityCurve && g[*w].name == eqName) {
                        g.add_edge(*v, *w);
                        foundEqCurve = true;
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                    }
                }
                // there should be only one dependency, in any case it is enough to insert one
                if (foundDiscount && foundEqCurve)
                    break;
            }
            if (!foundDiscount)
                buildErrors[g[*v].mapping] = "did not find required discount curve " + ccy + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
            if (!foundEqCurve)
                buildErrors[g[*v].mapping] = "did not find required equity curve " + eqName + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
        }

        // 6 Commodity Vol depends on price, discount

        if (g[*v].obj == MarketObject::CommodityVolatility) {
            bool foundDiscount = false, foundCommCurve = false;
            std::string commName = g[*v].name;
            auto commVolSpec = QuantLib::ext::dynamic_pointer_cast<CommodityVolatilityCurveSpec>(g[*v].curveSpec);
            QL_REQUIRE(commVolSpec, "could not cast to CommodityVolatilityCurveSpec");
            std::string ccy = commVolSpec->currency();
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v) {
                    if (g[*w].obj == MarketObject::DiscountCurve && g[*w].name == ccy) {
                        g.add_edge(*v, *w);
                        foundDiscount = true;
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                    }
                    if (g[*w].obj == MarketObject::CommodityCurve && g[*w].name == commName) {
                        g.add_edge(*v, *w);
                        foundCommCurve = true;
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                    }
                }
                // there should be only one dependency, in any case it is enough to insert one
                if (foundDiscount && foundCommCurve)
                    break;
            }
            if (!foundDiscount)
                buildErrors[g[*v].mapping] = "did not find required discount curve " + ccy + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
            if (!foundCommCurve)
                buildErrors[g[*v].mapping] = "did not find required commodity curve " + commName + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
        }

        // 7 Ibor fallback handling: an ibor index to replace depends on the fallback rfr index

        if (g[*v].obj == MarketObject::IndexCurve) {
            if (iborFallbackConfig_.isIndexReplaced(g[*v].name, asof_)) {
                bool foundRfrIndex = false;
                for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                    if (*w != *v) {
                        if (g[*w].obj == MarketObject::IndexCurve &&
                            g[*w].name == iborFallbackConfig_.fallbackData(g[*v].name).rfrIndex) {
                            g.add_edge(*v, *w);
                            foundRfrIndex = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                    }
                    if (foundRfrIndex)
                        break;
                }
                if (!foundRfrIndex)
                    buildErrors[g[*v].mapping] = "did not find required rfr index '" +
                                                 iborFallbackConfig_.fallbackData(g[*v].name).rfrIndex +
                                                 "' for replaced ibor index '" + g[*v].name +
                                                 "', is the rfr index configured in todays market parameters?";
            }
        }

        // 8 FX curve depends on discount

        if (g[*v].obj == MarketObject::FXSpot) {
            bool foundDiscount1 = false, foundDiscount2 = false;
            std::string fxName = g[*v].name;
            auto fxSpec = QuantLib::ext::dynamic_pointer_cast<FXSpotSpec>(g[*v].curveSpec);
            QL_REQUIRE(fxSpec, "could not cast to FXSpotSpec");
            std::string ccy1 = fxSpec->unitCcy();
            std::string ccy2 = fxSpec->ccy();
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v) {
                    if (g[*w].obj == MarketObject::DiscountCurve && (g[*w].name == ccy1 || g[*w].name == ccy2)) {
                        g.add_edge(*v, *w);
                        if (g[*w].name == ccy1)
                            foundDiscount1 = true;
                        if (g[*w].name == ccy2)
                            foundDiscount2 = true;
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                    }
                }
                // there should be only one dependency, in any case it is enough to insert one
                if (foundDiscount1 && foundDiscount2)
                    break;
            }
            if (!foundDiscount1)
                buildErrors[g[*v].mapping] = "did not find required discount curve " + ccy1 + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
            if (!foundDiscount2)
                buildErrors[g[*v].mapping] = "did not find required discount curve " + ccy2 + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
        }
    }

    DLOG("Dependency graph built with " << boost::num_vertices(g) << " vertices, " << boost::num_edges(g) << " edges.");

} // TodaysMarket::buildDependencyGraph

} // namespace data
} // namespace ore
