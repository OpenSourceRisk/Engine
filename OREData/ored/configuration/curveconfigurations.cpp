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

#include <ql/errors.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

// check if map has an entry for the given id
template <class T> bool has(const string& id, const map<string, boost::shared_ptr<T>>& m) { return m.count(id) == 1; }

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

template <class T>
void addNodes(XMLDocument& doc, XMLNode* parent, const char* nodeName, const map<string, boost::shared_ptr<T>>& m) {
    XMLNode* node = doc.allocNode(nodeName);
    XMLUtils::appendNode(parent, node);
    for (auto it : m)
        XMLUtils::appendNode(node, it.second->toXML(doc));
}

// Utility function for constructing the set of quotes needed by CurveConfigs
// Used in the quotes(...) method
template <class T>
void addQuotes(set<string>& quotes, const map<string, boost::shared_ptr<T>>& configs, CurveSpec::CurveType curveType) {
    // For each config in configs, add its quotes to the set
    for (auto m : configs) {
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    }
}

template <class T>
void CurveConfigurations::parseNode(XMLNode* node, const char* parentName, const char* childName,
                                    map<string, boost::shared_ptr<T>>& m) {

    XMLNode* parentNode = XMLUtils::getChildNode(node, parentName);
    if (parentNode) {
        for (XMLNode* child = XMLUtils::getChildNode(parentNode, childName); child;
             child = XMLUtils::getNextSibling(child, childName)) {
            boost::shared_ptr<T> curveConfig(new T());
            try {
                curveConfig->fromXML(child);
                const string& id = curveConfig->curveID();
                m[id] = curveConfig;
                DLOG("Added curve config with ID = " << id);
            } catch (std::exception& ex) {
                string id = "(unknown curve id)";
                try {
                    // try to get the curve id for which an error was thrown
                    id = XMLUtils::getChildValue(child, "CurveId", true);
                    // if we got it, store the error, so that get() can include that information in its error message
                    parseErrors_[std::make_pair(std::type_index(typeid(T)), id)] =
                        std::make_pair(string(parentName), ex.what());
                } catch (...) {
                }
                ALOG("Exception parsing curve config under node '" << parentName << "' for curve id '" << id
                                                                   << "': " << ex.what());
            }
        }
    }
}

