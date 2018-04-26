/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/equityindex.cpp
    \brief equity index class for holding equity fixing histories and forwarding.
    \ingroup indexes
*/

#ifndef quantext_equityindex_hpp
#define quantext_equityindex_hpp

#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>

using namespace QuantLib;
namespace QuantExt {

//! Equity Index
/*! \ingroup indexes */
class EquityIndex : public Index, public Observer {
public:
    /*! spot quote is interpreted as of today */
    EquityIndex(const std::string& familyName, const Calendar& fixingCalendar,
                const Handle<Quote> spotQuote = Handle<Quote>(),
                const Handle<YieldTermStructure>& rate = Handle<YieldTermStructure>(),
                const Handle<YieldTermStructure>& dividend = Handle<YieldTermStructure>());
    //! \name Index interface
    //@{
    std::string name() const;
    Calendar fixingCalendar() const;
    bool isValidFixingDate(const Date& fixingDate) const;
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
    virtual Real forecastFixing(const Date& fixingDate) const;
    virtual Real forecastFixing(const Time& fixingTime) const;
    Real pastFixing(const Date& fixingDate) const;
    // @}
    //! \name Additional methods
    //@{
    virtual boost::shared_ptr<EquityIndex> clone(
        const Handle<Quote> spotQuote, 
        const Handle<YieldTermStructure>& rate,
        const Handle<YieldTermStructure>& dividend) const;
    // @}
protected:
    std::string familyName_;
    const Handle<YieldTermStructure> rate_, dividend_;
    std::string name_;
    const Handle<Quote> spotQuote_;

private:
    Calendar fixingCalendar_;
};

// inline definitions

inline std::string EquityIndex::name() const { return name_; }

inline Calendar EquityIndex::fixingCalendar() const { return fixingCalendar_; }

inline bool EquityIndex::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

inline void EquityIndex::update() { notifyObservers(); }

inline Real EquityIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}
}

#endif
