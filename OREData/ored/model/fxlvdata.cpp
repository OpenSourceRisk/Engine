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

#include <ored/model/fxlvdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

void FxLvData::fromXML(XMLNode* node) {
    FxData::fromXML(node);
    std::string model_ = XMLUtils::getChildValue(node, "Model");
    std::string stochasticRatesCorrection_ = XMLUtils::getChildValue(node, "StochasticRatesCorrection");
    std::string calibrationMoneyness_ = XMLUtils::getChildValue(node, "CalibrationMoneyness");
    std::string calibrationGrid_ = XMLUtils::getChildValue(node, "CalibrationGrid");
}

XMLNode* FxLvData::toXML(XMLDocument& doc) {
    XMLNode* baseNode = FxData::toXML(doc);
    XMLUtils::addGenericChild(doc, baseNode, "Model", model_);
    XMLUtils::addGenericChild(doc, baseNode, "StochasitcRatesCorrection", stochasticRatesCorrection_);
    XMLUtils::addGenericChild(doc, baseNode, "CalibrationMoneyness", calibrationMoneyness_);
    XMLUtils::addGenericChild(doc, baseNode, "CalibrationGrid", calibrationGrid_);
    return baseNode;
}

QuantLib::ext::shared_ptr<FxData> FxLvData::clone(std::string foreignCcy) const {
    return QuantLib::ext::make_shared<FxLvData>(std::move(foreignCcy), domesticCcy_, model_, stochasticRatesCorrection_,
                                                calibrationMoneyness_, calibrationGrid_);
}

} // namespace data
} // namespace ore
