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

#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

bool operator==(const MarketConfiguration& lhs, const MarketConfiguration& rhs) {
    if (lhs.discountingCurvesId != rhs.discountingCurvesId || lhs.yieldCurvesId != rhs.yieldCurvesId ||
        lhs.indexForwardingCurvesId != rhs.indexForwardingCurvesId || lhs.fxSpotsId != rhs.fxSpotsId ||
        lhs.fxVolatilitiesId != rhs.fxVolatilitiesId || lhs.swaptionVolatilitiesId != rhs.swaptionVolatilitiesId ||
        lhs.defaultCurvesId != rhs.defaultCurvesId || lhs.swapIndexCurvesId != rhs.swapIndexCurvesId ||
        lhs.capFloorVolatilitiesId != rhs.capFloorVolatilitiesId || lhs.equityCurvesId != rhs.equityCurvesId || 
        lhs.equityVolatilitiesId != rhs.equityVolatilitiesId || lhs.securitySpreadsId != rhs.securitySpreadsId) {
        return false;
    } else {
        return true;
    }
}

bool TodaysMarketParameters::operator==(TodaysMarketParameters& rhs) {

    if (swapIndices_.size() == rhs.swapIndices_.size()) {
        // need to check also for existence of keys
        for (auto item = swapIndices_.begin(); item != swapIndices_.end(); ++item) {
            map<string, string> left = rhs.swapIndices_[item->first];
            map<string, string> right = swapIndices_[item->first];
            if (left != right) {
                return false;
            }
        }
    }

    if (discountingCurves_ != rhs.discountingCurves_ || yieldCurves_ != rhs.yieldCurves_ ||
        indexForwardingCurves_ != rhs.indexForwardingCurves_ || fxSpots_ != rhs.fxSpots_ ||
        fxVolatilities_ != rhs.fxVolatilities_ || swaptionVolatilities_ != rhs.swaptionVolatilities_ ||
        defaultCurves_ != rhs.defaultCurves_ || capFloorVolatilities_ != rhs.capFloorVolatilities_ ||
        equityCurves_ != rhs.equityCurves_ || equityVolatilities_ != rhs.equityVolatilities_ ||
        configurations_ != rhs.configurations_) {
        return false;
    }
    return true;
};

bool TodaysMarketParameters::operator!=(TodaysMarketParameters& rhs) { return !(*this == rhs); }

