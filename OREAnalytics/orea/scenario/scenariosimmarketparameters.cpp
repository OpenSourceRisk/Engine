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

namespace {
const vector<Period>& returnTenors(const map<string, vector<Period>>& m, const string& k) {
    if (m.count(k) > 0) {
        return m.at(k);
    } else if (m.count("") > 0) {
        return m.at("");
    } else
        QL_FAIL("no period vector for key \"" << k << "\" found.");
}
} // namespace

const vector<Period>& ScenarioSimMarketParameters::yieldCurveTenors(const string& key) const {
    return returnTenors(yieldCurveTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::capFloorVolExpiries(const string& key) const {
    return returnTenors(capFloorVolExpiries_, key);
}

const vector<Period>& ScenarioSimMarketParameters::defaultTenors(const string& key) const {
    return returnTenors(defaultTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::equityTenors(const string& key) const {
    return returnTenors(eqTenors_, key);
}

void ScenarioSimMarketParameters::setYieldCurveTenors(const string& key, const std::vector<Period>& p) {
    yieldCurveTenors_[key] = p;
}

void ScenarioSimMarketParameters::setCapFloorVolExpiries(const string& key, const std::vector<Period>& p) {
    capFloorVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setDefaultTenors(const string& key, const std::vector<Period>& p) {
    defaultTenors_[key] = p;
}

void ScenarioSimMarketParameters::setEquityTenors(const string& key, const std::vector<Period>& p) {
    eqTenors_[key] = p;
}

bool ScenarioSimMarketParameters::operator==(const ScenarioSimMarketParameters& rhs) {

    if (baseCcy_ != rhs.baseCcy_ || ccys_ != rhs.ccys_ || yieldCurveNames_ != rhs.yieldCurveNames_ ||
        yieldCurveCurrencies_ != rhs.yieldCurveCurrencies_ || yieldCurveTenors_ != rhs.yieldCurveTenors_ ||
        indices_ != rhs.indices_ || swapIndices_ != rhs.swapIndices_ || interpolation_ != rhs.interpolation_ ||
        extrapolate_ != rhs.extrapolate_ || swapVolTerms_ != rhs.swapVolTerms_ || swapVolCcys_ != rhs.swapVolCcys_ ||
        swapVolSimulate_ != rhs.swapVolSimulate_ || swapVolExpiries_ != rhs.swapVolExpiries_ ||
        swapVolDecayMode_ != rhs.swapVolDecayMode_ || capFloorVolSimulate_ != rhs.capFloorVolSimulate_ ||
        capFloorVolCcys_ != rhs.capFloorVolCcys_ || capFloorVolExpiries_ != rhs.capFloorVolExpiries_ ||
        capFloorVolStrikes_ != rhs.capFloorVolStrikes_ || capFloorVolDecayMode_ != rhs.capFloorVolDecayMode_ ||
        defaultNames_ != rhs.defaultNames_ || defaultTenors_ != rhs.defaultTenors_ || eqNames_ != rhs.eqNames_ ||
        eqTenors_ != rhs.eqTenors_ || fxVolSimulate_ != rhs.fxVolSimulate_ || fxVolExpiries_ != rhs.fxVolExpiries_ ||
        fxVolDecayMode_ != rhs.fxVolDecayMode_ || fxVolCcyPairs_ != rhs.fxVolCcyPairs_ ||
        fxCcyPairs_ != rhs.fxCcyPairs_ || eqVolSimulate_ != rhs.eqVolSimulate_ ||
        eqVolExpiries_ != rhs.eqVolExpiries_ || eqVolDecayMode_ != rhs.eqVolDecayMode_ ||
        eqVolNames_ != rhs.eqVolNames_ || additionalScenarioDataIndices_ != rhs.additionalScenarioDataIndices_ ||
        additionalScenarioDataCcys_ != rhs.additionalScenarioDataCcys_ || securities_ != rhs.securities_) {
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

    XMLNode* nodeChild = XMLUtils::getChildNode(node, "BenchmarkCurves");
    if (nodeChild) {
        yieldCurveNames_ = XMLUtils::getChildrenValues(nodeChild, "BenchmarkCurve", "Name");
        yieldCurveCurrencies_ = XMLUtils::getChildrenValues(nodeChild, "BenchmarkCurve", "Currency");
    }

    nodeChild = XMLUtils::getChildNode(node, "YieldCurves");
    nodeChild = XMLUtils::getChildNode(nodeChild, "Configuration");
    yieldCurveTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    // TODO read other keys
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

    nodeChild = XMLUtils::getChildNode(node, "FxRates");
    if (nodeChild)
        fxCcyPairs_ = XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true);
    else {
        fxCcyPairs_.resize(0);
        for (auto ccy : ccys_) {
            if (ccy != baseCcy_)
                fxCcyPairs_.push_back(ccy + baseCcy_);
        }
    }

    nodeChild = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    swapVolSimulate_ = false;
    XMLNode* swapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
    if (swapVolSimNode)
        swapVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(swapVolSimNode));
    swapVolTerms_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Terms", true);
    swapVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
    swapVolCcys_ = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", true);
    swapVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

    nodeChild = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    if (nodeChild) {
        capFloorVolSimulate_ = false;
        XMLNode* capVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (capVolSimNode)
            capFloorVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(capVolSimNode));
        capFloorVolExpiries_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        // TODO read other keys
        capFloorVolStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "Strikes", true);
        capFloorVolCcys_ = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", true);
        capFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    }

    nodeChild = XMLUtils::getChildNode(node, "DefaultCurves");
    defaultNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
    defaultTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    // TODO read other keys

    nodeChild = XMLUtils::getChildNode(node, "Equities");
    if (nodeChild) {
        eqNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        eqTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
        // TODO read other keys
    } else {
        eqNames_.clear();
        eqTenors_.clear();
    }

    nodeChild = XMLUtils::getChildNode(node, "FxVolatilities");
    fxVolSimulate_ = false;
    XMLNode* fxVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
    if (fxVolSimNode)
        fxVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(fxVolSimNode));
    fxVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
    fxVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    fxVolCcyPairs_ = XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true);

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

    nodeChild = XMLUtils::getChildNode(node, "Securities");
    if (nodeChild)
        securities_ = XMLUtils::getChildrenValues(node, "Securities", "Security");
}

