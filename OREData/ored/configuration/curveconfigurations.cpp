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

const boost::shared_ptr<YieldCurveConfig>& CurveConfigurations::yieldCurveConfig(const string& curveID) const {
    auto it = yieldCurveConfigs_.find(curveID);
    QL_REQUIRE(it != yieldCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<FXVolatilityCurveConfig>& CurveConfigurations::fxVolCurveConfig(const string& curveID) const {
    auto it = fxVolCurveConfigs_.find(curveID);
    QL_REQUIRE(it != fxVolCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<SwaptionVolatilityCurveConfig>&
CurveConfigurations::swaptionVolCurveConfig(const string& curveID) const {
    auto it = swaptionVolCurveConfigs_.find(curveID);
    QL_REQUIRE(it != swaptionVolCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<CapFloorVolatilityCurveConfig>&
CurveConfigurations::capFloorVolCurveConfig(const string& curveID) const {
    auto it = capFloorVolCurveConfigs_.find(curveID);
    QL_REQUIRE(it != capFloorVolCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<DefaultCurveConfig>& CurveConfigurations::defaultCurveConfig(const string& curveID) const {
    auto it = defaultCurveConfigs_.find(curveID);
    QL_REQUIRE(it != defaultCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<CDSVolatilityCurveConfig>& CurveConfigurations::cdsVolCurveConfig(const string& curveID) const {
    auto it = cdsVolCurveConfigs_.find(curveID);
    QL_REQUIRE(it != cdsVolCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<InflationCurveConfig>& CurveConfigurations::inflationCurveConfig(const string& curveID) const {
    auto it = inflationCurveConfigs_.find(curveID);
    QL_REQUIRE(it != inflationCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<InflationCapFloorPriceSurfaceConfig>&
CurveConfigurations::inflationCapFloorPriceSurfaceConfig(const string& curveID) const {
    auto it = inflationCapFloorPriceSurfaceConfigs_.find(curveID);
    QL_REQUIRE(it != inflationCapFloorPriceSurfaceConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<EquityCurveConfig>& CurveConfigurations::equityCurveConfig(const string& curveID) const {
    auto it = equityCurveConfigs_.find(curveID);
    QL_REQUIRE(it != equityCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

const boost::shared_ptr<EquityVolatilityCurveConfig>&
CurveConfigurations::equityVolCurveConfig(const string& curveID) const {
    auto it = equityVolCurveConfigs_.find(curveID);
    QL_REQUIRE(it != equityVolCurveConfigs_.end(), "No curve id for " << curveID);
    return it->second;
}

void CurveConfigurations::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CurveConfiguration");

    // Load YieldCurves
    XMLNode* yieldCurvesNode = XMLUtils::getChildNode(node, "YieldCurves");
    if (yieldCurvesNode) {
        for (XMLNode* child = XMLUtils::getChildNode(yieldCurvesNode, "YieldCurve"); child;
             child = XMLUtils::getNextSibling(child, "YieldCurve")) {
            boost::shared_ptr<YieldCurveConfig> yieldCurveConfig(new YieldCurveConfig());
            try {
                yieldCurveConfig->fromXML(child);
                const string& id = yieldCurveConfig->curveID();
                yieldCurveConfigs_[id] = yieldCurveConfig;
                DLOG("Added yield curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing yield curve config: " << ex.what());
            }
        }
    }

    // Load FXVols
    XMLNode* fxVolsNode = XMLUtils::getChildNode(node, "FXVolatilities");
    if (fxVolsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(fxVolsNode, "FXVolatility"); child;
             child = XMLUtils::getNextSibling(child, "FXVolatility")) {
            boost::shared_ptr<FXVolatilityCurveConfig> fxVolCurveConfig(new FXVolatilityCurveConfig());
            try {
                fxVolCurveConfig->fromXML(child);
                const string& id = fxVolCurveConfig->curveID();
                fxVolCurveConfigs_[id] = fxVolCurveConfig;
                DLOG("Added fxVol curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing fxVol curve config: " << ex.what());
            }
        }
    }

    // Load SwaptionVols
    XMLNode* swaptionVolsNode = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    if (swaptionVolsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(swaptionVolsNode, "SwaptionVolatility"); child;
             child = XMLUtils::getNextSibling(child, "SwaptionVolatility")) {
            boost::shared_ptr<SwaptionVolatilityCurveConfig> swaptionVolCurveConfig(
                new SwaptionVolatilityCurveConfig());
            try {
                swaptionVolCurveConfig->fromXML(child);
                const string& id = swaptionVolCurveConfig->curveID();
                swaptionVolCurveConfigs_[id] = swaptionVolCurveConfig;
                DLOG("Added swaptionVol curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing swaptionVol curve config: " << ex.what());
            }
        }
    }

    // Load CapFloor volatilities curve configuration
    XMLNode* capFloorVolsNode = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    if (capFloorVolsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(capFloorVolsNode, "CapFloorVolatility"); child;
             child = XMLUtils::getNextSibling(child, "CapFloorVolatility")) {
            boost::shared_ptr<CapFloorVolatilityCurveConfig> capFloorVolCurveConfig =
                boost::make_shared<CapFloorVolatilityCurveConfig>();
            try {
                capFloorVolCurveConfig->fromXML(child);
                const string& id = capFloorVolCurveConfig->curveID();
                capFloorVolCurveConfigs_[id] = capFloorVolCurveConfig;
                DLOG("Added capFloor volatility curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing capFloor volatility curve config: " << ex.what());
            }
        }
    }

    // Load DefaultCurves
    XMLNode* defaultCurvesNode = XMLUtils::getChildNode(node, "DefaultCurves");
    if (defaultCurvesNode) {
        for (XMLNode* child = XMLUtils::getChildNode(defaultCurvesNode, "DefaultCurve"); child;
             child = XMLUtils::getNextSibling(child, "DefaultCurve")) {
            boost::shared_ptr<DefaultCurveConfig> defaultCurveConfig(new DefaultCurveConfig());
            try {
                defaultCurveConfig->fromXML(child);
                const string& id = defaultCurveConfig->curveID();
                defaultCurveConfigs_[id] = defaultCurveConfig;
                DLOG("Added default curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing default curve config: " << ex.what());
            }
        }
    }

    // Load CDSVolCurves
    XMLNode* cdsVolsNode = XMLUtils::getChildNode(node, "CDSVolatilities");
    if (cdsVolsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(cdsVolsNode, "CDSVolatility"); child;
             child = XMLUtils::getNextSibling(child, "CDSVolatility")) {
            boost::shared_ptr<CDSVolatilityCurveConfig> cdsVolConfig(new CDSVolatilityCurveConfig());
            try {
                cdsVolConfig->fromXML(child);
                const string& id = cdsVolConfig->curveID();
                cdsVolCurveConfigs_[id] = cdsVolConfig;
                DLOG("Added CDS volatility config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing CDS volatility config: " << ex.what());
            }
        }
    }

    // Load EquityCurves
    XMLNode* equityCurvesNode = XMLUtils::getChildNode(node, "EquityCurves");
    if (equityCurvesNode) {
        for (XMLNode* child = XMLUtils::getChildNode(equityCurvesNode, "EquityCurve"); child;
             child = XMLUtils::getNextSibling(child, "EquityCurve")) {
            boost::shared_ptr<EquityCurveConfig> equityCurveConfig(new EquityCurveConfig());
            try {
                equityCurveConfig->fromXML(child);
                const string& id = equityCurveConfig->curveID();
                equityCurveConfigs_[id] = equityCurveConfig;
                DLOG("Added equity curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing equity curve config: " << ex.what());
            }
        }
    }

    // Load EquityVolCurves
    XMLNode* equityVolsNode = XMLUtils::getChildNode(node, "EquityVolatilities");
    if (equityVolsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(equityVolsNode, "EquityVolatility"); child;
             child = XMLUtils::getNextSibling(child, "EquityVolatility")) {
            boost::shared_ptr<EquityVolatilityCurveConfig> equityVolConfig(new EquityVolatilityCurveConfig());
            try {
                equityVolConfig->fromXML(child);
                const string& id = equityVolConfig->curveID();
                equityVolCurveConfigs_[id] = equityVolConfig;
                DLOG("Added equity volatility config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing equity volatility config: " << ex.what());
            }
        }
    }

    // Load InflationCurves
    XMLNode* inflationCurvesNode = XMLUtils::getChildNode(node, "InflationCurves");
    if (inflationCurvesNode) {
        for (XMLNode* child = XMLUtils::getChildNode(inflationCurvesNode, "InflationCurve"); child;
             child = XMLUtils::getNextSibling(child, "InflationCurve")) {
            boost::shared_ptr<InflationCurveConfig> inflationCurveConfig(new InflationCurveConfig());
            try {
                inflationCurveConfig->fromXML(child);
                const string& id = inflationCurveConfig->curveID();
                inflationCurveConfigs_[id] = inflationCurveConfig;
                DLOG("Added inflation curve config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing inflation curve config: " << ex.what());
            }
        }
    }

    // Load InflationCapFloorPriceSurfaces
    XMLNode* inflationCapFloorPriceSurfaceNode = XMLUtils::getChildNode(node, "InflationCapFloorPriceSurfaces");
    if (inflationCapFloorPriceSurfaceNode) {
        for (XMLNode* child =
                 XMLUtils::getChildNode(inflationCapFloorPriceSurfaceNode, "InflationCapFloorPriceSurface");
             child; child = XMLUtils::getNextSibling(child, "InflationCapFloorPriceSurface")) {
            boost::shared_ptr<InflationCapFloorPriceSurfaceConfig> inflationCapFloorPriceSurfaceConfig(
                new InflationCapFloorPriceSurfaceConfig());
            try {
                inflationCapFloorPriceSurfaceConfig->fromXML(child);
                const string& id = inflationCapFloorPriceSurfaceConfig->curveID();
                inflationCapFloorPriceSurfaceConfigs_[id] = inflationCapFloorPriceSurfaceConfig;
                DLOG("Added inflation cap floor price surface config with ID = " << id);
            } catch (std::exception& ex) {
                ALOG("Exception parsing inflation cap floor price surface config: " << ex.what());
            }
        }
    }
}

XMLNode* CurveConfigurations::toXML(XMLDocument& doc) {
    XMLNode* parent = doc.allocNode("CurveConfiguration");
    doc.appendNode(parent);

    XMLNode* node = doc.allocNode("YieldCurves");
    XMLUtils::appendNode(parent, node);
    for (auto it : yieldCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("FXVolatilities");
    XMLUtils::appendNode(parent, node);
    for (auto it : fxVolCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("SwaptionVolatilities");
    XMLUtils::appendNode(parent, node);
    for (auto it : swaptionVolCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("CapFloorVolatilities");
    XMLUtils::appendNode(parent, node);
    for (auto it : capFloorVolCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("DefaultCurves");
    XMLUtils::appendNode(parent, node);
    for (auto it : defaultCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));
    
    node = doc.allocNode("CDSVolatilities");
    XMLUtils::appendNode(parent, node);
    for (auto it : cdsVolCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("InflationCurves");
    XMLUtils::appendNode(parent, node);
    for (auto it : inflationCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("InflationCapFloorPriceSurfaces");
    XMLUtils::appendNode(parent, node);
    for (auto it : inflationCapFloorPriceSurfaceConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("EquityCurves");
    XMLUtils::appendNode(parent, node);
    for (auto it : equityCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    node = doc.allocNode("EquityVolatilities");
    XMLUtils::appendNode(parent, node);
    for (auto it : equityVolCurveConfigs_)
        XMLUtils::appendNode(node, it.second->toXML(doc));

    return parent;
}
}
}
