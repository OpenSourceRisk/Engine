/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

void FxLvData::SimpleMcParameters::fromXML(XMLNode* node) {
    timeStepsPerYear_ = parseInteger(XMLUtils::getChildValue(node, "TimeStepsPerYear", false, "24"));
    calibrationMoneynessMin_ = parseReal(XMLUtils::getChildValue(node, "CalibrationMoneynessMin", false, "-3.0"));
    calibrationMoneynessMax_ = parseReal(XMLUtils::getChildValue(node, "CalibrationMoneynessMax", false, "3.0"));
    nStrikes_ = parseInteger(XMLUtils::getChildValue(node, "nStrikes", false, "25"));
    samples_ = parseInteger(XMLUtils::getChildValue(node, "Samples", false, "10000"));
    d2CdK2Threshold_ = parseReal(XMLUtils::getChildValue(node, "d2CdK2Threshold", false, "0.01"));
    nPasses_ = parseInteger(XMLUtils::getChildValue(node, "nPasses", false, "1"));
}

XMLNode* FxLvData::SimpleMcParameters::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("SimpleMcParameters");
    XMLUtils::addGenericChild(doc, node, "TimeStepsPerYear", timeStepsPerYear_);
    XMLUtils::addGenericChild(doc, node, "CalibrationMoneynessMin", calibrationMoneynessMin_);
    XMLUtils::addGenericChild(doc, node, "CalibrationMoneynessMax", calibrationMoneynessMax_);
    XMLUtils::addGenericChild(doc, node, "nStrikes", nStrikes_);
    XMLUtils::addGenericChild(doc, node, "Samples", samples_);
    XMLUtils::addGenericChild(doc, node, "d2CdK2Threshold", d2CdK2Threshold_);
    XMLUtils::addGenericChild(doc, node, "nPasses", nPasses_);
    return node;
}

void FxLvData::fromXML(XMLNode* node) {
    FxData::fromXML(node);
    model_ = XMLUtils::getChildValue(node, "Model");
    stochasticRatesCorrection_ = XMLUtils::getChildValue(node, "StochasticRatesCorrection");
    calibrationMoneyness_ = XMLUtils::getChildValue(node, "CalibrationMoneyness");
    calibrationGrid_ = XMLUtils::getChildValue(node, "CalibrationGrid");
    if (auto tmp = XMLUtils::getChildNode(node, "SimpleMcParameters"))
        simpleMcParameters_.fromXML(tmp);
}

XMLNode* FxLvData::toXML(XMLDocument& doc) const {
    XMLNode* baseNode = FxData::toXML(doc);
    XMLUtils::addGenericChild(doc, baseNode, "Model", model_);
    XMLUtils::addGenericChild(doc, baseNode, "StochasitcRatesCorrection", stochasticRatesCorrection_);
    XMLUtils::addGenericChild(doc, baseNode, "CalibrationMoneyness", calibrationMoneyness_);
    XMLUtils::addGenericChild(doc, baseNode, "CalibrationGrid", calibrationGrid_);
    XMLUtils::appendNode(baseNode, simpleMcParameters_.toXML(doc));
    return baseNode;
}

QuantLib::ext::shared_ptr<FxData> FxLvData::clone(std::string foreignCcy) const {
    return QuantLib::ext::make_shared<FxLvData>(std::move(foreignCcy), domesticCcy_, model_, stochasticRatesCorrection_,
                                                calibrationMoneyness_, calibrationGrid_, simpleMcParameters_);
}

} // namespace data
} // namespace ore
