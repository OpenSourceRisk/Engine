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
#include <ored/utilities/log.hpp>
#include <ored/marketdata/curvespecparser.hpp>

#include <ql/errors.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

// utility function for getting a value from a map, throwing if it is not present
template <class T> const boost::shared_ptr<T>& get(const string& id, const map<string, boost::shared_ptr<T>>& m) {
    auto it = m.find(id);
    QL_REQUIRE(it != m.end(), "no curve id for " << id);
    return it->second;
}

// utility function for parsing a node of name "parentName" and decoding all
// child elements, storing the resulting config in the map
template <class T>
void parseNode(XMLNode* node, const char* parentName, const char* childName, map<string, boost::shared_ptr<T>>& m) {

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
                ALOG("Exception parsing curve config: " << ex.what());
            }
        }
    }
}

// utility function to add a set of nodes from a given map of curve configs
template <class T>
void addNodes(XMLDocument& doc, XMLNode* parent, const char* nodeName, map<string, boost::shared_ptr<T>>& m) {
    XMLNode* node = doc.allocNode(nodeName);
    XMLUtils::appendNode(parent, node);
    for (auto it : m)
        XMLUtils::appendNode(node, it.second->toXML(doc));
}

// Utility function for constructing the set of quotes needed by CurveConfigs
// Used in the quotes(...) method
template <class T>
void addQuotes(set<string>& quotes, const map<string, boost::shared_ptr<T>>& configs, bool insertAll, 
    CurveSpec::CurveType curveType, const map<CurveSpec::CurveType, set<string>>& configIds) {

    // For each config in configs, add its quotes to the set if the config's id is in the map configIds
    for (auto m : configs) {
        if (insertAll || (configIds.count(curveType) && configIds.at(curveType).count(m.second->curveID()))) {
            quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
        }
    }
}

std::set<string> CurveConfigurations::quotes(boost::shared_ptr<const TodaysMarketParameters> todaysMarketParams,
    const set<string>& configurations) const {
    
    // If tmparams is not null, organise its specs in to a map [CurveType, set of CurveConfigID]
    map<CurveSpec::CurveType, set<string>> curveConfigIds;
    // This set of FXSpotSpec is used below
    set<boost::shared_ptr<FXSpotSpec>> fxSpotSpecs;
    
    if (todaysMarketParams) {
        for (const auto& config : configurations) {
            for (const auto& strSpec : todaysMarketParams->curveSpecs(config)) {
                auto spec = parseCurveSpec(strSpec);
                if (curveConfigIds.count(spec->baseType())) {
                    curveConfigIds[spec->baseType()].insert(spec->curveConfigID());
                } else {
                    curveConfigIds[spec->baseType()] = { spec->curveConfigID() };
                }

                if (spec->baseType() == CurveSpec::CurveType::FX) {
                    boost::shared_ptr<FXSpotSpec> fxss = boost::dynamic_pointer_cast<FXSpotSpec>(spec);
                    QL_REQUIRE(fxss, "Expected an FXSpotSpec but did not get one");
                    fxSpotSpecs.insert(fxss);
                }
            }
        }
    }

    // Populate the set of quotes that will be returned
    set<string> quotes;
    bool insertAll = !todaysMarketParams;
    addQuotes(quotes, yieldCurveConfigs_, insertAll, CurveSpec::CurveType::Yield, curveConfigIds);
    addQuotes(quotes, fxVolCurveConfigs_, insertAll, CurveSpec::CurveType::FXVolatility, curveConfigIds);
    addQuotes(quotes, swaptionVolCurveConfigs_, insertAll, CurveSpec::CurveType::SwaptionVolatility, curveConfigIds);
    addQuotes(quotes, capFloorVolCurveConfigs_, insertAll, CurveSpec::CurveType::CapFloorVolatility, curveConfigIds);
    addQuotes(quotes, defaultCurveConfigs_, insertAll, CurveSpec::CurveType::Default, curveConfigIds);
    addQuotes(quotes, cdsVolCurveConfigs_, insertAll, CurveSpec::CurveType::CDSVolatility, curveConfigIds);
    addQuotes(quotes, baseCorrelationCurveConfigs_, insertAll, CurveSpec::CurveType::BaseCorrelation, curveConfigIds);
    addQuotes(quotes, inflationCurveConfigs_, insertAll, CurveSpec::CurveType::Inflation, curveConfigIds);
    addQuotes(quotes, inflationCapFloorPriceSurfaceConfigs_, insertAll, CurveSpec::CurveType::InflationCapFloorPrice, curveConfigIds);
    addQuotes(quotes, inflationCapFloorVolCurveConfigs_, insertAll, CurveSpec::CurveType::InflationCapFloorVolatility, curveConfigIds);
    addQuotes(quotes, equityCurveConfigs_, insertAll, CurveSpec::CurveType::Equity, curveConfigIds);
    addQuotes(quotes, equityVolCurveConfigs_, insertAll, CurveSpec::CurveType::EquityVolatility, curveConfigIds);
    addQuotes(quotes, securityConfigs_, insertAll, CurveSpec::CurveType::Security, curveConfigIds);
    addQuotes(quotes, fxSpotConfigs_, insertAll, CurveSpec::CurveType::FX, curveConfigIds);
    addQuotes(quotes, commodityCurveConfigs_, insertAll, CurveSpec::CurveType::Commodity, curveConfigIds);
    addQuotes(quotes, commodityVolatilityCurveConfigs_, insertAll, CurveSpec::CurveType::CommodityVolatility, curveConfigIds);

    // FX spot is special in that we generally do not enter a curve configuration for it. Above, we ran over the 
    // curve configurations asking each for its quotes. We may end up missing FX spot quotes that are specified in a 
    // TodaysMarketParameters but do not have a CurveConfig. If we have a TodaysMarketParameters instance we can add 
    // them here directly using it.
    for (const auto& fxss : fxSpotSpecs) {
        string strQuote = "FX/RATE/" + fxss->unitCcy() + "/" + fxss->ccy();
        quotes.insert(strQuote);
    }

    return quotes;
}
const boost::shared_ptr<YieldCurveConfig>& CurveConfigurations::yieldCurveConfig(const string& curveID) const {
    return get(curveID, yieldCurveConfigs_);
}

