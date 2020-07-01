/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/todaysmarket.cpp
    \brief An concerte implementation of the Market class that loads todays market and builds the required curves
    \ingroup
*/

#include <ored/marketdata/basecorrelationcurve.hpp>
#include <ored/marketdata/capfloorvolcurve.hpp>
#include <ored/marketdata/cdsvolcurve.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/marketdata/commodityvolcurve.hpp>
#include <ored/marketdata/correlationcurve.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/defaultcurve.hpp>
#include <ored/marketdata/equitycurve.hpp>
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/marketdata/fxspot.hpp>
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/marketdata/inflationcapfloorvolcurve.hpp>
#include <ored/marketdata/inflationcurve.hpp>
#include <ored/marketdata/security.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/marketdata/swaptionvolcurve.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/marketdata/yieldvolcurve.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/blackvolsurfacewithatm.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

#include <boost/graph/directed_graph.hpp>
#include <boost/graph/tiernan_all_cycles.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/reversed.hpp>

using namespace std;
using namespace QuantLib;

using QuantExt::EquityIndex;
using QuantExt::PriceTermStructure;
using QuantExt::PriceTermStructureAdapter;

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
} // namespace

TodaysMarket::TodaysMarket(const Date& asof, const TodaysMarketParameters& params, const Loader& loader,
                           const CurveConfigurations& curveConfigs, const Conventions& conventions,
                           const bool continueOnError, const bool loadFixings,
                           const boost::shared_ptr<ReferenceDataManager>& referenceData)
    : MarketImpl(conventions), params_(params), loader_(loader), curveConfigs_(curveConfigs),
      continueOnError_(continueOnError), loadFixings_(loadFixings), referenceData_(referenceData) {
    // this ctor does not allow for lazy builds, since we store references to the inputs only
    initialise(asof, false);
}

TodaysMarket::TodaysMarket(const Date& asof, const boost::shared_ptr<TodaysMarketParameters>& params,
                           const boost::shared_ptr<Loader>& loader,
                           const boost::shared_ptr<CurveConfigurations>& curveConfigs,
                           const boost::shared_ptr<Conventions>& conventions, const bool continueOnError,
                           const bool loadFixings, const boost::shared_ptr<ReferenceDataManager>& referenceData,
                           const bool lazyBuild)
    : MarketImpl(conventions), params_ref_(params), loader_ref_(loader), curveConfigs_ref_(curveConfigs),
      conventions_ref_(conventions), params_(*params_ref_), loader_(*loader_ref_), curveConfigs_(*curveConfigs_ref_),
      continueOnError_(continueOnError), loadFixings_(loadFixings), referenceData_(referenceData) {
    QL_REQUIRE(params_ref_, "TodaysMarket: TodaysMarketParameters are null");
    QL_REQUIRE(loader_ref_, "TodaysMarket: Loader is null");
    QL_REQUIRE(curveConfigs_ref_, "TodaysMarket: CurveConfigurations are null");
    QL_REQUIRE(conventions_ref_, "TodaysMarket: Conventions are null");
    initialise(asof, lazyBuild);
}

void TodaysMarket::initialise(const Date& asof, const bool lazyBuild) {

    asof_ = asof;

    // Fixings

    if (loadFixings_) {
        // Apply them now in case a curve builder needs them
        LOG("Todays Market Loading Fixings");
        applyFixings(loader_.loadFixings(), conventions_);
        LOG("Todays Market Loading Fixing done.");
    }

    // Dividends - apply them now in case a curve builder needs them

    LOG("Todays Market Loading Dividends");
    applyDividends(loader_.loadDividends());
    LOG("Todays Market Loading Dividends done.");

    // Add all FX quotes from the loader to Triangulation

    for (auto& md : loader_.loadQuotes(asof_)) {
        if (md->asofDate() == asof_ && md->instrumentType() == MarketDatum::InstrumentType::FX_SPOT) {
            boost::shared_ptr<FXSpotQuote> q = boost::dynamic_pointer_cast<FXSpotQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to FXSpotQuote");
            fxT_.addQuote(q->unitCcy() + q->ccy(), q->quote());
        }
    }

    // loop over convfigurations and build market objects

    map<string, string> buildErrors;

    for (const auto& configuration : params_.configurations()) {

        LOG("Build objects in TodaysMarket configuration " << configuration.first);

        // Build the graph of objects to build for the current configuration

        buildDependencyGraph(configuration.first, buildErrors);

        // Sort the graph topologically

        Graph& g = dependencies_[configuration.first];
        IndexMap index = boost::get(boost::vertex_index, g);
        std::vector<Vertex>& order = order_[configuration.first];
        try {
            boost::topological_sort(g, std::back_inserter(order));
        } catch (const std::exception& e) {
            order.clear();
            std::ostringstream cycles;
            CyclePrinter<std::ostringstream> cyclePrinter(cycles);
            boost::tiernan_all_cycles(g, cyclePrinter);
            buildErrors["CurveDependencyGraph"] = "Topological sort of dependency graph failed for configuration " +
                                                  configuration.first + " (" + ore::data::to_string(e.what()) +
                                                  "). Got cylcle(s): " + cycles.str();
        }

        TLOG("Can build objects in the following order:");
        for (auto const& m : order) {
            TLOG("vertex #" << index[m] << ": " << g[m]);
        }

        // Build the objects in the graph in topological order

        Size countSuccess = 0, countError = 0;
        for (auto const& m : order) {
            try {
                buildNode(configuration.first, g[m]);
                ++countSuccess;
                DLOG("built node " << g[m] << " in configuration " << configuration.first);
            } catch (const std::exception& e) {
                if (g[m].curveSpec)
                    buildErrors[g[m].curveSpec->name()] = e.what();
                else
                    buildErrors[g[m].name] = e.what();
                ++countError;
                ALOG("error while building node " << g[m] << " in configuration " << configuration.first << ": "
                                                  << e.what());
            }
        }

        LOG("Loading " << countSuccess + countError << " CurveSpecs done for configuration " << configuration.first
                       << " (thereof " << countError << " with errors)");

    } // loop over configurations

    // output errors

    if (!buildErrors.empty()) {
        for (auto const& error : buildErrors) {
            ALOG(StructuredCurveErrorMessage(error.first, "Failed to Build Curve", error.second));
        }
        if (!continueOnError_) {
            string errStr;
            for (auto const& error : buildErrors) {
                errStr += "(" + error.first + ": " + error.second + "); ";
            }
            QL_FAIL("Cannot build all required curves! Building failed for: " << errStr);
        }
    }

} // TodaysMarket::initialise()

