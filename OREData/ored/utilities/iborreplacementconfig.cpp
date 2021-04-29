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

#include <ored/utilities/iborreplacementconfig.hpp>

#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

IborReplacementConfig::IborReplacementConfig() {}

bool IborReplacementConfig::useRfrCurveInTodaysMarket() const { return useRfrCurveInTodaysMarket_; }
bool IborReplacementConfig::useRfrCurveInSimulationMarket() const { return useRfrCurveInSimulationMarket_; }
bool IborReplacementConfig::enableIborReplacements() const { return enableIborReplacements_; }

void IborReplacementConfig::addIndexReplacementRule(const string& iborIndex, const ReplacementData& replacementData) {
    replacements_[iborIndex] = replacementData;
}

bool IborReplacementConfig::isIndexReplaced(const string& iborIndex, const Date& asof) const {
    auto i = replacements_.find(iborIndex);
    return i == replacements_.end() || i->second.switchDate > asof;
}

const IborReplacementConfig::ReplacementData& IborReplacementConfig::replacementData(const string& iborIndex) const {
    auto i = replacements_.find(iborIndex);
    QL_REQUIRE(
        i != replacements_.end(),
        "No replacement data found for ibor index '"
            << iborIndex
            << "', client code should check whether an index is replaced with IsIndexReplaced() before querying data.");
    return i->second;
}

void IborReplacementConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IborReplacementConfig");
    if (auto global = XMLUtils::getChildNode(node, "GlobalSettings")) {
        enableIborReplacements_ = XMLUtils::getChildValueAsBool(global, "EnableIborReplacements", true);
        useRfrCurveInTodaysMarket_ = XMLUtils::getChildValueAsBool(global, "UseRfrCurveInTodaysMarket", true);
        useRfrCurveInSimulationMarket_ = XMLUtils::getChildValueAsBool(global, "UseRfrCurveInSimulationMarket", true);
    }
    for (auto const repl : XMLUtils::getChildrenNodes(node, "Replacements")) {
        XMLUtils::checkNode(repl, "Replacement");
        string ibor = XMLUtils::getChildValue(repl, "IborIndex", true);
        replacements_[ibor].rfrIndex = XMLUtils::getChildValue(repl, "RfrIndex", true);
        replacements_[ibor].spread = parseReal(XMLUtils::getChildValue(repl, "Spread", true));
        replacements_[ibor].switchDate = parseDate(XMLUtils::getChildValue(repl, "SwitchDate", true));
    }
}

XMLNode* IborReplacementConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("IborReplacementConfig");
    XMLNode* global = XMLUtils::addChild(doc, node, "GlobalSettings");
    XMLUtils::addChild(doc, global, "EnableIborReplacements", enableIborReplacements_);
    XMLUtils::addChild(doc, global, "UseRfrCurveInTodaysMarket", useRfrCurveInTodaysMarket_);
    XMLUtils::addChild(doc, global, "UseRfrCurveInSimulationMarket", useRfrCurveInSimulationMarket_);
    XMLNode* repl = XMLUtils::addChild(doc, node, "Replacements");
    for (auto const& r : replacements_) {
        XMLNode* tmp = XMLUtils::addChild(doc, repl, "Replacement");
        XMLUtils::addChild(doc, tmp, "IborIndex", r.first);
        XMLUtils::addChild(doc, tmp, "RfrIndex", r.second.rfrIndex);
        XMLUtils::addChild(doc, tmp, "Spread", r.second.spread);
        XMLUtils::addChild(doc, tmp, "SwitchDate", ore::data::to_string(r.second.switchDate));
    }
    return node;
}

} // namespace data
} // namespace ore
