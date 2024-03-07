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

namespace {

// container class to link the enum in the header with the various XML strings
struct MarketObjectMetaInfo {
    MarketObject obj;
    string name;                        // AKA marketObjectStrings
    string xmlName;                     // AKA marketObjectXMLNames
    pair<string, string> xmlSingleName; // AKA marketObjectXMLNamesSingle
};

// Note the order of elements in this array MUST respect the XML Schema
static const vector<MarketObjectMetaInfo> marketObjectData = {
    {MarketObject::YieldCurve, "YieldCurve", "YieldCurves", {"YieldCurve", "name"}},
    {MarketObject::DiscountCurve, "DiscountCurve", "DiscountingCurves", {"DiscountingCurve", "currency"}},
    {MarketObject::IndexCurve, "IndexCurve", "IndexForwardingCurves", {"Index", "name"}},
    {MarketObject::SwapIndexCurve, "SwapIndexCurve", "SwapIndexCurves", {"SwapIndex", "name"}},
    {MarketObject::ZeroInflationCurve,
     "ZeroInflationCurve",
     "ZeroInflationIndexCurves",
     {"ZeroInflationIndexCurve", "name"}},
    {MarketObject::YoYInflationCurve, "YoYInflationCurve", "YYInflationIndexCurves", {"YYInflationIndexCurve", "name"}},
    {MarketObject::FXSpot, "FXSpot", "FxSpots", {"FxSpot", "pair"}},
    {MarketObject::FXVol, "FXVol", "FxVolatilities", {"FxVolatility", "pair"}},
    {MarketObject::SwaptionVol, "SwaptionVol", "SwaptionVolatilities", {"SwaptionVolatility", "key"}},
    {MarketObject::YieldVol, "YieldVol", "YieldVolatilities", {"YieldVolatility", "name"}},
    {MarketObject::CapFloorVol, "CapFloorVol", "CapFloorVolatilities", {"CapFloorVolatility", "key"}},
    {MarketObject::CDSVol, "CDSVol", "CDSVolatilities", {"CDSVolatility", "name"}},
    {MarketObject::DefaultCurve, "DefaultCurve", "DefaultCurves", {"DefaultCurve", "name"}},
    {MarketObject::YoYInflationCapFloorVol,
     "YoYInflationCapFloorVol",
     "YYInflationCapFloorVolatilities",
     {"YYInflationCapFloorVolatility", "name"}},
    {MarketObject::ZeroInflationCapFloorVol,
     "ZeroInflationCapFloorVol",
     "ZeroInflationCapFloorVolatilities",
     {"ZeroInflationCapFloorVolatility", "name"}},
    {MarketObject::EquityCurve, "EquityCurves", "EquityCurves", {"EquityCurve", "name"}},
    {MarketObject::EquityVol, "EquityVols", "EquityVolatilities", {"EquityVolatility", "name"}},
    {MarketObject::Security, "Securities", "Securities", {"Security", "name"}},
    {MarketObject::BaseCorrelation, "BaseCorrelation", "BaseCorrelations", {"BaseCorrelation", "name"}},
    {MarketObject::CommodityCurve, "CommodityCurves", "CommodityCurves", {"CommodityCurve", "name"}},
    {MarketObject::CommodityVolatility,
     "CommodityVolatilities",
     "CommodityVolatilities",
     {"CommodityVolatility", "name"}},
    {MarketObject::Correlation, "Correlation", "Correlations", {"Correlation", "name"}}};

} // anonymous namespace

std::ostream& operator<<(std::ostream& out, const MarketObject& o) {
    for (Size i = 0; i < marketObjectData.size(); i++) {
        if (marketObjectData[i].obj == o)
            return out << marketObjectData[i].name;
    }
    return out << "Unknown";
}

std::set<MarketObject> getMarketObjectTypes() {
    thread_local static std::set<MarketObject> result;
    if (result.empty()) {
        for (auto const& o : marketObjectData) {
            result.insert(o.obj);
        }
    }
    return result;
}

MarketConfiguration::MarketConfiguration(map<MarketObject, string> marketObjectIds) {
    for (Size i = 0; i < marketObjectData.size(); ++i) {
        marketObjectIds_[marketObjectData[i].obj] = Market::defaultConfiguration;
    }
    
    for (const auto& moi : marketObjectIds)
        setId(moi.first, moi.second);
}

string MarketConfiguration::operator()(const MarketObject o) const {
    QL_REQUIRE(marketObjectIds_.find(o) != marketObjectIds_.end(),
               "MarketConfiguration: did not find MarketObject " << o << " (this is unexpected)");
    return marketObjectIds_.at(o);
}

void MarketConfiguration::setId(const MarketObject o, const string& id) {
    if (id != "")
        marketObjectIds_[o] = id;
}

void MarketConfiguration::add(const MarketConfiguration& o) {
    // overwrite if already existent
    for (auto const& x : o.marketObjectIds_)
        marketObjectIds_[x.first] = x.second;
}

