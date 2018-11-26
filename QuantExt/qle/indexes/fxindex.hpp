/*
 Copyright (C) 2015 Peter Caspers
 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/
 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program.
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

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

/*! \file fxindex.hpp
    \brief FX index class
        \ingroup indexes
*/

#ifndef quantext_fxindex_hpp
#define quantext_fxindex_hpp

#include <ql/currency.hpp>
#include <ql/exchangerate.hpp>
#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
using namespace QuantLib;
namespace QuantExt {

//! FX Index
/*! \ingroup indexes */
class FxIndex : public Index, public Observer {
public:
    /*! familyName may be e.g. ECB
        settlementDays determine the spot date of the currency pair
        source is the asset or foreign currency
        target is the numeraire or domestic currency
        fixingCalendar is the calendar defining good days for the pair
        this class uses the exchange rate manager to retrieve spot values */
    FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
            const Calendar& fixingCalendar, const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
            const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>());
    FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
            const Calendar& fixingCalendar, const Handle<Quote> fxQuote,
            const Handle<YieldTermStructure>& sourceYts = Handle<YieldTermStructure>(),
            const Handle<YieldTermStructure>& targetYts = Handle<YieldTermStructure>());
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
    Natural fixingDays() const { return fixingDays_; }
    Date fixingDate(const Date& valueDate) const;
    const Currency& sourceCurrency() const { return sourceCurrency_; }
    const Currency& targetCurrency() const { return targetCurrency_; }
    //@}
    /*! \name Date calculations */
    virtual Date valueDate(const Date& fixingDate) const;
    //! \name Fixing calculations
    //@{
    //! It can be overridden to implement particular conventions
    virtual Real forecastFixing(const Date& fixingDate) const;
    Real pastFixing(const Date& fixingDate) const;
    // @}
protected:
    std::string familyName_;
    Natural fixingDays_;
    Currency sourceCurrency_, targetCurrency_;
    const Handle<YieldTermStructure> sourceYts_, targetYts_;
    std::string name_;
    const Handle<Quote> fxQuote_;
    bool useQuote_;

private:
    Calendar fixingCalendar_;
};

// inline definitions

inline std::string FxIndex::name() const { return name_; }

inline Calendar FxIndex::fixingCalendar() const { return fixingCalendar_; }

inline bool FxIndex::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

inline void FxIndex::update() { notifyObservers(); }

inline Date FxIndex::fixingDate(const Date& valueDate) const {
    Date fixingDate = fixingCalendar().advance(valueDate, -static_cast<Integer>(fixingDays_), Days);
    return fixingDate;
}

inline Date FxIndex::valueDate(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return fixingCalendar().advance(fixingDate, fixingDays_, Days);
}

inline Real FxIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}
} // namespace QuantExt

#endif
