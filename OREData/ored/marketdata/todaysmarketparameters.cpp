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
static const Size numberOfMarketObjects = 23;
// clang-format off
// Note the order of elements in this array must respect the XML Schema
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
// clang-format on

// check that the lists above have all the correct length
static_assert(numberOfMarketObjects == sizeof(marketObjectStrings) / sizeof(marketObjectStrings[0]),
              "numberOfMarketObjects is inconsistent with marketObjectStrings");
static_assert(numberOfMarketObjects == sizeof(marketObjectXMLNames) / sizeof(marketObjectXMLNames[0]),
              "numberOfMarketObjects is inconsistent with marketObjectXMLNames");
static_assert(numberOfMarketObjects == sizeof(marketObjectXMLNamesSingle) / sizeof(marketObjectXMLNamesSingle[0]),
              "numberOfMarketObjects is inconsistent with marketObjectXMLNamesSingle");
} // anonymous namespace

std::ostream& operator<<(std::ostream& out, const MarketObject& o) {
    Size idx = static_cast<Size>(o);
    if (idx > numberOfMarketObjects)
        return out << "Unknown";
    else
        return out << marketObjectStrings[idx];
}

MarketConfiguration::MarketConfiguration() {
    for (Size i = 0; i < numberOfMarketObjects; ++i) {
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
            for (Size i = 0; i < numberOfMarketObjects; ++i) {
                tmp.setId(MarketObject(i), XMLUtils::getChildValue(n, marketObjectXMLNames[i] + "Id", false));
                addConfiguration(XMLUtils::getAttribute(n, "id"), tmp);
            }
        } else {
            Size i = 0;
            for (; i < numberOfMarketObjects; ++i) {
                if (XMLUtils::getNodeName(n) == marketObjectXMLNames[i]) {
                    string id = XMLUtils::getAttribute(n, "id");
                    if (id == "")
                        id = Market::defaultConfiguration;
                    // The XML schema for swap indices is different ...
                    if (MarketObject(i) == MarketObject::SwapIndexCurve) {
                        vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(n, marketObjectXMLNamesSingle[i].first);
                        map<string, string> swapIndices;
                        for (XMLNode* xn : nodes) {
                            string name = XMLUtils::getAttribute(xn, marketObjectXMLNamesSingle[i].second);
                            QL_REQUIRE(name != "", "no name given for SwapIndex");
                            QL_REQUIRE(swapIndices.find(name) == swapIndices.end(),
                                       "Duplicate SwapIndex found for " << name);
                            string disc = XMLUtils::getChildValue(xn, "Discounting", true);
                            swapIndices[name] = disc; //.emplace(name, { ibor, disc }); won't work?
                        }
                        addMarketObject(MarketObject::SwapIndexCurve, id, swapIndices);

                    } else {
                        auto mp = XMLUtils::getChildrenAttributesAndValues(n, marketObjectXMLNamesSingle[i].first,
                                                                           marketObjectXMLNamesSingle[i].second, false);
                        Size nc = XMLUtils::getChildrenNodes(n, "").size();
                        QL_REQUIRE(mp.size() == nc, "could not recognise " << (nc - mp.size()) << " sub nodes under "
                                                                           << marketObjectXMLNames[i]);
                        addMarketObject(MarketObject(i), id, mp);
                    }
                    break;
                }
            }
            QL_REQUIRE(i < numberOfMarketObjects,
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
            for (Size i = 0; i < numberOfMarketObjects; ++i) {
                XMLUtils::addChild(doc, configurationsNode, marketObjectXMLNames[i] + "Id",
                                   iterator->second(MarketObject(i))); // Added the "Id" for schema test
            }
        }
    }

    for (Size i = 0; i < numberOfMarketObjects; ++i) {
        if (marketObjects_.find(MarketObject(i)) != marketObjects_.end()) {
            auto mapping = marketObjects_.at(MarketObject(i));
            for (auto mappingSetIterator = mapping.begin(); mappingSetIterator != mapping.end(); mappingSetIterator++) {

                XMLNode* node = XMLUtils::addChild(doc, todaysMarketNode, marketObjectXMLNames[i]);
                XMLUtils::addAttribute(doc, node, "id", mappingSetIterator->first.c_str());

                for (auto singleMappingIterator = mappingSetIterator->second.begin();
                     singleMappingIterator != mappingSetIterator->second.end(); singleMappingIterator++) {
                    // Again, swap indices are different...
                    if (MarketObject(i) == MarketObject::SwapIndexCurve) {
                        XMLNode* swapIndexNode = XMLUtils::addChild(doc, node, marketObjectXMLNamesSingle[i].first);
                        XMLUtils::addAttribute(doc, swapIndexNode, marketObjectXMLNamesSingle[i].second,
                                               singleMappingIterator->first.c_str());
                        XMLUtils::addChild(doc, swapIndexNode, "Discounting",
                                           (string)singleMappingIterator->second.c_str());
                    } else {
                        XMLNode* singleMappingNode =
                            doc.allocNode(marketObjectXMLNamesSingle[i].first, singleMappingIterator->second);
                        XMLUtils::appendNode(node, singleMappingNode);
                        XMLUtils::addAttribute(doc, singleMappingNode, marketObjectXMLNamesSingle[i].second,
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
    for (Size i = 0; i < numberOfMarketObjects; ++i) {
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