const boost::shared_ptr<FXVolatilityCurveConfig>& CurveConfigurations::fxVolCurveConfig(const string& curveID) const {
    return get(curveID, fxVolCurveConfigs_);
}

const boost::shared_ptr<SwaptionVolatilityCurveConfig>&
CurveConfigurations::swaptionVolCurveConfig(const string& curveID) const {
    return get(curveID, swaptionVolCurveConfigs_);
}

const boost::shared_ptr<CapFloorVolatilityCurveConfig>&
CurveConfigurations::capFloorVolCurveConfig(const string& curveID) const {
    return get(curveID, capFloorVolCurveConfigs_);
}

const boost::shared_ptr<DefaultCurveConfig>& CurveConfigurations::defaultCurveConfig(const string& curveID) const {
    return get(curveID, defaultCurveConfigs_);
}

const boost::shared_ptr<CDSVolatilityCurveConfig>& CurveConfigurations::cdsVolCurveConfig(const string& curveID) const {
    return get(curveID, cdsVolCurveConfigs_);
}

const boost::shared_ptr<BaseCorrelationCurveConfig>&
CurveConfigurations::baseCorrelationCurveConfig(const string& curveID) const {
    return get(curveID, baseCorrelationCurveConfigs_);
}

const boost::shared_ptr<InflationCurveConfig>& CurveConfigurations::inflationCurveConfig(const string& curveID) const {
    return get(curveID, inflationCurveConfigs_);
}

const boost::shared_ptr<InflationCapFloorPriceSurfaceConfig>&
CurveConfigurations::inflationCapFloorPriceSurfaceConfig(const string& curveID) const {
    return get(curveID, inflationCapFloorPriceSurfaceConfigs_);
}

const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>&
CurveConfigurations::inflationCapFloorVolCurveConfig(const string& curveID) const {
    return get(curveID, inflationCapFloorVolCurveConfigs_);
}

const boost::shared_ptr<EquityCurveConfig>& CurveConfigurations::equityCurveConfig(const string& curveID) const {
    return get(curveID, equityCurveConfigs_);
}

const boost::shared_ptr<EquityVolatilityCurveConfig>&
CurveConfigurations::equityVolCurveConfig(const string& curveID) const {
    return get(curveID, equityVolCurveConfigs_);
}

