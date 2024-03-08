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

#include <ored/model/inflation/inflationmodeldata.hpp>

using std::string;
using std::vector;

namespace ore {
namespace data {

InflationModelData::InflationModelData() {}

InflationModelData::InflationModelData(
    CalibrationType calibrationType,
    const vector<CalibrationBasket>& calibrationBaskets,
    const string& currency,
    const string& index,
    const bool ignoreDuplicateCalibrationExpiryTimes)
    : ModelData(calibrationType, calibrationBaskets),
      currency_(currency),
      index_(index),
      ignoreDuplicateCalibrationExpiryTimes_(ignoreDuplicateCalibrationExpiryTimes) {}

const std::string& InflationModelData::currency() const {
    return currency_;
}

const std::string& InflationModelData::index() const {
    return index_;
}

bool InflationModelData::ignoreDuplicateCalibrationExpiryTimes() const {
    return ignoreDuplicateCalibrationExpiryTimes_;
}

void InflationModelData::fromXML(XMLNode* node) {
    index_ = XMLUtils::getAttribute(node, "index");
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    ModelData::fromXML(node);
}

void InflationModelData::append(XMLDocument& doc, XMLNode* node) const {
    XMLUtils::addAttribute(doc, node, "index", index_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    ModelData::append(doc, node);
}

}
}