void TodaysMarketParameters::addConfiguration(const string& id, const MarketConfiguration& configuration) {
    if (hasConfiguration(id)) {
        auto it =
            find_if(configurations_.begin(), configurations_.end(),
                    [&id](const pair<string, MarketConfiguration>& s) { return s.first == id; });
        it->second.add(configuration);
    } else 
        configurations_.push_back(make_pair(id, configuration));
}

void TodaysMarketParameters::clear() {
    // clear data members
    configurations_.clear();
    marketObjects_.clear();
}

bool TodaysMarketParameters::empty() { 
    return marketObjects_.size() == 0;
}

void TodaysMarketParameters::fromXML(XMLNode* node) {

    // add default configuration if we do not have one (may be overwritten below)
    if (!hasConfiguration(Market::defaultConfiguration))
        addConfiguration(Market::defaultConfiguration, MarketConfiguration());

    // fill data from XML
    XMLUtils::checkNode(node, "TodaysMarket");
    XMLNode* n = XMLUtils::getChildNode(node);
    while (n) {
        if (XMLUtils::getNodeName(n) == "Configuration") {
            MarketConfiguration tmp;
            for (Size i = 0; i < marketObjectData.size(); ++i) {
                tmp.setId(marketObjectData[i].obj,
                          XMLUtils::getChildValue(n, marketObjectData[i].xmlName + "Id", false));
                addConfiguration(XMLUtils::getAttribute(n, "id"), tmp);
            }
        } else {
            Size i = 0;
            for (; i < marketObjectData.size(); ++i) {
                if (XMLUtils::getNodeName(n) == marketObjectData[i].xmlName) {
                    string id = XMLUtils::getAttribute(n, "id");
                    if (id == "")
                        id = Market::defaultConfiguration;
                    // The XML schema for swap indices is different ...
                    if (marketObjectData[i].obj == MarketObject::SwapIndexCurve) {
                        vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(n, marketObjectData[i].xmlSingleName.first);
                        map<string, string> swapIndices;
                        for (XMLNode* xn : nodes) {
                            string name = XMLUtils::getAttribute(xn, marketObjectData[i].xmlSingleName.second);
                            QL_REQUIRE(name != "", "no name given for SwapIndex");
                            QL_REQUIRE(swapIndices.find(name) == swapIndices.end(),
                                       "Duplicate SwapIndex found for " << name);
                            string disc = XMLUtils::getChildValue(xn, "Discounting", true);
                            swapIndices[name] = disc; //.emplace(name, { ibor, disc }); won't work?
                        }
                        addMarketObject(MarketObject::SwapIndexCurve, id, swapIndices);

                    } else {
                        auto mp =
                            XMLUtils::getChildrenAttributesAndValues(n, marketObjectData[i].xmlSingleName.first,
                                                                     marketObjectData[i].xmlSingleName.second, false);
			// deprecated attribute currency for capfloor vols and swaption vols
                        if (marketObjectData[i].obj == MarketObject::CapFloorVol ||
                            marketObjectData[i].obj == MarketObject::SwaptionVol) {
                            auto mp2 = XMLUtils::getChildrenAttributesAndValues(
                                n, marketObjectData[i].xmlSingleName.first, "currency", false);
                            if (!mp2.empty()) {
                                mp.insert(mp2.begin(), mp2.end());
                                WLOG("TodaysMarketParameters: the attribute 'currency' is deprecated for '" +
                                     marketObjectData[i].xmlName + "', use 'key' instead.");
                            }
                        }
                        Size nc = XMLUtils::getChildrenNodes(n, "").size();
                        QL_REQUIRE(mp.size() == nc, "TodaysMarketParameters::fromXML(): possible duplicate entry in node " 
                            << marketObjectData[i].xmlName << ", check for XMLUtils warnings.");
                        addMarketObject(marketObjectData[i].obj, id, mp);
                    }
                    break;
                }
            }
            QL_REQUIRE(i < marketObjectData.size(),
                       "TodaysMarketParameters::fromXML(): node not recognized: " << XMLUtils::getNodeName(n));
        }
        n = XMLUtils::getNextSibling(n);
    } // while(n)
}

