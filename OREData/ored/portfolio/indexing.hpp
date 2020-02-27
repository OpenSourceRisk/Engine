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
    class Fx : public XMLSerializable {
    public:
        Fx() : hasData_(false), initialFixing_(Null<Real>()), fixingDays_(0), inArrearsFixing_(false) {}
        Fx(const std::string& index, const Real initialFixing = Null<Real>(), const Size fixingDays = 0,
           const string& fixingCalendar = "", const string& fixingConvention = "", const bool inArrearsFixing = false)
            : hasData_(true), index_(index), initialFixing_(initialFixing), fixingDays_(fixingDays),
              fixingCalendar_(fixingCalendar), fixingConvention_(fixingConvention), inArrearsFixing_(inArrearsFixing) {}

        //! Inspectors
        bool hasData() const { return hasData_; }
        const string& index() const { return index_; }
        Real initialFixing() const { return initialFixing_; }
        Size fixingDays() const { return fixingDays_; }
        const string& fixingCalendar() const { return fixingCalendar_; }
        const string& fixingConvention() const { return fixingConvention_; }
        bool inArrearsFixing() const { return inArrearsFixing_; }

        //! Serialisation
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) override;

    private:
        bool hasData_;
        string index_;
        Real initialFixing_;
        Size fixingDays_;
        string fixingCalendar_;
        string fixingConvention_;
        bool inArrearsFixing_;
    }; // Fx

    Indexing()
        : hasData_(false), quantity_(0.0), initialFixing_(Null<Real>()), fixingDays_(0), inArrearsFixing_(false) {}
    Indexing(const Real quantity, const std::string& index, const Real initialFixing = Null<Real>(),
             const ScheduleData& valuationSchedule = ScheduleData(), const Size fixingDays = 0,
             const string& fixingCalendar = "", const string& fixingConvention = "", const bool inArrearsFixing = false,
             const Fx& fx = {})
        : hasData_(true), quantity_(quantity), index_(index), initialFixing_(initialFixing),
          valuationSchedule_(valuationSchedule), fixingDays_(fixingDays), fixingCalendar_(fixingCalendar),
          fixingConvention_(fixingConvention), inArrearsFixing_(inArrearsFixing), fx_(fx) {}

    //! \name Inspectors
    //@{
    bool hasData() const { return hasData_; }
    Real quantity() const { return quantity_; }
    const string& index() const { return index_; }
    Real initialFixing() const { return initialFixing_; }
    const ScheduleData& valuationSchedule() const { return valuationSchedule_; }
    Size fixingDays() const { return fixingDays_; }
    const string& fixingCalendar() const { return fixingCalendar_; }
    const string& fixingConvention() const { return fixingConvention_; }
    bool inArrearsFixing() const { return inArrearsFixing_; }
    Fx fx() const { return fx_; }
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
    Real initialFixing_;
    ScheduleData valuationSchedule_;
    Size fixingDays_;
    string fixingCalendar_;
    string fixingConvention_;
    bool inArrearsFixing_;
    Fx fx_;
};

} // namespace data
} // namespace ore
