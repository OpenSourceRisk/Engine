/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/inflationcurveconfig.hpp>
#include <ored/configuration/inflationcapfloorvolcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/dependencies.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/marketdata.hpp>

using namespace std;

namespace ore {
namespace data {

CurveSpec::CurveType marketObjectToCurveType(MarketObject mo) {
    const static map<MarketObject, CurveSpec::CurveType> moct = {
        {MarketObject::DiscountCurve, CurveSpec::CurveType::Yield},
        {MarketObject::YieldCurve, CurveSpec::CurveType::Yield},
        {MarketObject::IndexCurve, CurveSpec::CurveType::Yield},
        {MarketObject::SwapIndexCurve, CurveSpec::CurveType::SwapIndex},
        {MarketObject::FXSpot, CurveSpec::CurveType::FX},
        {MarketObject::FXVol, CurveSpec::CurveType::FXVolatility},
        {MarketObject::SwaptionVol, CurveSpec::CurveType::SwaptionVolatility},
        {MarketObject::DefaultCurve, CurveSpec::CurveType::Default},
        {MarketObject::CDSVol, CurveSpec::CurveType::CDSVolatility},
        {MarketObject::BaseCorrelation, CurveSpec::CurveType::BaseCorrelation},
        {MarketObject::CapFloorVol, CurveSpec::CurveType::CapFloorVolatility},
        {MarketObject::ZeroInflationCurve, CurveSpec::CurveType::Inflation},
        {MarketObject::YoYInflationCurve, CurveSpec::CurveType::Inflation},
        {MarketObject::ZeroInflationCapFloorVol, CurveSpec::CurveType::InflationCapFloorVolatility},
        {MarketObject::YoYInflationCapFloorVol, CurveSpec::CurveType::InflationCapFloorVolatility},
        {MarketObject::EquityCurve, CurveSpec::CurveType::Equity},
        {MarketObject::EquityVol, CurveSpec::CurveType::EquityVolatility},
        {MarketObject::Security, CurveSpec::CurveType::Security},
        {MarketObject::CommodityCurve, CurveSpec::CurveType::Commodity},
        {MarketObject::CommodityVolatility, CurveSpec::CurveType::CommodityVolatility},
        {MarketObject::Correlation, CurveSpec::CurveType::Correlation},
        {MarketObject::YieldVol, CurveSpec::CurveType::YieldVolatility}};
     
    auto it = moct.find(mo);
    if (it == moct.end())
        QL_FAIL("Cannot convert market object " << mo << "to curve type");
    return it->second;
}

string marketObjectToCurveSpec(const MarketObject& mo, const string& name, const string& baseCcy,
                               const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs) {
    auto ct = marketObjectToCurveType(mo);
    CurveSpec* cs = nullptr;

    switch (ct) {
    case CurveSpec::CurveType::Yield: {
        auto cc = curveConfigs->yieldCurveConfig(name);
        cs = new YieldCurveSpec(cc->currency(), name);
        break;
    }
    case CurveSpec::CurveType::FX: {
        auto ccyPair = parseCurrencyPair(name, "");
        cs = new FXSpotSpec(ccyPair.first.code(), ccyPair.second.code());
        break;
    }
    case CurveSpec::CurveType::FXVolatility: {
        auto ccyPair = parseCurrencyPair(name, "");
        cs = new FXVolatilityCurveSpec(ccyPair.first.code(), ccyPair.second.code(), name);
        break;
    }
    case CurveSpec::CurveType::SwaptionVolatility: {
        string key = name;
        // if the key is an index and we don't have a cc for that, fall back to the ccy
        QuantLib::ext::shared_ptr<IborIndex> ind;
        if (tryParseIborIndex(name, ind) && !curveConfigs->hasSwaptionVolCurveConfig(name)) {
            key = ind->currency().code();
        }
        cs = new SwaptionVolatilityCurveSpec(key, key);
        break;
    }
    case CurveSpec::CurveType::Default: {
        std::string nameStrippedSec = creditCurveNameFromSecuritySpecificCreditCurveName(name);
        if (curveConfigs->hasDefaultCurveConfig(nameStrippedSec)) {
            auto cc = QuantLib::ext::dynamic_pointer_cast<DefaultCurveConfig>(curveConfigs->get(ct, nameStrippedSec));
            cs = new DefaultCurveSpec(cc->currency(), name);
        }
        else {
            StructuredCurveErrorMessage(
				name, "Market Object to curve spec",
				"No default curve config for curve '" + name +
					"'.  Cannot add this curve to todays market parameters. Add a curve config for this ID.")
				.log();
		}
        break;
    }
    case CurveSpec::CurveType::CDSVolatility: {
        cs = new CDSVolatilityCurveSpec(name);
        break;
    }
    case CurveSpec::CurveType::BaseCorrelation: {
        cs = new BaseCorrelationCurveSpec(name);
        break;
    }
    case CurveSpec::CurveType::CapFloorVolatility: {
        string key = name;
        // if the key is an index and we don't have a cc for that, fall back to the ccy
        QuantLib::ext::shared_ptr<IborIndex> ind;
        if (tryParseIborIndex(name, ind) && !curveConfigs->hasCapFloorVolCurveConfig(name)) {
            key = ind->currency().code();
        }
        cs = new CapFloorVolatilityCurveSpec(key, key);
        break;
    }
    case CurveSpec::CurveType::Inflation: {
        string cId;
        if (mo == MarketObject::ZeroInflationCurve) {
            if (auto cc = curveConfigs->findInflationCurveConfig(name, InflationCurveConfig::Type::ZC))
                cId = cc->curveID();
        } else if (auto cc = curveConfigs->findInflationCurveConfig(name, InflationCurveConfig::Type::YY))
            cId = cc->curveID();

        cs = new InflationCurveSpec(name, cId);
        break;
    }
    case CurveSpec::CurveType::InflationCapFloorVolatility: {
        string cId;
        if (mo == MarketObject::ZeroInflationCapFloorVol) {
            if (auto cc =
                    curveConfigs->findInflationVolCurveConfig(name, InflationCapFloorVolatilityCurveConfig::Type::ZC))
                cId = cc->curveID();
        } else if (auto cc = curveConfigs->findInflationVolCurveConfig(
                       name, InflationCapFloorVolatilityCurveConfig::Type::YY))
            cId = cc->curveID();

        cs = new InflationCapFloorVolatilityCurveSpec(name, cId);
        break;
    }
    case CurveSpec::CurveType::Equity: {
        if (curveConfigs->hasEquityCurveConfig(name)) {
            string eqName = boost::replace_all_copy(name, "/", "\\/");
            cs = new EquityCurveSpec(curveConfigs->equityCurveConfig(name)->currency(), eqName);
        } else {
            StructuredCurveErrorMessage(
                name, "Market Object to curve spec",
                "No equity curve config for curve '" + name +
                    "'.  Cannot add this curve to todays market parameters. Add a curve config for this ID.")
                .log();
        }
        break;
    }
    case CurveSpec::CurveType::EquityVolatility: {
        if (curveConfigs->hasEquityVolCurveConfig(name)) {
            string eqName = boost::replace_all_copy(name, "/", "\\/");
            cs = new EquityVolatilityCurveSpec(curveConfigs->equityVolCurveConfig(name)->ccy(), eqName);
        } else {
            StructuredCurveErrorMessage(
                name, "Market Object to curve spec",
                "No equity vol curve config for curve '" + name +
                    "'.  Cannot add this curve to todays market parameters. Add a curve config for this ID.")
                .log();
        }
        break;
    }
    case CurveSpec::CurveType::Security: {
        cs = new SecuritySpec(name);
        break;
    }
    case CurveSpec::CurveType::Commodity: {
        if (curveConfigs->hasCommodityCurveConfig(name)) {
            cs = new CommodityCurveSpec(curveConfigs->commodityCurveConfig(name)->currency(), name);
        } else {
            StructuredCurveErrorMessage(
                name, "Market Object to config",
                "No commodity curve config for curve '" + name +
                    "'.  Can not add this curve to todays market parameters. Add a curve config for this id.")
                .log();
        }
        break;
    }
    case CurveSpec::CurveType::CommodityVolatility: {
        if (curveConfigs->hasCommodityVolatilityConfig(name)) {
            cs = new CommodityVolatilityCurveSpec(curveConfigs->commodityVolatilityConfig(name)->currency(), name);
        } else {
            StructuredCurveErrorMessage(
                name, "Market Object to config",
                "No commodity vol curve config for curve '" + name +
                    "'.  Can not add this curve to todays market parameters. Add a curve config for this id.")
                .log();
        }
        break;
    }
    case CurveSpec::CurveType::Correlation: {
        // correlation name can be "foo:bar" or "foo&bar", we check the curve config
        if (!curveConfigs->hasCorrelationCurveConfig(name)) {
            // if we have a &, lets try to change it to :
            string tmp = boost::replace_all_copy(name, "&", ":");
            if (curveConfigs->hasCorrelationCurveConfig(tmp))
                cs = new CorrelationCurveSpec(tmp);
        } else
			cs = new CorrelationCurveSpec(name);
        break;
    }
    case CurveSpec::CurveType::YieldVolatility: {
        cs = new YieldVolatilityCurveSpec(name);
        break;
    }
    case CurveSpec::CurveType::SwapIndex: {
        return swapIndexDiscountCurve(name.substr(0, 3), baseCcy, name);
    }
	default:
		QL_FAIL("Cannot convert market object " << mo << "to curve spec");
    }
    if (cs == nullptr)
        return string();	    
    else
        return cs->name();
}


MarketObject curveTypeToMarketObject(CurveSpec::CurveType ct, const string& curve,
                                     const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs) {
    // one to one mapping, not all curvetypes can be mapped directly though
    const static map<CurveSpec::CurveType, MarketObject> ctm = {
		{CurveSpec::CurveType::SwapIndex, MarketObject::SwapIndexCurve},
		{CurveSpec::CurveType::FX, MarketObject::FXSpot},
		{CurveSpec::CurveType::FXVolatility, MarketObject::FXVol},
		{CurveSpec::CurveType::SwaptionVolatility, MarketObject::SwaptionVol},
		{CurveSpec::CurveType::Default, MarketObject::DefaultCurve},
		{CurveSpec::CurveType::CDSVolatility, MarketObject::CDSVol},
		{CurveSpec::CurveType::BaseCorrelation, MarketObject::BaseCorrelation},
		{CurveSpec::CurveType::CapFloorVolatility, MarketObject::CapFloorVol},
		{CurveSpec::CurveType::Equity, MarketObject::EquityCurve},
		{CurveSpec::CurveType::EquityVolatility, MarketObject::EquityVol},
		{CurveSpec::CurveType::Security, MarketObject::Security},
		{CurveSpec::CurveType::Commodity, MarketObject::CommodityCurve},
		{CurveSpec::CurveType::CommodityVolatility, MarketObject::CommodityVolatility},
		{CurveSpec::CurveType::Correlation, MarketObject::Correlation},
		{CurveSpec::CurveType::YieldVolatility, MarketObject::YieldVol}};
	 
	auto it = ctm.find(ct);
    if (it != ctm.end())
        return it->second;
    else {
        if (ct == CurveSpec::CurveType::Yield) {
            if (isIborIndex(curve))
                return MarketObject::IndexCurve;
            else if (CurrencyParser::instance().isValidCurrency(curve))
				return MarketObject::DiscountCurve;
            else
                return MarketObject::YieldCurve;
        }
        else if (ct == CurveSpec::CurveType::Inflation) {
            auto icc = curveConfigs->inflationCurveConfig(curve);
            if (icc->type() == InflationCurveConfig::Type::ZC)
				return MarketObject::ZeroInflationCurve;
			else
				return MarketObject::YoYInflationCurve;
        } else if (ct == CurveSpec::CurveType::InflationCapFloorVolatility) {
            auto icc = curveConfigs->inflationCapFloorVolCurveConfig(curve);
            if (icc->type() == InflationCapFloorVolatilityCurveConfig::Type::ZC)
                return MarketObject::ZeroInflationCapFloorVol;
            else
                return MarketObject::YoYInflationCapFloorVol;
        } else
            QL_FAIL("Cannot convert curve type " << ct << " to market object");
    }

}

string curveSpecToName(CurveSpec::CurveType ct, const string& cId,
                       const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs) { 
    if (ct == CurveSpec::CurveType::Inflation) {
        auto icc = QuantLib::ext::dynamic_pointer_cast<InflationCurveConfig>(curveConfigs->get(ct, cId));
        auto conv = icc->conventions();
        auto conventions = InstrumentConventions::instance().conventions();
        auto iconv = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(conventions->get(conv));
        return iconv->indexName();
    } 
    else if(ct == CurveSpec::CurveType::InflationCapFloorVolatility) 
    {
        auto icc =
            QuantLib::ext::dynamic_pointer_cast<InflationCapFloorVolatilityCurveConfig>(curveConfigs->get(ct, cId));
        auto conv = icc->conventions();
        auto conventions = InstrumentConventions::instance().conventions();
        auto iconv = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(conventions->get(conv));
        return iconv->indexName();
    }
    else
        return cId;

}

bool checkMarketObject(std::map<std::string, std::map<ore::data::MarketObject, std::set<std::string>>>* objects,
                       CurveSpec::CurveType ct, const string& cId,
                       const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs, std::string configuration) {
    MarketObject mo = curveTypeToMarketObject(ct, cId, curveConfigs);
    string name = curveSpecToName(ct, cId, curveConfigs);
    auto it = objects->find(configuration);
    if (it != objects->end()) {
        auto it1 = it->second.find(mo);
        if (it1 != it->second.end()) {
			if (it1->second.find(name) != it1->second.end())
				return true;
		}
    }
	return false;
}

void addMarketObjectDependencies(std::map<std::string, std::map<ore::data::MarketObject, std::set<std::string>>>* objects,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs, const string& baseCcy,
    const string& baseCcyDiscountCurve) {

    for (const auto& [config, mp] : *objects) {
        std::map<CurveSpec::CurveType, std::set<string>> dependencies;

        for (const auto& [o, s] : mp) {
            auto ct = marketObjectToCurveType(o);
            for (const auto& c : s) {
                DLOG("Get dependencies for " << o << " " << c << " in configuration " << config);
                string cId = c;
                if (!curveConfigs->has(ct, c)) {
                    // the object name is not a curve id, try to find the curve id in the curve config
                    // for most MarketObjects we cannot do much here, the curve id is the object name,
                    // but for some we can search by ccy etc
                    // We should only need to do this here when converting from the names returned from a portfolio
                    // to curve config Id's. In the loop below the dependencies obtained from the curveconfig
                    // should be valid curve config Id's
                    switch (o) {
                    case MarketObject::DiscountCurve: {
                        if (config == Market::inCcyConfiguration)
                            cId = swapIndexDiscountCurve(c, baseCcy);
                        else
                            cId = currencyToDiscountCurve(c, baseCcy, baseCcyDiscountCurve, curveConfigs);
                        break;
                    }
                    case MarketObject::SwaptionVol: {
                        // the name may be an Index, in this case, look for a SwaptionVolatility curve in the currency
                        QuantLib::ext::shared_ptr<IborIndex> ind;
                        if (tryParseIborIndex(c, ind) &&
                            curveConfigs->hasSwaptionVolCurveConfig(ind->currency().code())) {
                            cId = ind->currency().code();
                        }
                        break;
                    }
                    case MarketObject::SwapIndexCurve:
                    case MarketObject::CapFloorVol: {
                        QuantLib::ext::shared_ptr<IborIndex> ind;
                        if (tryParseIborIndex(c, ind) &&
                            curveConfigs->hasCapFloorVolCurveConfig(ind->currency().code())) {
                            cId = ind->currency().code();
                        }
                        break;
                    }
                    // inflation is tricky since we use one curveconfiguration type for both zero and yoy inflation
                    // so if we get a name like "EUHICPXT" there could be multiple inflation curveconfigs named
                    // EUHICPXT_YY_Swaps or EUHICPXT_ZC_Swaps, so we need to check the type and index
                    case MarketObject::ZeroInflationCurve: {
                        if (auto cc = curveConfigs->findInflationCurveConfig(c, InflationCurveConfig::Type::ZC)) {
                            cId = cc->curveID();
                        }
                        break;
                    }
                    case MarketObject::YoYInflationCurve: {
                        if (auto cc = curveConfigs->findInflationCurveConfig(c, InflationCurveConfig::Type::YY))
                            cId = cc->curveID();
                        break;
                    }
                    case MarketObject::ZeroInflationCapFloorVol: {
                        if (auto cc = curveConfigs->findInflationVolCurveConfig(
                                c, InflationCapFloorVolatilityCurveConfig::Type::ZC)) {
                            cId = cc->curveID();
                        }
                        break;
                    }
                    case MarketObject::YoYInflationCapFloorVol: {
                        if (auto cc = curveConfigs->findInflationVolCurveConfig(
                                c, InflationCapFloorVolatilityCurveConfig::Type::YY))
                            cId = cc->curveID();
                        break;
                    }
                    default:
                        continue;
                    }
                }
                if (!cId.empty()) {
                    auto deps = curveConfigs->requiredCurveIds(ct, cId);
                    for (const auto& [ct1, ids1] : deps) {
                        for (const auto& id : ids1) {
                            if (!checkMarketObject(objects, ct1, id, curveConfigs, config))
                                dependencies[ct1].insert(id);
                        }
                    }
                }
            }
        }
        Size i = 0; // check to prevent infinite loop
        while (dependencies.size() > 0 && i < 1000) {
            std::map<CurveSpec::CurveType, std::set<string>> newDependencies;
            for (const auto& [ct, ids] : dependencies) {
                for (const auto& cId : ids) {
                    MarketObject mo = curveTypeToMarketObject(ct, cId, curveConfigs);
                    string name = curveSpecToName(ct, cId, curveConfigs);
                    if (mo == MarketObject::IndexCurve && isGenericIborIndex(name))
                        continue;
                    (*objects)[Market::defaultConfiguration][mo].insert(name);
                    auto deps = curveConfigs->requiredCurveIds(ct, cId);
                    for (const auto& [ct1, ids1] : deps) {
                        for (const auto& id : ids1) {
                            if (!checkMarketObject(objects, ct1, id, curveConfigs, config))
                                newDependencies[ct1].insert(id);
                        }
                    }
                    // for SwapIndexes we are still missing the discount curve dependency
                    if (mo == MarketObject::SwapIndexCurve)
                        newDependencies[CurveSpec::CurveType::Yield].insert(
                            swapIndexDiscountCurve(name.substr(0, 3), baseCcy, name));
                }
            }
            dependencies = newDependencies;
            ++i;
        }
    }
}

string currencyToDiscountCurve(const string& ccy, const string& baseCcy, const string& baseCcyDiscountCurve,
                               const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs) {
    if (ccy == baseCcy) {
        // if a discount curve has been provided use that
        if (!baseCcyDiscountCurve.empty())
            return baseCcyDiscountCurve;

        // Use the discount curve of the standard swap in the given currency
        string discCurve = swapIndexDiscountCurve(ccy, baseCcy);

        // If we can't get a base currency discount curve, we should stop
        QL_REQUIRE(!discCurve.empty(), "ConfigurationBuilder cannot get a discount curve for base currency " << ccy);

        return discCurve;
    } else {
        buildCollateralCurveConfig(ccy + "-IN-" + baseCcy, baseCcy, baseCcyDiscountCurve, curveConfigs);
        return ccy + "-IN-" + baseCcy;
    }
}

string swapIndexDiscountCurve(const string& ccy, const string& baseCcy, const string& swapIndexConvId) {

    DLOG("Get the swap index discount curve for currency '" << ccy << "'");

    QuantLib::ext::shared_ptr<Convention> conv;
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

    std::string swapConvId;

    if (!swapIndexConvId.empty() && conventions->has(swapIndexConvId)) {

        // try to use the 3rd tag as in USD-CMS-SOFR1M-30Y, removing the tenor

        std::vector<string> tokens;
        boost::split(tokens, swapIndexConvId, boost::is_any_of("-"));
        if (tokens.size() == 4) {
            tokens[2].erase(
                std::find_if(tokens[2].begin(), tokens[2].end(), [](const char c) { return std::isdigit(c); }),
                tokens[2].end());
            QuantLib::ext::shared_ptr<IborIndex> index;
            if (tryParseIborIndex(tokens[0] + "-" + tokens[2], index) &&
                QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index)) {
                return tokens[0] + "-" + tokens[2];
            }
        }

        // otherwise extract the swap conv id which will be used below

        if (auto tmp = QuantLib::ext::dynamic_pointer_cast<SwapIndexConvention>(conventions->get(swapIndexConvId)))
            swapConvId = tmp->conventions();
    }
    
