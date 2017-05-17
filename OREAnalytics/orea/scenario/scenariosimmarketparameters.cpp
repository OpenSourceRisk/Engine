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

#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

bool ScenarioSimMarketParameters::operator==(const ScenarioSimMarketParameters& rhs) {

    if (baseCcy_ != rhs.baseCcy_ || ccys_ != rhs.ccys_ || yieldCurveTenors_ != rhs.yieldCurveTenors_ ||
        indices_ != rhs.indices_ || swapIndices_ != rhs.swapIndices_ || interpolation_ != rhs.interpolation_ ||
        extrapolate_ != rhs.extrapolate_ || swapVolTerms_ != rhs.swapVolTerms_ || swapVolCcys_ != rhs.swapVolCcys_ ||
        swapVolExpiries_ != rhs.swapVolExpiries_ || swapVolDecayMode_ != rhs.swapVolDecayMode_ ||
        capFloorVolCcys_ != rhs.capFloorVolCcys_ || capFloorVolDecayMode_ != rhs.capFloorVolDecayMode_ ||
        defaultNames_ != rhs.defaultNames_ || defaultTenors_ != rhs.defaultTenors_ || eqNames_ != rhs.eqNames_ ||
        eqTenors_ != rhs.eqTenors_ || fxVolSimulate_ != rhs.fxVolSimulate_ || fxVolExpiries_ != rhs.fxVolExpiries_ ||
        fxVolDecayMode_ != rhs.fxVolDecayMode_ || ccyPairs_ != rhs.ccyPairs_ || eqVolSimulate_ != rhs.eqVolSimulate_ ||
        eqVolExpiries_ != rhs.eqVolExpiries_ || eqVolDecayMode_ != rhs.eqVolDecayMode_ ||
        eqVolNames_ != rhs.eqVolNames_ || additionalScenarioDataIndices_ != rhs.additionalScenarioDataIndices_ ||
        additionalScenarioDataCcys_ != rhs.additionalScenarioDataCcys_) {
        return false;
    } else {
        return true;
    }
}

bool ScenarioSimMarketParameters::operator!=(const ScenarioSimMarketParameters& rhs) { return !(*this == rhs); }

void ScenarioSimMarketParameters::fromXML(XMLNode* root) {
    XMLNode* sim = XMLUtils::locateNode(root, "Simulation");
    XMLNode* node = XMLUtils::getChildNode(sim, "Market");
    XMLUtils::checkNode(node, "Market");

    // TODO: add in checks (checkNode or QL_REQUIRE) on mandatory nodes

    baseCcy_ = XMLUtils::getChildValue(node, "BaseCurrency");
    ccys_ = XMLUtils::getChildrenValues(node, "Currencies", "Currency");

    XMLNode* nodeChild = XMLUtils::getChildNode(node, "YieldCurves");
    nodeChild = XMLUtils::getChildNode(nodeChild, "Configuration");
    yieldCurveTenors_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    interpolation_ = XMLUtils::getChildValue(nodeChild, "Interpolation", true);
    extrapolate_ = XMLUtils::getChildValueAsBool(nodeChild, "Extrapolate");

    indices_ = XMLUtils::getChildrenValues(node, "Indices", "Index");

    nodeChild = XMLUtils::getChildNode(node, "SwapIndices");
    if (nodeChild) {
        for (XMLNode* n = XMLUtils::getChildNode(nodeChild, "SwapIndex"); n != nullptr;
             n = XMLUtils::getNextSibling(n, "SwapIndex")) {
            string name = XMLUtils::getChildValue(n, "Name");
            string disc = XMLUtils::getChildValue(n, "DiscountingIndex");
            swapIndices_[name] = disc;
        }
    }

    nodeChild = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    swapVolTerms_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Terms", true);
    swapVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
    swapVolCcys_ = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", true);
    swapVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

    nodeChild = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    if (nodeChild) {
        capFloorVolCcys_ = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", true);
        capFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    }

    nodeChild = XMLUtils::getChildNode(node, "DefaultCurves");
    defaultNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
    defaultTenors_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);

    nodeChild = XMLUtils::getChildNode(node, "Equities");
    if (nodeChild) {
        eqNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        eqTenors_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    } else {
        eqNames_.clear();
        eqTenors_.clear();
    }

    nodeChild = XMLUtils::getChildNode(node, "FxVolatilities");
    fxVolSimulate_ = XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true);
    fxVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
    fxVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    ccyPairs_ = XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true);

    nodeChild = XMLUtils::getChildNode(node, "EquityVolatilities");
    if (nodeChild) {
        eqVolSimulate_ = XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true);
        eqVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        eqVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
        eqVolNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
    } else {
        eqVolSimulate_ = false;
        eqVolExpiries_.clear();
        eqVolNames_.clear();
    }

    additionalScenarioDataIndices_ = XMLUtils::getChildrenValues(node, "AggregationScenarioDataIndices", "Index");
    additionalScenarioDataCcys_ =
        XMLUtils::getChildrenValues(node, "AggregationScenarioDataCurrencies", "Currency", true);
}

