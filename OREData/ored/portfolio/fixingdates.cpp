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
using QuantLib::IndexedCashFlow;
using QuantLib::CPICashFlow;
using QuantLib::CPICoupon;
using QuantLib::YoYInflationCoupon;
using QuantLib::CPI;
using QuantLib::OvernightIndexedCoupon;
using QuantLib::AverageBMACoupon;
using QuantExt::AverageONIndexedCoupon;
using QuantExt::EquityCoupon;
using QuantExt::FloatingRateFXLinkedNotionalCoupon;
using QuantExt::FXLinkedCashFlow;
using QuantLib::Leg;
using QuantLib::Settings;
using QuantLib::ZeroInflationIndex;
using QuantLib::YoYInflationIndex;
using QuantLib::Period;
using QuantLib::Frequency;
using QuantLib::Years;

using std::function;
using std::set;
using std::vector;
using std::pair;

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

// Return the set of dates on which a fixing will be required, if any.
set<Date> needsForecast(const Date& fixingDate, const Date& today, bool interpolated, 
    Frequency frequency, const Period& availabilityLag) {
    
    set<Date> result;

    Date todayMinusLag = today - availabilityLag;
    Date historicalFixingKnown = inflationPeriod(todayMinusLag, frequency).first - 1;
    
    pair<Date, Date> lim = inflationPeriod(fixingDate, frequency);
    result.insert(lim.first);
    Date latestNeededDate = fixingDate;
    if (interpolated) {
        if (fixingDate > lim.first) {
            latestNeededDate += Period(frequency);
            result.insert(lim.second + 1);
        }
    }
    
    if (latestNeededDate <= historicalFixingKnown) {
        // Know that fixings are available
        return result;
    } else if (latestNeededDate > today) {
        // Know that fixings are not available
        return {};
    } else {
        // Grey area here but for now return nothing
        return {};
    }
}

// Common code for CPICoupon and CPICashFlow
void addZeroInflationDates(set<Date>& dates, const Date& fixingDate, const Date& today, 
    boost::shared_ptr<ZeroInflationIndex> index, CPI::InterpolationType interpolation, Frequency f) {
    
    set<Date> fixingDates;
    
    if (interpolation == CPI::AsIndex) {
        fixingDates = needsForecast(fixingDate, today, index->interpolated(), 
            index->frequency(), index->availabilityLag());
    } else {
        pair<Date, Date> lim = inflationPeriod(fixingDate, f);
        fixingDates = needsForecast(lim.first, today, index->interpolated(),
            index->frequency(), index->availabilityLag());
        if (interpolation == CPI::Linear) {
            auto moreDates = needsForecast(lim.second + 1, today, index->interpolated(),
                index->frequency(), index->availabilityLag());
            fixingDates.insert(moreDates.begin(), moreDates.end());
        }
    }

    dates.insert(fixingDates.begin(), fixingDates.end());
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

void FixingDateGetter::visit(IndexedCashFlow& c) {
    if (CPICashFlow* cc = dynamic_cast<CPICashFlow*>(&c)) {
        visit(*cc);
    }
}

void FixingDateGetter::visit(CPICashFlow& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {
        
        Date fixingDate = c.fixingDate();
        
        // CPICashFlow must have a ZeroInflationIndex
        auto zeroInflationIndex = boost::dynamic_pointer_cast<ZeroInflationIndex>(c.index());
        QL_REQUIRE(zeroInflationIndex, "Expected CPICashFlow to have an index of type ZeroInflationIndex");
        
        // Mimic the logic in QuantLib::CPICashFlow::amount() to arrive at the fixing date(s) needed
        addZeroInflationDates(fixingDates_, fixingDate, today_, zeroInflationIndex, c.interpolation(), c.frequency());
    }
}

void FixingDateGetter::visit(CPICoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {

        Date fixingDate = c.fixingDate();

        // CPICashFlow must have a ZeroInflationIndex
        auto zeroInflationIndex = boost::dynamic_pointer_cast<ZeroInflationIndex>(c.cpiIndex());
        QL_REQUIRE(zeroInflationIndex, "Expected CPICashFlow to have an index of type ZeroInflationIndex");

        // Mimic the logic in QuantLib::CPICoupon::indexFixing() to arrive at the fixing date(s) needed
        addZeroInflationDates(fixingDates_, fixingDate, today_, zeroInflationIndex, c.observationInterpolation(), zeroInflationIndex->frequency());
    }
}

void FixingDateGetter::visit(YoYInflationCoupon& c) {
    if (!c.hasOccurred(today_, includeSettlementDateFlows_)) {

        Date fixingDate = c.fixingDate();

        auto yoyInflationIndex = boost::dynamic_pointer_cast<YoYInflationIndex>(c.yoyIndex());
        QL_REQUIRE(yoyInflationIndex, "Expected YoYInflationCoupon to have an index of type YoYInflationIndex");

        // Get necessary fixing date(s). May be empty
        auto fixingDates = needsForecast(fixingDate, today_, yoyInflationIndex->interpolated(), 
            yoyInflationIndex->frequency(), yoyInflationIndex->availabilityLag());
        fixingDates_.insert(fixingDates.begin(), fixingDates.end());

        // Add the previous year's date(s) also if any.
        for (const auto d : fixingDates) {
            fixingDates_.insert(d - 1 * Years);
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
