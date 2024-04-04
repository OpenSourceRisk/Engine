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

#include <ored/model/modelparameter.hpp>
#include <ored/utilities/to_string.hpp>

using QuantLib::Real;
using QuantLib::Time;
using std::vector;

namespace ore {
namespace data {

ModelParameter::ModelParameter()
    : calibrate_(false), type_(ParamType::Constant) {}

ModelParameter::ModelParameter(bool calibrate, ParamType type, vector<Time> times, vector<Real> values)
    : calibrate_(calibrate), type_(type), times_(std::move(times)), values_(std::move(values)) {
    check();
}

bool ModelParameter::calibrate() const {
    return calibrate_;
}

ParamType ModelParameter::type() const {
    return type_;
}

const vector<Time>& ModelParameter::times() const {
    return times_;
}

const vector<Real>& ModelParameter::values() const {
    return values_;
}

void ModelParameter::setTimes(std::vector<Real> times) { times_ = std::move(times); }

void ModelParameter::setValues(std::vector<Real> values) { values_ = std::move(values); }

void ModelParameter::mult(const Real f) {
    std::transform(values_.begin(), values_.end(), values_.begin(), [&f](const Real v) { return f * v; });
}

void ModelParameter::setCalibrate(const bool b) { calibrate_ = b; }

void ModelParameter::fromXML(XMLNode* node) {
    calibrate_ = XMLUtils::getChildValueAsBool(node, "Calibrate", true);
    type_ = parseParamType(XMLUtils::getChildValue(node, "ParamType", true));
    values_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "InitialValue", true);
    if (type_ != ParamType::Constant) {
        times_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "TimeGrid", true);
    }
    check();
}

void ModelParameter::append(XMLDocument& doc, XMLNode* node) const {
    XMLUtils::addChild(doc, node, "Calibrate", calibrate_);
    XMLUtils::addGenericChild(doc, node, "ParamType", type_);
    XMLUtils::addGenericChildAsList(doc, node, "TimeGrid", times_);
    XMLUtils::addGenericChildAsList(doc, node, "InitialValue", values_);
}

void ModelParameter::check() const {
    if (type_ == ParamType::Constant) {
        QL_REQUIRE(values_.size() == 1, "Parameter type is Constant so expecting a single InitialValue.");
        QL_REQUIRE(times_.empty(), "Parameter type is Constant so expecting an empty time vector.");
    } else if (type_ == ParamType::Piecewise) {
        QL_REQUIRE(values_.size() == times_.size() + 1, "Parameter type is Piecewise so expecting the size of the " <<
            "InitialValue vector (" << values_.size() << ") to be one greater than size of time vector (" <<
            times_.size() << ").");
    }
}

VolatilityParameter::VolatilityParameter()
    : volatilityType_(LgmData::VolatilityType::Hagan) {}

VolatilityParameter::VolatilityParameter(LgmData::VolatilityType volatilityType, bool calibrate, ParamType type,
                                         vector<Time> times, vector<Real> values)
    : ModelParameter(calibrate, type, times, values), volatilityType_(volatilityType) {}

VolatilityParameter::VolatilityParameter(LgmData::VolatilityType volatilityType, bool calibrate, QuantLib::Real value)
    : ModelParameter(calibrate, ParamType::Constant, {}, {value}), volatilityType_(volatilityType) {}

VolatilityParameter::VolatilityParameter(bool calibrate, ParamType type, vector<Time> times, vector<Real> values)
    : ModelParameter(calibrate, type, times, values) {}

VolatilityParameter::VolatilityParameter(bool calibrate, QuantLib::Real value)
    : ModelParameter(calibrate, ParamType::Constant, {}, {value}) {}

const boost::optional<LgmData::VolatilityType>& VolatilityParameter::volatilityType() const {
    return volatilityType_;
}

void VolatilityParameter::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Volatility");
    if (XMLNode* n = XMLUtils::getChildNode(node, "VolatilityType")) {
        volatilityType_ = parseVolatilityType(XMLUtils::getNodeValue(n));
    }
    ModelParameter::fromXML(node);
}

XMLNode* VolatilityParameter::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Volatility");
    if (volatilityType_)
        XMLUtils::addChild(doc, node, "VolatilityType", to_string(*volatilityType_));
    ModelParameter::append(doc, node);
    return node;
}

ReversionParameter::ReversionParameter()
    : reversionType_(LgmData::ReversionType::HullWhite) {}

ReversionParameter::ReversionParameter(LgmData::ReversionType reversionType, bool calibrate, ParamType type,
                                       vector<Time> times, vector<Real> values)
    : ModelParameter(calibrate, type, times, values), reversionType_(reversionType) {}

ReversionParameter::ReversionParameter(LgmData::ReversionType reversionType,
    bool calibrate,
    Real value)
    : ModelParameter(calibrate, ParamType::Constant, {}, { value }),
      reversionType_(reversionType) {}

LgmData::ReversionType ReversionParameter::reversionType() const {
    return reversionType_;
}

void ReversionParameter::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Reversion");
    reversionType_ = parseReversionType(XMLUtils::getChildValue(node, "ReversionType", true));
    ModelParameter::fromXML(node);
}

XMLNode* ReversionParameter::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Reversion");
    XMLUtils::addChild(doc, node, "ReversionType", to_string(reversionType_));
    ModelParameter::append(doc, node);
    return node;
}

}
}