const boost::shared_ptr<SecurityConfig>& CurveConfigurations::securityConfig(const string& curveID) const {
    return get(curveID, securityConfigs_);
}

const boost::shared_ptr<FXSpotConfig>& CurveConfigurations::fxSpotConfig(const string& curveID) const {
    return get(curveID, fxSpotConfigs_);
}

const boost::shared_ptr<CommodityCurveConfig>& CurveConfigurations::commodityCurveConfig(const string& curveID) const {
    return get(curveID, commodityCurveConfigs_);
}

const boost::shared_ptr<CommodityVolatilityCurveConfig>& CurveConfigurations::commodityVolatilityCurveConfig(const string& curveID) const {
    return get(curveID, commodityVolatilityCurveConfigs_);
}

void CurveConfigurations::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CurveConfiguration");

    // Load YieldCurves, FXVols, etc, etc
    parseNode(node, "YieldCurves", "YieldCurve", yieldCurveConfigs_);
    parseNode(node, "FXVolatilities", "FXVolatility", fxVolCurveConfigs_);
    parseNode(node, "SwaptionVolatilities", "SwaptionVolatility", swaptionVolCurveConfigs_);
    parseNode(node, "CapFloorVolatilities", "CapFloorVolatility", capFloorVolCurveConfigs_);
    parseNode(node, "DefaultCurves", "DefaultCurve", defaultCurveConfigs_);
    parseNode(node, "CDSVolatilities", "CDSVolatility", cdsVolCurveConfigs_);
    parseNode(node, "BaseCorrelations", "BaseCorrelation", baseCorrelationCurveConfigs_);
    parseNode(node, "EquityCurves", "EquityCurve", equityCurveConfigs_);
    parseNode(node, "EquityVolatilities", "EquityVolatility", equityVolCurveConfigs_);
    parseNode(node, "InflationCurves", "InflationCurve", inflationCurveConfigs_);
    parseNode(node, "InflationCapFloorPriceSurfaces", "InflationCapFloorPriceSurface",
              inflationCapFloorPriceSurfaceConfigs_);
    parseNode(node, "InflationCapFloorVolatilities", "InflationCapFloorVolatility",
              inflationCapFloorVolCurveConfigs_);
    parseNode(node, "Securities", "Security", securityConfigs_);
    parseNode(node, "FXSpots", "FXSpot", fxSpotConfigs_);
    parseNode(node, "CommodityCurves", "CommodityCurve", commodityCurveConfigs_);
    parseNode(node, "CommodityVolatilities", "CommodityVolatility", commodityVolatilityCurveConfigs_);
}

XMLNode* CurveConfigurations::toXML(XMLDocument& doc) {
    XMLNode* parent = doc.allocNode("CurveConfiguration");
    doc.appendNode(parent);

    addNodes(doc, parent, "YieldCurves", yieldCurveConfigs_);
    addNodes(doc, parent, "FXVolatilities", fxVolCurveConfigs_);
    addNodes(doc, parent, "SwaptionVolatilities", swaptionVolCurveConfigs_);
    addNodes(doc, parent, "CapFloorVolatilities", capFloorVolCurveConfigs_);
    addNodes(doc, parent, "DefaultCurves", defaultCurveConfigs_);
    addNodes(doc, parent, "CDSVolatilities", cdsVolCurveConfigs_);
    addNodes(doc, parent, "BaseCorrelations", baseCorrelationCurveConfigs_);
    addNodes(doc, parent, "EquityCurves", equityCurveConfigs_);
    addNodes(doc, parent, "EquityVolatilities", equityCurveConfigs_);
    addNodes(doc, parent, "InflationCurves", inflationCurveConfigs_);
    addNodes(doc, parent, "InflationCapFloorPriceSurfaces", inflationCapFloorPriceSurfaceConfigs_);
    addNodes(doc, parent, "InflationCapFloorVolatilities", inflationCapFloorVolCurveConfigs_);
    addNodes(doc, parent, "Securities", securityConfigs_);
    addNodes(doc, parent, "FXSpots", fxSpotConfigs_);
    addNodes(doc, parent, "CommodityCurves", commodityCurveConfigs_);
    addNodes(doc, parent, "CommodityVolatilities", commodityVolatilityCurveConfigs_);

    return parent;
}
} // namespace data
} // namespace ore
