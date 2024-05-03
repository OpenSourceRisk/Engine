/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/parametricsmileconfiguration.hpp
    \brief Class for holding parametric smile configurations
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/utilities/null.hpp>

namespace ore {
namespace data {

/*! Serializable parametric smile configuration
    \ingroup configuration
*/
class ParametricSmileConfiguration : public XMLSerializable {
public:
    class Parameter : public XMLSerializable {
    public:
        void fromXML(ore::data::XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        std::string name;
        std::vector<double> initialValue = {0.0};
        bool isFixed = false;
    };

    class Calibration : public XMLSerializable {
    public:
        void fromXML(ore::data::XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        std::size_t maxCalibrationAttempts = 10;
        double exitEarlyErrorThreshold = 0.0050;
        double maxAcceptableError = 0.05;
    };

    ParametricSmileConfiguration() {}
    ParametricSmileConfiguration(std::vector<Parameter> parameters, Calibration calibration);

    //! \name XMLSerializable interface
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const Parameter& parameter(const std::string& name) const;
    const Calibration& calibration() const;
    //@}

private:
    std::vector<Parameter> parameters_;
    Calibration calibration_;
};

} // namespace data
} // namespace ore
