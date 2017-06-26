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
}

bool EngineData::hasProduct(const std::string& productName) { return (model_.find(productName) != model_.end()); }

void EngineData::fromXML(XMLNode* root) {
    clear();
    XMLUtils::checkNode(root, "PricingEngines");
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

XMLNode* EngineData::toXML(XMLDocument& doc) {

    XMLNode* pricingEnginesNode = doc.allocNode("PricingEngines");

    for (auto modelIterator = model_.begin(); modelIterator != model_.end(); modelIterator++) {

        XMLNode* productNode = XMLUtils::addChild(doc, pricingEnginesNode, "Product");
        XMLUtils::addAttribute(doc, productNode, "type", modelIterator->first);

        XMLUtils::addChild(doc, productNode, "Model", model_[modelIterator->first]);
        XMLUtils::addChild(doc, productNode, "Engine", engine_[modelIterator->first]);
        XMLNode* modelParametersNode = XMLUtils::addChild(doc, productNode, "ModelParameters");
        for (auto modelParamsIterator = modelParams_[modelIterator->first].begin();
             modelParamsIterator != modelParams_[modelIterator->first].end(); modelParamsIterator++) {
            XMLNode* parameterNode = doc.allocNode("Parameter", modelParamsIterator->second);
            XMLUtils::appendNode(modelParametersNode, parameterNode);
            XMLUtils::addAttribute(doc, parameterNode, "name", modelParamsIterator->first);
        }
        XMLNode* engineParametersNode = XMLUtils::addChild(doc, productNode, "EngineParameters");
        for (auto engineParamsIterator = engineParams_[modelIterator->first].begin();
             engineParamsIterator != engineParams_[modelIterator->first].end(); engineParamsIterator++) {
            XMLNode* parameterNode = doc.allocNode("Parameter", engineParamsIterator->second);
            XMLUtils::appendNode(engineParametersNode, parameterNode);
            XMLUtils::addAttribute(doc, parameterNode, "name", engineParamsIterator->first);
        }
    }
    return pricingEnginesNode;
}
} // namespace data
} // namespace ore
