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

/*! \file ored/model/calibrationconfiguration.hpp
    \brief class for holding calibration configuration details
    \ingroup models
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/types.hpp>
#include <ql/math/optimization/constraint.hpp>
#include <map>

namespace ore {
namespace data {

/*! Class for holding calibration configuration details.
    \todo Possibly add information about optimisation method, optimisation parameters and end criteria.
    \ingroup models
*/
class CalibrationConfiguration : public XMLSerializable {
public:
    //! Constructor
    CalibrationConfiguration(QuantLib::Real rmseTolerance = 0.0001,
        QuantLib::Size maxIterations = 50);

    //! \name Inspectors
    //@{
    //! A final tolerance on the RMSE of the calibration that may be used by various builders.
    QuantLib::Real rmseTolerance() const;
    
    /*! High level maximum iterations. This may mean different to things to different builders. This is not the 
        maximum number of iterations used by the EndCriteria for optimisation. If this is needed, this should be added 
        in another XML object along with the other EndCriteria elements and included as a CalibrationConfiguration 
        member.
    */
    QuantLib::Size maxIterations() const;
    
    /*! Return constraint for the parameter \p name.
        
        Currently, only boundary constraints are supported. If the parameter \name does not have a constraint, a 
        NoConstraint instance is returned.
    */
    QuantLib::ext::shared_ptr<QuantLib::Constraint> constraint(const std::string& name) const;

    /*! Return the boundaries for the parameter \p name.

        If no boundaries have been given for parameter \p name, a pair with both elements set to Null<Real>() is 
        returned.
    */
    std::pair<QuantLib::Real, QuantLib::Real> boundaries(const std::string& name) const;
    //@}

    /*! Add a boundary constraint on the parameter \p name.
    */
    void add(const std::string& name, QuantLib::Real lowerBound, QuantLib::Real upperBound);

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

protected:
    QuantLib::Real rmseTolerance_;
    QuantLib::Size maxIterations_;
    std::map<std::string, std::pair<QuantLib::Real, QuantLib::Real>> constraints_;
};


}
}
