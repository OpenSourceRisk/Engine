/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file ored/model/inflation/infdkdata.hpp
    \brief Dodgson Kainth inflation model component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <ored/model/inflation/inflationmodeldata.hpp>
#include <ored/model/calibrationbasket.hpp>
#include <ored/model/modelparameter.hpp>
#include <string>
#include <vector>

namespace ore {
namespace data {

/*! Dodgson Kainth inflation model data
    
    Model data specifying the Dodgson Kainth inflation model described in <em>Modern Derivatives Pricing and Credit 
    Exposure Analysis</em>, Chapter 13.

    \ingroup models
*/

class InfDkData : public InflationModelData {

public:
    //! Default constructor
    InfDkData();

    //! Detailed constructor
    InfDkData(CalibrationType calibrationType,
        const std::vector<CalibrationBasket>& calibrationBaskets,
        const std::string& currency,
        const std::string& index,
        const ReversionParameter& reversion,
        const VolatilityParameter& volatility,
        const LgmReversionTransformation& reversionTransformation = LgmReversionTransformation(),
        const bool ignoreDuplicateCalibrationExpiryTimes = false);

    //! \name Inspectors
    //@{
    const ReversionParameter& reversion() const;
    const VolatilityParameter& volatility() const;
    const LgmReversionTransformation& reversionTransformation() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    ReversionParameter reversion_;
    VolatilityParameter volatility_;
    LgmReversionTransformation reversionTransformation_;

    //! Support legacy XML interface for reading calibration instruments.
    void populateCalibrationBaskets(XMLNode* node);
};

}
}
