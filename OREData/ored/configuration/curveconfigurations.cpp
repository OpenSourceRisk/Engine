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
template<class T>
const boost::shared_ptr<T>& get(const string& id, const map<string, boost::shared_ptr<T>>& m) {
    auto it = m.find(id);
    QL_REQUIRE(it != m.end(), "no curve id for " << id);
    return it->second;
}

// utility function for parsing a node of name "parentName" and decoding all
// child elements, storing the resulting config in the map
template<class T>
void parseNode(XMLNode *node, const char *parentName, const char *childName, map<string, boost::shared_ptr<T>>& m) {

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
template<class T>
void addNodes(XMLDocument &doc, XMLNode *parent, const char *nodeName, map<string, boost::shared_ptr<T>>& m) {
    XMLNode* node = doc.allocNode(nodeName);
    XMLUtils::appendNode(parent, node);
    for (auto it : m)
        XMLUtils::appendNode(node, it.second->toXML(doc));
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

const boost::shared_ptr<EquityCurveConfig>& CurveConfigurations::equityCurveConfig(const string& curveID) const {
    return get(curveID, equityCurveConfigs_);
}

const boost::shared_ptr<EquityVolatilityCurveConfig>&
CurveConfigurations::equityVolCurveConfig(const string& curveID) const {
    return get(curveID, equityVolCurveConfigs_);
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

    return parent;
}
} // namespace data
} // namespace ore
