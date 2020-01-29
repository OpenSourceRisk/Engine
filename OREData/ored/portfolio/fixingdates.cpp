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
#include <ored/utilities/indexparser.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <ql/settings.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

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
using QuantExt::CommodityFuturesIndex;
using QuantExt::EquityCoupon;
using QuantExt::FloatingRateFXLinkedNotionalCoupon;
using QuantExt::FXLinkedCashFlow;
using QuantExt::SubPeriodsCoupon;
using QuantLib::Leg;
using QuantLib::Settings;
using QuantLib::ZeroInflationIndex;
using QuantLib::YoYInflationIndex;
using QuantLib::Period;
using QuantLib::Frequency;
using QuantLib::Years;

using std::set;
using std::vector;
using std::pair;
using std::map;
using std::string;

namespace {

// Utility functions to avoid code duplication below

// Add fixing dates on coupons with fxFixingDate method i.e. FX fixings
template <class Coupon>
void addFxFixings(const Coupon& c, set<Date>& dates, const Date& today) {
    Date fxFixingDate = c.fxFixingDate();
    if (fxFixingDate <= today) {
        dates.insert(fxFixingDate);
    }
}

// Add fixing dates on coupons with fixingDates method i.e. multiple fixings
template <class Coupon>
void addMultipleFixings(const Coupon& c, set<Date>& dates, const Date& today) {
    // Assume that I get sorted fixing dates here
    vector<Date> fixingDates = c.fixingDates();
    if (fixingDates.front() <= today) {
        for (const auto& fixingDate : fixingDates) {
            if (fixingDate <= today) {
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

FixingDateGetter::FixingDateGetter(const Date& settlementDate) {

    today_ = settlementDate;
    if (today_ == Date())
        today_ = Settings::instance().evaluationDate();
}

void FixingDateGetter::visit(CashFlow& c) {
    // Do nothing if we fall through to here
}

void FixingDateGetter::visit(FloatingRateCoupon& c) {
    // Some pricing engines in QL (e.g CapFloor) don't respect
    // hasOccurred() and ask for the fixing regarless, so we
    // are conservative here and ask for a fixing if it is today.
    if (!c.hasOccurred(today_) || c.date() == today_) {
        Date fixingDate = c.fixingDate();
        if (fixingDate <= today_) {
            indicesDates_[c.index()->name()].insert(fixingDate);
        }
    }
}

void FixingDateGetter::visit(IndexedCashFlow& c) {
    if (CPICashFlow* cc = dynamic_cast<CPICashFlow*>(&c)) {
        visit(*cc);
    }
}

void FixingDateGetter::visit(CPICashFlow& c) {
    if (!c.hasOccurred(today_)) {
        
        Date fixingDate = c.fixingDate();
        
        // CPICashFlow must have a ZeroInflationIndex
        auto zeroInflationIndex = boost::dynamic_pointer_cast<ZeroInflationIndex>(c.index());
        QL_REQUIRE(zeroInflationIndex, "Expected CPICashFlow to have an index of type ZeroInflationIndex");
        
        // Mimic the logic in QuantLib::CPICashFlow::amount() to arrive at the fixing date(s) needed
        string name = zeroInflationIndex->name();
        addZeroInflationDates(indicesDates_[name], fixingDate, today_, zeroInflationIndex, c.interpolation(), c.frequency());
    }
}

void FixingDateGetter::visit(CPICoupon& c) {
    if (!c.hasOccurred(today_)) {

        Date fixingDate = c.fixingDate();

        // CPICashFlow must have a ZeroInflationIndex
        auto zeroInflationIndex = boost::dynamic_pointer_cast<ZeroInflationIndex>(c.cpiIndex());
        QL_REQUIRE(zeroInflationIndex, "Expected CPICashFlow to have an index of type ZeroInflationIndex");

        // Mimic the logic in QuantLib::CPICoupon::indexFixing() to arrive at the fixing date(s) needed
        string name = zeroInflationIndex->name();
        addZeroInflationDates(indicesDates_[name], fixingDate, today_, zeroInflationIndex, c.observationInterpolation(), zeroInflationIndex->frequency());
    }
}

void FixingDateGetter::visit(YoYInflationCoupon& c) {
    if (!c.hasOccurred(today_)) {

        Date fixingDate = c.fixingDate();

        auto yoyInflationIndex = boost::dynamic_pointer_cast<YoYInflationIndex>(c.yoyIndex());
        QL_REQUIRE(yoyInflationIndex, "Expected YoYInflationCoupon to have an index of type YoYInflationIndex");

        // Get necessary fixing date(s). May be empty
        string name = yoyInflationIndex->name();
        auto fixingDates = needsForecast(fixingDate, today_, yoyInflationIndex->interpolated(), 
            yoyInflationIndex->frequency(), yoyInflationIndex->availabilityLag());
        indicesDates_[name].insert(fixingDates.begin(), fixingDates.end());

        // Add the previous year's date(s) also if any.
        for (const auto d : fixingDates) {
            indicesDates_[name].insert(d - 1 * Years);
        }
    }
}

void FixingDateGetter::visit(OvernightIndexedCoupon& c) {
    if (!c.hasOccurred(today_)) {
        addMultipleFixings(c, indicesDates_[c.index()->name()], today_);
    }
}

void FixingDateGetter::visit(AverageBMACoupon& c) {
    if (!c.hasOccurred(today_)) {
        addMultipleFixings(c, indicesDates_[c.index()->name()], today_);
    }
}

void FixingDateGetter::visit(AverageONIndexedCoupon& c) {
    if (!c.hasOccurred(today_)) {
        addMultipleFixings(c, indicesDates_[c.index()->name()], today_);
    }
}

void FixingDateGetter::visit(EquityCoupon& c) {
    if (!c.hasOccurred(today_)) {
        addMultipleFixings(c, indicesDates_[c.equityCurve()->name()], today_);
    }
}

void FixingDateGetter::visit(FloatingRateFXLinkedNotionalCoupon& c) {
    if (!c.hasOccurred(today_)) {
        addFxFixings(c, indicesDates_[c.fxIndex()->name()], today_);
    }
}

void FixingDateGetter::visit(FXLinkedCashFlow& c) {
    if (!c.hasOccurred(today_)) {
        addFxFixings(c, indicesDates_[c.fxIndex()->name()], today_);
    }
}

void FixingDateGetter::visit(SubPeriodsCoupon& c) {
    if (!c.hasOccurred(today_)) {
        for (auto const& fixingDate : c.fixingDates()) {
            if (fixingDate <= today_) {
                indicesDates_[c.index()->name()].insert(fixingDate);
            }
        }
    }
}

set<Date> fixingDates(const Leg& leg, const Date& settlementDate) {
    
    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;
    
    FixingDateGetter fdg(d);
    return fixingDates(leg, d, fdg);
}

set<Date> fixingDates(const Leg& leg, const Date& settlementDate, FixingDateGetter& fdg) {

    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;

    map<string, set<Date>> m = fixingDatesIndices(leg, d, fdg);

    if (m.size() == 0) {
        return set<Date>();
    }

    if (m.size() > 1) {
        WLOG("The leg has fixing dates for multiple indices but we are just returning those for the first index");
    }

    return m.begin()->second;
}

map<string, set<Date>> fixingDatesIndices(const Leg& leg, const Date& settlementDate) {
    
    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;

    FixingDateGetter fdg(d);
    return fixingDatesIndices(leg, d, fdg);
}

map<string, set<Date>> fixingDatesIndices(const Leg& leg, const Date& settlementDate, FixingDateGetter& fdg) {

    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;

    // Set the FixingDateGetter's settlement date
    fdg.today() = d;

    for (const auto& cf : leg) {
        cf->accept(fdg);
    }

    return fdg.fixingDatesIndices();
}

void amendInflationFixingDates(map<string, set<Date>>& fixings) {
    // Loop over indices and amend any that are of type InflationIndex
    for (auto& kv : fixings) {
        if (isInflationIndex(kv.first)) {
            // We have an inflation index
            set<Date> newDates;
            for (const Date& d : kv.second) {
                if (d.dayOfMonth() == 1) {
                    // If the fixing date is 1st, push it to last day of month
                    Date newDate = Date::endOfMonth(d);
                    newDates.insert(newDate);
                } else {
                    // If the fixing date is not 1st, leave it as it is
                    newDates.insert(d);
                }
            }
            // Update the fixings map that was passed in with the new set of dates
            kv.second = newDates;
        }
    }
}

void addMarketFixingDates(map<std::string, set<Date>>& fixings, const TodaysMarketParameters& mktParams,
    const Period& iborLookback, const Period& inflationLookback, const string& configuration) {

    if (mktParams.hasConfiguration(configuration)) {
        
        // If there are ibor indices in the market parameters, add the lookback fixings
        if (mktParams.hasMarketObject(MarketObject::IndexCurve)) {
            
            QL_REQUIRE(iborLookback >= 0 * Days, "Ibor lookback period must be non-negative");

            // Dates that will be used for each of the ibor indices
            Date today = Settings::instance().evaluationDate();
            Date lookback = WeekendsOnly().advance(today, -iborLookback);
            set<Date> dates;
            do {
                TLOG("Adding date " << io::iso_date(lookback) << " to fixings for ibor indices");
                dates.insert(lookback);
                lookback = WeekendsOnly().advance(lookback, 1 * Days);
            } while (lookback <= today);
            
            // For each of the ibor indices in market parameters, insert the dates
            for (const auto& kv : mktParams.mapping(MarketObject::IndexCurve, configuration)) {
                TLOG("Adding extra fixing dates for ibor index " << kv.first);
                fixings[kv.first].insert(dates.begin(), dates.end());
            }
        }

        // If there are inflation indices in the market parameters, add the lookback fixings
        if (mktParams.hasMarketObject(MarketObject::ZeroInflationCurve) ||
            mktParams.hasMarketObject(MarketObject::YoYInflationCurve)) {
            
            QL_REQUIRE(inflationLookback >= 0 * Days, "Inflation lookback period must be non-negative");

            // Dates that will be used for each of the inflation indices
            Date today = Settings::instance().evaluationDate();
            Date lookback = NullCalendar().advance(today, -inflationLookback);
            lookback = Date(1, lookback.month(), lookback.year());
            set<Date> dates;
            do {
                TLOG("Adding date " << io::iso_date(lookback) << " to fixings for inflation indices");
                dates.insert(lookback);
                lookback = NullCalendar().advance(lookback, 1 * Months);
            } while (lookback <= today);

            // For each of the inflation indices in market parameters, insert the dates
            if (mktParams.hasMarketObject(MarketObject::ZeroInflationCurve)) {
                for (const auto& kv : mktParams.mapping(MarketObject::ZeroInflationCurve, configuration)) {
                    TLOG("Adding extra fixing dates for (zero) inflation index " << kv.first);
                    fixings[kv.first].insert(dates.begin(), dates.end());
                }
            }

            if (mktParams.hasMarketObject(MarketObject::YoYInflationCurve)) {
                for (const auto& kv : mktParams.mapping(MarketObject::YoYInflationCurve, configuration)) {
                    TLOG("Adding extra fixing dates for (yoy) inflation index " << kv.first);
                    fixings[kv.first].insert(dates.begin(), dates.end());
                }
            }
        }

        // If there are commodity curves, add "fixings" for this month and two previous months. We add "fixings" for 
        // future contracts with expiry from two months hence to two months prior.
        if (mktParams.hasMarketObject(MarketObject::CommodityCurve)) {

            // "Fixing" dates for commodities.
            Period commodityLookback = 2 * Months;
            Date today = Settings::instance().evaluationDate();
            Date lookback = today - commodityLookback;
            lookback = Date(1, lookback.month(), lookback.year());
            set<Date> dates;
            do {
                TLOG("Adding date " << io::iso_date(lookback) << " to fixings for commodities");
                dates.insert(lookback);
                lookback = WeekendsOnly().advance(lookback, 1 * Days);
            } while (lookback <= today);

            // Expiry months and years for which we require future contract fixings. For our purposes here, using the 
            // 1st of the month does not matter. We will just use the date to get the appropriate commodity future
            // index name below when adding the dates and the "-01" will be removed.
            Size numberMonths = 2;
            vector<Date> contractExpiries;
            Date startContract = today - numberMonths * Months;
            Date endContract = today + numberMonths * Months;
            do {
                Month m = startContract.month();
                Year y = startContract.year();
                TLOG("Adding contract month and year (" << m << "," << y << ")");
                contractExpiries.push_back(Date(1, m, y));
                startContract += 1 * Months;
            } while (startContract <= endContract);

            // For each of the commodity names, create the future contract name with the relevant expiry and insert 
            // the dates. There may be some spot commodities here treated like futures but this should not matter
            // i.e. we will just not get fixings for them.
            if (mktParams.hasMarketObject(MarketObject::CommodityCurve)) {
                for (const auto& kv : mktParams.mapping(MarketObject::CommodityCurve, configuration)) {
                    for (const Date& expiry : contractExpiries) {
                        auto indexName = CommodityFuturesIndex(kv.first, expiry, NullCalendar()).name();
                        TLOG("Adding extra fixing dates for commodity future " << indexName);
                        fixings[indexName].insert(dates.begin(), dates.end());
                    }
                }
            }
        }

    }
}

}
}