XMLNode* TodaysMarketParameters::toXML(XMLDocument& doc) const {

    XMLNode* todaysMarketNode = doc.allocNode("TodaysMarket");

    // configurations
    if (configurations_.size() > 0) {
        for (auto iterator = configurations_.begin(); iterator != configurations_.end(); iterator++) {
            XMLNode* configurationsNode = XMLUtils::addChild(doc, todaysMarketNode, "Configuration");
            XMLUtils::addAttribute(doc, configurationsNode, "id", iterator->first.c_str());
            for (Size i = 0; i < marketObjectData.size(); ++i) {
                XMLUtils::addChild(doc, configurationsNode, marketObjectData[i].xmlName + "Id",
                                   iterator->second(marketObjectData[i].obj)); // Added the "Id" for schema test
            }
        }
    }

    for (Size i = 0; i < marketObjectData.size(); ++i) {
        if (marketObjects_.find(marketObjectData[i].obj) != marketObjects_.end()) {
            auto mapping = marketObjects_.at(marketObjectData[i].obj);
            for (auto mappingSetIterator = mapping.begin(); mappingSetIterator != mapping.end(); mappingSetIterator++) {

                XMLNode* node = XMLUtils::addChild(doc, todaysMarketNode, marketObjectData[i].xmlName);
                XMLUtils::addAttribute(doc, node, "id", mappingSetIterator->first.c_str());

                for (auto singleMappingIterator = mappingSetIterator->second.begin();
                     singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                    // Again, swap indices are different...
                    if (marketObjectData[i].obj == MarketObject::SwapIndexCurve) {
                        XMLNode* swapIndexNode = XMLUtils::addChild(doc, node, marketObjectData[i].xmlSingleName.first);
                        XMLUtils::addAttribute(doc, swapIndexNode, marketObjectData[i].xmlSingleName.second,
                                               singleMappingIterator->first.c_str());
                        XMLUtils::addChild(doc, swapIndexNode, "Discounting",
                                           (string)singleMappingIterator->second.c_str());
                    } else {
                        XMLNode* singleMappingNode =
                            doc.allocNode(marketObjectData[i].xmlSingleName.first, singleMappingIterator->second);
                        XMLUtils::appendNode(node, singleMappingNode);
                        XMLUtils::addAttribute(doc, singleMappingNode, marketObjectData[i].xmlSingleName.second,
                                               singleMappingIterator->first);
                    }
                }
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
    for (Size i = 0; i < marketObjectData.size(); ++i) {
        MarketObject mo = marketObjectData[i].obj;
        // swap indices have to be excluded here...
        if (mo != MarketObject::SwapIndexCurve && marketObjects_.find(mo) != marketObjects_.end()) {
            curveSpecs(marketObjects_.at(mo), marketObjectId(mo, configuration), specs);
        }
    }
    return specs;
}

void TodaysMarketParameters::addMarketObject(const MarketObject o, const string& id,
                                             const map<string, string>& assignments) {

    // check that we do not have an inconsistent mapping within one market object
    auto mo = marketObjects_.find(o);
    if (mo != marketObjects_.end()) {
        auto mp = mo->second.find(id);
        if (mp != mo->second.end()) {
            for (auto const& m : mp->second) {
                auto a = assignments.find(m.first);
                if (a != assignments.end()) {
                    QL_REQUIRE(m.first != a->first || m.second == a->second,
                               "TodaysMarketParameters, inconsistent mapping is added for market object type "
                                   << o << ", id " << id << ": " << a->first << " " << a->second << ", but have "
                                   << m.first << " " << m.second << " already.");
                }
            }
        }
    }

    // check that we do not have an overlap of names for yield curves and index curves
    if (o == MarketObject::YieldCurve || o == MarketObject::IndexCurve) {
        auto mo =
            marketObjects_.find(o == MarketObject::YieldCurve ? MarketObject::IndexCurve : MarketObject::YieldCurve);
        if (mo != marketObjects_.end()) {
            auto mp = mo->second.find(id);
            if (mp != mo->second.end()) {
                for (auto const& m : mp->second) {
                    auto a = assignments.find(m.first);
                    QL_REQUIRE(a == assignments.end(),
                               "TodaysMarketParameters, overlap between YieldCurve and IndexCurve names, try to add "
                               "mapping for market object type "
                                   << o << ", id " << id << ": " << a->first << " " << a->second << ", but have "
                                   << m.first << " " << m.second << " already in other market object's mapping");
                }
            }
        }
    }

    // add the mapping
    marketObjects_[o][id].insert(assignments.begin(), assignments.end());
    for (auto s : assignments)
        DLOG("TodaysMarketParameters, add market objects of type " << o << ": " << id << " " << s.first << " "
                                                                   << s.second);
}

const map<string, string>& TodaysMarketParameters::mapping(const MarketObject o, const string& configuration) const {
    static map<string, string> empty;
    QL_REQUIRE(hasConfiguration(configuration), "configuration " << configuration << " not found");
    auto it = marketObjects_.find(o);
    if (it != marketObjects_.end()) {
        auto it2 = it->second.find(marketObjectId(o, configuration));
        if (it2 != it->second.end()) {
            return it2->second;
        }
    }
    return empty;
}

map<string, string>& TodaysMarketParameters::mappingReference(const MarketObject o, const string& configuration) {
    QL_REQUIRE(hasConfiguration(configuration), "configuration " << configuration << " not found");
    auto it = marketObjects_.find(o);
    if (it != marketObjects_.end()) {
        auto it2 = it->second.find(marketObjectId(o, configuration));
        if (it2 != it->second.end()) {
            return it2->second;
        } else {
	    return it->second[marketObjectId(o, configuration)];
        }
    } else {
        return marketObjects_[o][marketObjectId(o, configuration)];
    }
}

} // namespace data
} // namespace ore
