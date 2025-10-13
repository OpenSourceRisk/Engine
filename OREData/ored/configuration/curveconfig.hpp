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

/*! \file ored/configuration/curveconfig.hpp
    \brief Base curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <set>

namespace ore {
namespace data {
using ore::data::XMLSerializable;
using std::set;
using std::string;
using std::pair;

//! Base curve configuration
/*!
  \ingroup configuration
*/
class CurveConfig : public XMLSerializable {
public:
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    CurveConfig(const string& curveID, const string& curveDescription, const vector<string>& quotes = vector<string>())
        : curveID_(curveID), curveDescription_(curveDescription), quotes_(quotes) {}
    //! Default constructor
    CurveConfig() {}
    //@}

    //! \name Inspectors
    //@{
    const string& curveID() const { return curveID_; }
    const string& curveDescription() const { return curveDescription_; }
    set<string> requiredCurveIds(const CurveSpec::CurveType& curveType) const;
    set<string> requiredNames(const MarketObject o, const std::string& configuration) const;
    map<CurveSpec::CurveType, set<string>> requiredCurveIds() const;
    map<MarketObject, set<string>> requiredNames(const std::string& configuration) const;
    map<pair<MarketObject, string>, set<string>> requiredNames() const;
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    void setRequiredCurveIds(const CurveSpec::CurveType& curveType, const set<string>& ids);
    void setRequiredCurveIds(const map<CurveSpec::CurveType, set<string>>& ids);
    void setRequiredNames(const MarketObject o, const std::string& configuration, const set<string>& ids);
    void setRequiredNames(const map<std::pair<MarketObject, std::string>, set<string>>& ids);
    //@}

    //! Return all the market quotes required for this config
    virtual const vector<string>& quotes() { return quotes_; }

protected:
    virtual void populateRequiredIds() const = 0;

    mutable bool requiredIdsInitialized_ = false;
    mutable map<CurveSpec::CurveType, set<string>> requiredCurveIds_;
    // empty market configuration (second component of tuple) matches all configurations
    mutable map<std::pair<MarketObject, std::string>, set<string>> requiredNames_;

    string curveID_;
    string curveDescription_;
    vector<string> quotes_;
};

} // namespace data
} // namespace ore