template <class T>
const boost::shared_ptr<T>& CurveConfigurations::get(const string& id,
                                                     const map<string, boost::shared_ptr<T>>& m) const {
    auto it = m.find(id);
    if (it != m.end())
        return it->second;
    auto err = parseErrors_.find(std::make_pair(std::type_index(typeid(T)), id));
    if (err != parseErrors_.end()) {
        QL_FAIL("no curve id for '" << id << "' under node '" << err->second.first
                                    << "' due to parser error: " << err->second.second);
    } else {
        QL_FAIL("no curve id for '" << id << "', is the id present in the curve configuration?");
    }
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

    // Populate the set of quotes that will be returned
    set<string> quotes;
    addQuotes(quotes, yieldCurveConfigs_, CurveSpec::CurveType::Yield);
    addQuotes(quotes, fxVolCurveConfigs_, CurveSpec::CurveType::FXVolatility);
    addQuotes(quotes, swaptionVolCurveConfigs_, CurveSpec::CurveType::SwaptionVolatility);
    addQuotes(quotes, yieldVolCurveConfigs_, CurveSpec::CurveType::YieldVolatility);
    addQuotes(quotes, capFloorVolCurveConfigs_, CurveSpec::CurveType::CapFloorVolatility);
    addQuotes(quotes, defaultCurveConfigs_, CurveSpec::CurveType::Default);
    addQuotes(quotes, cdsVolCurveConfigs_, CurveSpec::CurveType::CDSVolatility);
    addQuotes(quotes, baseCorrelationCurveConfigs_, CurveSpec::CurveType::BaseCorrelation);
    addQuotes(quotes, inflationCurveConfigs_, CurveSpec::CurveType::Inflation);
    addQuotes(quotes, inflationCapFloorVolCurveConfigs_, CurveSpec::CurveType::InflationCapFloorVolatility);
    addQuotes(quotes, equityCurveConfigs_, CurveSpec::CurveType::Equity);
    addQuotes(quotes, equityVolCurveConfigs_, CurveSpec::CurveType::EquityVolatility);
    addQuotes(quotes, securityConfigs_, CurveSpec::CurveType::Security);
    addQuotes(quotes, fxSpotConfigs_, CurveSpec::CurveType::FX);
    addQuotes(quotes, commodityCurveConfigs_, CurveSpec::CurveType::Commodity);
    addQuotes(quotes, commodityVolatilityConfigs_, CurveSpec::CurveType::CommodityVolatility);
    addQuotes(quotes, correlationCurveConfigs_, CurveSpec::CurveType::Correlation);

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
        if (d.second->conventionID() != "")
            conventions.insert(d.second->conventionID());
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
    }

    // Load YieldCurves, FXVols, etc, etc
    parseNode(node, "YieldCurves", "YieldCurve", yieldCurveConfigs_);
    parseNode(node, "FXVolatilities", "FXVolatility", fxVolCurveConfigs_);
    parseNode(node, "SwaptionVolatilities", "SwaptionVolatility", swaptionVolCurveConfigs_);
    parseNode(node, "YieldVolatilities", "YieldVolatility", yieldVolCurveConfigs_);
    parseNode(node, "CapFloorVolatilities", "CapFloorVolatility", capFloorVolCurveConfigs_);
    parseNode(node, "DefaultCurves", "DefaultCurve", defaultCurveConfigs_);
    parseNode(node, "CDSVolatilities", "CDSVolatility", cdsVolCurveConfigs_);
    parseNode(node, "BaseCorrelations", "BaseCorrelation", baseCorrelationCurveConfigs_);
    parseNode(node, "EquityCurves", "EquityCurve", equityCurveConfigs_);
    parseNode(node, "EquityVolatilities", "EquityVolatility", equityVolCurveConfigs_);
    parseNode(node, "InflationCurves", "InflationCurve", inflationCurveConfigs_);
    parseNode(node, "InflationCapFloorVolatilities", "InflationCapFloorVolatility", inflationCapFloorVolCurveConfigs_);
    parseNode(node, "Securities", "Security", securityConfigs_);
    parseNode(node, "FXSpots", "FXSpot", fxSpotConfigs_);
    parseNode(node, "CommodityCurves", "CommodityCurve", commodityCurveConfigs_);
    parseNode(node, "CommodityVolatilities", "CommodityVolatility", commodityVolatilityConfigs_);
    parseNode(node, "Correlations", "Correlation", correlationCurveConfigs_);
}

XMLNode* CurveConfigurations::toXML(XMLDocument& doc) {
    XMLNode* parent = doc.allocNode("CurveConfiguration");

    addNodes(doc, parent, "FXSpots", fxSpotConfigs_);
    addNodes(doc, parent, "FXVolatilities", fxVolCurveConfigs_);
    addNodes(doc, parent, "SwaptionVolatilities", swaptionVolCurveConfigs_);
    addNodes(doc, parent, "YieldVolatilities", yieldVolCurveConfigs_);
    addNodes(doc, parent, "CapFloorVolatilities", capFloorVolCurveConfigs_);
    addNodes(doc, parent, "CDSVolatilities", cdsVolCurveConfigs_);
    addNodes(doc, parent, "DefaultCurves", defaultCurveConfigs_);
    addNodes(doc, parent, "YieldCurves", yieldCurveConfigs_);
    addNodes(doc, parent, "InflationCurves", inflationCurveConfigs_);
    addNodes(doc, parent, "InflationCapFloorVolatilities", inflationCapFloorVolCurveConfigs_);
    addNodes(doc, parent, "EquityCurves", equityCurveConfigs_);
    addNodes(doc, parent, "EquityVolatilities", equityVolCurveConfigs_);
    addNodes(doc, parent, "Securities", securityConfigs_);
    addNodes(doc, parent, "BaseCorrelations", baseCorrelationCurveConfigs_);
    addNodes(doc, parent, "CommodityCurves", commodityCurveConfigs_);
    addNodes(doc, parent, "CommodityVolatilities", commodityVolatilityConfigs_);
    addNodes(doc, parent, "Correlations", correlationCurveConfigs_);

    return parent;
}
} // namespace data
} // namespace ore
