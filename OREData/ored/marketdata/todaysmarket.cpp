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

#include <ored/marketdata/capfloorvolcurve.hpp>
#include <ored/marketdata/curveloader.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/defaultcurve.hpp>
#include <ored/marketdata/fxspot.hpp>
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/marketdata/swaptionvolcurve.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

TodaysMarket::TodaysMarket(const Date& asof, const TodaysMarketParameters& params, const Loader& loader,
                           const CurveConfigurations& curveConfigs, const Conventions& conventions)
    : MarketImpl(conventions) {

    // Fixings
    // Apply them now in case a curve builder needs them
    LOG("Todays Market Loading Fixings");
    applyFixings(loader.loadFixings());
    LOG("Todays Market Loading Fixing done.");

    // store all curves built, since they might appear in several configurations
    // and might therefore be reused
    map<string, boost::shared_ptr<YieldCurve>> requiredYieldCurves;
    map<string, boost::shared_ptr<FXSpot>> requiredFxSpots;
    map<string, boost::shared_ptr<FXVolCurve>> requiredFxVolCurves;
    map<string, boost::shared_ptr<SwaptionVolCurve>> requiredSwaptionVolCurves;
    map<string, boost::shared_ptr<CapFloorVolCurve>> requiredCapFloorVolCurves;
    map<string, boost::shared_ptr<DefaultCurve>> requiredDefaultCurves;

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

        // Loop over each spec, build the curve and add it to the MarketImpl container.
        for (const auto& spec : specs) {

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
                for (auto& it : params.discountingCurves(configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding DiscountCurve(" << it.first << ") with spec " << *ycspec << " to configuration "
                                                    << configuration.first);
                        discountCurves_[make_pair(configuration.first, it.first)] = itr->second->handle();
                    }
                }
                for (auto& it : params.yieldCurves(configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding YieldCurve(" << it.first << ") with spec " << *ycspec << " to configuration "
                                                 << configuration.first);
                        yieldCurves_[make_pair(configuration.first, it.first)] = itr->second->handle();
                    }
                }
                for (const auto& it : params.indexForwardingCurves(configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding Index(" << it.first << ") with spec " << *ycspec << " to configuration "
                                            << configuration.first);
                        iborIndices_[make_pair(configuration.first, it.first)] =
                            Handle<IborIndex>(parseIborIndex(it.first, itr->second->handle()));
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
                for (const auto& it : params.fxSpots(configuration.first)) {
                    if (it.second == spec->name()) {
                        LOG("Adding FXSpot (" << it.first << ") with spec " << *fxspec << " to configuration "
                                              << configuration.first);
                        fxSpots_[configuration.first].addQuote(it.first, itr->second->spot());
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
                    boost::shared_ptr<FXVolCurve> fxVolCurve =
                        boost::make_shared<FXVolCurve>(asof, *fxvolspec, loader, curveConfigs);
                    itr = requiredFxVolCurves.insert(make_pair(fxvolspec->name(), fxVolCurve)).first;
                }

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.fxVolatilities(configuration.first)) {
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
                    boost::shared_ptr<SwaptionVolCurve> swaptionVolCurve =
                        boost::make_shared<SwaptionVolCurve>(asof, *swvolspec, loader, curveConfigs);
                    itr = requiredSwaptionVolCurves.insert(make_pair(swvolspec->name(), swaptionVolCurve)).first;
                }

                boost::shared_ptr<SwaptionVolatilityCurveConfig> cfg =
                    curveConfigs.swaptionVolCurveConfig(swvolspec->curveConfigID());

                // add the handle to the Market Map (possible lots of times for proxies)
                for (const auto& it : params.swaptionVolatilities(configuration.first)) {
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
                for (const auto& it : params.capFloorVolatilities(configuration.first)) {
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

                for (const auto it : params.defaultCurves(configuration.first)) {
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

            default: {
                // maybe we just log an continue? need to update count then
                QL_FAIL("Unhandled spec " << *spec);
            }
            }
            LOG("Loading spec " << *spec << " done.");
            count++;
        }
        LOG("Loading " << count << " CurveSpecs done.");

        // Swap Indices
        for (const auto& it : params.swapIndices(configuration.first)) {
            const string& swapIndex = it.first;
            const string& discountIndex = it.second;

            addSwapIndex(swapIndex, discountIndex, configuration.first);
            LOG("Added SwapIndex " << swapIndex << " with DiscountingIndex " << discountIndex);
        }
    } // loop over configurations

} // CTOR
} // namespace data
} // namespace ore
