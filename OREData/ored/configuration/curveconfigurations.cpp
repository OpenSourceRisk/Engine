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
void addMinimalCurves(const char* nodeName, const map<string, QuantLib::ext::shared_ptr<T>>& m,
                      map<string, QuantLib::ext::shared_ptr<T>>& n, CurveSpec::CurveType curveType,
                      const map<CurveSpec::CurveType, set<string>> configIds) {
    for (auto it : m) {
        if ((configIds.count(curveType) && configIds.at(curveType).count(it.first))) {
            const string& id = it.second->curveID();
            n[id] = it.second;
        }
    }
}

void CurveConfigurations::addNodes(XMLDocument& doc, XMLNode* parent, const char* nodeName) const {
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

void CurveConfigurations::parseNode(const CurveSpec::CurveType& type, const string& curveId) const {
    QuantLib::ext::shared_ptr<CurveConfig> config;
    const auto& it = unparsed_.find(type);
    if (it != unparsed_.end()) {
        const auto& itc = it->second.find(curveId);
        if (itc != it->second.end()) {
            switch (type) {
            case CurveSpec::CurveType::Yield: {
                config = QuantLib::ext::make_shared<YieldCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Default: {
                config = QuantLib::ext::make_shared<DefaultCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::CDSVolatility: {
                config = QuantLib::ext::make_shared<CDSVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::BaseCorrelation: {
                config = QuantLib::ext::make_shared<BaseCorrelationCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::FX: {
                config = QuantLib::ext::make_shared<FXSpotConfig>();
                break;
            }
            case CurveSpec::CurveType::FXVolatility: {
                config = QuantLib::ext::make_shared<FXVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::SwaptionVolatility: {
                config = QuantLib::ext::make_shared<SwaptionVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::YieldVolatility: {
                config = QuantLib::ext::make_shared<YieldVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::CapFloorVolatility: {
                config = QuantLib::ext::make_shared<CapFloorVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Inflation: {
                config = QuantLib::ext::make_shared<InflationCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::InflationCapFloorVolatility: {
                config = QuantLib::ext::make_shared<InflationCapFloorVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Equity: {
                config = QuantLib::ext::make_shared<EquityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::EquityVolatility: {
                config = QuantLib::ext::make_shared<EquityVolatilityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::Security: {
                config = QuantLib::ext::make_shared<SecurityConfig>();
                break;
            }
            case CurveSpec::CurveType::Commodity: {
                config = QuantLib::ext::make_shared<CommodityCurveConfig>();
                break;
            }
            case CurveSpec::CurveType::CommodityVolatility: {
                config = QuantLib::ext::make_shared<CommodityVolatilityConfig>();
                break;
            }
            case CurveSpec::CurveType::Correlation: {
                config = QuantLib::ext::make_shared<CorrelationCurveConfig>();
                break;
            }
            }
            try {
                config->fromXMLString(itc->second);
                configs_[type][curveId] = config;
                unparsed_.at(type).erase(curveId);
            } catch (std::exception& ex) {
                string err = "Curve config under node '" + to_string(type) + " was requested, but could not be parsed.";
                StructuredCurveErrorMessage(curveId, err, ex.what()).log();
                QL_FAIL(err);
            }
        } else
            QL_FAIL("Could not find curveId " << curveId << " of type " << type << " in unparsed curve configurations");
    } else
        QL_FAIL("Could not find CurveType " << type << " in unparsed curve configurations");
}

void CurveConfigurations::add(const CurveSpec::CurveType& type, const string& curveId,
    const QuantLib::ext::shared_ptr<CurveConfig>& config) {
    configs_[type][curveId] = config;
}

bool CurveConfigurations::has(const CurveSpec::CurveType& type, const string& curveId) const {
    return (configs_.count(type) > 0 && configs_.at(type).count(curveId) > 0) ||
           (unparsed_.count(type) > 0 && unparsed_.at(type).count(curveId) > 0);
}

const QuantLib::ext::shared_ptr<CurveConfig>& CurveConfigurations::get(const CurveSpec::CurveType& type,
    const string& curveId) const {
    const auto& it = configs_.find(type);
    if (it != configs_.end()) {
        const auto& itc = it->second.find(curveId);
        if (itc != it->second.end()) {
            return itc->second;
        }
    }
    parseNode(type, curveId);
    return configs_.at(type).at(curveId);
}

void CurveConfigurations::parseAll() {
    for (const auto& u : unparsed_) {
        for (auto it = u.second.cbegin(), nit = it; it != u.second.cend(); it = nit) {
            nit++;
            parseNode(u.first, it->first);
        }
    }
}

void CurveConfigurations::getNode(XMLNode* node, const char* parentName, const char* childName) {
    const auto& type = parseCurveConfigurationType(parentName);
    XMLNode* parentNode = XMLUtils::getChildNode(node, parentName);
    if (parentNode) {
        for (XMLNode* child = XMLUtils::getChildNode(parentNode, childName); child;
             child = XMLUtils::getNextSibling(child, childName)) {
            const auto& id = XMLUtils::getChildValue(child, "CurveId", true);
            unparsed_[type][id] = XMLUtils::toString(child);
        }
    }
}

QuantLib::ext::shared_ptr<CurveConfigurations>
CurveConfigurations::minimalCurveConfig(const QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                        const set<string>& configurations) const {

    QuantLib::ext::shared_ptr<CurveConfigurations> minimum = QuantLib::ext::make_shared<CurveConfigurations>();
    // If tmparams is not null, organise its specs in to a map [CurveType, set of CurveConfigID]
    map<CurveSpec::CurveType, set<string>> curveConfigIds;

    for (const auto& config : configurations) {
        for (const auto& strSpec : todaysMarketParams->curveSpecs(config)) {
            auto spec = parseCurveSpec(strSpec);
            if (curveConfigIds.count(spec->baseType()))
                curveConfigIds[spec->baseType()].insert(spec->curveConfigID());
            else
                curveConfigIds[spec->baseType()] = {spec->curveConfigID()};
        }
    }

    for (const auto& it : curveConfigIds) {
        for (auto& c : it.second) {
            try {
                auto cc = get(it.first, c);
                minimum->add(it.first, c, get(it.first, c));
            } catch (...) {
            }
        }
    }
    return minimum;
}

std::set<string> CurveConfigurations::quotes(const QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
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
                QuantLib::ext::shared_ptr<FXSpotSpec> fxss = QuantLib::ext::dynamic_pointer_cast<FXSpotSpec>(spec);
                QL_REQUIRE(fxss, "Expected an FXSpotSpec but did not get one");
                quotes.insert("FX/RATE/" + fxss->unitCcy() + "/" + fxss->ccy());
                quotes.insert("FX/RATE/" + fxss->ccy() + "/" + fxss->unitCcy());
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

std::set<string> CurveConfigurations::conventions(const QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                                  const set<string>& configurations) const {
    set<string> conventions = minimalCurveConfig(todaysMarketParams, configurations)->conventions();

    // Checking for any swapIndices
    if (todaysMarketParams->hasMarketObject(MarketObject::SwapIndexCurve)) {
        for (const auto& m : todaysMarketParams->mapping(MarketObject::SwapIndexCurve, Market::defaultConfiguration)) {
            conventions.insert(m.first);
        }
    }

    return conventions;
}

std::set<string> CurveConfigurations::conventions() const {
    set<string> conventions;
    for (const auto& cc : configs_) {
        if (cc.first == CurveSpec::CurveType::Yield) {
            for (const auto& c : cc.second) {
                auto ycc = QuantLib::ext::dynamic_pointer_cast<YieldCurveConfig>(c.second);
                if (ycc) {
                    for (auto& s : ycc->curveSegments())
                        if (!s->conventionsID().empty())
                            conventions.insert(s->conventionsID());
                }
            }
        }

        if (cc.first == CurveSpec::CurveType::Default) {
            for (const auto& c : cc.second) {
                auto dcc = QuantLib::ext::dynamic_pointer_cast<DefaultCurveConfig>(c.second);
                if (dcc) {
                    for (auto& s : dcc->configs()) {
                        if (s.second.conventionID() != "")
                            conventions.insert(s.second.conventionID());
                    }
                }
            }
        }

        if (cc.first == CurveSpec::CurveType::Inflation) {
            for (const auto& c : cc.second) {
                auto icc = QuantLib::ext::dynamic_pointer_cast<InflationCurveConfig>(c.second);
                if (icc) {
                    if (icc->conventions() != "")
                        conventions.insert(icc->conventions());
                }
            }
        }

        if (cc.first == CurveSpec::CurveType::Correlation) {
            for (const auto& c : cc.second) {
                auto ccc = QuantLib::ext::dynamic_pointer_cast<CorrelationCurveConfig>(c.second);
                if (ccc) {
                    if (ccc->conventions() != "")
                        conventions.insert(ccc->conventions());
                }
            }
        }

        if (cc.first == CurveSpec::CurveType::FXVolatility) {
            for (const auto& c : cc.second) {
                auto fcc = QuantLib::ext::dynamic_pointer_cast<FXVolatilityCurveConfig>(c.second);
                if (fcc) {
                    if (fcc->conventionsID() != "")
                        conventions.insert(fcc->conventionsID());
                }
            }
        }
    }
    return conventions;
}

set<string> CurveConfigurations::yieldCurveConfigIds() {
    set<string> curves;
    const auto& it = configs_.find(CurveSpec::CurveType::Yield);
    if (it != configs_.end()) {
        for (const auto& c : it->second)
            curves.insert(c.first);
    }

    const auto& itu = unparsed_.find(CurveSpec::CurveType::Yield);
    if (itu != unparsed_.end()) {
        for (const auto& c : itu->second)
            curves.insert(c.first);
    }

    return curves;
}

std::map<CurveSpec::CurveType, std::set<string>>
CurveConfigurations::requiredCurveIds(const CurveSpec::CurveType& type, const std::string& curveId) const {
    QuantLib::ext::shared_ptr<CurveConfig> cc;
    std::map<CurveSpec::CurveType, std::set<string>> ids;
    if (!curveId.empty())
        try {
            cc = get(type, curveId);
        } catch (...) {
        }
    if (cc)
        ids = cc->requiredCurveIds();
    return ids;
}

bool CurveConfigurations::hasYieldCurveConfig(const string& curveID) const { return has(CurveSpec::CurveType::Yield, curveID); }
QuantLib::ext::shared_ptr<YieldCurveConfig> CurveConfigurations::yieldCurveConfig(const string& curveID) const {
    QuantLib::ext::shared_ptr<CurveConfig> cc = get(CurveSpec::CurveType::Yield, curveID);
    return QuantLib::ext::dynamic_pointer_cast<YieldCurveConfig>(cc);
}

bool CurveConfigurations::hasFxVolCurveConfig(const string& curveID) const { 
    return has(CurveSpec::CurveType::FXVolatility, curveID); 
}

QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> CurveConfigurations::fxVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::FXVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<FXVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasSwaptionVolCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::SwaptionVolatility, curveID);
}

QuantLib::ext::shared_ptr<SwaptionVolatilityCurveConfig>
CurveConfigurations::swaptionVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::SwaptionVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<SwaptionVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasYieldVolCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::YieldVolatility, curveID);
}

QuantLib::ext::shared_ptr<YieldVolatilityCurveConfig>
CurveConfigurations::yieldVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::YieldVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<YieldVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasCapFloorVolCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::CapFloorVolatility, curveID);
}

QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig>
CurveConfigurations::capFloorVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::CapFloorVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<CapFloorVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasDefaultCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::Default, curveID);
}

QuantLib::ext::shared_ptr<DefaultCurveConfig> CurveConfigurations::defaultCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::Default, curveID);
    return QuantLib::ext::dynamic_pointer_cast<DefaultCurveConfig>(cc);
}

bool CurveConfigurations::hasCdsVolCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::CDSVolatility, curveID);
}

QuantLib::ext::shared_ptr<CDSVolatilityCurveConfig> CurveConfigurations::cdsVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::CDSVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<CDSVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasBaseCorrelationCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::BaseCorrelation, curveID);
}

