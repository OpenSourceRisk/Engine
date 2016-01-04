/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*
 Copyright (C) 2012 Roland Lichters

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file fxindex.hpp
    \brief foreign exchange rate index
*/

#ifndef quantext_fxindex_hpp
#define quantext_fxindex_hpp

#include <ql/index.hpp>
#include <ql/time/calendar.hpp>
#include <ql/currency.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>

namespace QuantLib {

    //! base class for interest rate indexes
    /*! \todo add methods returning InterestRate */
    class FxIndex : public Index,
		    public Observer {
      public:
      FxIndex(const Currency& unitCurrency,
	      const Currency& priceCurrency,
	      const Calendar& fixingCalendar);
        //! \name Index interface
        //@{
        std::string name() const;
        Calendar fixingCalendar() const;
        bool isValidFixingDate(const Date& fixingDate) const;
        Rate fixing(const Date& fixingDate,
                    bool forecastTodaysFixing = false) const;
        //@}
        //! \name Observer interface
        //@{
        void update();
        //@}
        //! \name Inspectors
        //@{
        const Currency& unitCurrency() const { return unitCurrency_; }
        const Currency& priceCurrency() const { return priceCurrency_; }
        //@}
        //! \name Fixing calculations
        //@{
        Rate pastFixing(const Date& fixingDate) const;
        //@}
      private:
        std::string name_;
        Currency unitCurrency_;
        Currency priceCurrency_;
        Calendar fixingCalendar_;
    };


    // inline definitions

    inline std::string FxIndex::name() const {
        return name_;
    }

    inline Calendar FxIndex::fixingCalendar() const {
        return fixingCalendar_;
    }

    inline bool FxIndex::isValidFixingDate(const Date& d) const {
        return fixingCalendar().isBusinessDay(d);
    }

    inline void FxIndex::update() {
        notifyObservers();
    }

    inline Rate FxIndex::pastFixing(const Date& fixingDate) const {
        QL_REQUIRE(isValidFixingDate(fixingDate),
                   fixingDate << " is not a valid fixing date");
        return timeSeries()[fixingDate];
    }

}

#endif