void TodaysMarketParameters::fromXML(XMLNode* node) {

    // clear data members
    configurations_.clear();
    discountingCurves_.clear();
    yieldCurves_.clear();
    indexForwardingCurves_.clear();
    fxSpots_.clear();
    fxVolatilities_.clear();
    swaptionVolatilities_.clear();
    defaultCurves_.clear();
    swapIndices_.clear();
    capFloorVolatilities_.clear();
    equityCurves_.clear();
    equityVolatilities_.clear();
    securitySpreads_.clear();

    // add default configuration (may be overwritten below)
    MarketConfiguration defaultConfig;
    addConfiguration(Market::defaultConfiguration, defaultConfig);

    // fill data from XML
    XMLUtils::checkNode(node, "TodaysMarket");
    XMLNode* n = XMLUtils::getChildNode(node);
    while (n) {
        if (XMLUtils::getNodeName(n) == "Configuration") {
            MarketConfiguration tmp;
            tmp.discountingCurvesId = XMLUtils::getChildValue(n, "DiscountingCurvesId", false);
            tmp.yieldCurvesId = XMLUtils::getChildValue(n, "YieldCurvesId", false);
            tmp.indexForwardingCurvesId = XMLUtils::getChildValue(n, "IndexForwardingCurvesId", false),
            tmp.swapIndexCurvesId = XMLUtils::getChildValue(n, "SwapIndexCurvesId", false),
            tmp.fxSpotsId = XMLUtils::getChildValue(n, "FxSpotsId", false);
            tmp.fxVolatilitiesId = XMLUtils::getChildValue(n, "FxVolatilitiesId", false);
            tmp.swaptionVolatilitiesId = XMLUtils::getChildValue(n, "SwaptionVolatilitiesId", false);
            tmp.defaultCurvesId = XMLUtils::getChildValue(n, "DefaultCurvesId", false);
            tmp.capFloorVolatilitiesId = XMLUtils::getChildValue(n, "CapFloorVolatilitiesId", false);
            tmp.equityCurvesId = XMLUtils::getChildValue(n, "EquityCurvesId", false);
            tmp.equityVolatilitiesId = XMLUtils::getChildValue(n, "EquityVolatilitiesId", false);
            tmp.securitySpreadsId = XMLUtils::getChildValue(n, "SecuritySpreadsId", false);
            if (tmp.discountingCurvesId == "")
                tmp.discountingCurvesId = Market::defaultConfiguration;
            if (tmp.yieldCurvesId == "")
                tmp.yieldCurvesId = Market::defaultConfiguration;
            if (tmp.indexForwardingCurvesId == "")
                tmp.indexForwardingCurvesId = Market::defaultConfiguration;
            if (tmp.swapIndexCurvesId == "")
                tmp.swapIndexCurvesId = Market::defaultConfiguration;
            if (tmp.fxSpotsId == "")
                tmp.fxSpotsId = Market::defaultConfiguration;
            if (tmp.fxVolatilitiesId == "")
                tmp.fxVolatilitiesId = Market::defaultConfiguration;
            if (tmp.swaptionVolatilitiesId == "")
                tmp.swaptionVolatilitiesId = Market::defaultConfiguration;
            if (tmp.capFloorVolatilitiesId == "")
                tmp.capFloorVolatilitiesId = Market::defaultConfiguration;
            if (tmp.defaultCurvesId == "")
                tmp.defaultCurvesId = Market::defaultConfiguration;
            if (tmp.equityCurvesId == "")
                tmp.equityCurvesId = Market::defaultConfiguration;
            if (tmp.equityVolatilitiesId == "")
                tmp.equityVolatilitiesId = Market::defaultConfiguration;
            if (tmp.securitySpreadsId == "")
                tmp.securitySpreadsId = Market::defaultConfiguration;
            addConfiguration(XMLUtils::getAttribute(n, "id"), tmp);
        } else if (XMLUtils::getNodeName(n) == "DiscountingCurves") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addDiscountingCurves(id, XMLUtils::getChildrenAttributesAndValues(n, "DiscountingCurve", "currency", true));
        } else if (XMLUtils::getNodeName(n) == "YieldCurves") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addYieldCurves(id, XMLUtils::getChildrenAttributesAndValues(n, "YieldCurve", "name", false));
        } else if (XMLUtils::getNodeName(n) == "IndexForwardingCurves") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addIndexForwardingCurves(id, XMLUtils::getChildrenAttributesAndValues(n, "Index", "name", false));
        } else if (XMLUtils::getNodeName(n) == "SwapIndexCurves") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(n, "SwapIndex");
            map<string, string> swapIndices;
            for (XMLNode* xn : nodes) {
                string name = XMLUtils::getAttribute(xn, "name");
                QL_REQUIRE(name != "", "no name given for SwapIndex");
                QL_REQUIRE(swapIndices.find(name) == swapIndices.end(), "Duplicate SwapIndex found for " << name);
                string disc = XMLUtils::getChildValue(xn, "Discounting", true);
                swapIndices[name] = disc; //.emplace(name, { ibor, disc }); won't work?
            }
            addSwapIndices(id, swapIndices);
        } else if (XMLUtils::getNodeName(n) == "FxSpots") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addFxSpots(id, XMLUtils::getChildrenAttributesAndValues(n, "FxSpot", "pair", false));
        } else if (XMLUtils::getNodeName(n) == "FxVolatilities") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addFxVolatilities(id, XMLUtils::getChildrenAttributesAndValues(n, "FxVolatility", "pair", false));
        } else if (XMLUtils::getNodeName(n) == "SwaptionVolatilities") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addSwaptionVolatilities(
                id, XMLUtils::getChildrenAttributesAndValues(n, "SwaptionVolatility", "currency", false));
        } else if (XMLUtils::getNodeName(n) == "CapFloorVolatilities") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addCapFloorVolatilities(
                id, XMLUtils::getChildrenAttributesAndValues(n, "CapFloorVolatility", "currency", false));
        } else if (XMLUtils::getNodeName(n) == "DefaultCurves") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addDefaultCurves(id, XMLUtils::getChildrenAttributesAndValues(n, "DefaultCurve", "name", false));
        }
        else if (XMLUtils::getNodeName(n) == "EquityCurves") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addEquityCurves(id, XMLUtils::getChildrenAttributesAndValues(n, "EquityCurve", "name", false));
        }
        else if (XMLUtils::getNodeName(n) == "EquityVolatilities") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addEquityVolatilities(id, XMLUtils::getChildrenAttributesAndValues(n, "EquityVolatility", "name", false));
        } else if (XMLUtils::getNodeName(n) == "SecuritySpreads") {
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "")
                id = Market::defaultConfiguration;
            addSecuritySpreads(id, XMLUtils::getChildrenAttributesAndValues(n, "SecuritySpread", "name", false));
        } else
            QL_FAIL("TodaysMarketParameters::fromXML(): node not recognized: " << XMLUtils::getNodeName(n));
        n = XMLUtils::getNextSibling(n);
    }
}

