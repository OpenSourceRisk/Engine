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

#include <ored/configuration/marketconfig.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace std;

namespace ore {
namespace data {

void MarketConfiguration::clear() {

    discountCurveSpecs_.clear();
    yieldCurveSpecs_.clear();
    capVolSpec_.clear();
    swaptionVolSpec_.clear();
    fxSpec_.clear();
    fxVolSpec_.clear();
    iborIndexSpecs_.clear();
}

void MarketConfiguration::fromXML(XMLNode* node) {

    clear();

    XMLUtils::checkNode(node, "MarketConfiguration");

    baseCurrency_ = XMLUtils::getChildValue(node, "BaseCurrency", true);
    fxSpec_ = XMLUtils::getChildrenValues(node, "FxRates", "FxRate", "CurrencyPair", "Spec", true);
    fxVolSpec_ = XMLUtils::getChildrenValues(node, "FxVolatilities", "FxVolatility", "CurrencyPair", "Spec", true);
    capVolSpec_ = XMLUtils::getChildrenValues(node, "CapVolatilities", "CapVolatility", "Currency", "Spec", true);
    swaptionVolSpec_ =
        XMLUtils::getChildrenValues(node, "SwaptionVolatilities", "SwaptionVolatility", "Currency", "Spec", true);
    iborIndexSpecs_ = XMLUtils::getChildrenValues(node, "IborIndices", "IborIndex", "Name", "Spec", true);

    // Get discount curve specs and attributes
    XMLNode* discountNodes = XMLUtils::getChildNode(node, "DiscountCurves");
    QL_REQUIRE(discountNodes, "Must provide a DiscountCurves node in market configuration");
    for (XMLNode* child = XMLUtils::getChildNode(discountNodes); child; child = XMLUtils::getNextSibling(child)) {
        // Get the child node information
        string currency = XMLUtils::getChildValue(child, "Currency", true);
        string spec = XMLUtils::getChildValue(child, "Spec", true);
        discountCurveSpecs_.insert(make_pair(currency, spec));
    }

    // Get yield curve specs and attributes
    XMLNode* yieldNodes = XMLUtils::getChildNode(node, "YieldCurves");
    for (XMLNode* child = XMLUtils::getChildNode(yieldNodes); child; child = XMLUtils::getNextSibling(child)) {
      // Get the child node information
      string name = XMLUtils::getChildValue(child, "Name", true);
      string spec = XMLUtils::getChildValue(child, "Spec", true);
      yieldCurveSpecs_.insert(make_pair(name, spec));
    }
}

XMLNode* MarketConfiguration::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("MarketConfiguration");
    XMLUtils::addChild(doc, node, "BaseCurrency", baseCurrency_);
    XMLUtils::addChildren(doc, node, "FxRates", "FxRate", "CurrencyPair", "Spec", fxSpec_);
    XMLUtils::addChildren(doc, node, "FxVolatilities", "FxVolatility", "CurrencyPair", "Spec", fxVolSpec_);
    XMLUtils::addChildren(doc, node, "CapVolatilities", "CapVolatility", "Currency", "Spec", capVolSpec_);
    XMLUtils::addChildren(doc, node, "SwaptionVolatilities", "SwaptionVolatility", "Currency", "Spec",
                          swaptionVolSpec_);
    XMLUtils::addChildren(doc, node, "IborIndices", "IborIndex", "Name", "Spec", iborIndexSpecs_);

    // Add the DiscountCurves element
    XMLNode* discountNodes = XMLUtils::addChild(doc, node, "DiscountCurves");
    map<string, string>::const_iterator it;
    for (it = discountCurveSpecs_.begin(); it != discountCurveSpecs_.end(); ++it) {
        // Add the main node data
        XMLNode* discountNode = XMLUtils::addChild(doc, discountNodes, "DiscountCurve");
        XMLUtils::addChild(doc, discountNode, "Currency", it->first);
        XMLUtils::addChild(doc, discountNode, "Spec", it->second);
    }

    // Add the YieldCurves element
    XMLNode* yieldNodes = XMLUtils::addChild(doc, node, "YieldCurves");
    map<string, string>::const_iterator it;
    for (it = yieldCurveSpecs_.begin(); it != yieldCurveSpecs_.end(); ++it) {
      // Add the main node data
      XMLNode* yieldNode = XMLUtils::addChild(doc, yieldNodes, "YieldCurve");
      XMLUtils::addChild(doc, yieldNode, "Name", it->first);
      XMLUtils::addChild(doc, yieldNode, "Spec", it->second);
    }

    return node;
}
}
}
