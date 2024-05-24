/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/configuration/bootstrapconfig.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

BootstrapConfig::BootstrapConfig(Real accuracy, Real globalAccuracy, bool dontThrow, Size maxAttempts, Real maxFactor,
                                 Real minFactor, Size dontThrowSteps)
    : accuracy_(accuracy), globalAccuracy_(globalAccuracy == Null<Real>() ? accuracy_ : globalAccuracy),
      dontThrow_(dontThrow), maxAttempts_(maxAttempts), maxFactor_(maxFactor), minFactor_(minFactor),
      dontThrowSteps_(dontThrowSteps) {}

void BootstrapConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BootstrapConfig");

    accuracy_ = 1e-12;
    if (XMLNode* n = XMLUtils::getChildNode(node, "Accuracy")) {
        accuracy_ = parseReal(XMLUtils::getNodeValue(n));
        QL_REQUIRE(accuracy_ > 0, "Accuracy (" << accuracy_ << ") must be a positive number");
    }

    globalAccuracy_ = accuracy_;
    if (XMLNode* n = XMLUtils::getChildNode(node, "GlobalAccuracy")) {
        globalAccuracy_ = parseReal(XMLUtils::getNodeValue(n));
        QL_REQUIRE(globalAccuracy_ > 0, "GlobalAccuracy (" << globalAccuracy_ << ") must be a positive number");
    }

    dontThrow_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "DontThrow")) {
        dontThrow_ = parseBool(XMLUtils::getNodeValue(n));
    }

    maxAttempts_ = 5;
    if (XMLNode* n = XMLUtils::getChildNode(node, "MaxAttempts")) {
        Integer maxAttempts = parseInteger(XMLUtils::getNodeValue(n));
        QL_REQUIRE(maxAttempts > 0, "MaxAttempts (" << maxAttempts << ") must be a positive integer");
        maxAttempts_ = static_cast<Size>(maxAttempts);
    }

    maxFactor_ = 2.0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "MaxFactor")) {
        maxFactor_ = parseReal(XMLUtils::getNodeValue(n));
    }

    minFactor_ = 2.0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "MinFactor")) {
        minFactor_ = parseReal(XMLUtils::getNodeValue(n));
    }

    dontThrowSteps_ = 10;
    if (XMLNode* n = XMLUtils::getChildNode(node, "DontThrowSteps")) {
        Integer dontThrowSteps = parseInteger(XMLUtils::getNodeValue(n));
        QL_REQUIRE(dontThrowSteps > 0, "DontThrowSteps (" << dontThrowSteps << ") must be a positive integer");
        dontThrowSteps_ = static_cast<Size>(dontThrowSteps);
    }
}

XMLNode* BootstrapConfig::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("BootstrapConfig");
    XMLUtils::addChild(doc, node, "Accuracy", accuracy_);
    XMLUtils::addChild(doc, node, "GlobalAccuracy", globalAccuracy_);
    XMLUtils::addChild(doc, node, "DontThrow", dontThrow_);
    XMLUtils::addChild(doc, node, "MaxAttempts", static_cast<int>(maxAttempts_));
    XMLUtils::addChild(doc, node, "MaxFactor", maxFactor_);
    XMLUtils::addChild(doc, node, "MinFactor", minFactor_);
    XMLUtils::addChild(doc, node, "DontThrowSteps", static_cast<int>(dontThrowSteps_));

    return node;
}

} // namespace data
} // namespace ore
