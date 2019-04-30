/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/bondindex.hpp
    \brief Light-weight bond index class for holding clean bond price fixing histories,
    without forwarding which is left to the respective engines.
    \ingroup indexes
*/

#ifndef quantext_bondindex_hpp
#define quantext_bondindex_hpp

#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Bond Index
/*! \ingroup indexes */
class BondIndex : public Index, public Observer {
public:
    /*! spot quote is interpreted as of today */
    BondIndex(const std::string& name, const Calendar& fixingCalendar = NullCalendar());

    //! \name Index interface
    //@{
    std::string name() const;
    Calendar fixingCalendar() const;
    bool isValidFixingDate(const Date& fixingDate) const;
    // Bond price fixing - only historical so far
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const;
    //@}
    //! \name Observer interface
    //@{
    void update();
    //@}
    //! \name Inspectors
    //@{
    std::string familyName() const { return familyName_; }
    //@}
    //! \name Fixing calculations
    //@{
    Real pastFixing(const Date& fixingDate) const;
    //@}
protected:
    std::string familyName_;
    std::string name_;

private:
    Calendar fixingCalendar_;
};

// inline definitions

inline std::string BondIndex::name() const { return name_; }

inline Calendar BondIndex::fixingCalendar() const { return fixingCalendar_; }

inline bool BondIndex::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

inline void BondIndex::update() { notifyObservers(); }

inline Real BondIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}
} // namespace QuantExt

#endif
