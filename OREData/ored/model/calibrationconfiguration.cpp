/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/model/calibrationconfiguration.hpp>
#include <ored/utilities/parsers.hpp>

using QuantLib::BoundaryConstraint;
using QuantLib::Constraint;
using QuantLib::NoConstraint;
using QuantLib::Null;
using QuantLib::Real;
using QuantLib::Size;
using std::string;
using std::make_pair;
using std::map;
using std::pair;

namespace ore {
namespace data {

CalibrationConfiguration::CalibrationConfiguration(Real rmseTolerance, Size maxIterations)
    : rmseTolerance_(rmseTolerance), maxIterations_(maxIterations) {}

Real CalibrationConfiguration::rmseTolerance() const {
    return rmseTolerance_;
}

Size CalibrationConfiguration::maxIterations() const {
    return maxIterations_;
}

QuantLib::ext::shared_ptr<Constraint> CalibrationConfiguration::constraint(const string& name) const {
    auto it = constraints_.find(name);
    if (it != constraints_.end()) {
        return QuantLib::ext::make_shared<BoundaryConstraint>(it->second.first, it->second.second);
    } else {
        return QuantLib::ext::make_shared<NoConstraint>();
    }
}

pair<Real, Real> CalibrationConfiguration::boundaries(const string& name) const {
    auto it = constraints_.find(name);
    if (it != constraints_.end()) {
        return it->second;
    } else {
        return make_pair(Null<Real>(), Null<Real>());
    }
}

void CalibrationConfiguration::add(const string& name, QuantLib::Real lowerBound, QuantLib::Real upperBound) {
    QL_REQUIRE(lowerBound < upperBound, "CalibrationConfiguration: Lower bound (" << lowerBound <<
        ") must be less than upper bound (" << upperBound << ").");
    constraints_[name] = make_pair(lowerBound, upperBound);
    DLOG("Boundary constraint [" << lowerBound << "," << upperBound << "] added for parameter " << name << ".");
}

void CalibrationConfiguration::fromXML(XMLNode* node) {
    
    XMLUtils::checkNode(node, "CalibrationConfiguration");
    
    rmseTolerance_ = 0.0001;
    if (XMLNode* n = XMLUtils::getChildNode(node, "RmseTolerance")) {
        rmseTolerance_ = parseReal(XMLUtils::getNodeValue(n));
    }

    maxIterations_ = 50;
    if (XMLNode* n = XMLUtils::getChildNode(node, "MaxIterations")) {
        maxIterations_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    XMLNode* constraintsNode = XMLUtils::getChildNode(node, "Constraints");
    for (XMLNode* cn = XMLUtils::getChildNode(constraintsNode); cn; cn = XMLUtils::getNextSibling(cn)) {
        
        // Only support boundary constraints for the moment.
        string constraintName = XMLUtils::getNodeName(cn);
        if (constraintName != "BoundaryConstraint") {
            DLOG("CalibrationConfiguration skipping constraint with name " << constraintName << ". Only " <<
                "BoundaryConstraint is currently supported.");
            continue;
        }

        auto name = XMLUtils::getAttribute(cn, "parameter");
        auto lowerBound = parseReal(XMLUtils::getChildValue(cn, "LowerBound", true));
        auto upperBound = parseReal(XMLUtils::getChildValue(cn, "UpperBound", true));
        add(name, lowerBound, upperBound);
    }

}

XMLNode* CalibrationConfiguration::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CalibrationConfiguration");

    XMLUtils::addChild(doc, node, "RmseTolerance", rmseTolerance_);
    XMLUtils::addChild(doc, node, "MaxIterations", static_cast<int>(maxIterations_));

    XMLNode* constraintsNode = doc.allocNode("Constraints");
    for (const auto& kv : constraints_) {
        XMLNode* n = doc.allocNode("BoundaryConstraint");
        XMLUtils::addChild(doc, n, "LowerBound", kv.second.first);
        XMLUtils::addChild(doc, n, "UpperBound", kv.second.second);
        XMLUtils::addAttribute(doc, n, "parameter", kv.first);
        XMLUtils::appendNode(constraintsNode, n);
    }
    XMLUtils::appendNode(node, constraintsNode);

    return node;
}

}
}
