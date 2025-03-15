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

//std::string vectorToString(const std::vector<int>& vec) {
//    std::ostringstream oss;
//    for (size_t i = 0; i < vec.size(); ++i) {
//        if (i != 0) {
//            oss << ",";
//        }
//        oss << vec[i];
//    }
//    return oss.str();
//}

BaselTrafficLightData::BaselTrafficLightData() { clear(); }
BaselTrafficLightData::BaselTrafficLightData(const std::map<int, ObservationData>& baselTrafficLight) : 
                                                baselTrafficLight_(baselTrafficLight) {}

void BaselTrafficLightData::clear() {
    baselTrafficLight_.clear();
}
/*
<BaselTrafficLightConfig>
    <Configuration>
        <mporDays>10</mporDays>
        <ObservationThresholds>
            <ObservationCount>1,2,3,...,6190</ObservationCount>
            <AmberLimit>0,0,0,...,89</AmberLimit>
            <RedLimit>0,1,2,3...133</RedLimit>
        </ObservationThresholds>
    </Configuration>
    <Configuration>
        <mporDays>1</mporDays>
        <ObservationThresholds>
            <ObservationCount>1,2,3,..,2200</ObservationCount>
            <AmberLimit>1,1,1...,30</AmberLimit>
            <RedLimit>1,1,...41</RedLimit>
        </ObservationThresholds>
    </Configuration>
</BaselTrafficLightConfig>
*/
void BaselTrafficLightData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BaselTrafficLightConfig");
    for (auto const c : XMLUtils::getChildrenNodes(node, "Configuration")) {
        int mporDays = XMLUtils::getChildValueAsInt(c, "mporDays", true);
        auto obsThreshold = XMLUtils::getChildNode(c, "ObservationThresholds");
        auto observationCount = XMLUtils::getChildNode(obsThreshold, "ObservationCount");
        auto amberLimit = XMLUtils::getChildNode(obsThreshold, "AmberLimit");
        auto redLimit = XMLUtils::getChildNode(obsThreshold, "RedLimit");
        std::vector<int> observationCt = parseListOfValuesAsInt(XMLUtils::getNodeValue(observationCount));
        std::vector<int> amberLim = parseListOfValuesAsInt(XMLUtils::getNodeValue(amberLimit));
        std::vector<int> redLim = parseListOfValuesAsInt(XMLUtils::getNodeValue(redLimit));
        QL_REQUIRE(observationCt.size() == amberLim.size() && amberLim.size() == redLim.size(),
                   "We must have the same number number of observation and limits.");
        baselTrafficLight_[mporDays] = {observationCt, amberLim, redLim};
    }
}

XMLNode* BaselTrafficLightData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BaselTrafficLightConfig");
    for (const auto& pair : baselTrafficLight_) {
        int mporDays = pair.first;
        const ObservationData& data = pair.second;
        XMLNode* rdNode = XMLUtils::addChild(doc, node, "Configuration");
        XMLUtils::addChild(doc, rdNode, "mporDays", std::to_string(mporDays));
        XMLNode* oNode = XMLUtils::addChild(doc, rdNode, "ObservationThresholds");
        XMLUtils::addGenericChildAsList(doc, oNode, "ObservationCount", data.observationCount);
        XMLUtils::addGenericChildAsList(doc, oNode, "AmberLimit", data.amberLimit);
        XMLUtils::addGenericChildAsList(doc, oNode, "RedLimit", data.redLimit);
        /*XMLUtils::addChild(doc, oNode, "ObservationCount", vectorToString(data.observationCount));
        XMLUtils::addChild(doc, oNode, "AmberLimit", vectorToString(data.amberLimit));
        XMLUtils::addChild(doc, oNode, "RedLimit", vectorToString(data.redLimit));*/
    }

    return node;
}

} // namespace data
} // namespace ore
