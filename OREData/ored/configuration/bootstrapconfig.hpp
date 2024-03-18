/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/bootstrapconfig.hpp
    \brief Class for holding bootstrap configurations
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/utilities/null.hpp>

namespace ore {
namespace data {

/*! Serializable Bootstrap configuration
    \ingroup configuration
*/
class BootstrapConfig : public XMLSerializable {
public:
    //! Constructor
    BootstrapConfig(QuantLib::Real accuracy = 1.0e-12, QuantLib::Real globalAccuracy = QuantLib::Null<QuantLib::Real>(),
                    bool dontThrow = false, QuantLib::Size maxAttempts = 5, QuantLib::Real maxFactor = 2.0,
                    QuantLib::Real minFactor = 2.0, QuantLib::Size dontThrowSteps = 10);

    //! \name XMLSerializable interface
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::Real accuracy() const { return accuracy_; }
    QuantLib::Real globalAccuracy() const { return globalAccuracy_; }
    bool dontThrow() const { return dontThrow_; }
    QuantLib::Size maxAttempts() const { return maxAttempts_; }
    QuantLib::Real maxFactor() const { return maxFactor_; }
    QuantLib::Real minFactor() const { return minFactor_; }
    QuantLib::Size dontThrowSteps() const { return dontThrowSteps_; }
    //@}

private:
    QuantLib::Real accuracy_;
    QuantLib::Real globalAccuracy_;
    bool dontThrow_;
    QuantLib::Size maxAttempts_;
    QuantLib::Real maxFactor_;
    QuantLib::Real minFactor_;
    QuantLib::Size dontThrowSteps_;
};

} // namespace data
} // namespace ore
