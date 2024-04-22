/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/model/inflation/infjydata.hpp
    \brief Jarrow Yildirim inflation model component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <ored/model/inflation/inflationmodeldata.hpp>
#include <ored/model/calibrationbasket.hpp>
#include <ored/model/calibrationconfiguration.hpp>
#include <ored/model/modelparameter.hpp>
#include <string>
#include <vector>

namespace ore {
namespace data {

/*! Jarrow Yildirim inflation model data.
    
    Model data specifying the Jarrow Yildirim inflation model described in <em>Modern Derivatives Pricing and Credit 
    Exposure Analysis</em>, Chapter 13.

    \ingroup models
*/

class InfJyData : public InflationModelData {

public:
    //! Default constructor
    InfJyData();

    //! Detailed constructor
    /* Note: If linkRealRateToNominalRateParams == true, the realRateVolatility and realRateReversion should be set to
       the nominal rate parameters and the calibrate flag in these new parameters should be set to false. */
    InfJyData(CalibrationType calibrationType, const std::vector<CalibrationBasket>& calibrationBaskets,
              const std::string& currency, const std::string& index, const ReversionParameter& realRateReversion,
              const VolatilityParameter& realRateVolatility, const VolatilityParameter& indexVolatility,
              const LgmReversionTransformation& reversionTransformation = LgmReversionTransformation(),
              const CalibrationConfiguration& calibrationConfiguration = CalibrationConfiguration(),
              const bool ignoreDuplicateCalibrationExpiryTimes = false, const bool linkRealToNominalRateParams = false,
              const Real linkedRealRateVolatilityScaling = 1.0);

    //! \name Inspectors
    //@{
    const ReversionParameter& realRateReversion() const;
    const VolatilityParameter& realRateVolatility() const;
    const VolatilityParameter& indexVolatility() const;
    const LgmReversionTransformation& reversionTransformation() const;
    const CalibrationConfiguration& calibrationConfiguration() const;
    bool linkRealRateParamsToNominalRateParams() const;
    Real linkedRealRateVolatilityScaling() const;
    //@}

    //! \name Setters
    //@{
    void setRealRateReversion(ReversionParameter p);
    void setRealRateVolatility(VolatilityParameter p);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    ReversionParameter realRateReversion_;
    VolatilityParameter realRateVolatility_;
    VolatilityParameter indexVolatility_;
    LgmReversionTransformation reversionTransformation_;
    CalibrationConfiguration calibrationConfiguration_;
    bool linkRealToNominalRateParams_;
    Real linkedRealRateVolatilityScaling_;
};

}
}
