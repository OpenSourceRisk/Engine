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

/*! \file portfolio/basketdata.hpp
    \brief credit basket data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>

#include <vector>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

//! Serializable Basket Data
/*!
  \ingroup tradedata
*/

class BasketData : public XMLSerializable {
public:
    //! Default constructor
    BasketData() {}
    //! Constructor
    BasketData(const vector<string>& issuers, const vector<string>& creditCurves, const vector<double>& notionals,
               const vector<string>& currencies, const vector<string>& qualifiers)
        : issuers_(issuers), creditCurves_(creditCurves), notionals_(notionals), currencies_(currencies),
          qualifiers_(qualifiers) {}

    //! \name Inspectors
    //@{
    const vector<string>& issuers() const { return issuers_; }
    const vector<string>& creditCurves() const { return creditCurves_; }
    const vector<double>& notionals() const { return notionals_; }
    const vector<string>& currencies() const { return currencies_; }
    const vector<string>& qualifiers() const { return qualifiers_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    vector<string> issuers_;
    vector<string> creditCurves_;
    vector<double> notionals_;
    vector<string> currencies_;
    vector<string> qualifiers_;
};
}
}
