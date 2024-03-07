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

#include <ored/portfolio/enginedata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EngineData::clear() {
    modelParams_.clear();
    model_.clear();
    engine_.clear();
    engineParams_.clear();
    globalParams_.clear();
}

void EngineData::fromXML(XMLNode* root) {
    XMLUtils::checkNode(root, "PricingEngines");

    // Get global parameters if there are any
    if (XMLNode* node = XMLUtils::getChildNode(root, "GlobalParameters")) {
        DLOG("Processing the GlobalParameters node");
        globalParams_ = XMLUtils::getChildrenAttributesAndValues(node, "Parameter", "name", false);
    }

    for (XMLNode* node = XMLUtils::getChildNode(root, "Product"); node;
         node = XMLUtils::getNextSibling(node, "Product")) {
        std::string productName = XMLUtils::getAttribute(node, "type");

        model_[productName] = XMLUtils::getChildValue(node, "Model");
        DLOG("EngineData product=" << productName << " model=" << model_[productName]);

        XMLNode* modelParams = XMLUtils::getChildNode(node, "ModelParameters");
        map<string, string> modelParamMap;
        for (XMLNode* paramNode = XMLUtils::getChildNode(modelParams, "Parameter"); paramNode;
             paramNode = XMLUtils::getNextSibling(paramNode, "Parameter")) {
            string paramName = XMLUtils::getAttribute(paramNode, "name");
            string paramValue = XMLUtils::getNodeValue(paramNode);
            modelParamMap[paramName] = paramValue;
            DLOG("EngineData product=" << productName << " paramName=" << paramName << " paramValue=" << paramValue);
        }
        modelParams_[productName] = modelParamMap;

        engine_[productName] = XMLUtils::getChildValue(node, "Engine");
        DLOG("EngineData product=" << productName << " engine=" << engine_[productName]);

        XMLNode* engineParams = XMLUtils::getChildNode(node, "EngineParameters");
        map<string, string> engineParamMap;
        for (XMLNode* paramNode = XMLUtils::getChildNode(engineParams, "Parameter"); paramNode;
             paramNode = XMLUtils::getNextSibling(paramNode, "Parameter")) {
            string paramName = XMLUtils::getAttribute(paramNode, "name");
            string paramValue = XMLUtils::getNodeValue(paramNode);
            engineParamMap[paramName] = paramValue;
            DLOG("EngineData product=" << productName << " paramName=" << paramName << " paramValue=" << paramValue);
        }
        engineParams_[productName] = engineParamMap;
    }
}

XMLNode* EngineData::toXML(XMLDocument& doc) const {

    XMLNode* pricingEnginesNode = doc.allocNode("PricingEngines");

    // Add global parameters to XML
    XMLNode* globalParamsNode = XMLUtils::addChild(doc, pricingEnginesNode, "GlobalParameters");
    for (const auto& kv : globalParams_) {
        XMLNode* n = doc.allocNode("Parameter", kv.second);
        XMLUtils::addAttribute(doc, n, "name", kv.first);
        XMLUtils::appendNode(globalParamsNode, n);
        TLOG("Added pair [" << kv.first << "," << kv.second << "] to the GlobalParameters node");
    }

    for (auto modelIterator = model_.begin(); modelIterator != model_.end(); modelIterator++) {

        XMLNode* productNode = XMLUtils::addChild(doc, pricingEnginesNode, "Product");
        XMLUtils::addAttribute(doc, productNode, "type", modelIterator->first);
        auto engineIterator = engine_.find(modelIterator->first);
        XMLUtils::addChild(doc, productNode, "Model", modelIterator->second);
        XMLUtils::addChild(doc, productNode, "Engine", engineIterator->second);
        XMLNode* modelParametersNode = XMLUtils::addChild(doc, productNode, "ModelParameters");
        for (auto modelParamsIterator = modelParams_.find(modelIterator->first)->second.begin();
             modelParamsIterator != modelParams_.find(modelIterator->first)->second.end(); modelParamsIterator++) {
            XMLNode* parameterNode = doc.allocNode("Parameter", modelParamsIterator->second);
            XMLUtils::appendNode(modelParametersNode, parameterNode);
            XMLUtils::addAttribute(doc, parameterNode, "name", modelParamsIterator->first);
        }
        XMLNode* engineParametersNode = XMLUtils::addChild(doc, productNode, "EngineParameters");
        for (auto engineParamsIterator = engineParams_.find(modelIterator->first)->second.begin();
             engineParamsIterator != engineParams_.find(modelIterator->first)->second.end(); engineParamsIterator++) {
            XMLNode* parameterNode = doc.allocNode("Parameter", engineParamsIterator->second);
            XMLUtils::appendNode(engineParametersNode, parameterNode);
            XMLUtils::addAttribute(doc, parameterNode, "name", engineParamsIterator->first);
        }
    }
    return pricingEnginesNode;
}

// we assume all the maps have the same keys
bool EngineData::hasProduct(const std::string& productName) { return (model_.find(productName) != model_.end()); }

vector<string> EngineData::products() const {
    vector<string> res;
    for (auto it : model_)
        res.push_back(it.first);
    return res;
}

bool operator==(const EngineData& lhs, const EngineData& rhs) {
    vector<string> products = lhs.products();
    // this assumes they are both sorted the same
    if (products != rhs.products())
        return false;
    // now loop over the products and check everything
    for (auto product : products) {
        if (lhs.model(product) != rhs.model(product) || lhs.modelParameters(product) != rhs.modelParameters(product) ||
            lhs.engine(product) != rhs.engine(product) ||
            lhs.engineParameters(product) != rhs.engineParameters(product))
            return false;
    }
    return true;
}

bool operator!=(const EngineData& lhs, const EngineData& rhs) { return !(lhs == rhs); }

} // namespace data
} // namespace ore
