/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 StatPro Italia srl

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

/*! \file russia.hpp
    \brief Russian calendar, modified to extend MOEX before 2012
*/

#ifndef quantext_russia_modified_calendar_hpp
#define quantext_russia_modified_calendar_hpp

#include <ql/time/calendars/russia.hpp>

namespace QuantExt {
using namespace QuantLib;

    //! Russian calendars
    /*! Modified MOEX, using settlement implementation to extend before 2012.
        \ingroup calendars
    */
    class RussiaModified : public Calendar {
      private:
        class SettlementImpl : public Calendar::OrthodoxImpl {
          public:
            SettlementImpl();
            std::string name() const override { return "Russian settlement"; }
            bool isBusinessDay(const Date&) const override;
        private:
            Calendar originalSettlementCalendar_;
        };
        class ExchangeImpl : public Calendar::OrthodoxImpl {
          public:
            ExchangeImpl();
            std::string name() const override { return "Moscow exchange, modified"; }
            bool isBusinessDay(const Date&) const override;            
          private:
            Calendar originalSettlementCalendar_, originalExchangeCalendar_;
        };
    public:
        RussiaModified(Russia::Market = Russia::Settlement);
    };

}


#endif