QuantLib::ext::shared_ptr<BaseCorrelationCurveConfig>
CurveConfigurations::baseCorrelationCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::BaseCorrelation, curveID);
    return QuantLib::ext::dynamic_pointer_cast<BaseCorrelationCurveConfig>(cc);
}

bool CurveConfigurations::hasInflationCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::Inflation, curveID);
}

QuantLib::ext::shared_ptr<InflationCurveConfig> CurveConfigurations::inflationCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::Inflation, curveID);
    return QuantLib::ext::dynamic_pointer_cast<InflationCurveConfig>(cc);
}

bool CurveConfigurations::hasInflationCapFloorVolCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::InflationCapFloorVolatility, curveID);
}

QuantLib::ext::shared_ptr<InflationCapFloorVolatilityCurveConfig>
CurveConfigurations::inflationCapFloorVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::InflationCapFloorVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<InflationCapFloorVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasEquityCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::Equity, curveID);
}

QuantLib::ext::shared_ptr<EquityCurveConfig> CurveConfigurations::equityCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::Equity, curveID);
    return QuantLib::ext::dynamic_pointer_cast<EquityCurveConfig>(cc);
}

bool CurveConfigurations::hasEquityVolCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::EquityVolatility, curveID);
}

QuantLib::ext::shared_ptr<EquityVolatilityCurveConfig>
CurveConfigurations::equityVolCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::EquityVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<EquityVolatilityCurveConfig>(cc);
}

