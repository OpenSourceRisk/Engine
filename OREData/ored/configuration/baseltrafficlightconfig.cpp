/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/configuration/baseltrafficlightconfig.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/time/date.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

std::vector<int> splitStringToIntVector(const std::string& str) {
    std::vector<int> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(std::stoi(item));
    }
    return result;
}

BaselTrafficLightData::BaselTrafficLightData() { clear(); }
BaselTrafficLightData::BaselTrafficLightData(const std::map<int, ObservationData>& baselTrafficLight) : 
                                                baselTrafficLight_(baselTrafficLight) {}

void BaselTrafficLightData::clear() {
    baselTrafficLight_.clear();
}

void BaselTrafficLightData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BaselTrafficLightConfig");
    if (auto global = XMLUtils::getChildNode(node, "Configuration")) {
        for (auto const c : XMLUtils::getChildrenNodes(node, "Configuration")) {
            int mporDays = XMLUtils::getChildValueAsInt(c, "mporDays", true);
            auto obsThreshold = XMLUtils::getChildNode(c, "ObservationThresholds");
            auto observationCount = XMLUtils::getChildNode(obsThreshold, "ObservationCount");
            auto amberLimit = XMLUtils::getChildNode(obsThreshold, "AmberLimit");
            auto redLimit = XMLUtils::getChildNode(obsThreshold, "RedLimit");
            std::vector<int> observationCt = splitStringToIntVector(XMLUtils::getNodeValue(observationCount));
            std::vector<int> amberLim = splitStringToIntVector(XMLUtils::getNodeValue(amberLimit));
            std::vector<int> redLim = splitStringToIntVector(XMLUtils::getNodeValue(redLimit));
            baselTrafficLight_[mporDays] = {observationCt, amberLim, redLim};
        }
    }
}

XMLNode* BaselTrafficLightData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BaselTrafficLightConfig");
    return node;
}

} // namespace data
} // namespace ore