XMLNode* ScenarioSimMarketParameters::toXML(XMLDocument& doc) {

    XMLNode* marketNode = doc.allocNode("Market");

    // currencies
    XMLUtils::addChild(doc, marketNode, "BaseCurrency", baseCcy_);
    XMLUtils::addChildren(doc, marketNode, "Currencies", "Currency", ccys_);

    // yield curves
    XMLNode* yieldCurvesNode = XMLUtils::addChild(doc, marketNode, "YieldCurves");
    XMLNode* configurationNode = XMLUtils::addChild(doc, yieldCurvesNode, "Configuration");
    XMLUtils::addGenericChildAsList(doc, configurationNode, "Tenors", yieldCurveTenors_);
    XMLUtils::addChild(doc, configurationNode, "Interpolation", interpolation_);
    XMLUtils::addChild(doc, configurationNode, "Extrapolation", extrapolate_);

    // indices
    XMLUtils::addChildren(doc, marketNode, "Indices", "Index", indices_);

    // swap indices
    XMLNode* swapIndicesNode = XMLUtils::addChild(doc, marketNode, "SwapIndices");
    for (auto swapIndexInterator : swapIndices_) {
        XMLNode* swapIndexNode = XMLUtils::addChild(doc, swapIndicesNode, "SwapIndex");
        XMLUtils::addChild(doc, swapIndexNode, "Name", swapIndexInterator.first);
        XMLUtils::addChild(doc, swapIndexNode, "DiscountingIndex", swapIndexInterator.second);
    }

    // default curves
    XMLNode* defaultCurvesNode = XMLUtils::addChild(doc, marketNode, "DefaultCurves");
    XMLUtils::addChildren(doc, defaultCurvesNode, "Names", "Name", defaultNames_);
    XMLUtils::addGenericChildAsList(doc, defaultCurvesNode, "Tenors", defaultTenors_);

    // equities
    XMLNode* equitiesNode = XMLUtils::addChild(doc, marketNode, "Equities");
    XMLUtils::addChildren(doc, equitiesNode, "Names", "Name", eqNames_);
    XMLUtils::addGenericChildAsList(doc, equitiesNode, "Tenors", eqTenors_);

    // swaption volatilities
    XMLNode* swaptionVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "SwaptionVolatilities");
    XMLUtils::addChild(doc, swaptionVolatilitiesNode, "ReactionToTimeDecay", swapVolDecayMode_);
    XMLUtils::addChildren(doc, swaptionVolatilitiesNode, "Currencies", "Currency", swapVolCcys_);
    XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Expiries", swapVolExpiries_);
    XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Terms", swapVolTerms_);

    // fx volatilities
    XMLNode* fxVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "FxVolatilities");
    XMLUtils::addChild(doc, fxVolatilitiesNode, "Simulate", fxVolSimulate_);
    XMLUtils::addChild(doc, fxVolatilitiesNode, "ReactionToTimeDecay", fxVolDecayMode_);
    XMLUtils::addChildren(doc, fxVolatilitiesNode, "CurrencyPairs", "CurrencyPair", ccyPairs_);
    XMLUtils::addGenericChildAsList(doc, fxVolatilitiesNode, "Expiries", fxVolExpiries_);

    // eq volatilities
    XMLNode* eqVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "EquityVolatilities");
    XMLUtils::addChild(doc, eqVolatilitiesNode, "Simulate", eqVolSimulate_);
    XMLUtils::addChild(doc, eqVolatilitiesNode, "ReactionToTimeDecay", eqVolDecayMode_);
    XMLUtils::addChildren(doc, eqVolatilitiesNode, "Names", "Name", eqVolNames_);
    XMLUtils::addGenericChildAsList(doc, eqVolatilitiesNode, "Expiries", eqVolExpiries_);

    // additional scenario data currencies
    XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataCurrencies", "Currency",
                          additionalScenarioDataCcys_);

    // additional scenario data indices
    XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataIndices", "Index", additionalScenarioDataIndices_);

    return marketNode;
}
} // namespace analytics
} // namespace ore
