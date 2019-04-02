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
  enum MarketObject obj;
  string name;
  string xmlName;
  pair<string, string> xmlSingleName;
};

// Note the order of elements in this array MUST respect the XML Schema
static const vector<MarketObjectMetaInfo> marketObjectData = {
    { MarketObject::YieldCurve,     "YieldCurve",      "YieldCurves",        { "YieldCurve",       "name" } },
    { MarketObject::DiscountCurve,  "DiscountCurve",   "DiscountingCurves",  { "DiscountingCurve", "currency" } }
};

/*
static const string marketObjectStrings[] = {"YieldCurve", "DiscountCurve", "IndexCurve", "SwapIndexCurve",
                                             "ZeroInflationCurve", "ZeroInflationCapFloorVol", "YoYInflationCurve",
                                             "FXSpot", "BaseCorrelation", "FXVol", "SwaptionVol", "CapFloorVol", "CDSVol",
                                             "DefaultCurve", "InflationCapFloorPriceSurface", "YoYInflationCapFloorPriceSurface",
                                             "YoYInflationCapFloorVol", "EquityCurves", "EquityVols",
                                             "Securities", "CommodityCurves", "CommodityVolatilities", "Correlation"};
static const string marketObjectXMLNames[] = {"YieldCurves", "DiscountingCurves", "IndexForwardingCurves",
                                              "SwapIndexCurves", "ZeroInflationIndexCurves", "ZeroInflationCapFloorVolatilities",
                                              "YYInflationIndexCurves", "FxSpots", "BaseCorrelations", "FxVolatilities",
                                              "SwaptionVolatilities", "CapFloorVolatilities", "CDSVolatilities", "DefaultCurves",
                                              "InflationCapFloorPriceSurfaces", "YYInflationCapFloorPriceSurfaces",
                                              "YYInflationCapFloorVolatilities", "EquityCurves", "EquityVolatilities",
                                              "Securities", "CommodityCurves", "CommodityVolatilities", "Correlations"};
static const pair<string, string> marketObjectXMLNamesSingle[] = {
    {"YieldCurve", "name"}, {"DiscountingCurve", "currency"}, {"Index", "name"}, {"SwapIndex", "name"},
    {"ZeroInflationIndexCurve", "name"}, {"ZeroInflationCapFloorVolatility", "name" },
    {"YYInflationIndexCurve", "name"}, {"FxSpot", "pair"}, {"BaseCorrelation", "name"}, {"FxVolatility", "pair"},
    {"SwaptionVolatility", "currency"}, {"CapFloorVolatility", "currency"}, {"CDSVolatility", "name"},
    {"DefaultCurve", "name"}, {"InflationCapFloorPriceSurface", "name"},
    {"YYInflationCapFloorPriceSurface", "name"}, {"YYInflationCapFloorVolatility", "name"},
    {"EquityCurve", "name"}, {"EquityVolatility", "name"}, {"Security", "name"},
    {"CommodityCurve", "name"}, {"CommodityVolatility", "name"}, {"Correlation", "name"}};

*/
} // anonymous namespace

std::ostream& operator<<(std::ostream& out, const MarketObject& o) {
    for (Size i = 0; i < marketObjectData.size(); i++) {
        if (marketObjectData[i].obj == o)
            return out << marketObjectData[i].name;
    }
    return out << "Unknown";
}

MarketConfiguration::MarketConfiguration() {
    for (Size i = 0; i < marketObjectData.size(); ++i) {
        marketObjectIds_[MarketObject(i)] = Market::defaultConfiguration;
    }
}

void TodaysMarketParameters::fromXML(XMLNode* node) {

    // clear data members
    configurations_.clear();
    marketObjects_.clear();

    // add default configuration (may be overwritten below)
    MarketConfiguration defaultConfig;
    addConfiguration(Market::defaultConfiguration, defaultConfig);

    // fill data from XML
    XMLUtils::checkNode(node, "TodaysMarket");
    XMLNode* n = XMLUtils::getChildNode(node);
    while (n) {
        if (XMLUtils::getNodeName(n) == "Configuration") {
            MarketConfiguration tmp;
            for (Size i = 0; i < marketObjectData.size(); ++i) {
                tmp.setId(MarketObject(i), XMLUtils::getChildValue(n, marketObjectData[i].xmlName + "Id", false));
                addConfiguration(XMLUtils::getAttribute(n, "id"), tmp);
            }
        } else {
            Size i = 0;
            for (; i < marketObjectData.size(); ++i) {
                if (XMLUtils::getNodeName(n) == marketObjectData[i].name) {
                    string id = XMLUtils::getAttribute(n, "id");
                    if (id == "")
                        id = Market::defaultConfiguration;
                    // The XML schema for swap indices is different ...
                    if (MarketObject(i) == MarketObject::SwapIndexCurve) {
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
                        auto mp = XMLUtils::getChildrenAttributesAndValues(n, marketObjectData[i].xmlSingleName.first,
                                                                           marketObjectData[i].xmlSingleName.second, false);
                        Size nc = XMLUtils::getChildrenNodes(n, "").size();
                        QL_REQUIRE(mp.size() == nc, "could not recognise " << (nc - mp.size()) << " sub nodes under "
                                                                           << marketObjectData[i].xmlName);
                        addMarketObject(MarketObject(i), id, mp);
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

XMLNode* TodaysMarketParameters::toXML(XMLDocument& doc) {

    XMLNode* todaysMarketNode = doc.allocNode("TodaysMarket");

    // configurations
    if (configurations_.size() > 0) {
        for (auto iterator = configurations_.begin(); iterator != configurations_.end(); iterator++) {
            XMLNode* configurationsNode = XMLUtils::addChild(doc, todaysMarketNode, "Configuration");
            XMLUtils::addAttribute(doc, configurationsNode, "id", iterator->first.c_str());
            for (Size i = 0; i < marketObjectData.size(); ++i) {
                XMLUtils::addChild(doc, configurationsNode, marketObjectData[i].xmlName + "Id",
                                   iterator->second(MarketObject(i))); // Added the "Id" for schema test
            }
        }
    }

    for (Size i = 0; i < marketObjectData.size(); ++i) {
        if (marketObjects_.find(MarketObject(i)) != marketObjects_.end()) {
            auto mapping = marketObjects_.at(MarketObject(i));
            for (auto mappingSetIterator = mapping.begin(); mappingSetIterator != mapping.end(); mappingSetIterator++) {

                XMLNode* node = XMLUtils::addChild(doc, todaysMarketNode, marketObjectData[i].xmlName);
                XMLUtils::addAttribute(doc, node, "id", mappingSetIterator->first.c_str());

                for (auto singleMappingIterator = mappingSetIterator->second.begin();
                     singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                    // Again, swap indices are different...
                    if (MarketObject(i) == MarketObject::SwapIndexCurve) {
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
        // swap indices have to be exlcuded here...
        if (MarketObject(i) != MarketObject::SwapIndexCurve &&
            marketObjects_.find(MarketObject(i)) != marketObjects_.end()) {
            curveSpecs(marketObjects_.at(MarketObject(i)), marketObjectId(MarketObject(i), configuration), specs);
        }
    }
    return specs;
}

} // namespace data
} // namespace ore
