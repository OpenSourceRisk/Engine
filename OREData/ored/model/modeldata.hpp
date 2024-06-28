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

/*! \file ored/model/modeldata.hpp
    \brief base class for holding model data
    \ingroup models
*/

#pragma once

#include <ored/model/calibrationbasket.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {

/*! Abstract base class for holding model data.
    \ingroup models
*/
class ModelData : public XMLSerializable {

public:
    //! Default constructor
    ModelData();

    //! Detailed constructor
    ModelData(CalibrationType calibrationType,
        const std::vector<CalibrationBasket>& calibrationBaskets);

    //! \name Inspectors
    //@{
    CalibrationType calibrationType() const;
    const std::vector<CalibrationBasket>& calibrationBaskets() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    //@}

private:
    CalibrationType calibrationType_;

protected:
    //! Method used by toXML in derived classes to add the members here to a node.
    virtual void append(XMLDocument& doc, XMLNode* node) const;
    
    // Protected so that we can support population from legacy XML in fromXML in derived classes.
    std::vector<CalibrationBasket> calibrationBaskets_;
};

}
}