void TodaysMarket::buildDependencyGraph(const std::string& configuration,
                                        std::map<std::string, std::string>& buildErrors) {

    DLOG("Build Dependency Graph for configuration " << configuration);

    Graph& g = dependencies_[configuration];
    IndexMap index = boost::get(boost::vertex_index, g);

    // add the vertices

    auto t = getMarketObjectTypes();

    for (auto const& o : t) {
        auto mapping = params_.mapping(o, configuration);
        for (auto const& m : mapping) {
            Vertex v = boost::add_vertex(g);
            boost::shared_ptr<CurveSpec> spec;
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
            for (auto const& r : curveConfigs_.requiredCurveIds(g[*v].curveSpec->curveConfigID())) {
                for (auto const& cId : r.second) {
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
                        buildErrors[g[*v].mapping] = "did not find required curve id  " + cId + " of type " +
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

        // 1 CapFloorVolatility depends on underlying index curve

        if (g[*v].curveSpec && curveConfigs_.hasCapFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())) {
            string iborIndex = curveConfigs_.capFloorVolCurveConfig(g[*v].curveSpec->curveConfigID())->iborIndex();
            bool found = false;
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v && g[*w].obj == MarketObject::IndexCurve && g[*w].name == iborIndex) {
                    g.add_edge(*v, *w);
                    TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " " << g[*w]);
                    found = true;
                    // there should be only one dependency, in any case it is enough to insert one
                    break;
                }
            }
            if (!found)
                buildErrors[g[*v].mapping] = "did not find required ibor index " + iborIndex + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
        }

        // 2 Correlation depends on underlying swap indices (if CMS Spread Correlations are calibrated to prices)

        if (g[*v].curveSpec && curveConfigs_.hasCorrelationCurveConfig(g[*v].curveSpec->curveConfigID())) {
            auto config = curveConfigs_.correlationCurveConfig(g[*v].curveSpec->curveConfigID());
            if (config->correlationType() == CorrelationCurveConfig::CorrelationType::CMSSpread &&
                config->quoteType() == CorrelationCurveConfig::QuoteType::Price) {
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

        if (g[*v].curveSpec && curveConfigs_.hasSwaptionVolCurveConfig(g[*v].curveSpec->curveConfigID())) {
            auto config = curveConfigs_.swaptionVolCurveConfig(g[*v].curveSpec->curveConfigID());
            bool found1 = config->shortSwapIndexBase().empty(), found2 = config->swapIndexBase().empty();
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v) {
                    if (g[*w].name == config->shortSwapIndexBase()) {
                        g.add_edge(*v, *w);
                        found1 = true;
                        TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                      << g[*w]);
                    }
                    if (g[*w].name == config->swapIndexBase()) {
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
                buildErrors[g[*v].mapping] = "did not find required swap index " + config->shortSwapIndexBase() +
                                             " (required from " + ore::data::to_string(g[*v]) +
                                             ") in dependency graph for configuration " + configuration;
            if (!found2)
                buildErrors[g[*v].mapping] = "did not find required swap index " + config->swapIndexBase() +
                                             " (required from " + ore::data::to_string(g[*v]) +
                                             ") in dependency graph for configuration " + configuration;
        }

        // 4 Swap Indices depend on underlying ibor and discount indices

        if (g[*v].obj == MarketObject::SwapIndexCurve) {
            bool foundIbor = false, foundDiscount = false;
            std::string swapIndex = g[*v].name;
            auto swapCon = boost::dynamic_pointer_cast<data::SwapIndexConvention>(conventions_.get(swapIndex));
            QL_REQUIRE(swapCon, "Did not find SwapIndexConvention for " << swapIndex);
            auto con = boost::dynamic_pointer_cast<data::IRSwapConvention>(conventions_.get(swapCon->conventions()));
            QL_REQUIRE(con, "Cannot find IRSwapConventions " << swapCon->conventions());
            std::string iborIndex = con->indexName();
            std::string discountIndex = g[*v].mapping;
            for (std::tie(w, wend) = boost::vertices(g); w != wend; ++w) {
                if (*w != *v) {
                    if (g[*w].obj == MarketObject::IndexCurve) {
                        if (g[*w].name == discountIndex) {
                            g.add_edge(*v, *w);
                            foundDiscount = true;
                            TLOG("add edge from vertex #" << index[*v] << " " << g[*v] << " to #" << index[*w] << " "
                                                          << g[*w]);
                        }
                        if (g[*w].name == iborIndex) {
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
                buildErrors[g[*v].mapping] = "did not find required ibor index " + iborIndex + " (required from " +
                                             ore::data::to_string(g[*v]) + ") in dependency graph for configuration " +
                                             configuration;
            if (!foundDiscount)
                buildErrors[g[*v].mapping] = "did not find required discount index " + discountIndex +
                                             " (required from " + ore::data::to_string(g[*v]) +
                                             ") in dependency graph for configuration " + configuration;
        }

        // 5 Equity Vol depends on spot, discount, div

        if (g[*v].obj == MarketObject::EquityVol) {
            bool foundDiscount = false, foundEqCurve = false;
            std::string eqName = g[*v].name;
            auto eqVolSpec = boost::dynamic_pointer_cast<EquityVolatilityCurveSpec>(g[*v].curveSpec);
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
            auto commVolSpec = boost::dynamic_pointer_cast<CommodityVolatilityCurveSpec>(g[*v].curveSpec);
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
    }
    DLOG("Dependency graph built with " << boost::num_vertices(g) << " vertices, " << boost::num_edges(g) << " edges.");
}

void TodaysMarket::buildNode(const std::string& configuration, const Node& node) {
    if (node.curveSpec == nullptr) {

        // not spec-based node, this can only be a SwapIndexCurve

        QL_REQUIRE(node.obj == MarketObject::SwapIndexCurve,
                   "market object '" << node.obj << "' (" << node.name << ") without curve spec, this is unexpected.");
        const string& swapIndexName = node.name;
        const string& discountIndex = node.mapping;
        addSwapIndex(swapIndexName, discountIndex, configuration);
        DLOG("Added SwapIndex " << swapIndexName << " with DiscountingIndex " << discountIndex);
        requiredSwapIndices_[configuration][swapIndexName] = swapIndex(swapIndexName, configuration).currentLink();

    } else {

        // spec-based node

        auto spec = node.curveSpec;
        switch (spec->baseType()) {

        // Yield
        case CurveSpec::CurveType::Yield: {
            boost::shared_ptr<YieldCurveSpec> ycspec = boost::dynamic_pointer_cast<YieldCurveSpec>(spec);
            QL_REQUIRE(ycspec, "Failed to convert spec " << *spec << " to yield curve spec");

            auto itr = requiredYieldCurves_.find(ycspec->name());
            if (itr == requiredYieldCurves_.end()) {
                DLOG("Building YieldCurve for asof " << asof_);
                boost::shared_ptr<YieldCurve> yieldCurve = boost::make_shared<YieldCurve>(
                    asof_, *ycspec, curveConfigs_, loader_, conventions_, requiredYieldCurves_, fxT_, referenceData_);
                itr = requiredYieldCurves_.insert(make_pair(ycspec->name(), yieldCurve)).first;
                DLOG("Added YieldCurve \"" << ycspec->name() << "\" to requiredYieldCurves map");
                if (itr->second->currency().code() != ycspec->ccy()) {
                    WLOG("Warning: YieldCurve has ccy " << itr->second->currency() << " but spec has ccy "
                                                        << ycspec->ccy());
                }
            }

            if (node.obj == MarketObject::DiscountCurve) {
                DLOG("Adding DiscountCurve(" << node.name << ") with spec " << *ycspec << " to configuration "
                                             << configuration);
                yieldCurves_[make_tuple(configuration, YieldCurveType::Discount, node.name)] = itr->second->handle();

            } else if (node.obj == MarketObject::YieldCurve) {
                DLOG("Adding YieldCurve(" << node.name << ") with spec " << *ycspec << " to configuration "
                                          << configuration);
                yieldCurves_[make_tuple(configuration, YieldCurveType::Yield, node.name)] = itr->second->handle();

            } else if (node.obj == MarketObject::IndexCurve) {
                DLOG("Adding Index(" << node.name << ") with spec " << *ycspec << " to configuration "
                                     << configuration);
                iborIndices_[make_pair(configuration, node.name)] = Handle<IborIndex>(
                    parseIborIndex(node.name, itr->second->handle(),
                                   conventions_.has(node.name, Convention::Type::IborIndex) ||
                                           conventions_.has(node.name, Convention::Type::OvernightIndex)
                                       ? conventions_.get(node.name)
                                       : nullptr));
            } else {
                QL_FAIL("unexpected market object type '"
                        << node.obj << "' for yield curve, should be DiscountCurve, YieldCurve, IndexCurve");
            }
            break;
        }

        // FX Spot
        case CurveSpec::CurveType::FX: {
            boost::shared_ptr<FXSpotSpec> fxspec = boost::dynamic_pointer_cast<FXSpotSpec>(spec);
            QL_REQUIRE(fxspec, "Failed to convert spec " << *spec << " to fx spot spec");

            auto itr = requiredFxSpots_.find(fxspec->name());
            if (itr == requiredFxSpots_.end()) {
                DLOG("Building FXSpot for asof " << asof_);
                boost::shared_ptr<FXSpot> fxSpot = boost::make_shared<FXSpot>(asof_, *fxspec, fxT_);
                itr = requiredFxSpots_.insert(make_pair(fxspec->name(), fxSpot)).first;
                // we have added all quotes in the loader initially anyway
                // fxT_.addQuote(fxspec->subName().substr(0, 3) + fxspec->subName().substr(4, 3),
                // itr->second->handle());
            }

            DLOG("Adding FXSpot (" << node.name << ") with spec " << *fxspec << " to configuration " << configuration);
            fxSpots_[configuration].addQuote(node.name, itr->second->handle());
            break;
        }

        // FX Vol
        case CurveSpec::CurveType::FXVolatility: {
            boost::shared_ptr<FXVolatilityCurveSpec> fxvolspec =
                boost::dynamic_pointer_cast<FXVolatilityCurveSpec>(spec);
            QL_REQUIRE(fxvolspec, "Failed to convert spec " << *spec);

            // have we built the curve already ?
            auto itr = requiredFxVolCurves_.find(fxvolspec->name());
            if (itr == requiredFxVolCurves_.end()) {
                DLOG("Building FXVolatility for asof " << asof_);
                boost::shared_ptr<FXVolCurve> fxVolCurve = boost::make_shared<FXVolCurve>(
                    asof_, *fxvolspec, loader_, curveConfigs_, fxT_, requiredYieldCurves_, conventions_);
                itr = requiredFxVolCurves_.insert(make_pair(fxvolspec->name(), fxVolCurve)).first;
            }

            DLOG("Adding FXVol (" << node.name << ") with spec " << *fxvolspec << " to configuration "
                                  << configuration);
            fxVols_[make_pair(configuration, node.name)] =
                Handle<BlackVolTermStructure>(itr->second->volTermStructure());
            break;
        }

        // Swaption Vol
        case CurveSpec::CurveType::SwaptionVolatility: {
            boost::shared_ptr<SwaptionVolatilityCurveSpec> swvolspec =
                boost::dynamic_pointer_cast<SwaptionVolatilityCurveSpec>(spec);
            QL_REQUIRE(swvolspec, "Failed to convert spec " << *spec);

            auto itr = requiredSwaptionVolCurves_.find(swvolspec->name());
            if (itr == requiredSwaptionVolCurves_.end()) {
                DLOG("Building Swaption Volatility for asof " << asof_);
                boost::shared_ptr<SwaptionVolCurve> swaptionVolCurve = boost::make_shared<SwaptionVolCurve>(
                    asof_, *swvolspec, loader_, curveConfigs_, requiredSwapIndices_[configuration]);
                itr = requiredSwaptionVolCurves_.insert(make_pair(swvolspec->name(), swaptionVolCurve)).first;
            }

            boost::shared_ptr<SwaptionVolatilityCurveConfig> cfg =
                curveConfigs_.swaptionVolCurveConfig(swvolspec->curveConfigID());

            DLOG("Adding SwaptionVol (" << node.name << ") with spec " << *swvolspec << " to configuration "
                                        << configuration);
            swaptionCurves_[make_pair(configuration, node.name)] =
                Handle<SwaptionVolatilityStructure>(itr->second->volTermStructure());
            swaptionIndexBases_[make_pair(configuration, node.name)] =
                make_pair(cfg->shortSwapIndexBase(), cfg->swapIndexBase());
            break;
        }

        // Yield Vol
        case CurveSpec::CurveType::YieldVolatility: {
            boost::shared_ptr<YieldVolatilityCurveSpec> ydvolspec =
                boost::dynamic_pointer_cast<YieldVolatilityCurveSpec>(spec);
            QL_REQUIRE(ydvolspec, "Failed to convert spec " << *spec);
            auto itr = requiredYieldVolCurves_.find(ydvolspec->name());
            if (itr == requiredYieldVolCurves_.end()) {
                DLOG("Building Yield Volatility for asof " << asof_);
                boost::shared_ptr<YieldVolCurve> yieldVolCurve =
                    boost::make_shared<YieldVolCurve>(asof_, *ydvolspec, loader_, curveConfigs_);
                itr = requiredYieldVolCurves_.insert(make_pair(ydvolspec->name(), yieldVolCurve)).first;
            }
            DLOG("Adding YieldVol (" << node.name << ") with spec " << *ydvolspec << " to configuration "
                                     << configuration);
            yieldVolCurves_[make_pair(configuration, node.name)] =
                Handle<SwaptionVolatilityStructure>(itr->second->volTermStructure());
            break;
        }

        // Cap Floor Vol
        case CurveSpec::CurveType::CapFloorVolatility: {
            boost::shared_ptr<CapFloorVolatilityCurveSpec> cfVolSpec =
                boost::dynamic_pointer_cast<CapFloorVolatilityCurveSpec>(spec);
            QL_REQUIRE(cfVolSpec, "Failed to convert spec " << *spec);
            boost::shared_ptr<CapFloorVolatilityCurveConfig> cfg =
                curveConfigs_.capFloorVolCurveConfig(cfVolSpec->curveConfigID());

            auto itr = requiredCapFloorVolCurves_.find(cfVolSpec->name());
            if (itr == requiredCapFloorVolCurves_.end()) {
                DLOG("Building cap/floor volatility for asof " << asof_);

                // Firstly, need to retrieve ibor index and discount curve
                // Ibor index
                Handle<IborIndex> iborIndex = MarketImpl::iborIndex(cfg->iborIndex(), configuration);
                // Discount curve
                auto it = requiredYieldCurves_.find(cfg->discountCurve());
                QL_REQUIRE(it != requiredYieldCurves_.end(), "Discount curve with spec, "
                                                                 << cfg->discountCurve()
                                                                 << ", not found in loaded yield curves");
                Handle<YieldTermStructure> discountCurve = it->second->handle();

                // Now create cap/floor vol curve
                boost::shared_ptr<CapFloorVolCurve> capFloorVolCurve = boost::make_shared<CapFloorVolCurve>(
                    asof_, *cfVolSpec, loader_, curveConfigs_, iborIndex.currentLink(), discountCurve);
                itr = requiredCapFloorVolCurves_.insert(make_pair(cfVolSpec->name(), capFloorVolCurve)).first;
            }

            DLOG("Adding CapFloorVol (" << node.name << ") with spec " << *cfVolSpec << " to configuration "
                                        << configuration);
            capFloorCurves_[make_pair(configuration, node.name)] =
                Handle<OptionletVolatilityStructure>(itr->second->capletVolStructure());
            break;
        }

        // Default Curve
        case CurveSpec::CurveType::Default: {
            boost::shared_ptr<DefaultCurveSpec> defaultspec = boost::dynamic_pointer_cast<DefaultCurveSpec>(spec);
            QL_REQUIRE(defaultspec, "Failed to convert spec " << *spec);
            auto itr = requiredDefaultCurves_.find(defaultspec->name());
            if (itr == requiredDefaultCurves_.end()) {
                // build the curve
                DLOG("Building DefaultCurve for asof " << asof_);
                boost::shared_ptr<DefaultCurve> defaultCurve = boost::make_shared<DefaultCurve>(
                    asof_, *defaultspec, loader_, curveConfigs_, conventions_, requiredYieldCurves_);
                itr = requiredDefaultCurves_.insert(make_pair(defaultspec->name(), defaultCurve)).first;
            }
            DLOG("Adding DefaultCurve (" << node.name << ") with spec " << *defaultspec << " to configuration "
                                         << configuration);
            defaultCurves_[make_pair(configuration, node.name)] =
                Handle<DefaultProbabilityTermStructure>(itr->second->defaultTermStructure());
            recoveryRates_[make_pair(configuration, node.name)] =
                Handle<Quote>(boost::make_shared<SimpleQuote>(itr->second->recoveryRate()));
            break;
        }

        // CDS Vol
        case CurveSpec::CurveType::CDSVolatility: {
            boost::shared_ptr<CDSVolatilityCurveSpec> cdsvolspec =
                boost::dynamic_pointer_cast<CDSVolatilityCurveSpec>(spec);
            QL_REQUIRE(cdsvolspec, "Failed to convert spec " << *spec);
            auto itr = requiredCDSVolCurves_.find(cdsvolspec->name());
            if (itr == requiredCDSVolCurves_.end()) {
                DLOG("Building CDSVol for asof " << asof_);
                boost::shared_ptr<CDSVolCurve> cdsVolCurve =
                    boost::make_shared<CDSVolCurve>(asof_, *cdsvolspec, loader_, curveConfigs_);
                itr = requiredCDSVolCurves_.insert(make_pair(cdsvolspec->name(), cdsVolCurve)).first;
            }
            DLOG("Adding CDSVol (" << node.name << ") with spec " << *cdsvolspec << " to configuration "
                                   << configuration);
            cdsVols_[make_pair(configuration, node.name)] =
                Handle<BlackVolTermStructure>(itr->second->volTermStructure());
            break;
        }

        // Base Correlation
        case CurveSpec::CurveType::BaseCorrelation: {
            boost::shared_ptr<BaseCorrelationCurveSpec> baseCorrelationSpec =
                boost::dynamic_pointer_cast<BaseCorrelationCurveSpec>(spec);
            QL_REQUIRE(baseCorrelationSpec, "Failed to convert spec " << *spec);
            auto itr = requiredBaseCorrelationCurves_.find(baseCorrelationSpec->name());
            if (itr == requiredBaseCorrelationCurves_.end()) {
                DLOG("Building BaseCorrelation for asof " << asof_);
                boost::shared_ptr<BaseCorrelationCurve> baseCorrelationCurve =
                    boost::make_shared<BaseCorrelationCurve>(asof_, *baseCorrelationSpec, loader_, curveConfigs_);
                itr =
                    requiredBaseCorrelationCurves_.insert(make_pair(baseCorrelationSpec->name(), baseCorrelationCurve))
                        .first;
            }

            DLOG("Adding Base Correlation (" << node.name << ") with spec " << *baseCorrelationSpec
                                             << " to configuration " << configuration);
            baseCorrelations_[make_pair(configuration, node.name)] =
                Handle<BaseCorrelationTermStructure<BilinearInterpolation>>(
                    itr->second->baseCorrelationTermStructure());
            break;
        }

        // Inflation Curve
        case CurveSpec::CurveType::Inflation: {
            boost::shared_ptr<InflationCurveSpec> inflationspec = boost::dynamic_pointer_cast<InflationCurveSpec>(spec);
            QL_REQUIRE(inflationspec, "Failed to convert spec " << *spec << " to inflation curve spec");
            auto itr = requiredInflationCurves_.find(inflationspec->name());
            if (itr == requiredInflationCurves_.end()) {
                DLOG("Building InflationCurve " << inflationspec->name() << " for asof " << asof_);
                boost::shared_ptr<InflationCurve> inflationCurve = boost::make_shared<InflationCurve>(
                    asof_, *inflationspec, loader_, curveConfigs_, conventions_, requiredYieldCurves_);
                itr = requiredInflationCurves_.insert(make_pair(inflationspec->name(), inflationCurve)).first;
            }

            if (node.obj == MarketObject::ZeroInflationCurve) {
                DLOG("Adding ZeroInflationIndex (" << node.name << ") with spec " << *inflationspec
                                                   << " to configuration " << configuration);
                boost::shared_ptr<ZeroInflationTermStructure> ts =
                    boost::dynamic_pointer_cast<ZeroInflationTermStructure>(itr->second->inflationTermStructure());
                QL_REQUIRE(ts,
                           "expected zero inflation term structure for index " << node.name << ", but could not cast");
                // index is not interpolated
                auto tmp = parseZeroInflationIndex(node.name, false, Handle<ZeroInflationTermStructure>(ts));
                zeroInflationIndices_[make_pair(configuration, node.name)] = Handle<ZeroInflationIndex>(tmp);
            }

            if (node.obj == MarketObject::YoYInflationCurve) {
                DLOG("Adding YoYInflationIndex (" << node.name << ") with spec " << *inflationspec
                                                  << " to configuration " << configuration);
                boost::shared_ptr<YoYInflationTermStructure> ts =
                    boost::dynamic_pointer_cast<YoYInflationTermStructure>(itr->second->inflationTermStructure());
                QL_REQUIRE(ts,
                           "expected yoy inflation term structure for index " << node.name << ", but could not cast");
                yoyInflationIndices_[make_pair(configuration, node.name)] =
                    Handle<YoYInflationIndex>(boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                        parseZeroInflationIndex(node.name, false), false, Handle<YoYInflationTermStructure>(ts)));
            }
            break;
        }

        // Inflation Cap Floor Vol
        case CurveSpec::CurveType::InflationCapFloorVolatility: {
            boost::shared_ptr<InflationCapFloorVolatilityCurveSpec> infcapfloorspec =
                boost::dynamic_pointer_cast<InflationCapFloorVolatilityCurveSpec>(spec);
            QL_REQUIRE(infcapfloorspec, "Failed to convert spec " << *spec << " to inf cap floor spec");
            auto itr = requiredInflationCapFloorVolCurves_.find(infcapfloorspec->name());
            if (itr == requiredInflationCapFloorVolCurves_.end()) {
                DLOG("Building InflationCapFloorVolatilitySurface for asof " << asof_);
                boost::shared_ptr<InflationCapFloorVolCurve> inflationCapFloorVolCurve =
                    boost::make_shared<InflationCapFloorVolCurve>(asof_, *infcapfloorspec, loader_, curveConfigs_,
                                                                  requiredYieldCurves_, requiredInflationCurves_);
                itr = requiredInflationCapFloorVolCurves_
                          .insert(make_pair(infcapfloorspec->name(), inflationCapFloorVolCurve))
                          .first;
            }

            if (node.obj == MarketObject::ZeroInflationCapFloorVol) {
                DLOG("Adding InflationCapFloorVol (" << node.name << ") with spec " << *infcapfloorspec
                                                     << " to configuration " << configuration);
                cpiInflationCapFloorVolatilitySurfaces_[make_pair(configuration, node.name)] =
                    Handle<CPIVolatilitySurface>(itr->second->cpiInflationCapFloorVolSurface());
            }

            if (node.obj == MarketObject::YoYInflationCapFloorVol) {
                DLOG("Adding YoYOptionletVolatilitySurface (" << node.name << ") with spec " << *infcapfloorspec
                                                              << " to configuration " << configuration);
                yoyCapFloorVolSurfaces_[make_pair(configuration, node.name)] =
                    Handle<QuantExt::YoYOptionletVolatilitySurface>(itr->second->yoyInflationCapFloorVolSurface());
            }
            break;
        }

        // Equity Spot
        case CurveSpec::CurveType::Equity: {
            boost::shared_ptr<EquityCurveSpec> equityspec = boost::dynamic_pointer_cast<EquityCurveSpec>(spec);
            QL_REQUIRE(equityspec, "Failed to convert spec " << *spec);
            auto itr = requiredEquityCurves_.find(equityspec->name());
            if (itr == requiredEquityCurves_.end()) {
                DLOG("Building EquityCurve for asof " << asof_);
                boost::shared_ptr<EquityCurve> equityCurve = boost::make_shared<EquityCurve>(
                    asof_, *equityspec, loader_, curveConfigs_, conventions_, requiredYieldCurves_);
                itr = requiredEquityCurves_.insert(make_pair(equityspec->name(), equityCurve)).first;
            }

            DLOG("Adding EquityCurve (" << node.name << ") with spec " << *equityspec << " to configuration "
                                        << configuration);
            yieldCurves_[make_tuple(configuration, YieldCurveType::EquityDividend, node.name)] =
                itr->second->equityIndex()->equityDividendCurve();
            equitySpots_[make_pair(configuration, node.name)] = itr->second->equityIndex()->equitySpot();
            equityCurves_[make_pair(configuration, node.name)] = Handle<EquityIndex>(itr->second->equityIndex());
            break;
        }

        // Equity Vol
        case CurveSpec::CurveType::EquityVolatility: {
            boost::shared_ptr<EquityVolatilityCurveSpec> eqvolspec =
                boost::dynamic_pointer_cast<EquityVolatilityCurveSpec>(spec);
            QL_REQUIRE(eqvolspec, "Failed to convert spec " << *spec);
            auto itr = requiredEquityVolCurves_.find(eqvolspec->name());
            if (itr == requiredEquityVolCurves_.end()) {
                LOG("Building EquityVol for asof " << asof_);
                // First we need the Equity Index, this should already be built
                Handle<EquityIndex> eqIndex = MarketImpl::equityCurve(eqvolspec->curveConfigID(), configuration);
                boost::shared_ptr<EquityVolCurve> eqVolCurve =
                    boost::make_shared<EquityVolCurve>(asof_, *eqvolspec, loader_, curveConfigs_, eqIndex,
                                                       requiredEquityCurves_, requiredEquityVolCurves_);
                itr = requiredEquityVolCurves_.insert(make_pair(eqvolspec->name(), eqVolCurve)).first;
            }
            string eqName = node.name;
            DLOG("Adding EquityVol (" << eqName << ") with spec " << *eqvolspec << " to configuration "
                                      << configuration);

            boost::shared_ptr<BlackVolTermStructure> bvts(itr->second->volTermStructure());
            // Wrap it in QuantExt::BlackVolatilityWithATM as TodaysMarket might be used
            // for model calibration. This is not the ideal place to put this logic but
            // it can't be in EquityVolCurve as there are implicit, configuration dependent,
            // choices made already (e.g. what discount curve to use).
            // We do this even if it is an ATM curve, it does no harm.
            Handle<Quote> spot = equitySpot(eqName, configuration);
            Handle<YieldTermStructure> yts = discountCurve(eqvolspec->ccy(), configuration);
            Handle<YieldTermStructure> divYts = equityDividendCurve(eqName, configuration);
            bvts = boost::make_shared<QuantExt::BlackVolatilityWithATM>(bvts, spot, yts, divYts);

            equityVols_[make_pair(configuration, node.name)] = Handle<BlackVolTermStructure>(bvts);
            break;
        }

        // Security spread, rr, cpr
        case CurveSpec::CurveType::Security: {
            boost::shared_ptr<SecuritySpec> securityspec = boost::dynamic_pointer_cast<SecuritySpec>(spec);
            QL_REQUIRE(securityspec, "Failed to convert spec " << *spec << " to security spec");
            // FIXME this check needs to be moved somewhere else
            // QL_REQUIRE(requiredDefaultCurves_.find(securityspec->securityID()) == requiredDefaultCurves_.end(),
            //            "securities cannot have the same name as a default curve");
            auto itr = requiredSecurities_.find(securityspec->securityID());
            if (itr == requiredSecurities_.end()) {
                DLOG("Building Securities for asof " << asof_);
                boost::shared_ptr<Security> security =
                    boost::make_shared<Security>(asof_, *securityspec, loader_, curveConfigs_);
                itr = requiredSecurities_.insert(make_pair(securityspec->securityID(), security)).first;
            }
            DLOG("Adding Security (" << node.name << ") with spec " << *securityspec << " to configuration "
                                     << configuration);
            if (!itr->second->spread().empty())
                securitySpreads_[make_pair(configuration, node.name)] = itr->second->spread();
            if (!itr->second->recoveryRate().empty())
                recoveryRates_[make_pair(configuration, node.name)] = itr->second->recoveryRate();
            if (!itr->second->cpr().empty())
                cprs_[make_pair(configuration, node.name)] = itr->second->cpr();
            break;
        }

        // Commodity curve
        case CurveSpec::CurveType::Commodity: {
            boost::shared_ptr<CommodityCurveSpec> commodityCurveSpec =
                boost::dynamic_pointer_cast<CommodityCurveSpec>(spec);
            QL_REQUIRE(commodityCurveSpec, "Failed to convert spec, " << *spec << ", to CommodityCurveSpec");
            auto itr = requiredCommodityCurves_.find(commodityCurveSpec->name());
            if (itr == requiredCommodityCurves_.end()) {
                DLOG("Building CommodityCurve for asof " << asof_);
                boost::shared_ptr<CommodityCurve> commodityCurve =
                    boost::make_shared<CommodityCurve>(asof_, *commodityCurveSpec, loader_, curveConfigs_, conventions_,
                                                       fxT_, requiredYieldCurves_, requiredCommodityCurves_);
                itr = requiredCommodityCurves_.insert(make_pair(commodityCurveSpec->name(), commodityCurve)).first;
            }
            DLOG("Adding CommodityCurve, " << node.name << ", with spec " << *commodityCurveSpec << " to configuration "
                                           << configuration);
            commodityCurves_[make_pair(configuration, node.name)] =
                Handle<PriceTermStructure>(itr->second->commodityPriceCurve());
            break;
        }

        // Commodity Vol
        case CurveSpec::CurveType::CommodityVolatility: {

            boost::shared_ptr<CommodityVolatilityCurveSpec> commodityVolSpec =
                boost::dynamic_pointer_cast<CommodityVolatilityCurveSpec>(spec);
            QL_REQUIRE(commodityVolSpec, "Failed to convert spec " << *spec << " to commodity volatility spec");
            auto itr = requiredCommodityVolCurves_.find(commodityVolSpec->name());
            if (itr == requiredCommodityVolCurves_.end()) {
                DLOG("Building commodity volatility for asof " << asof_);
                boost::shared_ptr<CommodityVolCurve> commodityVolCurve = boost::make_shared<CommodityVolCurve>(
                    asof_, *commodityVolSpec, loader_, curveConfigs_, conventions_, requiredYieldCurves_,
                    requiredCommodityCurves_, requiredCommodityVolCurves_);
                itr = requiredCommodityVolCurves_.insert(make_pair(commodityVolSpec->name(), commodityVolCurve)).first;
            }

            string commodityName = node.name;
            DLOG("Adding commodity volatility (" << commodityName << ") with spec " << *commodityVolSpec
                                                 << " to configuration " << configuration);

            // Logic copied from Equity vol section of TodaysMarket for now
            boost::shared_ptr<BlackVolTermStructure> bvts(itr->second->volatility());
            Handle<YieldTermStructure> discount = discountCurve(commodityVolSpec->currency(), configuration);
            Handle<PriceTermStructure> priceCurve = commodityPriceCurve(commodityName, configuration);
            Handle<YieldTermStructure> yield =
                Handle<YieldTermStructure>(boost::make_shared<PriceTermStructureAdapter>(*priceCurve, *discount));
            Handle<Quote> spot(boost::make_shared<SimpleQuote>(priceCurve->price(0, true)));

            bvts = boost::make_shared<QuantExt::BlackVolatilityWithATM>(bvts, spot, discount, yield);
            commodityVols_[make_pair(configuration, node.name)] = Handle<BlackVolTermStructure>(bvts);
            break;
        }

        // Correlation
        case CurveSpec::CurveType::Correlation: {
            boost::shared_ptr<CorrelationCurveSpec> corrspec = boost::dynamic_pointer_cast<CorrelationCurveSpec>(spec);
            auto itr = requiredCorrelationCurves_.find(corrspec->name());
            if (itr == requiredCorrelationCurves_.end()) {
                DLOG("Building CorrelationCurve for asof " << asof_);
                boost::shared_ptr<CorrelationCurve> corrCurve = boost::make_shared<CorrelationCurve>(
                    asof_, *corrspec, loader_, curveConfigs_, conventions_, requiredSwapIndices_[configuration],
                    requiredYieldCurves_, requiredSwaptionVolCurves_);
                itr = requiredCorrelationCurves_.insert(make_pair(corrspec->name(), corrCurve)).first;
            }

            DLOG("Adding CorrelationCurve (" << node.name << ") with spec " << *corrspec << " to configuration "
                                             << configuration);

            // Look for & first as it avoids collisions with : which can be used in an index name
            // if it is not there we fall back on the old behaviour
            string delim;
            if (node.name.find('&') != std::string::npos)
                delim = "&";
            else
                delim = "/:";
            vector<string> tokens;
            boost::split(tokens, node.name, boost::is_any_of(delim));
            QL_REQUIRE(tokens.size() == 2, "Invalid correlation spec " << node.name);
            correlationCurves_[make_tuple(configuration, tokens[0], tokens[1])] =
                Handle<QuantExt::CorrelationTermStructure>(itr->second->corrTermStructure());
            break;
        }

        default: {
            QL_FAIL("Unhandled spec " << *spec);
        }

        } // switch(specName)
    }     // else-block (spec based node)
} // TodaysMarket::buildNode()

std::ostream& operator<<(std::ostream& o, const TodaysMarket::Node& n) {
    return o << n.obj << "(" << n.name << "," << n.mapping << ")";
}

} // namespace data
} // namespace ore
