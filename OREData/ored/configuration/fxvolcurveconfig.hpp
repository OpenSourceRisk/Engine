/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/configuration/fxvolcurveconfig.hpp
    \brief FX volatility curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ql/types.hpp>
#include <ql/time/period.hpp>
#include <ored/utilities/xmlutils.hpp>

using std::string;
using std::vector;
using openriskengine::data::XMLSerializable;
using openriskengine::data::XMLNode;
using openriskengine::data::XMLDocument;
using QuantLib::Period;

namespace openriskengine {
namespace data {

//! FX volatility structure configuration
/*!
  \ingroup configuration
*/
class FXVolatilityCurveConfig : public XMLSerializable {
public:
    //! supported volatility structure types
    enum class Dimension { ATM, Smile };

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    FXVolatilityCurveConfig() {}
    //! Detailed constructor
    FXVolatilityCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                            const vector<Period>& expiries);
    //! Default destructor
    virtual ~FXVolatilityCurveConfig() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    const string& curveID() const { return curveID_; }
    const string& curveDescription() const { return curveDescription_; }
    const Dimension& dimension() const { return dimension_; }
    const vector<Period>& expiries() const { return expiries_; }
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    Dimension& dimension() { return dimension_; }
    vector<Period>& expiries() { return expiries_; }
    //@}
private:
    string curveID_;
    string curveDescription_;
    Dimension dimension_;
    vector<Period> expiries_;
};
}
}
