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

/*! \file ored/model/calibrationbasket.hpp
    \brief class for holding details of the calibration instruments for a model
    \ingroup models
*/

#pragma once

#include <ql/types.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <vector>

namespace ore {
namespace data {

/*! Abstract base class for holding details of a model calibration instrument.
    \ingroup models
*/
class CalibrationInstrument : public XMLSerializable {
public:
    //! Constructor
    CalibrationInstrument(const std::string& instrumentType);

    //! Destructor
    virtual ~CalibrationInstrument() {}

    //! \name Inspectors
    //@{
    const std::string& instrumentType() const;
    //@}

protected:
    std::string instrumentType_;
};

/*! Class for holding calibration instruments of the same type for a model.

    If you need to calibrate a model to instruments of different types, use multiple calibration baskets.

    \ingroup models
*/
class CalibrationBasket : public XMLSerializable {
public:
    //! Default constructor, empty calibration basket.
    CalibrationBasket();

    //! Detailed constructor
    CalibrationBasket(const std::vector<QuantLib::ext::shared_ptr<CalibrationInstrument>>& instruments);

    //! \name Inspectors
    //@{
    const std::string& instrumentType() const;
    const std::vector<QuantLib::ext::shared_ptr<CalibrationInstrument>>& instruments() const;
    const std::string& parameter() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! Returns \c true if the calibration basket is empty.
    bool empty() const;

private:
    std::vector<QuantLib::ext::shared_ptr<CalibrationInstrument>> instruments_;
    std::string instrumentType_;
    //! The parameter tag may be given so that builders know how to use the calibration basket.
    std::string parameter_;

};


}
}