    // if a swap convention is provided we first check if that is an OIS convention, if so use it's index
    if (!swapConvId.empty() && conventions->has(swapConvId)) {
        conv = conventions->get(swapConvId);

        // check if it is an ois convention
        QuantLib::ext::shared_ptr<OisConvention> oisCompConv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv);
        QuantLib::ext::shared_ptr<AverageOisConvention> oisAvgConv = QuantLib::ext::dynamic_pointer_cast<AverageOisConvention>(conv);
        if (oisCompConv)
            return oisCompConv->indexName();
        else if (oisAvgConv)
            return oisAvgConv->indexName();
    }

    // next try find an OIS discount curve for the currency
    string oisId = ccy + "-OIS";
    if (conventions->has(oisId)) {
        conv = conventions->get(oisId);

        // check if it is an ois convention
        QuantLib::ext::shared_ptr<OisConvention> oisCompConv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv);
        QuantLib::ext::shared_ptr<AverageOisConvention> oisAvgConv = QuantLib::ext::dynamic_pointer_cast<AverageOisConvention>(conv);
        if (oisCompConv)
            return oisCompConv->indexName();
        else if (oisAvgConv)
            return oisAvgConv->indexName();
    } 

    string indexName;
    // next if no OIS curve, and a swapConvId provided, we look up the Ibor index for that swapConvId
    if (!swapConvId.empty() && conventions->has(swapConvId)) {
        conv = conventions->get(swapConvId);

        // check if it is an ois convention
        QuantLib::ext::shared_ptr<IRSwapConvention> irConv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv);
        if (irConv)
            indexName = irConv->indexName();
    }
    
    if (indexName.empty()) {
        // otherwise look up the Standard swap conention
        string convId = ccy + "-SWAP";
        if (!conventions->has(convId)) {
            DLOG("Could not get IR swap conventions with ID '" << convId);
        } else {
            conv = conventions->get(convId);
            QuantLib::ext::shared_ptr<IRSwapConvention> irConv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv);
            if (irConv)
                indexName = irConv->indexName();
        }
    }

    // We don't want a GENERIC curve as the discount
    if (isGenericIborIndex(indexName) || indexName.empty()) {
        if (baseCcy.empty())
            return string();
        QL_REQUIRE(ccy != baseCcy,
                   "ConfigurationBuilder: can not determine base ccy discount curve for "
                       << ccy << " because neither appropriate swap conventions nor discounting_index is given.");
        indexName = ccy + "-IN-" + baseCcy;
    }

    DLOG("Got the swap index discount curve for currency '" << ccy << "', - '" << indexName <<"'");
    return indexName;
}

