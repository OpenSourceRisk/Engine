/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/fixingdates.hpp>
#include <ql/settings.hpp>

using QuantLib::CashFlow;
using QuantLib::Date;
using QuantLib::FloatingRateCoupon;
using QuantLib::InflationCoupon;
using QuantLib::OvernightIndexedCoupon;
using QuantLib::AverageBMACoupon;
using QuantExt::AverageONIndexedCoupon;
using QuantExt::EquityCoupon;
using QuantExt::FloatingRateFXLinkedNotionalCoupon;
using QuantExt::FXLinkedCashFlow;
using QuantLib::Leg;
using QuantLib::Settings;

using std::function;
using std::set;
using std::vector;

namespace {

// Utility functions to avoid code duplication below

// Add fixing dates on coupons with fxFixingDate method i.e. FX fixings
template <class Coupon>
void addFxFixings(const Coupon& c, set<Date>& dates, const Date& today, bool ethf) {
    Date fxFixingDate = c.fxFixingDate();
    if (fxFixingDate < today || (fxFixingDate == today && ethf)) {
        dates.insert(fxFixingDate);
    }
}

// Add fixing dates on coupons with fixingDates method i.e. multiple fixings
template <class Coupon>
void addMultipleFixings(const Coupon& c, set<Date>& dates, const Date& today, bool ethf) {
    // Assume that I get sorted fixing dates here
    vector<Date> fixingDates = c.fixingDates();
    if (fixingDates.front() <= today) {
        for (const auto& fixingDate : fixingDates) {
            if (fixingDate < today || (fixingDate == today && ethf)) {
                dates.insert(fixingDate);
            }
        }
    }
}

}

namespace ore {
namespace data {

set<Date> fixingDates(const Leg& leg, bool includeSettlementDateFlows, Date settlementDate,
    function<boost::shared_ptr<CashFlow>(boost::shared_ptr<CashFlow>)> f) {
    
    FixingDateGetter fdg(includeSettlementDateFlows, settlementDate);
    
    for (const auto& cf: leg) {
        if (f)
            f(cf)->accept(fdg);
        else
            cf->accept(fdg);
    }
    
    return fdg.fixingDates();
}

FixingDateGetter::FixingDateGetter(bool includeSettlementDateFlows,
    const Date& settlementDate) : includeSettlementDateFlows_(includeSettlementDateFlows) {

    today_ = settlementDate;
    if (today_ == Date())
        today_ = Settings::instance().evaluationDate();
}

void FixingDateGetter::visit(CashFlow& c) {
    // Do nothing if we fall through to here
}

void FixingDateGetter::visit(FloatingRateCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        Date fixingDate = c.fixingDate();
        if (fixingDate < today_ || (fixingDate == today_ &&
            Settings::instance().enforcesTodaysHistoricFixings())) {
            fixingDates_.insert(fixingDate);
        }
    }
}

// TODO: Leave for now but will need to change this later
//       More logic than just FloatingRateCoupon needed
void FixingDateGetter::visit(InflationCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        Date fixingDate = c.fixingDate();
        if (fixingDate < today_ || (fixingDate == today_ &&
            Settings::instance().enforcesTodaysHistoricFixings())) {
            fixingDates_.insert(fixingDate);
        }
    }
}

void FixingDateGetter::visit(OvernightIndexedCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        addMultipleFixings(c, fixingDates_, today_, Settings::instance().enforcesTodaysHistoricFixings());
    }
}

void FixingDateGetter::visit(AverageBMACoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        addMultipleFixings(c, fixingDates_, today_, Settings::instance().enforcesTodaysHistoricFixings());
    }
}

void FixingDateGetter::visit(AverageONIndexedCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        addMultipleFixings(c, fixingDates_, today_, Settings::instance().enforcesTodaysHistoricFixings());
    }
}

void FixingDateGetter::visit(EquityCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        addMultipleFixings(c, fixingDates_, today_, Settings::instance().enforcesTodaysHistoricFixings());
    }
}

void FixingDateGetter::visit(FloatingRateFXLinkedNotionalCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        addFxFixings(c, fixingDates_, today_, Settings::instance().enforcesTodaysHistoricFixings());
    }
}

void FixingDateGetter::visit(FXLinkedCashFlow& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        addFxFixings(c, fixingDates_, today_, Settings::instance().enforcesTodaysHistoricFixings());
    }
}

}
}
