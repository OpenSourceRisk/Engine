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

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

// utility function to add a set of nodes from a given map of curve configs
template <class T>
void addMinimalCurves(const char* nodeName, const map<string, boost::shared_ptr<T>>& m,
                      map<string, boost::shared_ptr<T>>& n, CurveSpec::CurveType curveType,
                      const map<CurveSpec::CurveType, set<string>> configIds) {
    for (auto it : m) {
        if ((configIds.count(curveType) && configIds.at(curveType).count(it.first))) {
            const string& id = it.second->curveID();
            n[id] = it.second;
        }
    }
}

void CurveConfigurations::addNodes(XMLDocument& doc, XMLNode* parent, const char* nodeName) {
    const auto& ct = parseCurveConfigurationType(nodeName);
    const auto& it = configs_.find(ct);
    if (it != configs_.end()) {
        XMLNode* node = doc.allocNode(nodeName);
        XMLUtils::appendNode(parent, node);

        for (const auto& c : it->second) {
            XMLUtils::appendNode(node, c.second->toXML(doc));
        }
    }
}

const boost::shared_ptr<CurveConfig>& CurveConfigurations::parseNode(const CurveSpec::CurveType& type, const string& curveId) {
    boost::shared_ptr<CurveConfig> config;
    const auto& it = unparsed_.find(type);
    if (it != unparsed_.end()) {
        const auto& itc = it->second.find(curveId);
        if (itc != it->second.end()) {
            switch (type) {
            case CurveSpec::CurveType::Yield: {
                config = boost::make_shared<YieldCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Default: {
                config = boost::make_shared<DefaultCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::CDSVolatility: {
                config = boost::make_shared<CDSVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::BaseCorrelation: {
                config = boost::make_shared<BaseCorrelationCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::FX: {
                config = boost::make_shared<FXSpotConfig>();
                break;
            }
            case CurveSpec::CurveType::FXVolatility: {
                config = boost::make_shared<FXVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::SwaptionVolatility: {
                config = boost::make_shared<SwaptionVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::YieldVolatility: {
                config = boost::make_shared<YieldVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::CapFloorVolatility: {
                config = boost::make_shared<CapFloorVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Inflation: {
                config = boost::make_shared<InflationCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::InflationCapFloorVolatility: {
                config = boost::make_shared<InflationCapFloorVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Equity: {
                config = boost::make_shared<EquityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::EquityVolatility: {
                config = boost::make_shared<EquityVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Security: {
                config = boost::make_shared<SecurityConfig>();
                break;
            }
            case CurveSpec::CurveType::Commodity: {
                config = boost::make_shared<CommodityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::CommodityVolatility: {
                config = boost::make_shared<CommodityVolatilityConfig>();
                break;
            }
            case CurveSpec::CurveType::Correlation: {
                config = boost::make_shared<CorrelationCurveConfig>();
                break;
            }
            }
            try {
                config->fromXML(itc->second.get());
            } catch (std::exception& ex) {
                ALOG(StructuredCurveErrorMessage(curveId,
                                                 "Curve config '" + curveId + "' under node '" + to_string(type) +
                                                     "' was requested, but could not be parsed.",
                                                 ex.what()));
            }
            add(type, curveId, config);            
        } else
            QL_FAIL("Could not find curveId " << curveId << " of type " << type << " in unparsed curve configurations");
    } else
        QL_FAIL("Could not find CurveType " << type << " in unparsed curve configurations");
    return config;
}

void CurveConfigurations::add(const CurveSpec::CurveType& type, const string& curveId,
    const boost::shared_ptr<CurveConfig>& config) const {
    configs_[type][curveId] = config;
}

bool CurveConfigurations::has(const CurveSpec::CurveType& type, const string& curveId) {
    // look in the parsed configs first
    const auto& it = configs_.find(type);
    if (it != configs_.end()) {
        const auto& itc = it->second.find(curveId);
        if (itc != it->second.end()) {
            return true;
        }
    }

    // look in the unparsed nodes
    const auto& itu = unparsed_.find(type);
    if (itu != unparsed_.end()) {
        const auto& itc = itu->second.find(curveId);
        if (itc != itu->second.end()) {
            return true;
        }
    }

    return false;
}

const boost::shared_ptr<CurveConfig>& CurveConfigurations::get(const CurveSpec::CurveType& type,
    const string& curveId) {
    const auto& it = configs_.find(type);
    if (it != configs_.end()) {
        const auto& itc = it->second.find(curveId);
        if (itc != it->second.end()) {
            return itc->second;
        }
    }
    return parseNode(type, curveId);
}

boost::shared_ptr<CurveConfigurations>
CurveConfigurations::minimalCurveConfig(const boost::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                        const set<string>& configurations) const {

    boost::shared_ptr<CurveConfigurations> minimum = boost::make_shared<CurveConfigurations>();
    // If tmparams is not null, organise its specs in to a map [CurveType, set of CurveConfigID]
    map<CurveSpec::CurveType, set<string>> curveConfigIds;
    // This set of FXSpotSpec is used below
    set<boost::shared_ptr<FXSpotSpec>> fxSpotSpecs;

    for (const auto& config : configurations) {
        for (const auto& strSpec : todaysMarketParams->curveSpecs(config)) {
            auto spec = parseCurveSpec(strSpec);
            if (curveConfigIds.count(spec->baseType())) {
                curveConfigIds[spec->baseType()].insert(spec->curveConfigID());
            } else {
                curveConfigIds[spec->baseType()] = {spec->curveConfigID()};
            }

            if (spec->baseType() == CurveSpec::CurveType::FX) {
                boost::shared_ptr<FXSpotSpec> fxss = boost::dynamic_pointer_cast<FXSpotSpec>(spec);
                QL_REQUIRE(fxss, "Expected an FXSpotSpec but did not get one");
                fxSpotSpecs.insert(fxss);
            }
        }
    }

    for (const auto& it : curveConfigIds) {
        for (const auto& c : it.second) {
            minimum->add(it.first,c, get(it.first, c));
        }
    }

    for (auto it : m) {
        if ((configIds.count(curveType) && configIds.at(curveType).count(it.first))) {
            const string& id = it.second->curveID();
            n[id] = it.second;
        }
    }


    // follow order in xsd
    addMinimalCurves("FXSpots", fxSpotConfigs_, minimum->fxSpotConfigs_, CurveSpec::CurveType::FX, curveConfigIds);
    addMinimalCurves("FXVolatilities", fxVolCurveConfigs_, minimum->fxVolCurveConfigs_,
                     CurveSpec::CurveType::FXVolatility, curveConfigIds);
    addMinimalCurves("SwaptionVolatilities", swaptionVolCurveConfigs_, minimum->swaptionVolCurveConfigs_,
                     CurveSpec::CurveType::SwaptionVolatility, curveConfigIds);
    addMinimalCurves("YieldVolatilities", yieldVolCurveConfigs_, minimum->yieldVolCurveConfigs_,
                     CurveSpec::CurveType::YieldVolatility, curveConfigIds);
    addMinimalCurves("CapFloorVolatilities", capFloorVolCurveConfigs_, minimum->capFloorVolCurveConfigs_,
                     CurveSpec::CurveType::CapFloorVolatility, curveConfigIds);
    addMinimalCurves("CDSVolatilities", cdsVolCurveConfigs_, minimum->cdsVolCurveConfigs_,
                     CurveSpec::CurveType::CDSVolatility, curveConfigIds);
    addMinimalCurves("DefaultCurves", defaultCurveConfigs_, minimum->defaultCurveConfigs_,
                     CurveSpec::CurveType::Default, curveConfigIds);
    addMinimalCurves("YieldCurves", yieldCurveConfigs_, minimum->yieldCurveConfigs_, CurveSpec::CurveType::Yield,
                     curveConfigIds);
    addMinimalCurves("InflationCurves", inflationCurveConfigs_, minimum->inflationCurveConfigs_,
                     CurveSpec::CurveType::Inflation, curveConfigIds);
    addMinimalCurves("InflationCapFloorVolatilities", inflationCapFloorVolCurveConfigs_,
                     minimum->inflationCapFloorVolCurveConfigs_, CurveSpec::CurveType::InflationCapFloorVolatility,
                     curveConfigIds);
    addMinimalCurves("EquityCurves", equityCurveConfigs_, minimum->equityCurveConfigs_, CurveSpec::CurveType::Equity,
                     curveConfigIds);
    addMinimalCurves("EquityVolatilities", equityVolCurveConfigs_, minimum->equityVolCurveConfigs_,
                     CurveSpec::CurveType::EquityVolatility, curveConfigIds);
    addMinimalCurves("Securities", securityConfigs_, minimum->securityConfigs_, CurveSpec::CurveType::Security,
                     curveConfigIds);
    addMinimalCurves("BaseCorrelations", baseCorrelationCurveConfigs_, minimum->baseCorrelationCurveConfigs_,
                     CurveSpec::CurveType::BaseCorrelation, curveConfigIds);
    addMinimalCurves("CommodityCurves", commodityCurveConfigs_, minimum->commodityCurveConfigs_,
                     CurveSpec::CurveType::Commodity, curveConfigIds);
    addMinimalCurves("CommodityVolatilities", commodityVolatilityConfigs_, minimum->commodityVolatilityConfigs_,
                     CurveSpec::CurveType::CommodityVolatility, curveConfigIds);
    addMinimalCurves("Correlations", correlationCurveConfigs_, minimum->correlationCurveConfigs_,
                     CurveSpec::CurveType::Correlation, curveConfigIds);

    return minimum;
}

std::set<string> CurveConfigurations::quotes(const boost::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                             const set<string>& configurations) const {

    std::set<string> quotes = minimalCurveConfig(todaysMarketParams, configurations)->quotes();

    // FX spot is special in that we generally do not enter a curve configuration for it. Above, we ran over the
    // curve configurations asking each for its quotes. We may end up missing FX spot quotes that are specified in a
    // TodaysMarketParameters but do not have a CurveConfig. If we have a TodaysMarketParameters instance we can add
    // them here directly using it.

    for (const auto& config : configurations) {
        for (const auto& strSpec : todaysMarketParams->curveSpecs(config)) {
            auto spec = parseCurveSpec(strSpec);
            if (spec->baseType() == CurveSpec::CurveType::FX) {
                boost::shared_ptr<FXSpotSpec> fxss = boost::dynamic_pointer_cast<FXSpotSpec>(spec);
                QL_REQUIRE(fxss, "Expected an FXSpotSpec but did not get one");
                string strQuote = "FX/RATE/" + fxss->unitCcy() + "/" + fxss->ccy();
                quotes.insert(strQuote);
            }
        }
    }

    return quotes;
}

std::set<string> CurveConfigurations::quotes() const {
    set<string> quotes;

    // only add quotes for parsed configs
    for (const auto& ct : configs_) {
        for (const auto& c : ct.second) {
            quotes.insert(c.second->quotes().begin(), c.second->quotes().end());
        }
    }
    return quotes;
}

std::set<string> CurveConfigurations::conventions(const boost::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                                  const set<string>& configurations) const {
    set<string> conventions = minimalCurveConfig(todaysMarketParams, configurations)->conventions();
    // Checking for any swapIndices

    if (todaysMarketParams->hasMarketObject(MarketObject::SwapIndexCurve)) {
        auto mapping = todaysMarketParams->mapping(MarketObject::SwapIndexCurve, Market::defaultConfiguration);

        for (auto m : mapping)
            conventions.insert(m.first);
    }

    return conventions;
}

std::set<string> CurveConfigurations::conventions() const {
    set<string> conventions;

    for (auto& y : yieldCurveConfigs_) {
        for (auto& c : y.second->curveSegments()) {
            if (c->conventionsID() != "")
                conventions.insert(c->conventionsID());
        }
    }

    for (auto& d : defaultCurveConfigs_) {
        for (auto const& c : d.second->configs())
            if (c.second.conventionID() != "")
                conventions.insert(c.second.conventionID());
    }

    for (auto& i : inflationCurveConfigs_) {
        if (i.second->conventions() != "")
            conventions.insert(i.second->conventions());
    }

    for (auto& c : correlationCurveConfigs_) {
        if (c.second->conventions() != "")
            conventions.insert(c.second->conventions());
    }

    for (auto& c : fxVolCurveConfigs_) {
        if (c.second->conventionsID() != "") {
            conventions.insert(c.second->conventionsID());
        }
    }

    return conventions;
}

set<string> CurveConfigurations::yieldCurveConfigIds() {

    set<string> curves;
    for (auto yc : yieldCurveConfigs_)
        curves.insert(yc.first);

    return curves;
}

namespace {
template <typename T>
void addRequiredCurveIds(const std::string& curveId, const std::map<std::string, boost::shared_ptr<T>>& configs,
                         std::map<CurveSpec::CurveType, std::set<string>>& result) {
    auto c = configs.find(curveId);
    if (c != configs.end()) {
        auto r = c->second->requiredCurveIds();
        result.insert(r.begin(), r.end());
    }
}
} // namespace

std::map<CurveSpec::CurveType, std::set<string>>
CurveConfigurations::requiredCurveIds(const CurveSpec::CurveType& type, const std::string& curveId) const {
    std::map<CurveSpec::CurveType, std::set<string>> result;
    if (type == CurveSpec::CurveType::Yield)
        addRequiredCurveIds(curveId, yieldCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::FXVolatility)
        addRequiredCurveIds(curveId, fxVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::SwaptionVolatility)
        addRequiredCurveIds(curveId, swaptionVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::YieldVolatility)
        addRequiredCurveIds(curveId, yieldVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::CapFloorVolatility)
        addRequiredCurveIds(curveId, capFloorVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::Default)
        addRequiredCurveIds(curveId, defaultCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::CDSVolatility)
        addRequiredCurveIds(curveId, cdsVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::BaseCorrelation)
        addRequiredCurveIds(curveId, baseCorrelationCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::Inflation)
        addRequiredCurveIds(curveId, inflationCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::InflationCapFloorVolatility)
        addRequiredCurveIds(curveId, inflationCapFloorVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::Equity)
        addRequiredCurveIds(curveId, equityCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::EquityVolatility)
        addRequiredCurveIds(curveId, equityVolCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::Security)
        addRequiredCurveIds(curveId, securityConfigs_, result);
    else if (type == CurveSpec::CurveType::FX)
        addRequiredCurveIds(curveId, fxSpotConfigs_, result);
    else if (type == CurveSpec::CurveType::Commodity)
        addRequiredCurveIds(curveId, commodityCurveConfigs_, result);
    else if (type == CurveSpec::CurveType::CommodityVolatility)
        addRequiredCurveIds(curveId, commodityVolatilityConfigs_, result);
    else if (type == CurveSpec::CurveType::Correlation)
        addRequiredCurveIds(curveId, correlationCurveConfigs_, result);
    else {
        QL_FAIL("CurveConfigurations::requiredCurveIds(): unhandled curve spec type");
    }
    return result;
}

bool CurveConfigurations::hasYieldCurveConfig(const string& curveID) const { return has(curveID, yieldCurveConfigs_); }

const boost::shared_ptr<YieldCurveConfig>& CurveConfigurations::yieldCurveConfig(const string& curveID) const {
    return get(curveID, yieldCurveConfigs_);
}

bool CurveConfigurations::hasFxVolCurveConfig(const string& curveID) const { return has(curveID, fxVolCurveConfigs_); }

const boost::shared_ptr<FXVolatilityCurveConfig>& CurveConfigurations::fxVolCurveConfig(const string& curveID) const {
    return get(curveID, fxVolCurveConfigs_);
}

bool CurveConfigurations::hasSwaptionVolCurveConfig(const string& curveID) const {
    return has(curveID, swaptionVolCurveConfigs_);
}

const boost::shared_ptr<SwaptionVolatilityCurveConfig>&
CurveConfigurations::swaptionVolCurveConfig(const string& curveID) const {
    return get(curveID, swaptionVolCurveConfigs_);
}

bool CurveConfigurations::hasYieldVolCurveConfig(const string& curveID) const {
    return has(curveID, yieldVolCurveConfigs_);
}

const boost::shared_ptr<YieldVolatilityCurveConfig>&
CurveConfigurations::yieldVolCurveConfig(const string& curveID) const {
    return get(curveID, yieldVolCurveConfigs_);
}

bool CurveConfigurations::hasCapFloorVolCurveConfig(const string& curveID) const {
    return has(curveID, capFloorVolCurveConfigs_);
}

const boost::shared_ptr<CapFloorVolatilityCurveConfig>&
CurveConfigurations::capFloorVolCurveConfig(const string& curveID) const {
    return get(curveID, capFloorVolCurveConfigs_);
}

bool CurveConfigurations::hasDefaultCurveConfig(const string& curveID) const {
    return has(curveID, defaultCurveConfigs_);
}

const boost::shared_ptr<DefaultCurveConfig>& CurveConfigurations::defaultCurveConfig(const string& curveID) const {
    return get(curveID, defaultCurveConfigs_);
}

bool CurveConfigurations::hasCdsVolCurveConfig(const string& curveID) const {
    return has(curveID, cdsVolCurveConfigs_);
}

const boost::shared_ptr<CDSVolatilityCurveConfig>& CurveConfigurations::cdsVolCurveConfig(const string& curveID) const {
    return get(curveID, cdsVolCurveConfigs_);
}

bool CurveConfigurations::hasBaseCorrelationCurveConfig(const string& curveID) const {
    return has(curveID, baseCorrelationCurveConfigs_);
}

const boost::shared_ptr<BaseCorrelationCurveConfig>&
CurveConfigurations::baseCorrelationCurveConfig(const string& curveID) const {
    return get(curveID, baseCorrelationCurveConfigs_);
}

bool CurveConfigurations::hasInflationCurveConfig(const string& curveID) const {
    return has(curveID, inflationCurveConfigs_);
}

const boost::shared_ptr<InflationCurveConfig>& CurveConfigurations::inflationCurveConfig(const string& curveID) const {
    return get(curveID, inflationCurveConfigs_);
}

bool CurveConfigurations::hasInflationCapFloorVolCurveConfig(const string& curveID) const {
    return has(curveID, inflationCapFloorVolCurveConfigs_);
}

const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>&
CurveConfigurations::inflationCapFloorVolCurveConfig(const string& curveID) const {
    return get(curveID, inflationCapFloorVolCurveConfigs_);
}

bool CurveConfigurations::hasEquityCurveConfig(const string& curveID) const {
    return has(curveID, equityCurveConfigs_);
}

const boost::shared_ptr<EquityCurveConfig>& CurveConfigurations::equityCurveConfig(const string& curveID) const {
    return get(curveID, equityCurveConfigs_);
}

bool CurveConfigurations::hasEquityVolCurveConfig(const string& curveID) const {
    return has(curveID, equityVolCurveConfigs_);
}

const boost::shared_ptr<EquityVolatilityCurveConfig>&
CurveConfigurations::equityVolCurveConfig(const string& curveID) const {
    return get(curveID, equityVolCurveConfigs_);
}

bool CurveConfigurations::hasSecurityConfig(const string& curveID) const { return has(curveID, securityConfigs_); }

const boost::shared_ptr<SecurityConfig>& CurveConfigurations::securityConfig(const string& curveID) const {
    return get(curveID, securityConfigs_);
}

bool CurveConfigurations::hasFxSpotConfig(const string& curveID) const { return has(curveID, fxSpotConfigs_); }

const boost::shared_ptr<FXSpotConfig>& CurveConfigurations::fxSpotConfig(const string& curveID) const {
    return get(curveID, fxSpotConfigs_);
}

bool CurveConfigurations::hasCommodityCurveConfig(const string& curveID) const {
    return has(curveID, commodityCurveConfigs_);
}

const boost::shared_ptr<CommodityCurveConfig>& CurveConfigurations::commodityCurveConfig(const string& curveID) const {
    return get(curveID, commodityCurveConfigs_);
}

bool CurveConfigurations::hasCommodityVolatilityConfig(const string& curveID) const {
    return has(curveID, commodityVolatilityConfigs_);
}

const boost::shared_ptr<CommodityVolatilityConfig>&
CurveConfigurations::commodityVolatilityConfig(const string& curveID) const {
    return get(curveID, commodityVolatilityConfigs_);
}

bool CurveConfigurations::hasCorrelationCurveConfig(const string& curveID) const {
    return has(curveID, correlationCurveConfigs_);
}

const boost::shared_ptr<CorrelationCurveConfig>&
CurveConfigurations::correlationCurveConfig(const string& curveID) const {
    return get(curveID, correlationCurveConfigs_);
}

#include <iostream>
void CurveConfigurations::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CurveConfiguration");

    // Load global settings
    if (auto tmp = XMLUtils::getChildNode(node, "ReportConfiguration")) {
        if (auto tmp2 = XMLUtils::getChildNode(tmp, "EquityVolatilities")) {
            if (auto tmp3 = XMLUtils::getChildNode(tmp2, "Report"))
                reportConfigEqVols_.fromXML(tmp3);
        }
        if (auto tmp2 = XMLUtils::getChildNode(tmp, "FXVolatilities")) {
            if (auto tmp3 = XMLUtils::getChildNode(tmp2, "Report"))
                reportConfigFxVols_.fromXML(tmp3);
        }
        if (auto tmp2 = XMLUtils::getChildNode(tmp, "CommodityVolatilities")) {
            if (auto tmp3 = XMLUtils::getChildNode(tmp2, "Report"))
                reportConfigCommVols_.fromXML(tmp3);
        }
        if (auto tmp2 = XMLUtils::getChildNode(tmp, "IRCapFloorVolatilities")) {
            if (auto tmp3 = XMLUtils::getChildNode(tmp2, "Report"))
                reportConfigIrCapFloorVols_.fromXML(tmp3);
        }
        if (auto tmp2 = XMLUtils::getChildNode(tmp, "IRSwaptionVolatilities")) {
            if (auto tmp3 = XMLUtils::getChildNode(tmp2, "Report"))
                reportConfigIrSwaptionVols_.fromXML(tmp3);
        }
    }

    if (auto tmp = XMLUtils::getChildNode(node, "SmileDynamics")) {
      LOG("smile dynamics node found");
      smileDynamicsConfig_.fromXML(tmp);
    }
    else {
      WLOG("smile dynamics node not found in curve config, using default values");
    }
      
    
    // Load YieldCurves, FXVols, etc, etc
    getNode(node, "YieldCurves", "YieldCurve");
    getNode(node, "FXVolatilities", "FXVolatility");
    getNode(node, "SwaptionVolatilities", "SwaptionVolatility");
    getNode(node, "YieldVolatilities", "YieldVolatility");
    getNode(node, "CapFloorVolatilities", "CapFloorVolatility");
    getNode(node, "DefaultCurves", "DefaultCurve");
    getNode(node, "CDSVolatilities", "CDSVolatility");
    getNode(node, "BaseCorrelations", "BaseCorrelation");
    getNode(node, "EquityCurves", "EquityCurve");
    getNode(node, "EquityVolatilities", "EquityVolatility");
    getNode(node, "InflationCurves", "InflationCurve");
    getNode(node, "InflationCapFloorVolatilities", "InflationCapFloorVolatility");
    getNode(node, "Securities", "Security");
    getNode(node, "FXSpots", "FXSpot");
    getNode(node, "CommodityCurves", "CommodityCurve");
    getNode(node, "CommodityVolatilities", "CommodityVolatility");
    getNode(node, "Correlations", "Correlation");
}

XMLNode* CurveConfigurations::toXML(XMLDocument& doc) {
    XMLNode* parent = doc.allocNode("CurveConfiguration");

    addNodes(doc, parent, "FXSpots");
    addNodes(doc, parent, "FXVolatilities");
    addNodes(doc, parent, "SwaptionVolatilities");
    addNodes(doc, parent, "YieldVolatilities");
    addNodes(doc, parent, "CapFloorVolatilities");
    addNodes(doc, parent, "CDSVolatilities");
    addNodes(doc, parent, "DefaultCurves");
    addNodes(doc, parent, "YieldCurves");
    addNodes(doc, parent, "InflationCurves");
    addNodes(doc, parent, "InflationCapFloorVolatilities");
    addNodes(doc, parent, "EquityCurves");
    addNodes(doc, parent, "EquityVolatilities");
    addNodes(doc, parent, "Securities");
    addNodes(doc, parent, "BaseCorrelations");
    addNodes(doc, parent, "CommodityCurves");
    addNodes(doc, parent, "CommodityVolatilities");
    addNodes(doc, parent, "Correlations");

    return parent;
}
} // namespace data
} // namespace ore
