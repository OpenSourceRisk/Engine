/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file russia.hpp
    \brief Russian calendar, modified QuantLib Russia to extend MOEX before 2012
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
