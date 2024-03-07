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

/*! \file ored/model/calibrationinstruments/yoyswap.hpp
    \brief class for holding details of a year on year inflation swap calibration instrument.
    \ingroup models
*/

#pragma once

#include <ored/model/calibrationbasket.hpp>
#include <ored/model/calibrationinstrumentfactory.hpp>

namespace ore {
namespace data {

/*! Class for holding details of a year on year inflation cap floor calibration instrument.
    \ingroup models
*/
class YoYSwap : public CalibrationInstrument {
public:
    //! Default constructor
    YoYSwap();

    //! Detailed constructor
    YoYSwap(const QuantLib::Period& tenor);

    //! \name Inspectors
    //@{
    const QuantLib::Period& tenor() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    QuantLib::Period tenor_;
};

}
}
