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
    \brief An concrete implementation of the Market class that loads todays market and builds the required curves
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
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/indexes/dividendmanager.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/blackvolsurfacewithatm.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

#include <ql/tuple.hpp>

#include <boost/graph/topological_sort.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/timer/timer.hpp>

using namespace std;
using namespace QuantLib;

using QuantExt::CommodityIndex;
using QuantExt::EquityIndex2;
using QuantExt::FxIndex;
using QuantExt::PriceTermStructure;
using QuantExt::PriceTermStructureAdapter;
using QuantExt::applyDividends;

namespace ore {
namespace data {

TodaysMarket::TodaysMarket(const Date& asof, const QuantLib::ext::shared_ptr<TodaysMarketParameters>& params,
                           const QuantLib::ext::shared_ptr<Loader>& loader,
                           const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs, const bool continueOnError,
                           const bool loadFixings, const bool lazyBuild,
                           const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                           const bool preserveQuoteLinkage, const IborFallbackConfig& iborFallbackConfig,
                           const bool buildCalibrationInfo, const bool handlePseudoCurrencies)
    : MarketImpl(handlePseudoCurrencies), params_(params), loader_(loader), curveConfigs_(curveConfigs),
      continueOnError_(continueOnError), loadFixings_(loadFixings), lazyBuild_(lazyBuild),
      preserveQuoteLinkage_(preserveQuoteLinkage), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig), buildCalibrationInfo_(buildCalibrationInfo) {
    QL_REQUIRE(params_, "TodaysMarket: TodaysMarketParameters are null");
    QL_REQUIRE(loader_, "TodaysMarket: Loader is null");
    QL_REQUIRE(curveConfigs_, "TodaysMarket: CurveConfigurations are null");
    initialise(asof);
}

namespace {
struct Count {
    void inc() { ++count; }
    std::size_t count = 0;
};
} // namespace

void TodaysMarket::initialise(const Date& asof) {

    std::map<std::string, boost::timer::nanosecond_type> timings;
    std::map<std::string, Count> counts;
    boost::timer::cpu_timer timer;

    asof_ = asof;

    calibrationInfo_ = QuantLib::ext::make_shared<TodaysMarketCalibrationInfo>();
    calibrationInfo_->asof = asof;

    // Fixings

    if (loadFixings_) {
        // Apply them now in case a curve builder needs them
        LOG("Todays Market Loading Fixings");
        timer.start();
        applyFixings(loader_->loadFixings());
        timings["1 load fixings"] = timer.elapsed().wall;
        LOG("Todays Market Loading Fixing done.");
    }

    // Dividends - apply them now in case a curve builder needs them

    LOG("Todays Market Loading Dividends");
    timer.start();
    applyDividends(loader_->loadDividends());
    timings["2 load dividends"] = timer.elapsed().wall;
    LOG("Todays Market Loading Dividends done.");

    // Add all FX quotes from the loader to Triangulation
    timer.start();
    if (loader_->hasQuotes(asof_)) {
	std::map<std::string, Handle<Quote>> fxQuotes;
        for (auto& md : loader_->get(Wildcard("FX/RATE/*"), asof_)) {
            QuantLib::ext::shared_ptr<FXSpotQuote> q = QuantLib::ext::dynamic_pointer_cast<FXSpotQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to FXSpotQuote");
            fxQuotes[q->unitCcy() + q->ccy()] = q->quote();
        }
	fx_  = QuantLib::ext::make_shared<FXTriangulation>(fxQuotes);
    } else {
        WLOG("TodaysMarket::Initialise: no quotes available for date " << asof_);
        return;
    }
    timings["3 add all fx quotes"] = timer.elapsed().wall;

    // build the dependency graph for all configurations and  build all FX Spots
    timer.start();
    DependencyGraph dg(asof_, params_, curveConfigs_, iborFallbackConfig_);
    map<string, string> buildErrors;

    for (const auto& configuration : params_->configurations()) {
        // Build the graph of objects to build for the current configuration
        dg.buildDependencyGraph(configuration.first, buildErrors);
    }
    dependencies_ = dg.dependencies();
    timings["4 build dep graphs"] = timer.elapsed().wall;

    // if market is not build lazily, sort the dependency graph and build the objects

    if (!lazyBuild_) {

        // We need to build all discount curves first, since some curve builds ask for discount
        // curves from specific configurations
        timer.start();
        for (const auto& configuration : params_->configurations()) {
            map<string, string> discountCurves;
            if (params_->hasMarketObject(MarketObject::DiscountCurve)) {
                discountCurves = params_->mapping(MarketObject::DiscountCurve, configuration.first);
            }
            for (const auto& dc : discountCurves)
                require(MarketObject::DiscountCurve, dc.first, configuration.first, true);
        }
        timings["6 build " + ore::data::to_string(MarketObject::DiscountCurve)] += timer.elapsed().wall;
        counts["6 build " + ore::data::to_string(MarketObject::DiscountCurve)].inc();

        for (const auto& configuration : params_->configurations()) {

            LOG("Build objects in TodaysMarket configuration " << configuration.first);

            // Sort the graph topologically

            timer.start();
            Graph& g = dependencies_[configuration.first];
            IndexMap index = QuantLib::ext::get(boost::vertex_index, g);
            std::vector<Vertex> order;
            try {
                boost::topological_sort(g, std::back_inserter(order));
            } catch (const std::exception& e) {
                // topological_sort() might have produced partial results, that we have to discard
                order.clear();
                // set error (most likely a circle), and output cycles if any
                buildErrors["CurveDependencyGraph"] = "Topological sort of dependency graph failed for configuration " +
                                                      configuration.first + " (" + ore::data::to_string(e.what()) +
                                                      "). Got cycle(s): " + getCycles(g);
            }
            timings["5 topological sort dep graphs"] += timer.elapsed().wall;

            TLOG("Can build objects in the following order:");
            for (auto const& m : order) {
                TLOG("vertex #" << index[m] << ": " << g[m]);
            }

            // Build the objects in the graph in topological order

            Size countSuccess = 0, countError = 0;
            for (auto const& m : order) {
                timer.start();
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
                timings["6 build " + ore::data::to_string(g[m].obj)] += timer.elapsed().wall;
                counts["6 build " + ore::data::to_string(g[m].obj)].inc();
            }

            LOG("Loaded CurvesSpecs: success: " << countSuccess << ", error: " << countError);
        }

    } else {
        LOG("Build objects in TodaysMarket lazily, i.e. when requested.");
    }

    // output stats on initialisation phase

    LOG("TodaysMarket build stats:");
    boost::timer::nanosecond_type sum = 0;
    for (auto const& t : timings) {
        std::size_t c = counts[t.first].count == 0 ? 1 : counts[t.first].count;
        double timing = static_cast<double>(t.second) / 1.0E6;
        LOG(std::left << std::setw(34) << t.first << ": " << std::right << std::setprecision(3) << std::setw(15)
                      << timing << " ms" << std::setw(10) << c << std::setw(15) << timing / c << " ms");
        sum += t.second;
    }
    LOG("Total build time              : " << std::setw(15) << static_cast<double>(sum) / 1.0E6 << " ms");

    // output errors from initialisation phase

    if (!buildErrors.empty()) {
        for (auto const& error : buildErrors) {
            StructuredCurveErrorMessage(error.first, "Failed to Build Curve", error.second).log();
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

void TodaysMarket::buildNode(const std::string& configuration, Node& node) const {

    // if the node is already built, there is nothing to do

    if (node.built)
        return;

    if (node.curveSpec == nullptr) {

        // not spec-based node, this can only be a SwapIndexCurve

        QL_REQUIRE(node.obj == MarketObject::SwapIndexCurve,
                   "market object '" << node.obj << "' (" << node.name << ") without curve spec, this is unexpected.");
        const string& swapIndexName = node.name;
        const string& discountIndex = node.mapping;
        addSwapIndex(swapIndexName, discountIndex, configuration);
        DLOG("Added SwapIndex " << swapIndexName << " with DiscountingIndex " << discountIndex);
        requiredSwapIndices_[configuration][swapIndexName] =
            swapIndices_.at(std::make_pair(configuration, swapIndexName)).currentLink();

    } else {

        // spec-based node

        auto spec = node.curveSpec;
        switch (spec->baseType()) {

        // Yield
        case CurveSpec::CurveType::Yield: {
            QuantLib::ext::shared_ptr<YieldCurveSpec> ycspec = QuantLib::ext::dynamic_pointer_cast<YieldCurveSpec>(spec);
            QL_REQUIRE(ycspec, "Failed to convert spec " << *spec << " to yield curve spec");

            QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

            auto itr = requiredYieldCurves_.find(ycspec->name());
            if (itr == requiredYieldCurves_.end()) {
                DLOG("Building YieldCurve for asof " << asof_);
                QuantLib::ext::shared_ptr<YieldCurve> yieldCurve = QuantLib::ext::make_shared<YieldCurve>(
                    asof_, *ycspec, *curveConfigs_, *loader_, requiredYieldCurves_, requiredDefaultCurves_, *fx_,
                    referenceData_, iborFallbackConfig_, preserveQuoteLinkage_, buildCalibrationInfo_, this);
                calibrationInfo_->yieldCurveCalibrationInfo[ycspec->name()] = yieldCurve->calibrationInfo();
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
                // ibor fallback handling
                auto tmpIndex = parseIborIndex(node.name, itr->second->handle());
                if (iborFallbackConfig_.isIndexReplaced(node.name, asof_)) {
                    auto fallbackData = iborFallbackConfig_.fallbackData(node.name);
                    QuantLib::ext::shared_ptr<IborIndex> rfrIndex;
                    auto f = iborIndices_.find(make_pair(configuration, fallbackData.rfrIndex));
                    if (f == iborIndices_.end()) {
                        f = iborIndices_.find(make_pair(Market::defaultConfiguration, fallbackData.rfrIndex));
                        QL_REQUIRE(f != iborIndices_.end(),
                                   "Failed to build ibor fallback index '"
                                       << node.name << "', did not find rfr index '" << fallbackData.rfrIndex
                                       << "' in configuration '" << configuration
                                       << "' or default - is the rfr index configuration in todays market parameters?");
                    }
                    auto oi = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(*f->second);
                    QL_REQUIRE(oi,
                               "Found rfr index '"
                                   << fallbackData.rfrIndex << "' as fallback for ibor index '" << node.name
                                   << "', but this is not an overnight index. Are the fallback rules correct here?");
                    if (auto original = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(tmpIndex))
                        tmpIndex = QuantLib::ext::make_shared<QuantExt::FallbackOvernightIndex>(
                            original, oi, fallbackData.spread, fallbackData.switchDate,
                            iborFallbackConfig_.useRfrCurveInTodaysMarket());
                    else
                        tmpIndex = QuantLib::ext::make_shared<QuantExt::FallbackIborIndex>(
                            tmpIndex, oi, fallbackData.spread, fallbackData.switchDate,
                            iborFallbackConfig_.useRfrCurveInTodaysMarket());
                    TLOG("built ibor fall back index for '" << node.name << "' in configuration " << configuration
                                                            << " using rfr index '" << fallbackData.rfrIndex
                                                            << "', spread " << fallbackData.spread
                                                            << ", will use rfr curve in t0 market: " << std::boolalpha
                                                            << iborFallbackConfig_.useRfrCurveInTodaysMarket());
                }
                iborIndices_[make_pair(configuration, node.name)] = Handle<IborIndex>(tmpIndex);
            } else {
                QL_FAIL("unexpected market object type '"
                        << node.obj << "' for yield curve, should be DiscountCurve, YieldCurve, IndexCurve");
            }
            break;
        }

        // FX Spot
        case CurveSpec::CurveType::FX: {
            DLOG("Building FXSpot (" << node.name << ") does not require any action.");
            break;
        }

        // FX Vol
        case CurveSpec::CurveType::FXVolatility: {
            QuantLib::ext::shared_ptr<FXVolatilityCurveSpec> fxvolspec =
                QuantLib::ext::dynamic_pointer_cast<FXVolatilityCurveSpec>(spec);
            QL_REQUIRE(fxvolspec, "Failed to convert spec " << *spec);

            // have we built the curve already ?
            auto itr = requiredFxVolCurves_.find(fxvolspec->name());
            if (itr == requiredFxVolCurves_.end()) {
                DLOG("Building FXVolatility for asof " << asof_);
                QuantLib::ext::shared_ptr<FXVolCurve> fxVolCurve = QuantLib::ext::make_shared<FXVolCurve>(
                    asof_, *fxvolspec, *loader_, *curveConfigs_, *fx_, requiredYieldCurves_, requiredFxVolCurves_,
                    requiredCorrelationCurves_, buildCalibrationInfo_);
                calibrationInfo_->fxVolCalibrationInfo[fxvolspec->name()] = fxVolCurve->calibrationInfo();
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
            QuantLib::ext::shared_ptr<SwaptionVolatilityCurveSpec> swvolspec =
                QuantLib::ext::dynamic_pointer_cast<SwaptionVolatilityCurveSpec>(spec);
            QL_REQUIRE(swvolspec, "Failed to convert spec " << *spec);

            auto itr = requiredGenericYieldVolCurves_.find(swvolspec->name());
            if (itr == requiredGenericYieldVolCurves_.end()) {
                DLOG("Building Swaption Volatility (" << node.name << ") for asof " << asof_);
                QuantLib::ext::shared_ptr<SwaptionVolCurve> swaptionVolCurve = QuantLib::ext::make_shared<SwaptionVolCurve>(
                    asof_, *swvolspec, *loader_, *curveConfigs_, requiredSwapIndices_[configuration],
                    requiredGenericYieldVolCurves_, buildCalibrationInfo_);
                calibrationInfo_->irVolCalibrationInfo[swvolspec->name()] = swaptionVolCurve->calibrationInfo();
                itr = requiredGenericYieldVolCurves_.insert(make_pair(swvolspec->name(), swaptionVolCurve)).first;
            }

            QuantLib::ext::shared_ptr<SwaptionVolatilityCurveConfig> cfg =
                curveConfigs_->swaptionVolCurveConfig(swvolspec->curveConfigID());

            DLOG("Adding SwaptionVol (" << node.name << ") with spec " << *swvolspec << " to configuration "
                                        << configuration);
            swaptionCurves_[make_pair(configuration, node.name)] =
                Handle<SwaptionVolatilityStructure>(itr->second->volTermStructure());
            swaptionIndexBases_[make_pair(configuration, node.name)] =
                cfg->proxySourceCurveId().empty()
                    ? make_pair(cfg->shortSwapIndexBase(), cfg->swapIndexBase())
                    : make_pair(cfg->proxyTargetShortSwapIndexBase(), cfg->proxyTargetSwapIndexBase());
            break;
        }

        // Yield Vol
        case CurveSpec::CurveType::YieldVolatility: {
            QuantLib::ext::shared_ptr<YieldVolatilityCurveSpec> ydvolspec =
                QuantLib::ext::dynamic_pointer_cast<YieldVolatilityCurveSpec>(spec);
            QL_REQUIRE(ydvolspec, "Failed to convert spec " << *spec);
            auto itr = requiredGenericYieldVolCurves_.find(ydvolspec->name());
            if (itr == requiredGenericYieldVolCurves_.end()) {
                DLOG("Building Yield Volatility for asof " << asof_);
                QuantLib::ext::shared_ptr<YieldVolCurve> yieldVolCurve = QuantLib::ext::make_shared<YieldVolCurve>(
                    asof_, *ydvolspec, *loader_, *curveConfigs_, buildCalibrationInfo_);
                calibrationInfo_->irVolCalibrationInfo[ydvolspec->name()] = yieldVolCurve->calibrationInfo();
                itr = requiredGenericYieldVolCurves_.insert(make_pair(ydvolspec->name(), yieldVolCurve)).first;
            }
            DLOG("Adding YieldVol (" << node.name << ") with spec " << *ydvolspec << " to configuration "
                                     << configuration);
            yieldVolCurves_[make_pair(configuration, node.name)] =
                Handle<SwaptionVolatilityStructure>(itr->second->volTermStructure());
            break;
        }

        // Cap Floor Vol
        case CurveSpec::CurveType::CapFloorVolatility: {
            QuantLib::ext::shared_ptr<CapFloorVolatilityCurveSpec> cfVolSpec =
                QuantLib::ext::dynamic_pointer_cast<CapFloorVolatilityCurveSpec>(spec);
            QL_REQUIRE(cfVolSpec, "Failed to convert spec " << *spec);
            QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig> cfg =
                curveConfigs_->capFloorVolCurveConfig(cfVolSpec->curveConfigID());

            auto itr = requiredCapFloorVolCurves_.find(cfVolSpec->name());
            if (itr == requiredCapFloorVolCurves_.end()) {
                DLOG("Building cap/floor volatility for asof " << asof_);

                // Firstly, need to retrieve ibor index and discount curve
                // Ibor index
                std::string iborIndexName = cfg->index();
                QuantLib::Period rateComputationPeriod = cfg->rateComputationPeriod();
                Handle<IborIndex> iborIndex = MarketImpl::iborIndex(iborIndexName, configuration);
                Handle<YieldTermStructure> discountCurve;
                // Discount curve
                if (!cfg->discountCurve().empty()) {
                    auto it = requiredYieldCurves_.find(cfg->discountCurve());
                    QL_REQUIRE(it != requiredYieldCurves_.end(), "Discount curve with spec, "
                                                                     << cfg->discountCurve()
                                                                     << ", not found in loaded yield curves");
                    discountCurve = it->second->handle();
                }

                // for proxy curves we need the source and target indices
                QuantLib::ext::shared_ptr<IborIndex> sourceIndex, targetIndex;
                if (!cfg->proxySourceCurveId().empty()) {
                    if (!cfg->proxySourceIndex().empty())
                        sourceIndex = *MarketImpl::iborIndex(cfg->proxySourceIndex(), configuration);
                    if (!cfg->proxyTargetIndex().empty()) {
                        targetIndex = *MarketImpl::iborIndex(cfg->proxyTargetIndex(), configuration);
                        iborIndexName = cfg->proxyTargetIndex();
                        rateComputationPeriod = cfg->proxyTargetRateComputationPeriod();
                    }
                }

                // Now create cap/floor vol curve
                QuantLib::ext::shared_ptr<CapFloorVolCurve> capFloorVolCurve = QuantLib::ext::make_shared<CapFloorVolCurve>(
                    asof_, *cfVolSpec, *loader_, *curveConfigs_, iborIndex.currentLink(), discountCurve, sourceIndex,
                    targetIndex, requiredCapFloorVolCurves_, buildCalibrationInfo_);
                calibrationInfo_->irVolCalibrationInfo[cfVolSpec->name()] = capFloorVolCurve->calibrationInfo();
                itr = requiredCapFloorVolCurves_
                          .insert(make_pair(
                              cfVolSpec->name(),
                              std::make_pair(capFloorVolCurve, std::make_pair(iborIndexName, rateComputationPeriod))))
                          .first;
            }

            DLOG("Adding CapFloorVol (" << node.name << ") with spec " << *cfVolSpec << " to configuration "
                                        << configuration);
            capFloorCurves_[make_pair(configuration, node.name)] =
                Handle<OptionletVolatilityStructure>(itr->second.first->capletVolStructure());
            capFloorIndexBase_[make_pair(configuration, node.name)] = itr->second.second;
            break;
        }

        // Default Curve
        case CurveSpec::CurveType::Default: {
            QuantLib::ext::shared_ptr<DefaultCurveSpec> defaultspec = QuantLib::ext::dynamic_pointer_cast<DefaultCurveSpec>(spec);
            QL_REQUIRE(defaultspec, "Failed to convert spec " << *spec);
            auto itr = requiredDefaultCurves_.find(defaultspec->name());
            if (itr == requiredDefaultCurves_.end()) {
                // build the curve
                DLOG("Building DefaultCurve for asof " << asof_);
                QuantLib::ext::shared_ptr<DefaultCurve> defaultCurve = QuantLib::ext::make_shared<DefaultCurve>(
                    asof_, *defaultspec, *loader_, *curveConfigs_, requiredYieldCurves_, requiredDefaultCurves_);
                itr = requiredDefaultCurves_.insert(make_pair(defaultspec->name(), defaultCurve)).first;
            }
            DLOG("Adding DefaultCurve (" << node.name << ") with spec " << *defaultspec << " to configuration "
                                         << configuration);
            defaultCurves_[make_pair(configuration, node.name)] =
                Handle<QuantExt::CreditCurve>(itr->second->creditCurve());
            recoveryRates_[make_pair(configuration, node.name)] =
                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(itr->second->recoveryRate()));
            break;
        }

        // CDS Vol
        case CurveSpec::CurveType::CDSVolatility: {
            QuantLib::ext::shared_ptr<CDSVolatilityCurveSpec> cdsvolspec =
                QuantLib::ext::dynamic_pointer_cast<CDSVolatilityCurveSpec>(spec);
            QL_REQUIRE(cdsvolspec, "Failed to convert spec " << *spec);
            auto itr = requiredCDSVolCurves_.find(cdsvolspec->name());
            if (itr == requiredCDSVolCurves_.end()) {
                DLOG("Building CDSVol for asof " << asof_);
                QuantLib::ext::shared_ptr<CDSVolCurve> cdsVolCurve = QuantLib::ext::make_shared<CDSVolCurve>(
                    asof_, *cdsvolspec, *loader_, *curveConfigs_, requiredCDSVolCurves_, requiredDefaultCurves_);
                itr = requiredCDSVolCurves_.insert(make_pair(cdsvolspec->name(), cdsVolCurve)).first;
            }
            DLOG("Adding CDSVol (" << node.name << ") with spec " << *cdsvolspec << " to configuration "
                                   << configuration);
            cdsVols_[make_pair(configuration, node.name)] =
                Handle<QuantExt::CreditVolCurve>(itr->second->volTermStructure());
            break;
        }

        // Base Correlation
        case CurveSpec::CurveType::BaseCorrelation: {
            QuantLib::ext::shared_ptr<BaseCorrelationCurveSpec> baseCorrelationSpec =
                QuantLib::ext::dynamic_pointer_cast<BaseCorrelationCurveSpec>(spec);
            QL_REQUIRE(baseCorrelationSpec, "Failed to convert spec " << *spec);
            auto itr = requiredBaseCorrelationCurves_.find(baseCorrelationSpec->name());
            if (itr == requiredBaseCorrelationCurves_.end()) {
                DLOG("Building BaseCorrelation for asof " << asof_);
                QuantLib::ext::shared_ptr<BaseCorrelationCurve> baseCorrelationCurve = QuantLib::ext::make_shared<BaseCorrelationCurve>(
                    asof_, *baseCorrelationSpec, *loader_, *curveConfigs_, referenceData_);
                itr =
                    requiredBaseCorrelationCurves_.insert(make_pair(baseCorrelationSpec->name(), baseCorrelationCurve))
                        .first;
            }

            DLOG("Adding Base Correlation (" << node.name << ") with spec " << *baseCorrelationSpec
                                             << " to configuration " << configuration);
            baseCorrelations_[make_pair(configuration, node.name)] =
                Handle<QuantExt::BaseCorrelationTermStructure>(
                    itr->second->baseCorrelationTermStructure());
            break;
        }

        // Inflation Curve
        case CurveSpec::CurveType::Inflation: {
            QuantLib::ext::shared_ptr<InflationCurveSpec> inflationspec = QuantLib::ext::dynamic_pointer_cast<InflationCurveSpec>(spec);
            QL_REQUIRE(inflationspec, "Failed to convert spec " << *spec << " to inflation curve spec");
            auto itr = requiredInflationCurves_.find(inflationspec->name());
            if (itr == requiredInflationCurves_.end()) {
                DLOG("Building InflationCurve " << inflationspec->name() << " for asof " << asof_);
                QuantLib::ext::shared_ptr<InflationCurve> inflationCurve = QuantLib::ext::make_shared<InflationCurve>(
                    asof_, *inflationspec, *loader_, *curveConfigs_, requiredYieldCurves_, buildCalibrationInfo_);
                itr = requiredInflationCurves_.insert(make_pair(inflationspec->name(), inflationCurve)).first;
                calibrationInfo_->inflationCurveCalibrationInfo[inflationspec->name()] =
                    inflationCurve->calibrationInfo();
            }

            if (node.obj == MarketObject::ZeroInflationCurve) {
                DLOG("Adding ZeroInflationIndex (" << node.name << ") with spec " << *inflationspec
                                                   << " to configuration " << configuration);
                QuantLib::ext::shared_ptr<ZeroInflationTermStructure> ts =
                    QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(itr->second->inflationTermStructure());
                QL_REQUIRE(ts,
                           "expected zero inflation term structure for index " << node.name << ", but could not cast");
                // index is not interpolated
                auto tmp = parseZeroInflationIndex(node.name, Handle<ZeroInflationTermStructure>(ts));
                zeroInflationIndices_[make_pair(configuration, node.name)] = Handle<ZeroInflationIndex>(tmp);
            }

            if (node.obj == MarketObject::YoYInflationCurve) {
                DLOG("Adding YoYInflationIndex (" << node.name << ") with spec " << *inflationspec
                                                  << " to configuration " << configuration);
                QuantLib::ext::shared_ptr<YoYInflationTermStructure> ts =
                    QuantLib::ext::dynamic_pointer_cast<YoYInflationTermStructure>(itr->second->inflationTermStructure());
                QL_REQUIRE(ts,
                           "expected yoy inflation term structure for index " << node.name << ", but could not cast");
                yoyInflationIndices_[make_pair(configuration, node.name)] =
                    Handle<YoYInflationIndex>(QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(
                        parseZeroInflationIndex(node.name, Handle<ZeroInflationTermStructure>()), false,
                        Handle<YoYInflationTermStructure>(ts)));
            }
            break;
        }

        // Inflation Cap Floor Vol
        case CurveSpec::CurveType::InflationCapFloorVolatility: {
            QuantLib::ext::shared_ptr<InflationCapFloorVolatilityCurveSpec> infcapfloorspec =
                QuantLib::ext::dynamic_pointer_cast<InflationCapFloorVolatilityCurveSpec>(spec);
            QL_REQUIRE(infcapfloorspec, "Failed to convert spec " << *spec << " to inf cap floor spec");
            auto itr = requiredInflationCapFloorVolCurves_.find(infcapfloorspec->name());
            if (itr == requiredInflationCapFloorVolCurves_.end()) {
                DLOG("Building InflationCapFloorVolatilitySurface for asof " << asof_);
                QuantLib::ext::shared_ptr<InflationCapFloorVolCurve> inflationCapFloorVolCurve =
                    QuantLib::ext::make_shared<InflationCapFloorVolCurve>(asof_, *infcapfloorspec, *loader_, *curveConfigs_,
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
            QuantLib::ext::shared_ptr<EquityCurveSpec> equityspec = QuantLib::ext::dynamic_pointer_cast<EquityCurveSpec>(spec);
            QL_REQUIRE(equityspec, "Failed to convert spec " << *spec);
            auto itr = requiredEquityCurves_.find(equityspec->name());
            if (itr == requiredEquityCurves_.end()) {
                DLOG("Building EquityCurve for asof " << asof_);
                QuantLib::ext::shared_ptr<EquityCurve> equityCurve = QuantLib::ext::make_shared<EquityCurve>(
                    asof_, *equityspec, *loader_, *curveConfigs_, requiredYieldCurves_, buildCalibrationInfo_);
                itr = requiredEquityCurves_.insert(make_pair(equityspec->name(), equityCurve)).first;
                calibrationInfo_->dividendCurveCalibrationInfo[equityspec->name()] = equityCurve->calibrationInfo();
            }

            DLOG("Adding EquityCurve (" << node.name << ") with spec " << *equityspec << " to configuration "
                                        << configuration);
            yieldCurves_[make_tuple(configuration, YieldCurveType::EquityDividend, node.name)] =
                itr->second->equityIndex()->equityDividendCurve();
            equitySpots_[make_pair(configuration, node.name)] = itr->second->equityIndex()->equitySpot();
            equityCurves_[make_pair(configuration, node.name)] = Handle<EquityIndex2>(itr->second->equityIndex());
            IndexNameTranslator::instance().add(itr->second->equityIndex()->name(),
                                                "EQ-" + itr->second->equityIndex()->name());
            break;
        }

        // Equity Vol
        case CurveSpec::CurveType::EquityVolatility: {

            QuantLib::ext::shared_ptr<EquityVolatilityCurveSpec> eqvolspec =
                QuantLib::ext::dynamic_pointer_cast<EquityVolatilityCurveSpec>(spec);

            QL_REQUIRE(eqvolspec, "Failed to convert spec " << *spec);
            auto itr = requiredEquityVolCurves_.find(eqvolspec->name());
            if (itr == requiredEquityVolCurves_.end()) {
                LOG("Building EquityVol for asof " << asof_);
                // First we need the Equity Index, we don't have a dependency for this in the graph, rather
                // pull it directly from MarketImpl, which will trigger the build if necessary -
                // this works, but contradicts the idea of managing the dependencies fully in a graph.
                // The EQVol builder should rather get the index from the requiredEquityCurves_.
                // In addition we should maybe specify the eqIndex name in the vol curve config explicitly
                // instead of assuming that it has the same curve id as the vol curve to be build?
                Handle<EquityIndex2> eqIndex = MarketImpl::equityCurve(eqvolspec->curveConfigID(), configuration);
                QuantLib::ext::shared_ptr<EquityVolCurve> eqVolCurve = QuantLib::ext::make_shared<EquityVolCurve>(
                    asof_, *eqvolspec, *loader_, *curveConfigs_, eqIndex, requiredEquityCurves_,
                    requiredEquityVolCurves_, requiredFxVolCurves_, requiredCorrelationCurves_, this,
                    buildCalibrationInfo_);
                itr = requiredEquityVolCurves_.insert(make_pair(eqvolspec->name(), eqVolCurve)).first;
                calibrationInfo_->eqVolCalibrationInfo[eqvolspec->name()] = eqVolCurve->calibrationInfo();
            }
            string eqName = node.name;
            DLOG("Adding EquityVol (" << eqName << ") with spec " << *eqvolspec << " to configuration "
                                      << configuration);

            QuantLib::ext::shared_ptr<BlackVolTermStructure> bvts(itr->second->volTermStructure());
            // Wrap it in QuantExt::BlackVolatilityWithATM as TodaysMarket might be used
            // for model calibration. This is not the ideal place to put this logic but
            // it can't be in EquityVolCurve as there are implicit, configuration dependent,
            // choices made already (e.g. what discount curve to use).
            // We do this even if it is an ATM curve, it does no harm.
            Handle<Quote> spot = equitySpot(eqName, configuration);
            Handle<YieldTermStructure> yts = discountCurve(eqvolspec->ccy(), configuration);
            Handle<YieldTermStructure> divYts = equityDividendCurve(eqName, configuration);
            bvts = QuantLib::ext::make_shared<QuantExt::BlackVolatilityWithATM>(bvts, spot, yts, divYts);

            equityVols_[make_pair(configuration, node.name)] = Handle<BlackVolTermStructure>(bvts);
            break;
        }

        // Security spread, rr, cpr
        case CurveSpec::CurveType::Security: {
            QuantLib::ext::shared_ptr<SecuritySpec> securityspec = QuantLib::ext::dynamic_pointer_cast<SecuritySpec>(spec);
            QL_REQUIRE(securityspec, "Failed to convert spec " << *spec << " to security spec");
            auto itr = requiredSecurities_.find(securityspec->securityID());
            if (itr == requiredSecurities_.end()) {
                DLOG("Building Securities for asof " << asof_);
                QuantLib::ext::shared_ptr<Security> security =
                    QuantLib::ext::make_shared<Security>(asof_, *securityspec, *loader_, *curveConfigs_);
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
            QuantLib::ext::shared_ptr<CommodityCurveSpec> commodityCurveSpec =
                QuantLib::ext::dynamic_pointer_cast<CommodityCurveSpec>(spec);
            QL_REQUIRE(commodityCurveSpec, "Failed to convert spec, " << *spec << ", to CommodityCurveSpec");
            auto itr = requiredCommodityCurves_.find(commodityCurveSpec->name());
            if (itr == requiredCommodityCurves_.end()) {
                DLOG("Building CommodityCurve " << commodityCurveSpec->name() << " for asof " << asof_);
                QuantLib::ext::shared_ptr<CommodityCurve> commodityCurve =
                    QuantLib::ext::make_shared<CommodityCurve>(asof_, *commodityCurveSpec, *loader_, *curveConfigs_, *fx_,
                                                       requiredYieldCurves_, requiredCommodityCurves_, buildCalibrationInfo_);
                itr = requiredCommodityCurves_.insert(make_pair(commodityCurveSpec->name(), commodityCurve)).first;
            }

            DLOG("Adding CommodityCurve, " << node.name << ", with spec " << *commodityCurveSpec << " to configuration "
                                           << configuration);
            Handle<CommodityIndex> commIdx(itr->second->commodityIndex());
            commodityIndices_[make_pair(configuration, node.name)] = commIdx;
            calibrationInfo_->commodityCurveCalibrationInfo[commodityCurveSpec->name()] = itr->second->calibrationInfo();
            break;
        }

        // Commodity Vol
        case CurveSpec::CurveType::CommodityVolatility: {

            QuantLib::ext::shared_ptr<CommodityVolatilityCurveSpec> commodityVolSpec =
                QuantLib::ext::dynamic_pointer_cast<CommodityVolatilityCurveSpec>(spec);
            QL_REQUIRE(commodityVolSpec, "Failed to convert spec " << *spec << " to commodity volatility spec");
            auto itr = requiredCommodityVolCurves_.find(commodityVolSpec->name());
            if (itr == requiredCommodityVolCurves_.end()) {
                DLOG("Building commodity volatility for asof " << asof_);
                QuantLib::ext::shared_ptr<CommodityVolCurve> commodityVolCurve = QuantLib::ext::make_shared<CommodityVolCurve>(
                    asof_, *commodityVolSpec, *loader_, *curveConfigs_, requiredYieldCurves_, requiredCommodityCurves_,
                    requiredCommodityVolCurves_, requiredFxVolCurves_, requiredCorrelationCurves_, this, buildCalibrationInfo_);
                itr = requiredCommodityVolCurves_.insert(make_pair(commodityVolSpec->name(), commodityVolCurve)).first;
                calibrationInfo_->commVolCalibrationInfo[commodityVolSpec->name()] = commodityVolCurve->calibrationInfo();
            }

            string commodityName = node.name;
            DLOG("Adding commodity volatility (" << commodityName << ") with spec " << *commodityVolSpec
                                                 << " to configuration " << configuration);

            // Logic copied from Equity vol section of TodaysMarket for now
            QuantLib::ext::shared_ptr<BlackVolTermStructure> bvts(itr->second->volatility());
            Handle<YieldTermStructure> discount = discountCurve(commodityVolSpec->currency(), configuration);
            Handle<PriceTermStructure> priceCurve = commodityPriceCurve(commodityName, configuration);
            Handle<YieldTermStructure> yield =
                Handle<YieldTermStructure>(QuantLib::ext::make_shared<PriceTermStructureAdapter>(*priceCurve, *discount));
            Handle<Quote> spot(QuantLib::ext::make_shared<SimpleQuote>(priceCurve->price(0, true)));

            bvts = QuantLib::ext::make_shared<QuantExt::BlackVolatilityWithATM>(bvts, spot, discount, yield);
            commodityVols_[make_pair(configuration, node.name)] = Handle<BlackVolTermStructure>(bvts);
            break;
        }

        // Correlation
        case CurveSpec::CurveType::Correlation: {
            QuantLib::ext::shared_ptr<CorrelationCurveSpec> corrspec = QuantLib::ext::dynamic_pointer_cast<CorrelationCurveSpec>(spec);
            auto itr = requiredCorrelationCurves_.find(corrspec->name());
            if (itr == requiredCorrelationCurves_.end()) {
                DLOG("Building CorrelationCurve for asof " << asof_);
                QuantLib::ext::shared_ptr<CorrelationCurve> corrCurve = QuantLib::ext::make_shared<CorrelationCurve>(
                    asof_, *corrspec, *loader_, *curveConfigs_, requiredSwapIndices_[configuration],
                    requiredYieldCurves_, requiredGenericYieldVolCurves_);
                itr = requiredCorrelationCurves_.insert(make_pair(corrspec->name(), corrCurve)).first;
            }

            DLOG("Adding CorrelationCurve (" << node.name << ") with spec " << *corrspec << " to configuration "
                                             << configuration);
            auto tokens = getCorrelationTokens(node.name);
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

    node.built = true;
} // TodaysMarket::buildNode()

void TodaysMarket::require(const MarketObject o, const string& name, const string& configuration,
                           const bool forceBuild) const {

    // if the market is not lazily built, do nothing

    if (!lazyBuild_ && !forceBuild)
        return;

    // search the node (o, name) in the dependency graph

    DLOG("market object " << o << "(" << name << ") required for configuration '" << configuration << "'");

    auto tmp = dependencies_.find(configuration);
    if (tmp == dependencies_.end()) {
        if (configuration != Market::defaultConfiguration) {
            StructuredCurveWarningMessage(
                ore::data::to_string(o) + "(" + name + ")", "Unknown market configuration.",
                "Configuration '" + configuration +
                    "' not known - check why this is used. Will retry with default configuration.")
                .log();
            require(o, name, Market::defaultConfiguration);
            return;
        } else {
            StructuredCurveErrorMessage(ore::data::to_string(o) + "(" + name + ")", "Failed to Build Curve",
                                        "Configuration 'default' not known, this is unexpected. Do nothing.")
                .log();
            return;
        }
    }

    Vertex node = nullptr;

    Graph& g = tmp->second;
    IndexMap index = QuantLib::ext::get(boost::vertex_index, g);

    VertexIterator v, vend;
    bool found = false;
    for (std::tie(v, vend) = boost::vertices(g); v != vend; ++v) {
        if (g[*v].obj == o) {
            if (o == MarketObject::Correlation) {
                // split the required name and the node name and compare the tokens
                found = getCorrelationTokens(name) == getCorrelationTokens(g[*v].name);
            } else {
                found = (g[*v].name == name);
            }
            if (found) {
                node = *v;
                break;
            }
        }
    }

    // if we did not find a node, we retry with the default configuration, as required by the interface

    if (!found && configuration != Market::defaultConfiguration) {
        DLOG("not found, retry with default configuration");
        require(o, name, Market::defaultConfiguration);
        return;
    }

    // if we still have no node, we do nothing, the error handling is done in MarketImpl

    if (!found) {
        DLOG("not found, do nothing");
        return;
    }

    // if the node is already built, we are done

    if (g[node].built) {
        DLOG("node already built, do nothing.");
        return;
    }

    // run a DFS from the found node to identify the required nodes to be built and get a possible order to do this

    map<string, string> buildErrors;
    std::vector<Vertex> order;
    bool foundCycle = false;

    DfsVisitor<Vertex> dfs(order, foundCycle);
    auto colorMap = boost::make_vector_property_map<boost::default_color_type>(index);
    boost::depth_first_visit(g, node, dfs, colorMap);

    if (foundCycle) {
        order.clear();
        buildErrors[g[node].curveSpec ? g[node].curveSpec->name() : g[node].name] = "found cycle";
    }

    // build the nodes

    TLOG("Can build objects in the following order:");
    for (auto const& m : order) {
        TLOG("vertex #" << index[m] << ": " << g[m] << (g[m].built ? " (already built)" : " (not yet built)"));
    }

    Size countSuccess = 0, countError = 0;
    for (auto const& m : order) {
        if (g[m].built)
            continue;
        try {
            buildNode(configuration, g[m]);
            ++countSuccess;
            DLOG("built node " << g[m] << " in configuration " << configuration);
        } catch (const std::exception& e) {
            if (g[m].curveSpec)
                buildErrors[g[m].curveSpec->name()] = e.what();
            else
                buildErrors[g[m].name] = e.what();
            ++countError;
            ALOG("error while building node " << g[m] << " in configuration " << configuration << ": " << e.what());
        }
    }

    if (countSuccess + countError > 0) {
        DLOG("Loaded CurvesSpecs: success: " << countSuccess << ", error: " << countError);
    }

    // output errors

    if (!buildErrors.empty()) {
        for (auto const& error : buildErrors) {
            StructuredCurveErrorMessage(error.first, "Failed to Build Curve", error.second).log();
        }
        if (!continueOnError_) {
            string errStr;
            for (auto const& error : buildErrors) {
                errStr += "(" + error.first + ": " + error.second + "); ";
            }
            QL_FAIL("Cannot build all required curves! Building failed for: " << errStr);
        }
    }
} // TodaysMarket::require()

std::ostream& operator<<(std::ostream& o, const DependencyGraph::Node& n) {
    return o << n.obj << "(" << n.name << "," << n.mapping << ")";
}

} // namespace data
} // namespace ore
