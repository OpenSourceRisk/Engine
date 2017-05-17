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

/*!
  \file ored/configuration/inflationcpfloorpricesurfaceconfig.hpp
  \brief Inflation cap floor price surface config
  \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

using std::string;
using std::vector;
using ore::data::XMLSerializable;
using ore::data::XMLNode;
using ore::data::XMLDocument;
using QuantLib::Period;
using QuantLib::DayCounter;
using QuantLib::Calendar;
using QuantLib::BusinessDayConvention;

namespace ore {
namespace data {

class InflationCapFloorPriceSurfaceConfig : public XMLSerializable {
public:
    enum class Type { ZC, YY };

    InflationCapFloorPriceSurfaceConfig() {}
    InflationCapFloorPriceSurfaceConfig(const string& curveID, const string& curveDescription, const Type type,
                                        const Period& observationLag, const Calendar& calendar,
                                        const BusinessDayConvention& businessDayConvention,
                                        const DayCounter& dayCounter, const string& index, const string& indexCurve,
                                        const string& yieldTermStructure, const vector<Real>& capStrikes,
                                        const vector<Real>& floorStrikes, const vector<Period>& maturities);
    virtual ~InflationCapFloorPriceSurfaceConfig() {}

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);

    // Inspectors
    const string& curveID() const { return curveID_; }
    const string& curveDescription() const { return curveDescription_; }
    const Type& type() const { return type_; }
    const Real& startRate() const { return startRate_; }
    const Period& observationLag() const { return observationLag_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const string& index() const { return index_; }
    const string& indexCurve() const { return indexCurve_; }
    const string& yieldTermStructure() const { return yieldTermStructure_; }
    const vector<Real>& capStrikes() const { return capStrikes_; }
    const vector<Real>& floorStrikes() const { return floorStrikes_; }
    const vector<Period>& maturities() const { return maturities_; }

    // Setters
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    Type& type() { return type_; }
    Real& startRate() { return startRate_; }
    Period& observationLag() { return observationLag_; }
    Calendar& calendar() { return calendar_; }
    BusinessDayConvention& businessDayConvention() { return businessDayConvention_; }
    DayCounter& dayCounter() { return dayCounter_; }
    string& index() { return index_; }
    string& indexCurve() { return indexCurve_; }
    string& yieldTermStructure() { return yieldTermStructure_; }
    vector<Real>& capStrikes() { return capStrikes_; }
    vector<Real>& floorStrikes() { return floorStrikes_; }
    vector<Period>& maturities() { return maturities_; }

private:
    string curveID_;
    string curveDescription_;
    Type type_;
    Real startRate_;
    Period observationLag_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    DayCounter dayCounter_;
    string index_;
    string indexCurve_;
    string yieldTermStructure_;
    vector<Real> capStrikes_;
    vector<Real> floorStrikes_;
    vector<Period> maturities_;
};
} // namespace data
} // namespace ore
