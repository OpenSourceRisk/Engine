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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/indexes/commodityindex.hpp>

#include <ql/cashflow.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/digitalcoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace QuantLib;
using namespace QuantExt;

using std::map;
using std::pair;
using std::set;
using std::string;
using std::tuple;
using std::vector;

namespace ore {
namespace data {

namespace {

// Return the set of dates on which a fixing will be required, if any.
set<Date> needsForecast(const Date& fixingDate, const Date& today, const bool interpolated, const Frequency frequency,
                        const Period& availabilityLag) {

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

// Common code for zero inflation index based coupons
void addZeroInflationDates(set<Date>& dates, const Date& fixingDate, const Date& today, const bool indexInterpolated,
                           const Frequency indexFrequency, const Period& indexAvailabilityLag,
                           const CPI::InterpolationType interpolation, const Frequency f) {

    set<Date> fixingDates;

    if (interpolation == CPI::AsIndex) {
        fixingDates = needsForecast(fixingDate, today, indexInterpolated, indexFrequency, indexAvailabilityLag);
    } else {
        pair<Date, Date> lim = inflationPeriod(fixingDate, f);
        fixingDates = needsForecast(lim.first, today, indexInterpolated, indexFrequency, indexAvailabilityLag);
        if (interpolation == CPI::Linear) {
            auto moreDates =
                needsForecast(lim.second + 1, today, indexInterpolated, indexFrequency, indexAvailabilityLag);
            fixingDates.insert(moreDates.begin(), moreDates.end());
        }
    }

    dates.insert(fixingDates.begin(), fixingDates.end());
}
} // namespace

void RequiredFixings::clear() {
    fixingDates_.clear();
    zeroInflationFixingDates_.clear();
    yoyInflationFixingDates_.clear();
}

void RequiredFixings::addData(const RequiredFixings& requiredFixings) {
    fixingDates_.insert(requiredFixings.fixingDates_.begin(), requiredFixings.fixingDates_.end());
    zeroInflationFixingDates_.insert(requiredFixings.zeroInflationFixingDates_.begin(),
                                     requiredFixings.zeroInflationFixingDates_.end());
    yoyInflationFixingDates_.insert(requiredFixings.yoyInflationFixingDates_.begin(),
                                    requiredFixings.yoyInflationFixingDates_.end());
}

void RequiredFixings::unsetPayDates() {
    // we can't modify the elements of a set directly, need to make a copy and reassign
    std::set<FixingEntry> newFixingDates;
    std::set<ZeroInflationFixingEntry> newZeroInflationFixingDates;
    std::set<InflationFixingEntry> newYoYInflationFixingDates;
    for (auto f : fixingDates_) {
        std::get<2>(f) = Date::maxDate();
        newFixingDates.insert(f);
    }
    for (auto f : zeroInflationFixingDates_) {
        std::get<2>(std::get<0>(std::get<0>(f))) = Date::maxDate();
        newZeroInflationFixingDates.insert(f);
    }
    for (auto f : yoyInflationFixingDates_) {
        std::get<2>(std::get<0>(f)) = Date::maxDate();
        newYoYInflationFixingDates.insert(f);
    }
    fixingDates_ = newFixingDates;
    zeroInflationFixingDates_ = newZeroInflationFixingDates;
    yoyInflationFixingDates_ = newYoYInflationFixingDates;
}

std::map<std::string, std::set<Date>> RequiredFixings::fixingDatesIndices(const Date& settlementDate) const {

    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;

    std::map<std::string, std::set<Date>> result;

    // handle the general case
    for (auto const& f : fixingDates_) {
        // get the data
        std::string indexName = std::get<0>(f);
        Date fixingDate = std::get<1>(f);
        Date payDate = std::get<2>(f);
        bool alwaysAddIfPaysOnSettlement = std::get<3>(f);
        // add to result
        if (fixingDate > d)
            continue;
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            result[indexName].insert(fixingDate);
        }
    }