XMLNode* TodaysMarketParameters::toXML(XMLDocument& doc) {

    XMLNode* todaysMarketNode = doc.allocNode("TodaysMarket");

    // configurations
    if (configurations_.size() > 0) {
        for (auto iterator = configurations_.begin(); iterator != configurations_.end(); iterator++) {

            XMLNode* configurationsNode = XMLUtils::addChild(doc, todaysMarketNode, "Configuration");
            XMLUtils::addAttribute(doc, configurationsNode, "id", iterator->first.c_str());

            if (iterator->second.discountingCurvesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "DiscountingCurvesId",
                                   iterator->second.discountingCurvesId);
            }
            if (iterator->second.yieldCurvesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "YieldCurvesId", iterator->second.yieldCurvesId);
            }
            if (iterator->second.indexForwardingCurvesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "IndexForwardingCurvesId",
                                   iterator->second.indexForwardingCurvesId);
            }
            if (iterator->second.swapIndexCurvesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "SwapIndexCurvesId", iterator->second.swapIndexCurvesId);
            }
            if (iterator->second.fxSpotsId != "") {
                XMLUtils::addChild(doc, configurationsNode, "FxSpotsId", iterator->second.fxSpotsId);
            }
            if (iterator->second.fxVolatilitiesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "FxVolatilitiesId", iterator->second.fxVolatilitiesId);
            }
            if (iterator->second.swaptionVolatilitiesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "SwaptionVolatilitiesId",
                                   iterator->second.swaptionVolatilitiesId);
            }
            if (iterator->second.defaultCurvesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "DefaultCurvesId", iterator->second.defaultCurvesId);
            }
            if (iterator->second.equityCurvesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "EquityCurvesId", iterator->second.equityCurvesId);
            }
            if (iterator->second.equityVolatilitiesId != "") {
                XMLUtils::addChild(doc, configurationsNode, "EquityVolatilitiesId", iterator->second.equityVolatilitiesId);
            }
            if (iterator->second.securitySpreadsId != "") {
                XMLUtils::addChild(doc, configurationsNode, "SecuritySpreadsId", iterator->second.securitySpreadsId);
            }
        }
    }

    // discounting curves
    if (discountingCurves_.size() > 0) {

        for (auto mappingSetIterator = discountingCurves_.begin(); mappingSetIterator != discountingCurves_.end();
             mappingSetIterator++) {

            XMLNode* discountingCurvesNode = XMLUtils::addChild(doc, todaysMarketNode, "DiscountingCurves");
            XMLUtils::addAttribute(doc, discountingCurvesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* singleMappingNode = doc.allocNode("DiscountingCurve", singleMappingIterator->second);
                XMLUtils::appendNode(discountingCurvesNode, singleMappingNode);
                XMLUtils::addAttribute(doc, singleMappingNode, "currency", singleMappingIterator->first);
            }
        }
    }

    // yield curves
    if (yieldCurves_.size() > 0) {

        for (auto mappingSetIterator = yieldCurves_.begin(); mappingSetIterator != yieldCurves_.end();
             mappingSetIterator++) {

            XMLNode* yieldCurvesNode = XMLUtils::addChild(doc, todaysMarketNode, "YieldCurves");
            XMLUtils::addAttribute(doc, yieldCurvesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("YieldCurve", singleMappingIterator->second);
                XMLUtils::appendNode(yieldCurvesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "name", singleMappingIterator->first);
            }
        }
    }

    // index Forwarding curves
    if (indexForwardingCurves_.size() > 0) {

        for (auto mappingSetIterator = indexForwardingCurves_.begin();
             mappingSetIterator != indexForwardingCurves_.end(); mappingSetIterator++) {

            XMLNode* indexForwardingCurvesNode = XMLUtils::addChild(doc, todaysMarketNode, "IndexForwardingCurves");
            XMLUtils::addAttribute(doc, indexForwardingCurvesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("Index", singleMappingIterator->second);
                XMLUtils::appendNode(indexForwardingCurvesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "name", singleMappingIterator->first);
            }
        }
    }

    // fx Spots
    if (fxSpots_.size() > 0) {

        for (auto mappingSetIterator = fxSpots_.begin(); mappingSetIterator != fxSpots_.end(); mappingSetIterator++) {

            XMLNode* fxSpotsNode = XMLUtils::addChild(doc, todaysMarketNode, "FxSpots");
            XMLUtils::addAttribute(doc, fxSpotsNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("FxSpot", singleMappingIterator->second);
                XMLUtils::appendNode(fxSpotsNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "pair", singleMappingIterator->first);
            }
        }
    }

    // fx Volatilities
    if (fxVolatilities_.size() > 0) {

        for (auto mappingSetIterator = fxVolatilities_.begin(); mappingSetIterator != fxVolatilities_.end();
             mappingSetIterator++) {

            XMLNode* fxVolatilitiesNode = XMLUtils::addChild(doc, todaysMarketNode, "FxVolatilities");
            XMLUtils::addAttribute(doc, fxVolatilitiesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("FxVolatility", singleMappingIterator->second);
                XMLUtils::appendNode(fxVolatilitiesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "pair", singleMappingIterator->first.c_str());
            }
        }
    }

    // swaption Volatilities
    if (swaptionVolatilities_.size() > 0) {

        for (auto mappingSetIterator = swaptionVolatilities_.begin(); mappingSetIterator != swaptionVolatilities_.end();
             mappingSetIterator++) {

            XMLNode* swaptionVolatilitiesNode = XMLUtils::addChild(doc, todaysMarketNode, "SwaptionVolatilities");
            XMLUtils::addAttribute(doc, swaptionVolatilitiesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("SwaptionVolatility", singleMappingIterator->second);
                XMLUtils::appendNode(swaptionVolatilitiesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "currency", singleMappingIterator->first);
            }
        }
    }

    // default Curves
    if (defaultCurves_.size() > 0) {

        for (auto mappingSetIterator = defaultCurves_.begin(); mappingSetIterator != defaultCurves_.end();
             mappingSetIterator++) {

            XMLNode* defaultCurvesNode = XMLUtils::addChild(doc, todaysMarketNode, "DefaultCurves");
            XMLUtils::addAttribute(doc, defaultCurvesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("DefaultCurve", singleMappingIterator->second);
                XMLUtils::appendNode(defaultCurvesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "name", singleMappingIterator->first);
            }
        }
    }

    // equity Curves
    if (equityCurves_.size() > 0) {

        for (auto mappingSetIterator = equityCurves_.begin(); mappingSetIterator != equityCurves_.end();
            mappingSetIterator++) {

            XMLNode* equityCurvesNode = XMLUtils::addChild(doc, todaysMarketNode, "EquityCurves");
            XMLUtils::addAttribute(doc, equityCurvesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("EquityCurve", singleMappingIterator->second);
                XMLUtils::appendNode(equityCurvesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "name", singleMappingIterator->first);
            }
        }
    }

    // equity Volatilities
    if (equityVolatilities_.size() > 0) {

        for (auto mappingSetIterator = equityCurves_.begin(); mappingSetIterator != equityCurves_.end();
            mappingSetIterator++) {

            XMLNode* equityVolatilitiesNode = XMLUtils::addChild(doc, todaysMarketNode, "EquityVolatilities");
            XMLUtils::addAttribute(doc, equityVolatilitiesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("EquityVolatility", singleMappingIterator->second);
                XMLUtils::appendNode(equityVolatilitiesNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "name", singleMappingIterator->first);
            }
        }
    }

    // swap Indices
    if (swapIndices_.size() > 0) {

        for (auto mappingSetIterator = swapIndices_.begin(); mappingSetIterator != swapIndices_.end();
             mappingSetIterator++) {

            XMLNode* swapIndexCurvesNode = XMLUtils::addChild(doc, todaysMarketNode, "SwapIndexCurves");
            XMLUtils::addAttribute(doc, swapIndexCurvesNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* swapIndexNode = XMLUtils::addChild(doc, swapIndexCurvesNode, "SwapIndex");
                XMLUtils::addAttribute(doc, swapIndexNode, "name", singleMappingIterator->first.c_str());
                XMLUtils::addChild(doc, swapIndexNode, "Discounting", (string)singleMappingIterator->second.c_str());
            }
        }
    }

    // security Spreads
    if (securitySpreads_.size() > 0) {

        for (auto mappingSetIterator = securitySpreads_.begin(); mappingSetIterator != securitySpreads_.end();
             mappingSetIterator++) {

            XMLNode* bondSpreadsNode = XMLUtils::addChild(doc, todaysMarketNode, "BondSpreads");
            XMLUtils::addAttribute(doc, bondSpreadsNode, "id", mappingSetIterator->first.c_str());

            for (auto singleMappingIterator = mappingSetIterator->second.begin();
                 singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                XMLNode* mappingNode = doc.allocNode("BondSpreads", singleMappingIterator->second);
                XMLUtils::appendNode(bondSpreadsNode, mappingNode);
                XMLUtils::addAttribute(doc, mappingNode, "pair", singleMappingIterator->first);
            }
        }
    }

    return todaysMarketNode;
}

