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

/*! \file marketdata/todaysmarket.hpp
    \brief An concrete implementation of the Market class that loads todays market and builds the required curves
    \ingroup marketdata
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/marketdata/dependencygraph.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/graph_traits.hpp>
#include <ql/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <map>

namespace ore {
namespace data {

class ReferenceDataManager;
class YieldCurve;
class FXSpot;
class FXVolCurve;
class GenericYieldVolCurve;
class YieldVolCurve;
class CapFloorVolCurve;
class DefaultCurve;
class CDSVolCurve;
class BaseCorrelationCurve;
class InflationCurve;
class InflationCapFloorVolCurve;
class EquityCurve;
class EquityVolCurve;
class Security;
class CommodityCurve;
class CommodityVolCurve;
class CorrelationCurve;

// TODO: rename class
//! Today's Market
/*!
  Today's Market differs from MarketImpl in that it actually loads market data
  and builds term structure objects.

  We label this object Today's Market in contrast to the Simulation Market which can
  differ in composition and granularity. The Simulation Market is initialised using a Today's Market
  object.

  Today's market's purpose is t0 pricing, the Simulation Market's purpose is
  pricing under future scenarios.

  \ingroup marketdata
 */
class TodaysMarket : public MarketImpl {
public:
    //! Constructor taking pointers and allowing for a lazy build of the market objects
    TodaysMarket( //! Valuation date
        const Date& asof,
        //! Description of the market composition
        const QuantLib::ext::shared_ptr<TodaysMarketParameters>& params,
        //! Market data loader
        const QuantLib::ext::shared_ptr<Loader>& loader,
        //! Description of curve compositions
        const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs,
        //! Continue even if build errors occur
        const bool continueOnError = false,
        //! Optional Load Fixings
        const bool loadFixings = true,
        //! If yes, build market objects lazily
        const bool lazyBuild = false,
        //! Optional reference data manager, needed to build fitted bond curves
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
        //! If true, preserve link to loader quotes, this might heavily interfere with XVA simulations!
        const bool preserveQuoteLinkage = false,
        //! the ibor fallback config
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
        //! build calibration info?
        const bool buildCalibrationInfo = true,
        //! support pseudo currencies
        const bool handlePseudoCurrencies = true);

    QuantLib::ext::shared_ptr<TodaysMarketCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }

private:
    // MarketImpl interface
    void require(const MarketObject o, const string& name, const string& configuration,
                 const bool forceBuild = false) const override;

    // input parameters

    const QuantLib::ext::shared_ptr<TodaysMarketParameters> params_;
    const QuantLib::ext::shared_ptr<Loader> loader_;
    const QuantLib::ext::shared_ptr<const CurveConfigurations> curveConfigs_;

    const bool continueOnError_;
    const bool loadFixings_;
    const bool lazyBuild_;
    const bool preserveQuoteLinkage_;
    const QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;
    const IborFallbackConfig iborFallbackConfig_;
    const bool buildCalibrationInfo_;

    // initialise market
    void initialise(const Date& asof);

    // some typedefs for graph related data types
    using Node = DependencyGraph::Node;
    using Graph = boost::directed_graph<Node>;
    using IndexMap = boost::property_map<Graph, boost::vertex_index_t>::type;
    using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
    using VertexIterator = boost::graph_traits<Graph>::vertex_iterator;

    // the dependency graphs for each configuration
    mutable std::map<std::string, Graph> dependencies_;

    // build a single market object
    void buildNode(const std::string& configuration, Node& node) const;

    // calibration results
    QuantLib::ext::shared_ptr<TodaysMarketCalibrationInfo> calibrationInfo_;

    // cached market objects, the key of the maps is the curve spec name, except for swap indices, see below
    mutable map<string, QuantLib::ext::shared_ptr<YieldCurve>> requiredYieldCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<FXVolCurve>> requiredFxVolCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>> requiredGenericYieldVolCurves_;
    mutable map<string, std::pair<QuantLib::ext::shared_ptr<CapFloorVolCurve>, std::pair<std::string, QuantLib::Period>>>
        requiredCapFloorVolCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<DefaultCurve>> requiredDefaultCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<CDSVolCurve>> requiredCDSVolCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<BaseCorrelationCurve>> requiredBaseCorrelationCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<InflationCurve>> requiredInflationCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<InflationCapFloorVolCurve>> requiredInflationCapFloorVolCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<EquityCurve>> requiredEquityCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<EquityVolCurve>> requiredEquityVolCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<Security>> requiredSecurities_;
    mutable map<string, QuantLib::ext::shared_ptr<CommodityCurve>> requiredCommodityCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<CommodityVolCurve>> requiredCommodityVolCurves_;
    mutable map<string, QuantLib::ext::shared_ptr<CorrelationCurve>> requiredCorrelationCurves_;
    // for swap indices we map the configuration name to a map (swap index name => index)
    mutable map<string, map<string, QuantLib::ext::shared_ptr<SwapIndex>>> requiredSwapIndices_;
};

std::ostream& operator<<(std::ostream& o, const DependencyGraph::Node& n);

} // namespace data
} // namespace ore