    // handle zero inflation index based coupons
    for (auto const& f : zeroInflationFixingDates_) {
        // get the data
        InflationFixingEntry inflationFixingEntry = std::get<0>(f);
        FixingEntry fixingEntry = std::get<0>(inflationFixingEntry);
        std::string indexName = std::get<0>(fixingEntry);
        Date fixingDate = std::get<1>(fixingEntry);
        Date payDate = std::get<2>(fixingEntry);
        bool alwaysAddIfPaysOnSettlement = std::get<3>(fixingEntry);
        bool indexInterpolated = std::get<1>(inflationFixingEntry);
        Frequency indexFrequency = std::get<2>(inflationFixingEntry);
        Period indexAvailabilityLag = std::get<3>(inflationFixingEntry);
        CPI::InterpolationType couponInterpolation = std::get<1>(f);
        Frequency couponFrequency = std::get<2>(f);
        // add to result
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            std::set<Date> tmp;
            addZeroInflationDates(tmp, fixingDate, d, indexInterpolated, indexFrequency, indexAvailabilityLag,
                                  couponInterpolation, couponFrequency);
            if (!tmp.empty())
                result[indexName].insert(tmp.begin(), tmp.end());
        }
    }

    // handle yoy inflation index based coupons
    for (auto const& f : yoyInflationFixingDates_) {
        // get the data
        FixingEntry fixingEntry = std::get<0>(f);
        std::string indexName = std::get<0>(fixingEntry);
        Date fixingDate = std::get<1>(fixingEntry);
        Date payDate = std::get<2>(fixingEntry);
        bool alwaysAddIfPaysOnSettlement = std::get<3>(fixingEntry);
        bool indexInterpolated = std::get<1>(f);
        Frequency indexFrequency = std::get<2>(f);
        Period indexAvailabilityLag = std::get<3>(f);
        // add to result
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            auto fixingDates = needsForecast(fixingDate, d, indexInterpolated, indexFrequency, indexAvailabilityLag);
            if (!fixingDates.empty())
                result[indexName].insert(fixingDates.begin(), fixingDates.end());
            // Add the previous year's date(s) also if any.
            for (const auto d : fixingDates) {
                result[indexName].insert(d - 1 * Years);
            }
        }
    }

    return result;
}

void RequiredFixings::addFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                    const QuantLib::Date& payDate, const bool alwaysAddIfPaysOnSettlement) {
    fixingDates_.insert(std::make_tuple(indexName, fixingDate, payDate, alwaysAddIfPaysOnSettlement));
}

void RequiredFixings::addFixingDates(const std::vector<QuantLib::Date>& fixingDates, const std::string& indexName,
                                     const QuantLib::Date& payDate, const bool alwaysAddIfPaysOnSettlement) {
    for (auto const& d : fixingDates) {
        fixingDates_.insert(std::make_tuple(indexName, d, payDate, alwaysAddIfPaysOnSettlement));
    }
}

void RequiredFixings::addZeroInflationFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                                 const bool indexInterpolated, const Frequency indexFrequency,
                                                 const Period& indexAvailabilityLag,
                                                 const CPI::InterpolationType couponInterpolation,
                                                 const Frequency couponFrequency, const QuantLib::Date& payDate,
                                                 const bool alwaysAddIfPaysOnSettlement) {
    zeroInflationFixingDates_.insert(
        std::make_tuple(std::make_tuple(std::make_tuple(indexName, fixingDate, payDate, alwaysAddIfPaysOnSettlement),
                                        indexInterpolated, indexFrequency, indexAvailabilityLag),
                        couponInterpolation, couponFrequency));
}

void RequiredFixings::addYoYInflationFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                                const bool indexInterpolated, const Frequency indexFrequency,
                                                const Period& indexAvailabilityLag, const QuantLib::Date& payDate,
                                                const bool alwaysAddIfPaysOnSettlement) {
    yoyInflationFixingDates_.insert(
        std::make_tuple(std::make_tuple(indexName, fixingDate, payDate, alwaysAddIfPaysOnSettlement), indexInterpolated,
                        indexFrequency, indexAvailabilityLag));
}

