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

CurveConfigurations CurveConfigurations::subset(const std::vector<boost::shared_ptr<CurveSpec>>& specs) {
    CurveConfigurations curveConfigs;
    for(const auto& spec : specs) {

        switch (spec->baseType()) {

        case CurveSpec::CurveType::Yield: {
            const boost::shared_ptr<YieldCurveSpec> ycspec = boost::dynamic_pointer_cast<YieldCurveSpec>(spec);
            QL_REQUIRE(ycspec, "Failed to convert spec " << *spec << " to yield curve spec");
            const std::string configId = ycspec->curveConfigID();
            const boost::shared_ptr<YieldCurveConfig> curveConfig = yieldCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find yiled curve " << configId);
            curveConfigs.yieldCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::FX: {
            const boost::shared_ptr<FXSpotSpec> fxspec = boost::dynamic_pointer_cast<FXSpotSpec>(spec);
            QL_REQUIRE(fxspec, "Failed to convert spec " << *spec << " to fx spot spec");
            const std::string configId = fxspec->unitCcy() + fxspec->ccy();
            const boost::shared_ptr<FXSpotConfig> curveConfig(new FXSpotConfig(configId, "")); //  do not extract, just create
            curveConfigs.fxSpotConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::FXVolatility: {
            const boost::shared_ptr<FXVolatilityCurveSpec> fxvolspec = boost::dynamic_pointer_cast<FXVolatilityCurveSpec>(spec);
            QL_REQUIRE(fxvolspec, "Failed to convert spec " << *spec << " to fx vol curve spec");
            const std::string configId = fxvolspec->curveConfigID();
            const boost::shared_ptr<FXVolatilityCurveConfig> curveConfig = fxVolCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find fx vol curve " << configId);
            curveConfigs.fxVolCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::SwaptionVolatility: {
            const boost::shared_ptr<SwaptionVolatilityCurveSpec> swvolspec = 
                boost::dynamic_pointer_cast<SwaptionVolatilityCurveSpec>(spec);
            QL_REQUIRE(swvolspec, "Failed to convert spec " << *spec << " to swaption vol curve spec");
            const std::string configId = swvolspec->curveConfigID();
            const boost::shared_ptr<SwaptionVolatilityCurveConfig> curveConfig = swaptionVolCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find swaption vol curve " );
            curveConfigs.swaptionVolCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::CapFloorVolatility: {
            const boost::shared_ptr<CapFloorVolatilityCurveSpec> cfvolspec = 
                boost::dynamic_pointer_cast<CapFloorVolatilityCurveSpec>(spec);
            QL_REQUIRE(cfvolspec, "Failed to convert spec " << *spec << " to cap floor vol curve spec");
            const std::string configId = cfvolspec->curveConfigID();
            const boost::shared_ptr<CapFloorVolatilityCurveConfig> curveConfig = capFloorVolCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find cap floor vol curve " << configId);
            curveConfigs.capFloorVolCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::Default: {
            const boost::shared_ptr<DefaultCurveSpec> defaultspec = boost::dynamic_pointer_cast<DefaultCurveSpec>(spec);
            QL_REQUIRE(defaultspec, "Failed to convert spec " << *spec << " to default curve spec");
            const std::string configId = defaultspec->curveConfigID();
            const boost::shared_ptr<DefaultCurveConfig> curveConfig = defaultCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find default curve " << configId);
            curveConfigs.defaultCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::CDSVolatility: {
            const boost::shared_ptr<CDSVolatilityCurveSpec> cdsvolspec = boost::dynamic_pointer_cast<CDSVolatilityCurveSpec>(spec);
            QL_REQUIRE(cdsvolspec, "Failed to convert spec " << *spec << " to cds vol curve spec");
            const std::string configId = cdsvolspec->curveConfigID();
            const boost::shared_ptr<CDSVolatilityCurveConfig> curveConfig = cdsVolCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find cds vol curve " << configId);
            curveConfigs.cdsVolCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::BaseCorrelation: {            
            const boost::shared_ptr<BaseCorrelationCurveSpec> basecorrelationspec = 
                boost::dynamic_pointer_cast<BaseCorrelationCurveSpec>(spec);
            QL_REQUIRE(basecorrelationspec, "Failed to convert spec " << *spec << " to base correlation curve spec");
            const std::string configId = basecorrelationspec->curveConfigID();
            const boost::shared_ptr<BaseCorrelationCurveConfig> curveConfig = baseCorrelationCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find base correlation curve " << configId);
            curveConfigs.baseCorrelationCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::Inflation: {
            const boost::shared_ptr<InflationCurveSpec> inflationspec = boost::dynamic_pointer_cast<InflationCurveSpec>(spec);
            QL_REQUIRE(inflationspec, "Failed to convert spec " << *spec << " to inflation curve spec");
            const std::string configId = inflationspec->curveConfigID();
            const boost::shared_ptr<InflationCurveConfig> curveConfig = inflationCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find inflation curve " << configId);
            curveConfigs.inflationCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::InflationCapFloorPrice: {
            const boost::shared_ptr<InflationCapFloorPriceSurfaceSpec> infcapfloorspec =
                boost::dynamic_pointer_cast<InflationCapFloorPriceSurfaceSpec>(spec);
            QL_REQUIRE(infcapfloorspec, "Failed to convert spec " << *spec << " to inf cap floor price surface spec");
            const std::string configId = infcapfloorspec->curveConfigID();
            const boost::shared_ptr<InflationCapFloorPriceSurfaceConfig> curveConfig = 
                inflationCapFloorPriceSurfaceConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find inflation cap floor price surface " << configId);
            curveConfigs.inflationCapFloorPriceSurfaceConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::InflationCapFloorVolatility: {
            const boost::shared_ptr<InflationCapFloorVolatilityCurveSpec> infcapfloorvolspec =
                boost::dynamic_pointer_cast<InflationCapFloorVolatilityCurveSpec>(spec);
            QL_REQUIRE(infcapfloorvolspec, "Failed to convert spec " << *spec << " to inf cap floor vol curve spec");
            const std::string configId = infcapfloorvolspec->curveConfigID();
            const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig> curveConfig = 
                inflationCapFloorVolCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find inflation cap floor vol curve " << configId);
            curveConfigs.inflationCapFloorVolCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::Equity: {
            const boost::shared_ptr<EquityCurveSpec> equityspec = boost::dynamic_pointer_cast<EquityCurveSpec>(spec);
            QL_REQUIRE(equityspec, "Failed to convert spec " << *spec << " to equity curve spec");
            const std::string configId = equityspec->curveConfigID();
            const boost::shared_ptr<EquityCurveConfig> curveConfig = equityCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find equity curve " << configId);
            curveConfigs.equityCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::EquityVolatility: {
            const boost::shared_ptr<EquityVolatilityCurveSpec> eqvolspec =
                boost::dynamic_pointer_cast<EquityVolatilityCurveSpec>(spec);
            QL_REQUIRE(eqvolspec, "failed to convert spec " << *spec << " to equity vol curve spec");
            const std::string configId = eqvolspec->curveConfigID();
            const boost::shared_ptr<EquityVolatilityCurveConfig> curveConfig = equityVolCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find equity vol curve " << configId);
            curveConfigs.equityVolCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::Security: {
            const boost::shared_ptr<SecuritySpec> securityspec = boost::dynamic_pointer_cast<SecuritySpec>(spec);
            QL_REQUIRE(securityspec, "failed to convert spec " << *spec << " to security spec");
            const std::string configId = securityspec->securityID();
            const boost::shared_ptr<SecurityConfig> curveConfig = securityConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find security " << configId);
            curveConfigs.securityConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::Commodity: {
            const boost::shared_ptr<CommodityCurveSpec> commodityspec = boost::dynamic_pointer_cast<CommodityCurveSpec>(spec);
            QL_REQUIRE(commodityspec, "Failed to convert spec, " << *spec << ", to commodity spec");
            const std::string configId = commodityspec->curveConfigID();
            const boost::shared_ptr<CommodityCurveConfig> curveConfig = commodityCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find commodity curve " << configId);
            curveConfigs.commodityCurveConfigs_[configId] = curveConfig;
            break;
        }

        case CurveSpec::CurveType::CommodityVolatility: {
            const boost::shared_ptr<CommodityVolatilityCurveSpec> commodityvolspec =
                boost::dynamic_pointer_cast<CommodityVolatilityCurveSpec>(spec);
            QL_REQUIRE(commodityvolspec, "Failed to convert spec " << *spec << " to commodity vol spec");
            const std::string configId = commodityvolspec->curveConfigID();
            const boost::shared_ptr<CommodityVolatilityCurveConfig> curveConfig = commodityVolatilityCurveConfig(configId);
            QL_REQUIRE(curveConfig, "Cannot find commodity vol curve " << configId);
            curveConfigs.commodityVolatilityCurveConfigs_[configId] = curveConfig;
            break;
        }

        default: {
            QL_FAIL("Unhandled spec " << *spec);
        }
        }
        
    }
    return curveConfigs;
}

std::set<string> CurveConfigurations::quotes() const {
    set<string> quotes;
    for (auto m : yieldCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : fxVolCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : swaptionVolCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : capFloorVolCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : defaultCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : cdsVolCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : baseCorrelationCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : inflationCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : inflationCapFloorPriceSurfaceConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : inflationCapFloorVolCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : equityCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : equityVolCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : securityConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : fxSpotConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : commodityCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());
    for (auto m : commodityVolatilityCurveConfigs_)
        quotes.insert(m.second->quotes().begin(), m.second->quotes().end());

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
