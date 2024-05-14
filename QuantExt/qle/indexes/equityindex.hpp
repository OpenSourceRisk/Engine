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
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <ql/currency.hpp>
#include <qle/indexes/eqfxindexbase.hpp>
#include <qle/indexes/dividendmanager.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Equity Index
/*! \ingroup indexes 
  Renamed to EquityIndex2, because Quantlib has introduced an EquityIndex class in v1.30
  which causes name conflicts in the compilation of the joint SWIG wrapper across 
  QuantLib and QuantExt.
*/
class EquityIndex2 : public EqFxIndexBase {
public:
    /*! spot quote is interpreted as of today */
    EquityIndex2(const std::string& familyName, const Calendar& fixingCalendar, const Currency& currency,
                const Handle<Quote> spotQuote = Handle<Quote>(),
                const Handle<YieldTermStructure>& rate = Handle<YieldTermStructure>(),
                const Handle<YieldTermStructure>& dividend = Handle<YieldTermStructure>());
    //! \name Index interface
    //@{
    std::string name() const override;
    Currency currency() const { return currency_; }
    Calendar fixingCalendar() const override;
    bool isValidFixingDate(const Date& fixingDate) const override;
    // Equity fixing price - can be either fixed historical or forecasted.
    // Forecasted price can include dividend returns by setting incDividend = true
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing, bool incDividend) const;
    // Dividends
    //! stores the historical dividend at the given date
    /*! the date passed as arguments must be the actual calendar
        date of the dividend.
    */
    virtual void addDividend(const Dividend& fixing, bool forceOverwrite = false);
    virtual const std::set<Dividend>& dividendFixings() const { return DividendManager::instance().getHistory(name()); }
    Real dividendsBetweenDates(const Date& startDate, const Date& endDate) const;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
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
    virtual Real forecastFixing(const Time& fixingTime) const override;
    virtual Real forecastFixing(const Date& fixingDate, bool incDividend) const;
    virtual Real forecastFixing(const Time& fixingTime, bool incDividend) const;
    virtual Real pastFixing(const Date& fixingDate) const override;
    // @}
    //! \name Additional methods
    //@{
    virtual QuantLib::ext::shared_ptr<EquityIndex2> clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
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

inline std::string EquityIndex2::name() const { return name_; }

inline Calendar EquityIndex2::fixingCalendar() const { return fixingCalendar_; }

inline bool EquityIndex2::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

inline void EquityIndex2::update() { notifyObservers(); }

inline Real EquityIndex2::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}
} // namespace QuantExt

#endif
