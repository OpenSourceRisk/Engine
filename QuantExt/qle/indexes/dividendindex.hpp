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

/*! \file qle/indexes/dividendindex.hpp
    \brief dividend index class for adding historic dividends to the fixing manager.
    \ingroup indexes
*/

#ifndef quantext_dividendindex_hpp
#define quantext_dividendindex_hpp

#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>

using namespace QuantLib;
namespace QuantExt {

//! Equity Index
/*! \ingroup indexes */
class DividendIndex : public Index, public Observer {
public:
    /*! spot quote is interpreted as of today */
    DividendIndex(const std::string& equityName, const Calendar& fixingCalendar);
    //! \name Index interface
    //@{
    std::string name() const;
    Calendar fixingCalendar() const;
    bool isValidFixingDate(const Date& fixingDate) const;
    // Dividend fixing - must be historical, for now.
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const;
    //@}
    //! \name Observer interface
    //@{
    void update();
    //@}
    //! \name Inspectors
    //@{
    std::string equityName() const { return equityName_; }
    //@}
    //! \name Fixing calculations
    //@{
    Real pastFixing(const Date& fixingDate) const;
    // @}
    //! \name Additional methods
    //@{
    virtual boost::shared_ptr<DividendIndex> clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
                                                 const Handle<YieldTermStructure>& dividend) const;
    // @}
protected:
    std::string equityName_;
    std::string name_;

private:
    Calendar fixingCalendar_;
};

// inline definitions

inline std::string DividendIndex::name() const { return name_; }

inline Calendar DividendIndex::fixingCalendar() const { return fixingCalendar_; }

inline bool DividendIndex::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

inline void DividendIndex::update() { notifyObservers(); }

inline Real DividendIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}
} // namespace QuantExt

#endif
