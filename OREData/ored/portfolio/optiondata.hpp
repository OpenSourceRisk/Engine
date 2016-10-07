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

/*! \file ored/portfolio/optiondata.hpp
    \brief trade option data model and serialization
    \ingroup openxva::portfolio
*/

#pragma once

#include <ored/portfolio/schedule.hpp>

namespace ore {
namespace data {

//! Serializable obejct holding option data
/*!
  \ingroup tradedata
*/
class OptionData : public XMLSerializable {
public:
    //! Default constructor
    OptionData() : payoffAtExpiry_(true), premium_(0.0) {}
    //! Constructor
    OptionData(string longShort, string callPut, string style, bool payoffAtExpiry, vector<string> exerciseDates,
               // notice days ?
               string settlement = "Cash", double premium = 0, string premiumCcy = "", string premiumPayDate = "",
               vector<double> exerciseFees = vector<Real>(), vector<double> exercisePrices = vector<Real>())
        : longShort_(longShort), callPut_(callPut), style_(style), payoffAtExpiry_(payoffAtExpiry),
          exerciseDates_(exerciseDates), noticePeriod_("0D"), // FIXME ?
          settlement_(settlement), premium_(premium), premiumCcy_(premiumCcy), premiumPayDate_(premiumPayDate),
          exerciseFees_(exerciseFees), exercisePrices_(exercisePrices) {}

    //! \name Inspectors
    //@{
    const string& longShort() const { return longShort_; }
    const string& callPut() const { return callPut_; }
    const string& style() const { return style_; }
    const bool& payoffAtExpiry() const { return payoffAtExpiry_; }
    const vector<string>& exerciseDates() const { return exerciseDates_; }
    const string& noticePeriod() const { return noticePeriod_; }
    const string& settlement() const { return settlement_; }
    double premium() const { return premium_; }
    const string& premiumCcy() const { return premiumCcy_; }
    const string& premiumPayDate() const { return premiumPayDate_; }
    const vector<double>& exerciseFees() const { return exerciseFees_; }
    const vector<double>& exercisePrices() const { return exercisePrices_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    string longShort_;    // long or short
    string callPut_;      // call or put
    string style_;        // European, Bermudan, American
    bool payoffAtExpiry_; // Y or N
    vector<string> exerciseDates_;
    string noticePeriod_;
    string settlement_; // Cash or Physical, default Cash.
    double premium_;
    string premiumCcy_;
    string premiumPayDate_;
    vector<double> exerciseFees_;
    vector<double> exercisePrices_;
};
}
}
