/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/equityvolcurveconfig.hpp
    \brief Equity volatility curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ql/types.hpp>
#include <ored/utilities/xmlutils.hpp>

using std::string;
using std::vector;
using ore::data::XMLSerializable;
using ore::data::XMLNode;
using ore::data::XMLDocument;
using QuantLib::Period;

namespace ore {
namespace data {

//! Equity volatility structure configuration
/*!
  \ingroup configuration
*/
class EquityVolatilityCurveConfig : public XMLSerializable {
public:
    //! supported volatility structure types
    enum class Dimension { ATM, Smile };

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    EquityVolatilityCurveConfig() {}
    //! Detailed constructor
    EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription, 
        const string& currency, const Dimension& dimension,
        const vector<string>& expiries);
    //! Default destructor
    virtual ~EquityVolatilityCurveConfig() {}
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
    const string& ccy() const { return ccy_; }
    const Dimension& dimension() const { return dimension_; }
    const vector<string>& expiries() const { return expiries_; }
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    string& ccy() { return ccy_; }
    Dimension& dimension() { return dimension_; }
    vector<string>& expiries() { return expiries_; }
    //@}
private:
    string curveID_;
    string curveDescription_;
    string ccy_;
    Dimension dimension_;
    vector<string> expiries_;
};
}
}
