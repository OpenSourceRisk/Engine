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

/*! \file ored/configuration/iborfallbackconfig.hpp
    \brief ibor fallback configuration
    \ingroup utilities
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

class BaselTrafficLightData : public XMLSerializable {
public:
    struct ObservationData {
        std::vector<int> observationCount;
        std::vector<int> amberLimit;
        std::vector<int> redLimit;
    };
    BaselTrafficLightData();
    BaselTrafficLightData(const string& filename) { fromFile(filename); }
    BaselTrafficLightData(const std::map<int, ObservationData>& baselTrafficLight);

    void clear();

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    std::map<int, ObservationData>& baselTrafficLightData() { return baselTrafficLight_; }
    void setbaselTrafficLightData(std::map<int, ObservationData> baselTrafficLight) {
        baselTrafficLight_ = baselTrafficLight;
    }
    
private:
    std::map<int, ObservationData> baselTrafficLight_;
};

} // namespace data
} // namespace ore