std::ostream& operator<<(std::ostream& out, const RequiredFixings& requiredFixings) {
    out << "IndexName FixingDate PayDate AlwaysAddIfPaysOnSettlement\n";
    for (auto const& f : requiredFixings.fixingDates_) {
        std::string indexName = std::get<0>(f);
        Date fixingDate = std::get<1>(f);
        Date payDate = std::get<2>(f);
        bool alwaysAddIfPaysOnSettlement = std::get<3>(f);
        out << indexName << " " << QuantLib::io::iso_date(fixingDate) << " " << QuantLib::io::iso_date(payDate) << " "
            << std::boolalpha << alwaysAddIfPaysOnSettlement << "\n";
    }
    for (auto const& f : requiredFixings.zeroInflationFixingDates_) {
        RequiredFixings::InflationFixingEntry inflationFixingEntry = std::get<0>(f);
        RequiredFixings::FixingEntry fixingEntry = std::get<0>(inflationFixingEntry);
        std::string indexName = std::get<0>(fixingEntry);
        Date fixingDate = std::get<1>(fixingEntry);
        Date payDate = std::get<2>(fixingEntry);
        bool alwaysAddIfPaysOnSettlement = std::get<3>(fixingEntry);
        out << indexName << " " << QuantLib::io::iso_date(fixingDate) << " " << QuantLib::io::iso_date(payDate) << " "
            << std::boolalpha << alwaysAddIfPaysOnSettlement << "\n";
    }
    for (auto const& f : requiredFixings.yoyInflationFixingDates_) {
        RequiredFixings::FixingEntry fixingEntry = std::get<0>(f);
        std::string indexName = std::get<0>(fixingEntry);
        Date fixingDate = std::get<1>(fixingEntry);
        Date payDate = std::get<2>(fixingEntry);
        bool alwaysAddIfPaysOnSettlement = std::get<3>(fixingEntry);
        out << indexName << " " << QuantLib::io::iso_date(fixingDate) << " " << QuantLib::io::iso_date(payDate) << " "
            << std::boolalpha << alwaysAddIfPaysOnSettlement << "\n";
    }
    return out;
}

std::string FixingDateGetter::oreIndexName(const std::string& qlIndexName) const {
    auto n = qlToOREIndexNames_.find(qlIndexName);
    QL_REQUIRE(n != qlToOREIndexNames_.end(), "FixingDateGetter: no mapping for ql index '" << qlIndexName << "'");
    return n->second;
}

void FixingDateGetter::visit(CashFlow& c) {
    // Do nothing if we fall through to here
}

void FixingDateGetter::visit(FloatingRateCoupon& c) {
    // Enforce fixing to be added even if coupon pays on settlement.
    requiredFixings_.addFixingDate(c.fixingDate(), oreIndexName(c.index()->name()), c.date(), true);
}

void FixingDateGetter::visit(IborCoupon& c) {
    if (auto bma = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(c.index())) {
        // Handle bma indices which we allow in IborCoupon as an approximation to BMA
        // coupons. For these we allow fixing dates that are invalid as BMA fixing dates
        // and adjust these dates to the last valid BMA fixing date in the BMAIndexWrapper.
        // It is this adjusted date that we want to record here.
        // Enforce fixing to be added even if coupon pays on settlement.
        requiredFixings_.addFixingDate(c.fixingDate(), oreIndexName(c.index()->name()), c.date(), true);
    } else {
        // otherwise fall through to FloatingRateCoupon handling
        visit(static_cast<FloatingRateCoupon&>(c));
    }
}

void FixingDateGetter::visit(CappedFlooredCoupon& c) {
    // handle the underlying
    c.underlying()->accept(*this);
}

void FixingDateGetter::visit(IndexedCashFlow& c) {
    if (CPICashFlow* cc = dynamic_cast<CPICashFlow*>(&c)) {
        visit(*cc);
    }
}