bool CurveConfigurations::hasSecurityConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::Security, curveID);
}

QuantLib::ext::shared_ptr<SecurityConfig> CurveConfigurations::securityConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::Security, curveID);
    return QuantLib::ext::dynamic_pointer_cast<SecurityConfig>(cc);
}

bool CurveConfigurations::hasFxSpotConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::FX, curveID);
}

QuantLib::ext::shared_ptr<FXSpotConfig> CurveConfigurations::fxSpotConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::FX, curveID);
    return QuantLib::ext::dynamic_pointer_cast<FXSpotConfig>(cc);
}

bool CurveConfigurations::hasCommodityCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::Commodity, curveID);
}

QuantLib::ext::shared_ptr<CommodityCurveConfig> CurveConfigurations::commodityCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::Commodity, curveID);
    return QuantLib::ext::dynamic_pointer_cast<CommodityCurveConfig>(cc);
}

bool CurveConfigurations::hasCommodityVolatilityConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::CommodityVolatility, curveID);
}

QuantLib::ext::shared_ptr<CommodityVolatilityConfig>
CurveConfigurations::commodityVolatilityConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::CommodityVolatility, curveID);
    return QuantLib::ext::dynamic_pointer_cast<CommodityVolatilityConfig>(cc);
}