void buildCollateralCurveConfig(const string& id, const string& baseCcy, const ::string& baseCcyDiscountCurve,
                                const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs) {

    vector<string> tokens;
    if (!isCollateralCurve(id, tokens))
        return;

    if (!curveConfigs->hasYieldCurveConfig(id)) {
        string ccy = tokens[0];
        string base = tokens[2];
        string baseCurve = (base == baseCcy && !baseCcyDiscountCurve.empty()) ? baseCcyDiscountCurve : swapIndexDiscountCurve(base);
        set<string> baseDiscountCcys = getCollateralisedDiscountCcy(base, curveConfigs);

        DLOG("Curve configuration missing for discount curve " << id
                                                               << ", attempting to generate from available curves");

        set<string> baseCcys = getCollateralisedDiscountCcy(ccy, curveConfigs);

        string commonDiscount = "";
        // Look for a common discount curve curve to use as a cross
        auto it = baseCcys.begin();
        while (it != baseCcys.end() && commonDiscount == "") {
            auto pos = baseDiscountCcys.find(*it);
            if (pos != baseDiscountCcys.end()) {
                commonDiscount = *pos;
            }
            it++;
        }

        if (commonDiscount == "") {
            DLOG("Cannot create a discount curve config for currency " + ccy);
        } else {
            vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments;
            segments.push_back(QuantLib::ext::make_shared<DiscountRatioYieldCurveSegment>(
                "Discount Ratio", baseCurve, baseCcy, ccy + "-IN-" + commonDiscount, ccy,
                baseCcy + "-IN-" + commonDiscount, baseCcy));

            QuantLib::ext::shared_ptr<YieldCurveConfig> ycc = QuantLib::ext::make_shared<YieldCurveConfig>(
                id, ccy + " collateralised in " + baseCcy, ccy, "", segments);

            curveConfigs->add(CurveSpec::CurveType::Yield, id, ycc);
        }
    }
}

set<string> getCollateralisedDiscountCcy(const string& ccy,
                                         const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs) {
    set<string> discountCcys;
    set<string> ids = curveConfigs->yieldCurveConfigIds();
    for (auto id : ids) {
        vector<string> tokens;
        if (isCollateralCurve(id, tokens) && tokens[0] == ccy)
            discountCcys.insert(tokens[2]);
    }

    return discountCcys;
}

const bool isCollateralCurve(const string& id, vector<string>& tokens) {
    boost::split(tokens, id, boost::is_any_of("-"));
    if (tokens.size() == 3 && tokens[1] == "IN")
        return true;
    return false;
}
	
} // namespace data
} // namespace ore