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

#include <qle/calendars/russia.hpp>
#include <ql/errors.hpp>

namespace QuantExt {

    RussiaModified::SettlementImpl::SettlementImpl() {
        originalSettlementCalendar_ = Russia(Russia::Settlement);
    }

    RussiaModified::ExchangeImpl::ExchangeImpl() {
        originalSettlementCalendar_ = Russia(Russia::Settlement);
        originalExchangeCalendar_ = Russia(Russia::MOEX);
    }

    RussiaModified::RussiaModified(Russia::Market market) {
        // all calendar instances share the same implementation instance
        static ext::shared_ptr<Calendar::Impl> settlementImpl(
                                                  new RussiaModified::SettlementImpl);
        static ext::shared_ptr<Calendar::Impl> exchangeImpl(
                                                    new RussiaModified::ExchangeImpl);

        switch (market) {
          case Russia::Settlement:
            impl_ = settlementImpl;
            break;
          case Russia::MOEX:
            impl_ = exchangeImpl;
            break;
          default:
            QL_FAIL("unknown market");
        }
    }

    bool RussiaModified::SettlementImpl::isBusinessDay(const Date& date) const {
        return originalSettlementCalendar_.isBusinessDay(date);
    }

    bool RussiaModified::ExchangeImpl::isBusinessDay(const Date& date) const {
        // The exchange was formally established in 2011, so data are only
        // available from 2012 to present.
        // We use the settlement calendar as a proxy before 2012
        if (date.year() < 2012)
            return originalSettlementCalendar_.isBusinessDay(date);
        else
            return originalExchangeCalendar_.isBusinessDay(date);
    }

}
