/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_configuration_ext_i
#define ored_configuration_ext_i

%include ored_curveconfigurations.i

%shared_ptr(ore::data::BaselTrafficLightData)
%shared_ptr(ore::data::AdjustmentFactors)
%feature("flatnested") BaselTrafficLightData;
%rename(BaselTrafficLightObservationData) ore::data::BaselTrafficLightData::ObservationData;

namespace ore {
namespace data {

class BaselTrafficLightData : public ore::data::XMLSerializable {
public:
    struct ObservationData {
        ObservationData();
        std::vector<int> observationCount;
        std::vector<int> amberLimit;
        std::vector<int> redLimit;
    };

    BaselTrafficLightData();
    BaselTrafficLightData(const std::string& filename);
    BaselTrafficLightData(const std::map<int, ObservationData>& baselTrafficLight);

    void clear();

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    std::map<int, ObservationData>& baselTrafficLightData();
    void setbaselTrafficLightData(std::map<int, ObservationData> baselTrafficLight);

    %extend {
        void setObservationData(int key, const std::vector<int>& observationCount,
                                const std::vector<int>& amberLimit, const std::vector<int>& redLimit) {
            ore::data::BaselTrafficLightData::ObservationData data;
            data.observationCount = observationCount;
            data.amberLimit = amberLimit;
            data.redLimit = redLimit;
            self->baselTrafficLightData()[key] = data;
        }
    }
};

class AdjustmentFactors : public ore::data::XMLSerializable {
public:
    AdjustmentFactors(QuantLib::Date asof);

    bool hasFactor(const std::string& name) const;
    QuantLib::Real getFactor(const std::string& name, const QuantLib::Date& d) const;
    void addFactor(std::string name, QuantLib::Date d, QuantLib::Real factor);

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    std::set<std::string> names() const;
    std::set<QuantLib::Date> dates(const std::string& name) const;
    QuantLib::Real getFactorContribution(const std::string& name, const QuantLib::Date& d) const;

    %extend {
        static ext::shared_ptr<ore::data::AdjustmentFactors> create(const QuantLib::Date& asof) {
            return QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(asof);
        }
    }
};

} // namespace data
} // namespace ore

%template(BaselTrafficLightObservationDataMap) std::map<int, ore::data::BaselTrafficLightData::ObservationData>;

#if defined(SWIGPYTHON)
%pythoncode %{
if 'BaselTrafficLightObservationData' in globals():
    BaselTrafficLightData.ObservationData = BaselTrafficLightObservationData
%}
#endif

#endif
