/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <orea/scenario/historicalscenarioreturn.hpp>
#include <boost/algorithm/string/find.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/termstructures/scenario.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

ReturnConfiguration::ReturnConfiguration() {
    const static std::vector<std::pair<RiskFactorKey::KeyType, ReturnConfiguration::ReturnType>> defaultConfig = {
        {RiskFactorKey::KeyType::DiscountCurve, ReturnConfiguration::ReturnType::Log},
        {RiskFactorKey::KeyType::YieldCurve, ReturnConfiguration::ReturnType::Log},
        {RiskFactorKey::KeyType::IndexCurve, ReturnConfiguration::ReturnType::Log},
        {RiskFactorKey::KeyType::SwaptionVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::YieldVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::OptionletVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::FXSpot, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::FXVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::EquitySpot, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::EquityVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::DividendYield, ReturnConfiguration::ReturnType::Log},
        {RiskFactorKey::KeyType::SurvivalProbability, ReturnConfiguration::ReturnType::Log},
        {RiskFactorKey::KeyType::RecoveryRate, ReturnConfiguration::ReturnType::Absolute},
        {RiskFactorKey::KeyType::CDSVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::BaseCorrelation, ReturnConfiguration::ReturnType::Absolute},
        {RiskFactorKey::KeyType::CPIIndex, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::ZeroInflationCurve, ReturnConfiguration::ReturnType::Absolute},
        {RiskFactorKey::KeyType::YoYInflationCurve, ReturnConfiguration::ReturnType::Absolute},
        {RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::CommodityCurve, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::CommodityVolatility, ReturnConfiguration::ReturnType::Relative},
        {RiskFactorKey::KeyType::SecuritySpread, ReturnConfiguration::ReturnType::Absolute},
        {RiskFactorKey::KeyType::Correlation, ReturnConfiguration::ReturnType::Absolute}};

    // Default Configuration for risk factor returns
    // For all yield curves we have DFs in the Scenario, for credit we have SurvProbs,
    // so a relative / log change is equivalent to an absolute zero / hazard rate change.
    for (const auto& [key, rt] : defaultConfig) {
        Return r;
        r.displacement = 0.0;
        r.type = rt;
        RiskFactorConfig data = std::make_pair(r, OverrideConfigs());
        returnType_.insert(std::make_pair(key, data));
    }
}

ReturnConfiguration::ReturnConfiguration(
    const std::map<RiskFactorKey::KeyType, ReturnConfiguration::ReturnType>& returnType) {
    for (const auto& [key, rt] : returnType) {
        Return r;
        r.displacement = 0.0;
        r.type = rt;
        RiskFactorConfig data = std::make_pair(r, OverrideConfigs());
        returnType_.insert(std::make_pair(key, data));
    }
}

ReturnConfiguration::ReturnConfiguration(
    const std::map<RiskFactorKey::KeyType, ReturnConfiguration::RiskFactorConfig>& configs)
    : returnType_(configs) {}

Real ReturnConfiguration::returnValue(const RiskFactorKey& key, const Real v1, const Real v2, const QuantLib::Date& d1,
                                      const QuantLib::Date& d2) const {

    // Checks
    check(key);

    const auto rt = returnType(key);
    // check if we have a config for the key otherwise use

    // Calculate the return
    switch (rt.type) {
    case ReturnConfiguration::ReturnType::Absolute:
        return v2 - v1;
        break;
    case ReturnConfiguration::ReturnType::Relative:
        if (!close(v1 + rt.displacement, 0)) {
            return (v2 + rt.displacement) / (v1 + rt.displacement) - 1.0;
        } else {
            ALOG("Cannot calculate the relative return for key " << key << " so just returning 0: (" << d1 << "," << v1
                                                                 << ") to (" << d2 << "," << v2 << ")");
            return 0.0;
        }
        break;
    case ReturnConfiguration::ReturnType::Log:
        if (!close(v1 + rt.displacement, 0) && (v2 + rt.displacement) / (v1 + rt.displacement) > 0) {
            return std::log((v2 + rt.displacement) / (v1 + rt.displacement));
        } else {
            ALOG("Cannot calculate the relative return for key " << key << " so just returning 0: (" << d1 << "," << v1
                                                                 << ") to (" << d2 << "," << v2 << ")");
            return 0.0;
        }
        break;
    default:
        QL_FAIL("ReturnConfiguration: return type not covered for key " << key << ".");
    }
}

Real ReturnConfiguration::applyReturn(const RiskFactorKey& key, const Real baseValue, const Real returnValue) const {

    // Checks
    check(key);
    const auto rt = returnType(key);
    // Apply the return to the base value to generate the return value
    Real value;
    switch (rt.type) {
    case ReturnConfiguration::ReturnType::Absolute:
        value = baseValue + returnValue;
        break;
    case ReturnConfiguration::ReturnType::Relative:
        value = baseValue * (1.0 + returnValue);
        break;
    case ReturnConfiguration::ReturnType::Log:
        value = baseValue * std::exp(returnValue);
        break;
    default:
        QL_FAIL("ReturnConfiguration: return type for key " << key << " not covered");
    }
    const auto keyType = key.keytype;
    // Apply cap / floors to guarantee admissable values
    if ((keyType == RiskFactorKey::KeyType::BaseCorrelation || keyType == RiskFactorKey::KeyType::Correlation) &&
        (value > 1.0 || value < -1.0)) {
        DLOG("Base correlation value, " << value << ", is not in range [-1.0, 1.0]");
        value = std::max(std::min(value, 1.0), -1.0);
        DLOG("Base correlation value amended to " << value);
    }

    if ((keyType == RiskFactorKey::KeyType::RecoveryRate || keyType == RiskFactorKey::KeyType::SurvivalProbability) &&
        (value > 1.0 || value < 0.0)) {
        DLOG("Value of risk factor " << key << ", " << value << ", is not in range [0.0, 1.0]");
        value = std::max(std::min(value, 1.0), 0.0);
        DLOG("Value of risk factor " << key << " amended to " << value);
    }

    return value;
}

