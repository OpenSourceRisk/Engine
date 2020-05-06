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

/*! \file portfolio/indexing.hpp
    \brief leg indexing data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/schedule.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

//! Serializable object holding indexing data
/*!
  \ingroup tradedata
*/
class Indexing : public XMLSerializable {
public:
    Indexing()
        : hasData_(false), quantity_(1.0), initialFixing_(Null<Real>()), fixingDays_(0), inArrearsFixing_(false) {}
    explicit Indexing(const std::string& index, const Size indexFixingDays = 0, const string& indexFixingCalendar = "",
                      const Real quantity = 1.0, const Real initialFixing = Null<Real>(),
                      const ScheduleData& valuationSchedule = ScheduleData(), const Size fixingDays = 0,
                      const string& fixingCalendar = "", const string& fixingConvention = "",
                      const bool inArrearsFixing = false)
        : hasData_(true), quantity_(quantity), index_(index), initialFixing_(initialFixing),
          valuationSchedule_(valuationSchedule), fixingDays_(fixingDays), fixingCalendar_(fixingCalendar),
          fixingConvention_(fixingConvention), inArrearsFixing_(inArrearsFixing) {}

    //! \name Inspectors
    //@{
    bool hasData() const { return hasData_; }
    Real quantity() const { return quantity_; }
    const string& index() const { return index_; }
    const Size indexFixingDays() const { return indexFixingDays_; }
    const string& indexFixingCalendar() const { return indexFixingCalendar_; }
    Real initialFixing() const { return initialFixing_; }
    const ScheduleData& valuationSchedule() const { return valuationSchedule_; }
    Size fixingDays() const { return fixingDays_; }
    const string& fixingCalendar() const { return fixingCalendar_; }
    const string& fixingConvention() const { return fixingConvention_; }
    bool inArrearsFixing() const { return inArrearsFixing_; }
    //@}

    //! \name Modifiers
    //@{
    Real& quantity() { return quantity_; }
    string& index() { return index_; }
    Size& indexFixingDays() { return indexFixingDays_; }
    string& indexFixingCalendar() { return indexFixingCalendar_; }
    Real& initialFixing() { return initialFixing_; }
    ScheduleData& valuationSchedule() { return valuationSchedule_; }
    Size& fixingDays() { return fixingDays_; }
    string& fixingCalendar() { return fixingCalendar_; }
    string& fixingConvention() { return fixingConvention_; }
    bool& inArrearsFixing() { return inArrearsFixing_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    bool hasData_;
    Real quantity_;
    string index_;
    Size indexFixingDays_;
    string indexFixingCalendar_;
    Real initialFixing_;
    ScheduleData valuationSchedule_;
    Size fixingDays_;
    string fixingCalendar_;
    string fixingConvention_;
    bool inArrearsFixing_;
};

} // namespace data
} // namespace ore