void TodaysMarketParameters::curveSpecs(const map<string, map<string, string>>& m, const string& id,
                                        vector<string>& specs) const {
    // extract all the curve specs
    auto it = m.find(id);
    if (it != m.end()) {
        for (auto kv : it->second) {
            specs.push_back(kv.second);
            DLOG("Add spec " << kv.second);
        }
    }
}

vector<string> TodaysMarketParameters::curveSpecs(const string& configuration) const {
    vector<string> specs;
    curveSpecs(discountingCurves_, discountingCurvesId(configuration), specs);
    curveSpecs(yieldCurves_, yieldCurvesId(configuration), specs);
    curveSpecs(indexForwardingCurves_, indexForwardingCurvesId(configuration), specs);
    curveSpecs(fxSpots_, fxSpotsId(configuration), specs);
    curveSpecs(fxVolatilities_, fxVolatilitiesId(configuration), specs);
    curveSpecs(swaptionVolatilities_, swaptionVolatilitiesId(configuration), specs);
    curveSpecs(capFloorVolatilities_, capFloorVolatilitiesId(configuration), specs);
    curveSpecs(defaultCurves_, defaultCurvesId(configuration), specs);
    curveSpecs(equityCurves_, equityCurvesId(configuration), specs);
    curveSpecs(equityVolatilities_, equityVolatilitiesId(configuration), specs);
    curveSpecs(securitySpreads_, securitySpreadsId(configuration), specs);
    return specs;
}
} // namespace data
} // namespace ore