bool CurveConfigurations::hasCorrelationCurveConfig(const string& curveID) const {
    return has(CurveSpec::CurveType::Correlation, curveID);
}

QuantLib::ext::shared_ptr<CorrelationCurveConfig>
CurveConfigurations::correlationCurveConfig(const string& curveID) const {
    auto cc = get(CurveSpec::CurveType::Correlation, curveID);
    return QuantLib::ext::dynamic_pointer_cast<CorrelationCurveConfig>(cc);
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

XMLNode* CurveConfigurations::toXML(XMLDocument& doc) const {
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

void CurveConfigurations::addAdditionalCurveConfigs(const CurveConfigurations& c) {

    // add parsed configs

    for (auto const& [curveType, configs] : c.configs_) {
        for (auto const& [name, config] : configs) {
            auto c1 = configs_.find(curveType);
            if (c1 == configs_.end()) {
                configs_[curveType] = {{name, config}};
                continue;
            }
            auto c2 = c1->second.find(name);
            if (c2 == c1->second.end()) {
                c1->second[name] = config;
                continue;
            }
        }
    }

    // add unparsed configs

    for (auto const& [curveType, configs] : c.unparsed_) {
        for (auto const& [name, config] : configs) {
            auto c1 = unparsed_.find(curveType);
            if (c1 == unparsed_.end()) {
                unparsed_[curveType] = {{name, config}};
                continue;
            }
            auto c2 = c1->second.find(name);
            if (c2 == c1->second.end()) {
                c1->second[name] = config;
                continue;
            }
        }
    }
}

void CurveConfigurationsManager::add(const QuantLib::ext::shared_ptr<CurveConfigurations>& config, std::string id) {
    configs_[id] = config;
}

const QuantLib::ext::shared_ptr<CurveConfigurations>& CurveConfigurationsManager::get(std::string id) const {
    auto it = configs_.find(id);
    if (it == configs_.end()) {
        WLOG("CurveConfigurationsManager: could not find CurveConfiguration for id "
             << id << ", attempting to get default curveConfig.");
        it = configs_.find("");
        QL_REQUIRE(it != configs_.end(), "CurveConfigurationsManager: could not find CurveConfiguration for id " << id);
    }
    return it->second;
}

const bool CurveConfigurationsManager::has(std::string id) const { 
    auto it = configs_.find(id); 
    return it != configs_.end();
}

const std::map<std::string, QuantLib::ext::shared_ptr<CurveConfigurations>>& CurveConfigurationsManager::curveConfigurations() const {
    return configs_;
}

const bool CurveConfigurationsManager::empty() const { 
    return configs_.size() == 0; 
}
} // namespace data
} // namespace ore
