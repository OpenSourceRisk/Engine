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

/*! \file ored/model/inflation/inflationmodeldata.hpp
    \brief base class for holding inflation model data
    \ingroup models
*/

#pragma once

#include <ored/model/modeldata.hpp>
#include <string>

namespace ore {
namespace data {

/*! Abstract base class for holding inflation model data.
    \ingroup models
*/
class InflationModelData : public ModelData {

public:
    //! Default constructor. The currency and inflation index are empty.
    InflationModelData();

    /*! Detailed constructor populating the currency and inflation index.
        \param calibrationType    the type of model calibration.
        \param calibrationBaskets the calibration baskets for the model.
        \param currency           the currency of the inflation model.
        \param index              the name of the inflation index being modeled.
        \param ignoreDuplicateCalibrationExpiryTimes if true, a calibration instrument
               with an expiry time equal to that of a previously added instrument
               is skipped. If false, an error is thrown if such an instrument is found.
               Notice that two instruments with different option expiry dates can
               still have the same expiry time due to the way dates are converted
               to times for inflation instruments.
     */
    InflationModelData(CalibrationType calibrationType,
        const std::vector<CalibrationBasket>& calibrationBaskets,
        const std::string& currency,
        const std::string& index,
        const bool ignoreDuplicateCalibrationExpiryTimes);

    //! \name Inspectors
    //@{
    const std::string& currency() const;
    const std::string& index() const;
    bool ignoreDuplicateCalibrationExpiryTimes() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    //@}

protected:
    //! Method used by toXML in derived classes to add the members here to a node.
    void append(XMLDocument& doc, XMLNode* node) const override;

private:
    std::string currency_;
    std::string index_;
    bool ignoreDuplicateCalibrationExpiryTimes_ = false;
};

}
}