XMLNode* ScenarioSimMarketParameters::toXML(XMLDocument& doc) {

    XMLNode* marketNode = doc.allocNode("Market");

    // currencies
    XMLUtils::addChild(doc, marketNode, "BaseCurrency", baseCcy_);
    XMLUtils::addChildren(doc, marketNode, "Currencies", "Currency", ccys_);

    // benchmark yield curves
    XMLNode* benchmarkCurvesNode = XMLUtils::addChild(doc, marketNode, "BenchmarkCurves");
    for (Size i = 0; i < yieldCurveNames_.size(); ++i) {
        XMLNode* benchmarkCurveNode = XMLUtils::addChild(doc, benchmarkCurvesNode, "BenchmarkCurve");
        XMLUtils::addChild(doc, benchmarkCurveNode, "Currency", yieldCurveCurrencies_[i]);
        XMLUtils::addChild(doc, benchmarkCurveNode, "Name", yieldCurveNames_[i]);
    }

    // yield curves
    XMLNode* yieldCurvesNode = XMLUtils::addChild(doc, marketNode, "YieldCurves");
    XMLNode* configurationNode = XMLUtils::addChild(doc, yieldCurvesNode, "Configuration");
    XMLUtils::addGenericChildAsList(doc, configurationNode, "Tenors", returnTenors(yieldCurveTenors_, ""));
    // TODO write other keys
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
    XMLUtils::addGenericChildAsList(doc, defaultCurvesNode, "Tenors", returnTenors(defaultTenors_, ""));
    // TODO write other keys

    // equities
    XMLNode* equitiesNode = XMLUtils::addChild(doc, marketNode, "Equities");
    XMLUtils::addChildren(doc, equitiesNode, "Names", "Name", eqNames_);
    XMLUtils::addGenericChildAsList(doc, equitiesNode, "Tenors", returnTenors(eqTenors_, ""));
    // TODO write other keys

    // swaption volatilities
    XMLNode* swaptionVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "SwaptionVolatilities");
    XMLUtils::addChild(doc, swaptionVolatilitiesNode, "Simulate", swapVolSimulate_);
    XMLUtils::addChild(doc, swaptionVolatilitiesNode, "ReactionToTimeDecay", swapVolDecayMode_);
    XMLUtils::addChildren(doc, swaptionVolatilitiesNode, "Currencies", "Currency", swapVolCcys_);
    XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Expiries", swapVolExpiries_);
    XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Terms", swapVolTerms_);

    // cap/floor volatilities
    XMLNode* capFloorVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "CapFloorVolatilities");
    XMLUtils::addChild(doc, capFloorVolatilitiesNode, "Simulate", capFloorVolSimulate_);
    XMLUtils::addChild(doc, capFloorVolatilitiesNode, "ReactionToTimeDecay", capFloorVolDecayMode_);
    XMLUtils::addChildren(doc, capFloorVolatilitiesNode, "Currencies", "Currency", capFloorVolCcys_);
    XMLUtils::addGenericChildAsList(doc, capFloorVolatilitiesNode, "Expiries", returnTenors(capFloorVolExpiries_, ""));
    // TODO write other keys
    XMLUtils::addGenericChildAsList(doc, capFloorVolatilitiesNode, "Strikes", capFloorVolStrikes_);

    // fx volatilities
    XMLNode* fxVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "FxVolatilities");
    XMLUtils::addChild(doc, fxVolatilitiesNode, "Simulate", fxVolSimulate_);
    XMLUtils::addChild(doc, fxVolatilitiesNode, "ReactionToTimeDecay", fxVolDecayMode_);
    XMLUtils::addChildren(doc, fxVolatilitiesNode, "CurrencyPairs", "CurrencyPair", fxVolCcyPairs_);
    XMLUtils::addGenericChildAsList(doc, fxVolatilitiesNode, "Expiries", fxVolExpiries_);

    // fx rates
    XMLNode* fxRatesNode = XMLUtils::addChild(doc, marketNode, "FxRates");
    XMLUtils::addChildren(doc, fxRatesNode, "CurrencyPairs", "CurrencyPair", fxCcyPairs_);

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

    // additional scenario data indices
    XMLUtils::addChildren(doc, marketNode, "Securities", "Security", securities_);

    return marketNode;
}
} // namespace analytics
} // namespace ore
