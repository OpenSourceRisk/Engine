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

#ifndef ored_bootstrapconfig_i
#define ored_bootstrapconfig_i

%include ored_xmlutils.i

%shared_ptr(ore::data::BootstrapConfig)
namespace ore {
namespace data {
class BootstrapConfig : public ore::data::XMLSerializable {
public:
    BootstrapConfig(QuantLib::Real accuracy = 1.0e-12,
                    QuantLib::Real globalAccuracy = QuantLib::Null<QuantLib::Real>(),
                    bool dontThrow = false, QuantLib::Size maxAttempts = 5, QuantLib::Real maxFactor = 2.0,
                    QuantLib::Real minFactor = 2.0, QuantLib::Size dontThrowSteps = 10, bool global = false,
                    Real smoothnessLambda = 0.0);
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};
} // namespace data
} // namespace ore

#endif
