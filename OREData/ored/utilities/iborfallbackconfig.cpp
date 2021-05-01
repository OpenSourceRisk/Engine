/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/utilities/iborfallbackconfig.hpp>

#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

IborFallbackConfig::IborFallbackConfig() {}

bool IborFallbackConfig::useRfrCurveInTodaysMarket() const { return useRfrCurveInTodaysMarket_; }
bool IborFallbackConfig::useRfrCurveInSimulationMarket() const { return useRfrCurveInSimulationMarket_; }
bool IborFallbackConfig::enableIborFallbacks() const { return enableIborFallbacks_; }

void IborFallbackConfig::addIndexFallbackRule(const string& iborIndex, const FallbackData& fallbackData) {
    fallbacks_[iborIndex] = fallbackData;
}

bool IborFallbackConfig::isIndexReplaced(const string& iborIndex, const Date& asof) const {
    auto i = fallbacks_.find(iborIndex);
    return i == fallbacks_.end() || i->second.switchDate > asof;
}

const IborFallbackConfig::FallbackData& IborFallbackConfig::fallbackData(const string& iborIndex) const {
    auto i = fallbacks_.find(iborIndex);
    QL_REQUIRE(
        i != fallbacks_.end(),
        "No fallback data found for ibor index '"
            << iborIndex
            << "', client code should check whether an index is replaced with IsIndexReplaced() before querying data.");
    return i->second;
}

void IborFallbackConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IborFallbackConfig");
    if (auto global = XMLUtils::getChildNode(node, "GlobalSettings")) {
        enableIborFallbacks_ = XMLUtils::getChildValueAsBool(global, "EnableIborFallbacks", true);
        useRfrCurveInTodaysMarket_ = XMLUtils::getChildValueAsBool(global, "UseRfrCurveInTodaysMarket", true);
        useRfrCurveInSimulationMarket_ = XMLUtils::getChildValueAsBool(global, "UseRfrCurveInSimulationMarket", true);
    }
    for (auto const repl : XMLUtils::getChildrenNodes(node, "Fallbacks")) {
        XMLUtils::checkNode(repl, "Fallback");
        string ibor = XMLUtils::getChildValue(repl, "IborIndex", true);
        fallbacks_[ibor].rfrIndex = XMLUtils::getChildValue(repl, "RfrIndex", true);
        fallbacks_[ibor].spread = parseReal(XMLUtils::getChildValue(repl, "Spread", true));
        fallbacks_[ibor].switchDate = parseDate(XMLUtils::getChildValue(repl, "SwitchDate", true));
    }
}

XMLNode* IborFallbackConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("IborFallbackConfig");
    XMLNode* global = XMLUtils::addChild(doc, node, "GlobalSettings");
    XMLUtils::addChild(doc, global, "EnableIborFallbacks", enableIborFallbacks_);
    XMLUtils::addChild(doc, global, "UseRfrCurveInTodaysMarket", useRfrCurveInTodaysMarket_);
    XMLUtils::addChild(doc, global, "UseRfrCurveInSimulationMarket", useRfrCurveInSimulationMarket_);
    XMLNode* repl = XMLUtils::addChild(doc, node, "Fallbacks");
    for (auto const& r : fallbacks_) {
        XMLNode* tmp = XMLUtils::addChild(doc, repl, "Fallback");
        XMLUtils::addChild(doc, tmp, "IborIndex", r.first);
        XMLUtils::addChild(doc, tmp, "RfrIndex", r.second.rfrIndex);
        XMLUtils::addChild(doc, tmp, "Spread", r.second.spread);
        XMLUtils::addChild(doc, tmp, "SwitchDate", ore::data::to_string(r.second.switchDate));
    }
    return node;
}

} // namespace data
} // namespace ore
