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

#include <ql/cashflow.hpp>

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
    explicit Indexing(const std::string& index, const string& indexFixingCalendar = "", const bool indexIsDirty = false,
                      const bool indexIsRelative = true, const bool indexIsConditionalOnSurvival = true,
                      const Real quantity = 1.0, const Real initialFixing = Null<Real>(),
                      const Real initialNotionalFixing = Null<Real>(),
                      const ScheduleData& valuationSchedule = ScheduleData(), const Size fixingDays = 0,
                      const string& fixingCalendar = "", const string& fixingConvention = "",
                      const bool inArrearsFixing = false)
        : hasData_(true), quantity_(quantity), index_(index), indexFixingCalendar_(indexFixingCalendar),
          indexIsDirty_(indexIsDirty), indexIsRelative_(indexIsRelative),
          indexIsConditionalOnSurvival_(indexIsConditionalOnSurvival), initialFixing_(initialFixing),
          initialNotionalFixing_(initialNotionalFixing), valuationSchedule_(valuationSchedule), fixingDays_(fixingDays),
          fixingCalendar_(fixingCalendar), fixingConvention_(fixingConvention), inArrearsFixing_(inArrearsFixing) {}

    //! \name Inspectors
    //@{
    bool hasData() const { return hasData_; }
    Real quantity() const { return quantity_; }
    const string& index() const { return index_; }
    // only used for FX, Bond indices
    const string& indexFixingCalendar() const { return indexFixingCalendar_; }
    // only used for Bond indices
    bool indexIsDirty() const { return indexIsDirty_; }
    bool indexIsRelative() const { return indexIsRelative_; }
    bool indexIsConditionalOnSurvival() const { return indexIsConditionalOnSurvival_; }
    //
    Real initialFixing() const { return initialFixing_; }
    Real initialNotionalFixing() const { return initialNotionalFixing_; }
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
    // only used for Bond indices
    string& indexFixingCalendar() { return indexFixingCalendar_; }
    // only used for Bond indices
    bool& indexIsDirty() { return indexIsDirty_; }
    bool& indexIsRelative() { return indexIsRelative_; }
    bool& indexIsConditionalOnSurvival() { return indexIsConditionalOnSurvival_; }
    //
    Real& initialFixing() { return initialFixing_; }
    Real& initialNotionalFixing() { return initialNotionalFixing_; }
    ScheduleData& valuationSchedule() { return valuationSchedule_; }
    Size& fixingDays() { return fixingDays_; }
    string& fixingCalendar() { return fixingCalendar_; }
    string& fixingConvention() { return fixingConvention_; }
    bool& inArrearsFixing() { return inArrearsFixing_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    bool hasData_;
    Real quantity_;
    string index_;
    string indexFixingCalendar_;
    bool indexIsDirty_;
    bool indexIsRelative_;
    bool indexIsConditionalOnSurvival_;
    Real initialFixing_;
    Real initialNotionalFixing_;
    ScheduleData valuationSchedule_;
    Size fixingDays_;
    string fixingCalendar_;
    string fixingConvention_;
    bool inArrearsFixing_;
};

} // namespace data
} // namespace ore