void FixingDateGetter::visit(CPICashFlow& c) {
    // CPICashFlow must have a ZeroInflationIndex
    auto zeroInflationIndex = boost::dynamic_pointer_cast<ZeroInflationIndex>(c.index());
    QL_REQUIRE(zeroInflationIndex, "Expected CPICashFlow to have an index of type ZeroInflationIndex");

    requiredFixings_.addZeroInflationFixingDate(c.fixingDate(), oreIndexName(c.index()->name()),
                                                zeroInflationIndex->interpolated(), zeroInflationIndex->frequency(),
                                                zeroInflationIndex->availabilityLag(), c.interpolation(), c.frequency(),
                                                c.date());
}

void FixingDateGetter::visit(CPICoupon& c) {
    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDate(), oreIndexName(c.cpiIndex()->name()), c.cpiIndex()->interpolated(), c.cpiIndex()->frequency(),
        c.cpiIndex()->availabilityLag(), c.observationInterpolation(), c.cpiIndex()->frequency(), c.date());
}

void FixingDateGetter::visit(YoYInflationCoupon& c) {
    requiredFixings_.addYoYInflationFixingDate(c.fixingDate(), oreIndexName(c.yoyIndex()->name()),
                                               c.yoyIndex()->interpolated(), c.yoyIndex()->frequency(),
                                               c.yoyIndex()->availabilityLag(), c.date());
}

void FixingDateGetter::visit(QuantLib::OvernightIndexedCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), oreIndexName(c.index()->name()), c.date());
}

void FixingDateGetter::visit(QuantExt::OvernightIndexedCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), oreIndexName(c.index()->name()), c.date());
}

void FixingDateGetter::visit(QuantExt::CappedFlooredOvernightIndexedCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(AverageBMACoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), oreIndexName(c.index()->name()), c.date());
}

void FixingDateGetter::visit(CmsSpreadCoupon& c) {
    // Enforce fixing to be added even if coupon pays on settlement.
    requiredFixings_.addFixingDate(c.fixingDate(), oreIndexName(c.swapSpreadIndex()->swapIndex1()->name()), c.date(),
                                   true);
    requiredFixings_.addFixingDate(c.fixingDate(), oreIndexName(c.swapSpreadIndex()->swapIndex2()->name()), c.date(),
                                   true);
}

void FixingDateGetter::visit(DigitalCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(StrippedCappedFlooredCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(AverageONIndexedCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), oreIndexName(c.index()->name()), c.date());
}

void FixingDateGetter::visit(EquityCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), oreIndexName(c.equityCurve()->name()), c.date());
    if (c.fxIndex() != nullptr)
        requiredFixings_.addFixingDate(c.fixingStartDate(), oreIndexName(c.fxIndex()->name()), c.date());
}

void FixingDateGetter::visit(FloatingRateFXLinkedNotionalCoupon& c) {
    requiredFixings_.addFixingDate(c.fxFixingDate(), oreIndexName(c.fxIndex()->name()), c.date());
    c.underlying()->accept(*this);
}

void FixingDateGetter::visit(FXLinkedCashFlow& c) {
    requiredFixings_.addFixingDate(c.fxFixingDate(), oreIndexName(c.fxIndex()->name()), c.date());
}

void FixingDateGetter::visit(SubPeriodsCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), oreIndexName(c.index()->name()), c.date());
}

void FixingDateGetter::visit(IndexedCoupon& c) {
    // the coupon's index might be null if an initial fixing is provided
    if (c.index())
        requiredFixings_.addFixingDate(c.fixingDate(), oreIndexName(c.index()->name()), c.date());
    QL_REQUIRE(c.underlying(), "FixingDateGetter::visit(IndexedCoupon): underlying() is null");
    c.underlying()->accept(*this);
}

void addToRequiredFixings(const QuantLib::Leg& leg, const boost::shared_ptr<FixingDateGetter>& fixingDateGetter) {
    for (auto const& c : leg) {
        QL_REQUIRE(c, "addToRequiredFixings(), got null cashflow, this is unexpected");
        c->accept(*fixingDateGetter);
    }
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

} // namespace data
} // namespace ore
