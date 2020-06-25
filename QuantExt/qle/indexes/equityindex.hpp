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

/*! \file qle/indexes/equityindex.hpp
    \brief equity index class for holding equity fixing histories and forwarding.
    \ingroup indexes
*/

#ifndef quantext_equityindex_hpp
#define quantext_equityindex_hpp

#include <ql/currency.hpp>
#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Equity Index
/*! \ingroup indexes */
class EquityIndex : public Index, public Observer {
public:
    /*! spot quote is interpreted as of today */
    EquityIndex(const std::string& familyName, const Calendar& fixingCalendar, const Currency& currency,
                const Handle<Quote> spotQuote = Handle<Quote>(),
                const Handle<YieldTermStructure>& rate = Handle<YieldTermStructure>(),
                const Handle<YieldTermStructure>& dividend = Handle<YieldTermStructure>());
    //! \name Index interface
    //@{
    std::string name() const;
    void resetName() { name_ = familyName(); }
    std::string dividendName() const { return name() + "_div"; }
    Currency currency() const { return currency_; }
    Calendar fixingCalendar() const;
    bool isValidFixingDate(const Date& fixingDate) const;
    // Equity fixing price - can be either fixed hstorical or forecasted.
    // Forecasted price can include dividend returns by setting incDividend = true
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const;
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing, bool incDividend) const;
    // Dividend Fixings
    //! stores the historical dividend fixing at the given date
    /*! the date passed as arguments must be the actual calendar
        date of the fixing; no settlement days must be used.
    */
    virtual void addDividend(const Date& fixingDate, Real fixing, bool forceOverwrite = false);
    const TimeSeries<Real>& dividendFixings() const { return IndexManager::instance().getHistory(dividendName()); }
    Real dividendsBetweenDates(const Date& startDate, const Date& endDate) const;
    //@}
    //! \name Observer interface
    //@{
    void update();
    //@}
    //! \name Inspectors
    //@{
    std::string familyName() const { return familyName_; }
    const Handle<Quote>& equitySpot() const { return spotQuote_; }
    const Handle<YieldTermStructure>& equityForecastCurve() const { return rate_; }
    const Handle<YieldTermStructure>& equityDividendCurve() const { return dividend_; }
    //@}
    //! \name Fixing calculations
    //@{
    virtual Real forecastFixing(const Date& fixingDate) const;
    virtual Real forecastFixing(const Time& fixingTime) const;
    virtual Real forecastFixing(const Date& fixingDate, bool incDividend) const;
    virtual Real forecastFixing(const Time& fixingTime, bool incDividend) const;
    Real pastFixing(const Date& fixingDate) const;
    // @}
    //! \name Additional methods
    //@{
    virtual boost::shared_ptr<EquityIndex> clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
                                                 const Handle<YieldTermStructure>& dividend) const;
    // @}
protected:
    std::string familyName_;
    Currency currency_;
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
} // namespace QuantExt

#endif
