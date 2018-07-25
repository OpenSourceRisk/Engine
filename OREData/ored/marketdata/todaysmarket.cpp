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
#include <ored/marketdata/curveloader.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/defaultcurve.hpp>
#include <ored/marketdata/equitycurve.hpp>
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/marketdata/fxspot.hpp>
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/marketdata/inflationcapfloorpricesurface.hpp>
#include <ored/marketdata/inflationcurve.hpp>
#include <ored/marketdata/inflationcapfloorvolcurve.hpp>
#include <ored/marketdata/security.hpp>
#include <ored/marketdata/swaptionvolcurve.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/blackvolsurfacewithatm.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>
#include <qle/indexes/equityindex.hpp>

using namespace std;
using namespace QuantLib;

using QuantExt::PriceTermStructure;
using QuantExt::PriceTermStructureAdapter;
using QuantExt::EquityIndex;

namespace ore {
namespace data {

TodaysMarket::TodaysMarket(const Date& asof, const TodaysMarketParameters& params, const Loader& loader,
                           const CurveConfigurations& curveConfigs, const Conventions& conventions)
    : MarketImpl(conventions) {

    // Fixings
    // Apply them now in case a curve builder needs them
    LOG("Todays Market Loading Fixings");
    applyFixings(loader.loadFixings(), conventions);
    LOG("Todays Market Loading Fixing done.");

    // store all curves built, since they might appear in several configurations
    // and might therefore be reused
    map<string, boost::shared_ptr<YieldCurve>> requiredYieldCurves;
    map<string, boost::shared_ptr<SwapIndex>> requiredSwapIndices;
    map<string, boost::shared_ptr<FXSpot>> requiredFxSpots;
    map<string, boost::shared_ptr<FXVolCurve>> requiredFxVolCurves;
    map<string, boost::shared_ptr<SwaptionVolCurve>> requiredSwaptionVolCurves;
    map<string, boost::shared_ptr<CapFloorVolCurve>> requiredCapFloorVolCurves;
    map<string, boost::shared_ptr<DefaultCurve>> requiredDefaultCurves;
    map<string, boost::shared_ptr<CDSVolCurve>> requiredCDSVolCurves;
    map<string, boost::shared_ptr<BaseCorrelationCurve>> requiredBaseCorrelationCurves;
    map<string, boost::shared_ptr<InflationCurve>> requiredInflationCurves;
    map<string, boost::shared_ptr<InflationCapFloorPriceSurface>> requiredInflationCapFloorPriceSurfaces;
    map<string, boost::shared_ptr<InflationCapFloorVolCurve>> requiredInflationCapFloorVolCurves;
    map<string, boost::shared_ptr<EquityCurve>> requiredEquityCurves;
    map<string, boost::shared_ptr<EquityVolCurve>> requiredEquityVolCurves;
    map<string, boost::shared_ptr<Security>> requiredSecurities;
    map<string, boost::shared_ptr<CommodityCurve>> requiredCommodityCurves;
    map<string, boost::shared_ptr<CommodityVolCurve>> requiredCommodityVolCurves;

    for (const auto& configuration : params.configurations()) {

        LOG("Build objects in TodaysMarket configuration " << configuration.first);

        asof_ = asof;

        Size count = 0;

        // Build the curve specs
        vector<boost::shared_ptr<CurveSpec>> specs;
        for (const auto& it : params.curveSpecs(configuration.first)) {
            specs.push_back(parseCurveSpec(it));
            DLOG("CurveSpec: " << specs.back()->name());
        }

        // order them
        order(specs, curveConfigs);
        bool swapIndicesBuilt = false;

        // Loop over each spec, build the curve and add it to the MarketImpl container.
        for (Size count = 0; count < specs.size(); ++count) {

            auto spec = specs[count];
            LOG("Loading spec " << *spec);

            switch (spec->baseType()) {

            case CurveSpec::CurveType::Yield: {
                boost::shared_ptr<YieldCurveSpec> ycspec = boost::dynamic_pointer_cast<YieldCurveSpec>(spec);
                QL_REQUIRE(ycspec, "Failed to convert spec " << *spec << " to yield curve spec");

                // have we built the curve already ?
                auto itr = requiredYieldCurves.find(ycspec->name());
                if (itr == requiredYieldCurves.end()) {
                    // build
                    LOG("Building YieldCurve for asof " << asof);
                    boost::shared_ptr<YieldCurve> yieldCurve = boost::make_shared<YieldCurve>(
                        asof, *ycspec, curveConfigs, loader, conventions, requiredYieldCurves);
                    itr = requiredYieldCurves.insert(make_pair(ycspec->name(), yieldCurve)).first;
                }

                DLOG("Added YieldCurve \"" << ycspec->name() << "\" to requiredYieldCurves map");

                if (itr->second->currency().code() != ycspec->ccy()) {
                    WLOG("Warning: YieldCurve has ccy " << itr->second->currency() << " but spec has ccy "
                                                        << ycspec->ccy());
                }

                // We may have to add this spec multiple times (for discounting, yield and forwarding curves)
                vector<YieldCurveType> yieldCurveTypes = {YieldCurveType::Discount, YieldCurveType::Yield};
                for (auto& y : yieldCurveTypes) {
                    MarketObject o = static_cast<MarketObject>(y);
                    if (params.hasMarketObject(o)) {
                        for (auto& it : params.mapping(o, configuration.first)) {
                            if (it.second == spec->name()) {
                                LOG("Adding YieldCurve(" << it.first << ") with spec " << *ycspec << " to configuration "
                                    << configuration.first);
                                yieldCurves_[make_tuple(configuration.first, y, it.first)] = itr->second->handle();
                            }
                        }
                    }
                }

                if (params.hasMarketObject(MarketObject::IndexCurve)) {
                    for (const auto& it : params.mapping(MarketObject::IndexCurve, configuration.first)) {
                        if (it.second == spec->name()) {
                            LOG("Adding Index(" << it.first << ") with spec " << *ycspec << " to configuration "
                                << configuration.first);
                            iborIndices_[make_pair(configuration.first, it.first)] =
                                Handle<IborIndex>(parseIborIndex(it.first, itr->second->handle()));
                        }
                    }
                }
                break;
            }

            case CurveSpec::CurveType::FX: {
                boost::shared_ptr<FXSpotSpec> fxspec = boost::dynamic_pointer_cast<FXSpotSpec>(spec);
                QL_REQUIRE(fxspec, "Failed to convert spec " << *spec << " to fx spot spec");

                // have we built the curve already ?
                auto itr = requiredFxSpots.find(fxspec->name());
                if (itr == requiredFxSpots.end()) {
                    // build the curve
                    LOG("Building FXSpot for asof " << asof);
                    boost::shared_ptr<FXSpot> fxSpot = boost::make_shared<FXSpot>(asof, *fxspec, loader);
                    itr = requiredFxSpots.insert(make_pair(fxspec->name(), fxSpot)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::FXSpot, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding FXSpot (" << it.first << ") with spec " << *fxspec << " to configuration "
                                              << configuration.first);
                        fxSpots_[configuration.first].addQuote(it.first, itr->second->handle());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::FXVolatility: {
                // convert to fxspec
                boost::shared_ptr<FXVolatilityCurveSpec> fxvolspec =
                    boost::dynamic_pointer_cast<FXVolatilityCurveSpec>(spec);
                QL_REQUIRE(fxvolspec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredFxVolCurves.find(fxvolspec->name());
                if (itr == requiredFxVolCurves.end()) {
                    // build the curve
                    LOG("Building FXVolatility for asof " << asof);
                    boost::shared_ptr<FXVolCurve> fxVolCurve = boost::make_shared<FXVolCurve>(
                        asof, *fxvolspec, loader, curveConfigs, requiredFxSpots, requiredYieldCurves);
                    itr = requiredFxVolCurves.insert(make_pair(fxvolspec->name(), fxVolCurve)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::FXVol, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding FXVol (" << it.first << ") with spec " << *fxvolspec << " to configuration "
                                             << configuration.first);
                        fxVols_[make_pair(configuration.first, it.first)] =
                            Handle<BlackVolTermStructure>(itr->second->volTermStructure());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::SwaptionVolatility: {
                // convert to volspec
                boost::shared_ptr<SwaptionVolatilityCurveSpec> swvolspec =
                    boost::dynamic_pointer_cast<SwaptionVolatilityCurveSpec>(spec);
                QL_REQUIRE(swvolspec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredSwaptionVolCurves.find(swvolspec->name());
                if (itr == requiredSwaptionVolCurves.end()) {
                    // build the curve
                    LOG("Building Swaption Volatility for asof " << asof);
                    boost::shared_ptr<SwaptionVolCurve> swaptionVolCurve = boost::make_shared<SwaptionVolCurve>(
                        asof, *swvolspec, loader, curveConfigs, requiredSwapIndices);
                    itr = requiredSwaptionVolCurves.insert(make_pair(swvolspec->name(), swaptionVolCurve)).first;
                }

                boost::shared_ptr<SwaptionVolatilityCurveConfig> cfg =
                    curveConfigs.swaptionVolCurveConfig(swvolspec->curveConfigID());

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::SwaptionVol, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding SwaptionVol (" << it.first << ") with spec " << *swvolspec << " to configuration "
                                                   << configuration.first);
                        swaptionCurves_[make_pair(configuration.first, it.first)] =
                            Handle<SwaptionVolatilityStructure>(itr->second->volTermStructure());
                        swaptionIndexBases_[make_pair(configuration.first, it.first)] =
                            make_pair(cfg->shortSwapIndexBase(), cfg->swapIndexBase());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::CapFloorVolatility: {
                // attempt to convert to cap/floor curve spec
                boost::shared_ptr<CapFloorVolatilityCurveSpec> cfVolSpec =
                    boost::dynamic_pointer_cast<CapFloorVolatilityCurveSpec>(spec);
                QL_REQUIRE(cfVolSpec, "Failed to convert spec " << *spec);

                // Get the cap/floor volatility "curve" config
                boost::shared_ptr<CapFloorVolatilityCurveConfig> cfg =
                    curveConfigs.capFloorVolCurveConfig(cfVolSpec->curveConfigID());

                // Build the market data object if we have not built it already
                auto itr = requiredCapFloorVolCurves.find(cfVolSpec->name());
                if (itr == requiredCapFloorVolCurves.end()) {
                    LOG("Building cap/floor volatility for asof " << asof);

                    // Firstly, need to retrieve ibor index and discount curve
                    // Ibor index
                    Handle<IborIndex> iborIndex = MarketImpl::iborIndex(cfg->iborIndex(), configuration.first);
                    // Discount curve
                    auto it = requiredYieldCurves.find(cfg->discountCurve());
                    QL_REQUIRE(it != requiredYieldCurves.end(), "Discount curve with spec, "
                                                                    << cfg->discountCurve()
                                                                    << ", not found in loaded yield curves");
                    Handle<YieldTermStructure> discountCurve = it->second->handle();

                    // Now create cap/floor vol curve
                    boost::shared_ptr<CapFloorVolCurve> capFloorVolCurve = boost::make_shared<CapFloorVolCurve>(
                        asof, *cfVolSpec, loader, curveConfigs, iborIndex.currentLink(), discountCurve);
                    itr = requiredCapFloorVolCurves.insert(make_pair(cfVolSpec->name(), capFloorVolCurve)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::CapFloorVol, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding CapFloorVol (" << it.first << ") with spec " << *cfVolSpec << " to configuration "
                                                   << configuration.first);
                        capFloorCurves_[make_pair(configuration.first, it.first)] =
                            Handle<OptionletVolatilityStructure>(itr->second->capletVolStructure());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::Default: {
                boost::shared_ptr<DefaultCurveSpec> defaultspec = boost::dynamic_pointer_cast<DefaultCurveSpec>(spec);
                QL_REQUIRE(defaultspec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredDefaultCurves.find(defaultspec->name());
                if (itr == requiredDefaultCurves.end()) {
                    // build the curve
                    LOG("Building DefaultCurve for asof " << asof);
                    boost::shared_ptr<DefaultCurve> defaultCurve = boost::make_shared<DefaultCurve>(
                        asof, *defaultspec, loader, curveConfigs, conventions, requiredYieldCurves);
                    itr = requiredDefaultCurves.insert(make_pair(defaultspec->name(), defaultCurve)).first;
                }

                for (const auto it : params.mapping(MarketObject::DefaultCurve, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding DefaultCurve (" << it.first << ") with spec " << *defaultspec
                                                    << " to configuration " << configuration.first);
                        defaultCurves_[make_pair(configuration.first, it.first)] =
                            Handle<DefaultProbabilityTermStructure>(itr->second->defaultTermStructure());
                        recoveryRates_[make_pair(configuration.first, it.first)] =
                            Handle<Quote>(boost::make_shared<SimpleQuote>(itr->second->recoveryRate()));
                    }
                }
                break;
            }

            case CurveSpec::CurveType::CDSVolatility: {
                // convert to cds vol spec
                boost::shared_ptr<CDSVolatilityCurveSpec> cdsvolspec =
                    boost::dynamic_pointer_cast<CDSVolatilityCurveSpec>(spec);
                QL_REQUIRE(cdsvolspec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredCDSVolCurves.find(cdsvolspec->name());
                if (itr == requiredCDSVolCurves.end()) {
                    // build the curve
                    LOG("Building CDSVol for asof " << asof);
                    boost::shared_ptr<CDSVolCurve> cdsVolCurve =
                        boost::make_shared<CDSVolCurve>(asof, *cdsvolspec, loader, curveConfigs);
                    itr = requiredCDSVolCurves.insert(make_pair(cdsvolspec->name(), cdsVolCurve)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::CDSVol, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding CDSVol (" << it.first << ") with spec " << *cdsvolspec << " to configuration "
                                              << configuration.first);
                        cdsVols_[make_pair(configuration.first, it.first)] =
                            Handle<BlackVolTermStructure>(itr->second->volTermStructure());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::BaseCorrelation: {
                // convert to base correlation spec
                boost::shared_ptr<BaseCorrelationCurveSpec> baseCorrelationSpec =
                    boost::dynamic_pointer_cast<BaseCorrelationCurveSpec>(spec);
                QL_REQUIRE(baseCorrelationSpec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredBaseCorrelationCurves.find(baseCorrelationSpec->name());
                if (itr == requiredBaseCorrelationCurves.end()) {
                    // build the curve
                    LOG("Building BaseCorrelation for asof " << asof);
                    boost::shared_ptr<BaseCorrelationCurve> baseCorrelationCurve =
                        boost::make_shared<BaseCorrelationCurve>(asof, *baseCorrelationSpec, loader, curveConfigs);
                    itr = requiredBaseCorrelationCurves
                              .insert(make_pair(baseCorrelationSpec->name(), baseCorrelationCurve))
                              .first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::BaseCorrelation, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding Base Correlatin (" << it.first << ") with spec " << *baseCorrelationSpec
                                                       << " to configuration " << configuration.first);
                        baseCorrelations_[make_pair(configuration.first, it.first)] =
                            Handle<BaseCorrelationTermStructure<BilinearInterpolation>>(
                                itr->second->baseCorrelationTermStructure());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::Inflation: {
                boost::shared_ptr<InflationCurveSpec> inflationspec =
                    boost::dynamic_pointer_cast<InflationCurveSpec>(spec);
                QL_REQUIRE(inflationspec, "Failed to convert spec " << *spec << " to inflation curve spec");

                // have we built the curve already ?
                auto itr = requiredInflationCurves.find(inflationspec->name());
                if (itr == requiredInflationCurves.end()) {
                    LOG("Building InflationCurve " << inflationspec->name() << " for asof " << asof);
                    boost::shared_ptr<InflationCurve> inflationCurve = boost::make_shared<InflationCurve>(
                        asof, *inflationspec, loader, curveConfigs, conventions, requiredYieldCurves);
                    itr = requiredInflationCurves.insert(make_pair(inflationspec->name(), inflationCurve)).first;
                }
                // this try-catch is necessary to handle cases where no ZC inflation index curves exist in scope
                map<string, string> zcInfMap;
                try {
                    zcInfMap = params.mapping(MarketObject::ZeroInflationCurve, configuration.first);
                } catch (QuantLib::Error& e) {
                    LOG(e.what());
                }
                for (const auto it : zcInfMap) {
                    if (it.second == spec->name()) {
                        LOG("Adding ZeroInflationIndex (" << it.first << ") with spec " << *inflationspec
                                                          << " to configuration " << configuration.first);
                        boost::shared_ptr<ZeroInflationTermStructure> ts =
                            boost::dynamic_pointer_cast<ZeroInflationTermStructure>(
                                itr->second->inflationTermStructure());
                        QL_REQUIRE(ts, "expected zero inflation term structure for index " << it.first
                                                                                           << ", but could not cast");
                        // index is not interpolated
                        auto tmp = parseZeroInflationIndex(it.first, false, Handle<ZeroInflationTermStructure>(ts));
                        zeroInflationIndices_[make_pair(configuration.first, it.first)] =
                            Handle<ZeroInflationIndex>(tmp);
                    }
                }
                // this try-catch is necessary to handle cases where no YoY inflation index curves exist in scope
                map<string, string> yyInfMap;
                try {
                    yyInfMap = params.mapping(MarketObject::YoYInflationCurve, configuration.first);
                } catch (QuantLib::Error& e) {
                    LOG(e.what());
                }
                for (const auto it : yyInfMap) {
                    if (it.second == spec->name()) {
                        LOG("Adding YoYInflationIndex (" << it.first << ") with spec " << *inflationspec
                                                         << " to configuration " << configuration.first);
                        boost::shared_ptr<YoYInflationTermStructure> ts =
                            boost::dynamic_pointer_cast<YoYInflationTermStructure>(
                                itr->second->inflationTermStructure());
                        QL_REQUIRE(ts, "expected yoy inflation term structure for index " << it.first
                                                                                          << ", but could not cast");
                        yoyInflationIndices_[make_pair(configuration.first, it.first)] =
                            Handle<YoYInflationIndex>(boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                                parseZeroInflationIndex(it.first, false), false,
                                Handle<YoYInflationTermStructure>(ts)));                        
                    }
                }
                break;
            }

            case CurveSpec::CurveType::InflationCapFloorPrice: {
                boost::shared_ptr<InflationCapFloorPriceSurfaceSpec> infcapfloorspec =
                    boost::dynamic_pointer_cast<InflationCapFloorPriceSurfaceSpec>(spec);
                QL_REQUIRE(infcapfloorspec, "Failed to convert spec " << *spec << " to inf cap floor spec");

                // have we built the curve already ?
                auto itr = requiredInflationCapFloorPriceSurfaces.find(infcapfloorspec->name());
                if (itr == requiredInflationCapFloorPriceSurfaces.end()) {
                    LOG("Building InflationCapFloorPriceSurface for asof " << asof);
                    boost::shared_ptr<InflationCapFloorPriceSurface> inflationCapFloorPriceSurface =
                        boost::make_shared<InflationCapFloorPriceSurface>(asof, *infcapfloorspec, loader, curveConfigs,
                                                                          requiredYieldCurves, requiredInflationCurves);
                    itr = requiredInflationCapFloorPriceSurfaces
                              .insert(make_pair(infcapfloorspec->name(),inflationCapFloorPriceSurface))
                              .first;
                }

                map<string, string> zcInfMap;
                try {
                    zcInfMap = params.mapping(MarketObject::InflationCapFloorPriceSurface, configuration.first);
                }
                catch (QuantLib::Error& e) {
                    LOG(e.what());
                }
                for (const auto it : zcInfMap) {
                    if (it.second == spec->name()) {
                        LOG("Adding InflationCapFloorPriceSurface (" << it.first << ") with spec " << *infcapfloorspec
                                                                     << " to configuration " << configuration.first);
                        cpiInflationCapFloorPriceSurfaces_[make_pair(configuration.first, it.first)] =
                            Handle<CPICapFloorTermPriceSurface>(
                                boost::dynamic_pointer_cast<CPICapFloorTermPriceSurface>(itr->second->inflationCapFloorPriceSurface()));
                    }
                }

                map<string, string> yyInfMap;
                try {
                    yyInfMap = params.mapping(MarketObject::YoYInflationCapFloorPriceSurface, configuration.first);
                }
                catch (QuantLib::Error& e) {
                    LOG(e.what());
                }
                for (const auto it : yyInfMap) {
                    if (it.second == spec->name()) {
                        LOG("Adding YoYInflationCapFloorPriceSurface (" << it.first << ") with spec " << *infcapfloorspec
                            << " to configuration " << configuration.first);
                        yoyInflationCapFloorPriceSurfaces_[make_pair(configuration.first, it.first)] =
                            Handle<YoYCapFloorTermPriceSurface>(
                                boost::dynamic_pointer_cast<YoYCapFloorTermPriceSurface>(
                                    itr->second->inflationCapFloorPriceSurface()));

                        LOG("Adding YoYOptionletVolatilitySurface (" << it.first << ") with spec " << *infcapfloorspec
                            << " to configuration " << configuration.first);
                        yoyCapFloorVolSurfaces_[make_pair(configuration.first, it.first)] =
                            Handle<QuantExt::YoYOptionletVolatilitySurface>(
                                boost::dynamic_pointer_cast<QuantExt::YoYOptionletVolatilitySurface>(
                                    itr->second->yoyInflationCapFloorVolSurface()));
                                                                   
                        if (!itr->second->useMarketYoyCurve()) {
                            LOG("Adding YoYInflationCurve (" << it.first << ") to configuration " << configuration.first);
                            boost::shared_ptr<YoYInflationTermStructure> ts = itr->second->yoyInflationAtmCurve();
                            QL_REQUIRE(ts, "expected yoy inflation term structure for index " << it.first
                                << ", but could not cast");
                            yoyInflationIndices_[make_pair(configuration.first, it.first)] =
                                Handle<YoYInflationIndex>(boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                                    parseZeroInflationIndex(it.first, false), false,
                                    Handle<YoYInflationTermStructure>(ts)));
                        }

                    }
                }

                break;
            }

            case CurveSpec::CurveType::InflationCapFloorVolatility: {
                boost::shared_ptr<InflationCapFloorVolatilityCurveSpec> infcapfloorspec =
                    boost::dynamic_pointer_cast<InflationCapFloorVolatilityCurveSpec>(spec);
                QL_REQUIRE(infcapfloorspec, "Failed to convert spec " << *spec << " to inf cap floor spec");

                // have we built the curve already ?
                auto itr = requiredInflationCapFloorVolCurves.find(infcapfloorspec->name());
                if (itr == requiredInflationCapFloorVolCurves.end()) {
                    LOG("Building InflationCapFloorPriceSurface for asof " << asof);
                    boost::shared_ptr<InflationCapFloorVolCurve> inflationCapFloorVolCurve =
                        boost::make_shared<InflationCapFloorVolCurve>(asof, *infcapfloorspec, loader, curveConfigs,
                            requiredYieldCurves, requiredInflationCurves);
                    itr = requiredInflationCapFloorVolCurves
                        .insert(make_pair(infcapfloorspec->name(), inflationCapFloorVolCurve))
                        .first;
                }

                map<string, string> zcInfMap;
                try {
                    zcInfMap = params.mapping(MarketObject::ZeroInflationCapFloorVol, configuration.first);
                }
                catch (QuantLib::Error& e) {
                    LOG(e.what());
                }
                for (const auto it : zcInfMap) {
                    if (it.second == spec->name()) {
                        LOG("Adding InflationCapFloorVol (" << it.first << ") with spec " << *infcapfloorspec
                            << " to configuration " << configuration.first);
                        // Add Zero Inflation Vol curves
                    }
                }

                map<string, string> yyInfMap;
                try {
                    yyInfMap = params.mapping(MarketObject::YoYInflationCapFloorVol, configuration.first);
                }
                catch (QuantLib::Error& e) {
                    LOG(e.what());
                }
                for (const auto it : yyInfMap) {
                    if (it.second == spec->name()) {
                        LOG("Adding YoYOptionletVolatilitySurface (" << it.first << ") with spec " << *infcapfloorspec
                            << " to configuration " << configuration.first);
                        yoyCapFloorVolSurfaces_[make_pair(configuration.first, it.first)] =
                            Handle<QuantExt::YoYOptionletVolatilitySurface>(itr->second->yoyInflationCapFloorVolSurface());
                    }
                }
                break;
            }

            case CurveSpec::CurveType::Equity: {
                boost::shared_ptr<EquityCurveSpec> equityspec = boost::dynamic_pointer_cast<EquityCurveSpec>(spec);
                QL_REQUIRE(equityspec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredEquityCurves.find(equityspec->name());
                if (itr == requiredEquityCurves.end()) {
                    // build the curve
                    LOG("Building EquityCurve for asof " << asof);
                    boost::shared_ptr<EquityCurve> equityCurve = boost::make_shared<EquityCurve>(
                        asof, *equityspec, loader, curveConfigs, conventions, requiredYieldCurves);
                    itr = requiredEquityCurves.insert(make_pair(equityspec->name(), equityCurve)).first;
                }

                for (const auto it : params.mapping(MarketObject::EquityCurve, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding EquityCurve (" << it.first << ") with spec " << *equityspec << " to configuration "
                                                   << configuration.first);
                        Handle<YieldTermStructure> yts;
                        boost::shared_ptr<EquityCurveConfig> equityConfig =
                            curveConfigs.equityCurveConfig(equityspec->curveConfigID());
                        boost::shared_ptr<YieldTermStructure> divYield = itr->second->divYieldTermStructure(asof);
                        Handle<YieldTermStructure> div_h(divYield);
                        Handle<Quote> eqSpot = Handle<Quote>(boost::make_shared<SimpleQuote>(itr->second->equitySpot()));

                        boost::shared_ptr<EquityIndex> eqCurve = 
                            boost::make_shared<EquityIndex>(it.first, parseCalendar(equityspec->ccy()), eqSpot,
                                itr->second->forecastingYieldTermStructure(),div_h);
                        Handle<EquityIndex> eq_h(eqCurve);
                        yieldCurves_[make_tuple(configuration.first, YieldCurveType::EquityDividend, it.first)] = div_h;
                        yieldCurves_[make_tuple(configuration.first, YieldCurveType::EquityForecast, it.first)] =
                            itr->second->forecastingYieldTermStructure();
                        equitySpots_[make_pair(configuration.first, it.first)] = eqSpot;
                        equityCurves_[make_pair(configuration.first, it.first)] = eq_h;
                    }
                }
                break;
            }

            case CurveSpec::CurveType::EquityVolatility: {
                // convert to eqvolspec
                boost::shared_ptr<EquityVolatilityCurveSpec> eqvolspec =
                    boost::dynamic_pointer_cast<EquityVolatilityCurveSpec>(spec);
                QL_REQUIRE(eqvolspec, "Failed to convert spec " << *spec);

                // have we built the curve already ?
                auto itr = requiredEquityVolCurves.find(eqvolspec->name());
                if (itr == requiredEquityVolCurves.end()) {
                    // build the curve
                    LOG("Building EquityVol for asof " << asof);

                    boost::shared_ptr<EquityVolCurve> eqVolCurve =
                        boost::make_shared<EquityVolCurve>(asof, *eqvolspec, loader, curveConfigs);
                    itr = requiredEquityVolCurves.insert(make_pair(eqvolspec->name(), eqVolCurve)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::EquityVol, configuration.first)) {
                    if (it.second == spec->name()) {
                        string eqName = it.first;
                        LOG("Adding EquityVol (" << eqName << ") with spec " << *eqvolspec << " to configuration "
                                                 << configuration.first);

                        boost::shared_ptr<BlackVolTermStructure> bvts(itr->second->volTermStructure());
                        // Wrap it in QuantExt::BlackVolatilityWithATM as TodaysMarket might be used
                        // for model calibration. This is not the ideal place to put this logic but
                        // it can't be in EquityVolCurve as there are implicit, configuration dependent,
                        // choices made already (e.g. what discount curve to use).
                        // We do this even if it is an ATM curve, it does no harm.
                        Handle<Quote> spot = equitySpot(eqName, configuration.first);
                        Handle<YieldTermStructure> yts = discountCurve(eqvolspec->ccy(), configuration.first);
                        Handle<YieldTermStructure> divYts = equityDividendCurve(eqName, configuration.first);
                        bvts = boost::make_shared<QuantExt::BlackVolatilityWithATM>(bvts, spot, yts, divYts);

                        equityVols_[make_pair(configuration.first, it.first)] = Handle<BlackVolTermStructure>(bvts);
                    }
                }
                break;
            }

            case CurveSpec::CurveType::Security: {
                boost::shared_ptr<SecuritySpec> securityspec = boost::dynamic_pointer_cast<SecuritySpec>(spec);
                QL_REQUIRE(securityspec, "Failed to convert spec " << *spec << " to security spec");

                auto check = requiredDefaultCurves.find(securityspec->securityID());
                if (check != requiredDefaultCurves.end())
                    QL_FAIL("securities cannot have the same name as a default curve");

                // have we built the security spread already?
                auto itr = requiredSecurities.find(securityspec->securityID());

                if (itr == requiredSecurities.end()) {
                    // build the curve
                    LOG("Building Securities for asof " << asof);
                    boost::shared_ptr<Security> security = boost::make_shared<Security>(asof, *securityspec, loader);
                    itr = requiredSecurities.insert(make_pair(securityspec->securityID(), security)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::Security, configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding Security (" << it.first << ") with spec " << *securityspec << " to configuration "
                                                << configuration.first);
                        securitySpreads_[make_pair(configuration.first, it.first)] = itr->second->spread();
                        recoveryRates_[make_pair(configuration.first, it.first)] = itr->second->recoveryRate();
                    }
                }

                break;
            }

            case CurveSpec::CurveType::Commodity: {
                boost::shared_ptr<CommodityCurveSpec> commodityCurveSpec = boost::dynamic_pointer_cast<CommodityCurveSpec>(spec);
                QL_REQUIRE(commodityCurveSpec, "Failed to convert spec, " << *spec << ", to CommodityCurveSpec");

                // Have we built the curve already?
                auto itr = requiredCommodityCurves.find(commodityCurveSpec->name());
                if (itr == requiredCommodityCurves.end()) {
                    // build the curve
                    LOG("Building CommodityCurve for asof " << asof);
                    boost::shared_ptr<CommodityCurve> commodityCurve = boost::make_shared<CommodityCurve>(
                        asof, *commodityCurveSpec, loader, curveConfigs, conventions);
                    itr = requiredCommodityCurves.insert(make_pair(commodityCurveSpec->name(), commodityCurve)).first;
                }

                for (const auto it : params.mapping(MarketObject::CommodityCurve, configuration.first)) {
                    if (it.second == commodityCurveSpec->name()) {
                        LOG("Adding CommodityCurve, " << it.first << ", with spec " << 
                            *commodityCurveSpec << " to configuration " << configuration.first);
                        commodityCurves_[make_pair(configuration.first, it.first)] = 
                            Handle<PriceTermStructure>(itr->second->commodityPriceCurve());
                        commoditySpots_[make_pair(configuration.first, it.first)] =
                            Handle<Quote>(boost::make_shared<SimpleQuote>(itr->second->commoditySpot()));
                    }
                }
                break;
            }

            case CurveSpec::CurveType::CommodityVolatility: {

                boost::shared_ptr<CommodityVolatilityCurveSpec> commodityVolSpec =
                    boost::dynamic_pointer_cast<CommodityVolatilityCurveSpec>(spec);
                QL_REQUIRE(commodityVolSpec, "Failed to convert spec " << *spec << " to commodity volatility spec");

                // Build the volatility structure if we have not built it before
                auto itr = requiredCommodityVolCurves.find(commodityVolSpec->name());
                if (itr == requiredCommodityVolCurves.end()) {
                    LOG("Building commodity volatility for asof " << asof);

                    boost::shared_ptr<CommodityVolCurve> commodityVolCurve =
                        boost::make_shared<CommodityVolCurve>(asof, *commodityVolSpec, loader, curveConfigs);
                    itr = requiredCommodityVolCurves.insert(make_pair(commodityVolSpec->name(), commodityVolCurve)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.mapping(MarketObject::CommodityVolatility, configuration.first)) {
                    if (it.second == spec->name()) {
                        string commodityName = it.first;
                        LOG("Adding commodity volatility (" << commodityName << ") with spec " << 
                            *commodityVolSpec << " to configuration " << configuration.first);

                        // Logic copied from Equity vol section of TodaysMarket for now
                        boost::shared_ptr<BlackVolTermStructure> bvts(itr->second->volatility());
                        Handle<Quote> spot = commoditySpot(commodityName, configuration.first);
                        Handle<YieldTermStructure> discount = discountCurve(commodityVolSpec->currency(), configuration.first);
                        Handle<PriceTermStructure> priceCurve = commodityPriceCurve(commodityName, configuration.first);
                        Handle<YieldTermStructure> yield = Handle<YieldTermStructure>(
                            boost::make_shared<PriceTermStructureAdapter>(*spot, *priceCurve, *discount));

                        bvts = boost::make_shared<QuantExt::BlackVolatilityWithATM>(bvts, spot, discount, yield);
                        commodityVols_[make_pair(configuration.first, it.first)] = Handle<BlackVolTermStructure>(bvts);
                    }
                }
                break;
            }

            default: {
                // maybe we just log and continue? need to update count then
                QL_FAIL("Unhandled spec " << *spec);
            }
            }

            // Swap Indices
            // Assumes we build all yield curves before anything else (which order() does)
            // Once we have a non-Yield curve spec, we make sure to build all swap indices
            // add add them to requiredSwapIndices for later.
            if (swapIndicesBuilt == false &&
                (count == specs.size() - 1 || specs[count + 1]->baseType() != CurveSpec::CurveType::Yield)) {
                LOG("building swap indices...");
                for (const auto& it : params.mapping(MarketObject::SwapIndexCurve, configuration.first)) {
                    const string& swapIndexName = it.first;
                    const string& discountIndex = it.second;

                    addSwapIndex(swapIndexName, discountIndex, configuration.first);
                    LOG("Added SwapIndex " << swapIndexName << " with DiscountingIndex " << discountIndex);
                    requiredSwapIndices[swapIndexName] = swapIndex(swapIndexName, configuration.first).currentLink();
                }
                swapIndicesBuilt = true;
            }

            LOG("Loading spec " << *spec << " done.");
        }
        LOG("Loading " << count << " CurveSpecs done.");

    } // loop over configurations

} // CTOR
} // namespace data
} // namespace ore
