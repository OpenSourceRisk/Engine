/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/configuration/parametricsmileconfiguration.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void ParametricSmileConfiguration::Parameter::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Parameter");
    name = XMLUtils::getChildValue(node, "Name", true);
    initialValue = parseListOfValues<Real>(XMLUtils::getChildValue(node, "InitialValue", true), parseReal);
    if (auto n = XMLUtils::getChildNode(node, "IsFixed")) {
        WLOG("parametric smile configuration: the usage of IsFixed = true, false is deprecated, use Calibration = "
             "Fixed, Calibrated, Implied.");
        calibration = parseBool(XMLUtils::getNodeValue(n))
                          ? QuantExt::ParametricVolatility::ParameterCalibration::Fixed
                          : QuantExt::ParametricVolatility::ParameterCalibration::Implied;
    } else {
        calibration = parseParametricSmileParameterCalibration(XMLUtils::getChildValue(node, "Calibration", true));
    }
}

XMLNode* ParametricSmileConfiguration::Parameter::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("Parameter");
    XMLUtils::addChild(doc, n, "Name", name);
    XMLUtils::addChild(doc, n, "InitialValue", initialValue);
    XMLUtils::addChild(doc, n, "Calibration", ore::data::to_string(calibration));
    return n;
}

void ParametricSmileConfiguration::Calibration::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Calibration");
    maxCalibrationAttempts = parseInteger(XMLUtils::getChildValue(node, "MaxCalibrationAttempts", true));
    exitEarlyErrorThreshold = parseReal(XMLUtils::getChildValue(node, "ExitEarlyErrorThreshold", true));
    maxAcceptableError = parseReal(XMLUtils::getChildValue(node, "MaxAcceptableError", true));
}

XMLNode* ParametricSmileConfiguration::Calibration::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("Calibration");
    XMLUtils::addChild(doc, n, "MaxCalibrationAttempts", (Integer)maxCalibrationAttempts);
    XMLUtils::addChild(doc, n, "ExitEarlyErrorThreshold", exitEarlyErrorThreshold);
    XMLUtils::addChild(doc, n, "MaxAcceptableError", maxAcceptableError);
    return n;
}

ParametricSmileConfiguration::ParametricSmileConfiguration(std::vector<Parameter> parameters, Calibration calibration)
    : parameters_(std::move(parameters)), calibration_(std::move(calibration)) {}

void ParametricSmileConfiguration::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "ParametricSmileConfiguration");

    parameters_.clear();

    if (XMLNode* n = XMLUtils::getChildNode(node, "Parameters")) {
        for (auto m : XMLUtils::getChildrenNodes(n, "Parameter")) {
            ParametricSmileConfiguration::Parameter p;
            p.fromXML(m);
            parameters_.push_back(p);
        }
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "Calibration")) {
        calibration_.fromXML(n);
    }
}

XMLNode* ParametricSmileConfiguration::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("ParametricSmileConfiguration");

    XMLNode* m = XMLUtils::addChild(doc, node, "Parameters");
    for (auto const& p : parameters_) {
        XMLUtils::appendNode(m, p.toXML(doc));
    }

    XMLUtils::appendNode(node, calibration_.toXML(doc));

    return node;
}

const ParametricSmileConfiguration::Parameter& ParametricSmileConfiguration::parameter(const std::string& name) const {
    auto p =
        std::find_if(parameters_.begin(), parameters_.end(), [&name](const Parameter& p) { return p.name == name; });
    QL_REQUIRE(p != parameters_.end(), "ParametricSmileConfiguration: parameter '" << name << "' is not present.");
    return *p;
}

const ParametricSmileConfiguration::Calibration& ParametricSmileConfiguration::calibration() const {
    return calibration_;
}

QuantExt::ParametricVolatility::ParameterCalibration parseParametricSmileParameterCalibration(const std::string& s) {
    if (s == "Fixed") {
        return QuantExt::ParametricVolatility::ParameterCalibration::Fixed;
    } else if (s == "Calibrated") {
        return QuantExt::ParametricVolatility::ParameterCalibration::Calibrated;
    } else if (s == "Implied") {
        return QuantExt::ParametricVolatility::ParameterCalibration::Implied;
    } else {
        QL_FAIL("parseParametricSmileParameterCalibration: '"
                << s << "' not recognized. Expected one of Fixed, Calibrated, Implied.");
    }
}

std::ostream& operator<<(std::ostream& os, QuantExt::ParametricVolatility::ParameterCalibration c) {
    switch (c) {
    case QuantExt::ParametricVolatility::ParameterCalibration::Fixed:
        return os << "Fixed";
    case QuantExt::ParametricVolatility::ParameterCalibration::Calibrated:
        return os << "Calibrated";
    case QuantExt::ParametricVolatility::ParameterCalibration::Implied:
        return os << "Implied";
    default:
        QL_FAIL("operator<<(ParametricVolatility::ParameterCalibration): enum value "
                << static_cast<int>(c) << " not handled. This is an internal error.");
    }
}

} // namespace data
} // namespace ore