const ReturnConfiguration::Return& ReturnConfiguration::returnType(const RiskFactorKey& key) const {
    const auto it = returnType_.find(key.keytype);
    QL_REQUIRE(it != returnType_.end(), "No ReturnType found for keyType" << key.keytype);
    const auto& [config, overrideConfigs] = it->second;
    const auto overrideIt = overrideConfigs.find(key.name);
    return (overrideIt == overrideConfigs.end()) ? config : overrideIt->second;
}

std::ostream& operator<<(std::ostream& out, const ReturnConfiguration::ReturnType t) {
    switch (t) {
    case ReturnConfiguration::ReturnType::Absolute:
        return out << "Absolute";
    case ReturnConfiguration::ReturnType::Relative:
        return out << "Relative";
    case ReturnConfiguration::ReturnType::Log:
        return out << "Log";
    default:
        return out << "Unknown ReturnType (" << static_cast<int>(t) << ")";
    }
}

void ReturnConfiguration::check(const RiskFactorKey& key) const {

    auto keyType = key.keytype;
    QL_REQUIRE(keyType != RiskFactorKey::KeyType::None, "unsupported key type none for key " << key);
    QL_REQUIRE(returnType_.find(keyType) != returnType_.end(),
               "ReturnConfiguration: key type " << keyType << " for key " << key << " not found");
}

ReturnConfiguration::ReturnType parseReturnType(const std::string& typeStr) {
    if (typeStr == "Log")
        return ReturnConfiguration::ReturnType::Log;
    else if (typeStr == "Absolute")
        return ReturnConfiguration::ReturnType::Absolute;
    else if (typeStr == "Relative")
        return ReturnConfiguration::ReturnType::Relative;
    else
        QL_FAIL("Unknown ReturnType: " << typeStr);
}

void ReturnConfiguration::fromXML(XMLNode* node) {
    returnType_.clear();
    XMLUtils::checkNode(node, "ReturnConfigurations");
    for (auto* rcNode : XMLUtils::getChildrenNodes(node, "ReturnConfiguration")) {
        // Key für den RiskFactor (z.B. "IR:EUR")
        std::string keyStr = XMLUtils::getAttribute(rcNode, "key");
        auto key = QuantExt::parseRiskFactorKeyType(keyStr);
        // Default Return
        XMLNode* retNode = XMLUtils::getChildNode(rcNode, "Return");
        QL_REQUIRE(retNode, "Return node missing in ReturnConfiguration");
        Return defaultReturn;
        std::string typeStr = XMLUtils::getChildValue(retNode, "Type", true);
        defaultReturn.type = parseReturnType(typeStr);
        defaultReturn.displacement = XMLUtils::getChildValueAsDouble(retNode, "Displacement", false, 0.0);

        // OverrideConfigs
        OverrideConfigs overrideConfigs;
        XMLNode* specNode = XMLUtils::getChildNode(rcNode, "OverrideConfigurations");
        if (specNode) {
            for (auto* sRetNode : XMLUtils::getChildrenNodes(specNode, "Return")) {
                std::string sKey = XMLUtils::getAttribute(sRetNode, "key");
                Return overrideReturn = defaultReturn; // Default übernehmen, falls nicht überschrieben
                std::string sTypeStr = XMLUtils::getChildValue(sRetNode, "Type", false);
                if (!sTypeStr.empty()) {
                    overrideReturn.type = parseReturnType(sTypeStr);
                }
                overrideReturn.displacement =
                    XMLUtils::getChildValueAsDouble(sRetNode, "Displacement", false, overrideReturn.displacement);
                overrideConfigs[sKey] = overrideReturn;
            }
        }
        returnType_[key] = std::make_pair(defaultReturn, overrideConfigs);
    }
}

XMLNode* ReturnConfiguration::toXML(XMLDocument& doc) const {
    XMLNode* root = doc.allocNode("ReturnConfigurations");
    for (const auto& [key, config] : returnType_) {
        const auto& [returnConfig, overrideConfigs] = config;
        XMLNode* rcNode = doc.allocNode("ReturnConfiguration");
        // Key als Attribut
        XMLUtils::addAttribute(doc, rcNode, "key", ore::data::to_string(key));
        // Default Return
        XMLNode* retNode = doc.allocNode("Return");
        XMLUtils::addChild(doc, retNode, "Type", ore::data::to_string(returnConfig.type));
        XMLUtils::addChild(doc, retNode, "Displacement", returnConfig.displacement);
        XMLUtils::appendNode(rcNode, retNode);

        // Specialized Configurations
        if (!overrideConfigs.empty()) {
            XMLNode* specNode = doc.allocNode("OverrideConfigurations");
            for (const auto& [overrideName, overrideRetrun] : overrideConfigs) {
                XMLNode* sRetNode = doc.allocNode("Return");
                XMLUtils::addAttribute(doc, sRetNode, "key", overrideName);
                XMLUtils::addChild(doc, sRetNode, "Type", ore::data::to_string(overrideRetrun.type));
                XMLUtils::addChild(doc, sRetNode, "Displacement", overrideRetrun.displacement);
                XMLUtils::appendNode(specNode, sRetNode);
            }
            XMLUtils::appendNode(rcNode, specNode);
        }
        XMLUtils::appendNode(root, rcNode);
    }
    return root;
}

} // namespace analytics
} // namespace ore
