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

#include <ored/model/modeldata.hpp>

using std::string;
using std::vector;

namespace ore {
namespace data {

ModelData::ModelData()
    : calibrationType_(CalibrationType::None) {}

ModelData::ModelData(CalibrationType calibrationType,
    const vector<CalibrationBasket>& calibrationBaskets)
    : calibrationType_(calibrationType), calibrationBaskets_(calibrationBaskets) {}

CalibrationType ModelData::calibrationType() const {
    return calibrationType_;
}

const vector<CalibrationBasket>& ModelData::calibrationBaskets() const {
    return calibrationBaskets_;
}

void ModelData::fromXML(XMLNode* node) {

    calibrationType_ = parseCalibrationType(XMLUtils::getChildValue(node, "CalibrationType", true));
    
    if (XMLNode* n = XMLUtils::getChildNode(node, "CalibrationBaskets")) {
        for (XMLNode* cn = XMLUtils::getChildNode(n, "CalibrationBasket"); cn;
            cn = XMLUtils::getNextSibling(cn, "CalibrationBasket")) {
            CalibrationBasket cb;
            cb.fromXML(cn);
            calibrationBaskets_.push_back(cb);
        }
    }
}

void ModelData::append(XMLDocument& doc, XMLNode* node) const {
    
    XMLUtils::addGenericChild(doc, node, "CalibrationType", calibrationType_);
    
    if (!calibrationBaskets_.empty()) {
        XMLNode* cbsNode = doc.allocNode("CalibrationBaskets");
        for (const CalibrationBasket& cb : calibrationBaskets_) {
            XMLUtils::appendNode(cbsNode, cb.toXML(doc));
        }
        XMLUtils::appendNode(node, cbsNode);
    }
}

}
}
