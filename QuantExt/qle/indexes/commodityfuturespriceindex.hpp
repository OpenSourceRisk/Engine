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

/*! \file commodityfuturespriceindex.hpp
    \brief commodity futures price index
*/

#ifndef quantext_commodityfuturespriceindex_hpp
#define quantext_commodityfuturespriceindex_hpp

#include <ql/index.hpp>
#include <ql/time/calendar.hpp>
#include <ql/currency.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>

namespace QuantLib {

    //! commodity futures price index
    class CommodityFuturesPriceIndex : public Index,
                                       public Observer {
      public:
        CommodityFuturesPriceIndex(std::string baseName, Period expiry, 
                                   Calendar fixingCalendar);
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
        const Period& expiry() const { return expiry_; }
        //@}
        //! \name Fixing calculations
        //@{
        Rate pastFixing(const Date& fixingDate) const;
        //@}
      private:
        std::string name_;
        Period expiry_;
        Calendar fixingCalendar_;
    };


    // inline definitions

    inline std::string CommodityFuturesPriceIndex::name() const {
        return name_;
    }

    inline Calendar CommodityFuturesPriceIndex::fixingCalendar() const {
        return fixingCalendar_;
    }

    inline bool CommodityFuturesPriceIndex::isValidFixingDate(const Date& d) const {
        return fixingCalendar().isBusinessDay(d);
    }

    inline void CommodityFuturesPriceIndex::update() {
        notifyObservers();
    }

    inline Rate CommodityFuturesPriceIndex::pastFixing(const Date& fixingDate) const {
        QL_REQUIRE(isValidFixingDate(fixingDate),
                   fixingDate << " is not a valid fixing date");
        return timeSeries()[fixingDate];
    }

}

#endif
