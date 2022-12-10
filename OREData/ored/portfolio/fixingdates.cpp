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

#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/indexparser.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cmbcoupon.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitymargincoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/nonstandardyoyinflationcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/offpeakpowerindex.hpp>

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

namespace {

// Generate lookback dates
set<Date> generateLookbackDates(const Period& lookbackPeriod, const Calendar& calendar) {

    set<Date> dates;

    Date today = Settings::instance().evaluationDate();
    Date lookback = calendar.advance(today, -lookbackPeriod);
    do {
        TLOG("Adding date " << io::iso_date(lookback) << " to fixings.");
        dates.insert(lookback);
        lookback = calendar.advance(lookback, 1 * Days);
    } while (lookback <= today);

    return dates;
}

} // namespace

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
        std::get<3>(f) = true;
        newFixingDates.insert(f);
    }
    for (auto f : zeroInflationFixingDates_) {
        std::get<2>(std::get<0>(std::get<0>(f))) = Date::maxDate();
        std::get<3>(std::get<0>(std::get<0>(f))) = true;
        newZeroInflationFixingDates.insert(f);
    }
    for (auto f : yoyInflationFixingDates_) {
        std::get<2>(std::get<0>(f)) = Date::maxDate();
        std::get<3>(std::get<0>(f)) = true;
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
            for (const auto& d : fixingDates) {
                result[indexName].insert(d - 1 * Years);
            }
        }
    }

    return result;
}

void RequiredFixings::addFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                    const QuantLib::Date& payDate, const bool alwaysAddIfPaysOnSettlement) {
    fixingDates_.insert(
        std::make_tuple(indexName, fixingDate, payDate, payDate == Date::maxDate() || alwaysAddIfPaysOnSettlement));
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

void FixingDateGetter::visit(CashFlow& c) {
    // Do nothing if we fall through to here
}

void FixingDateGetter::visit(FloatingRateCoupon& c) {
    // Enforce fixing to be added even if coupon pays on settlement.
    requiredFixings_.addFixingDate(c.fixingDate(), IndexNameTranslator::instance().oreName(c.index()->name()), c.date(),
                                   true);
}

void FixingDateGetter::visit(IborCoupon& c) {
    if (auto bma = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(c.index())) {
        // Handle bma indices which we allow in IborCoupon as an approximation to BMA
        // coupons. For these we allow fixing dates that are invalid as BMA fixing dates
        // and adjust these dates to the last valid BMA fixing date in the BMAIndexWrapper.
        // It is this adjusted date that we want to record here.
        // Enforce fixing to be added even if coupon pays on settlement.
        requiredFixings_.addFixingDate(bma->adjustedFixingDate(c.fixingDate()),
                                       IndexNameTranslator::instance().oreName(c.index()->name()), c.date(), true);
    } else {
        auto fallback = boost::dynamic_pointer_cast<FallbackIborIndex>(c.index());
        if (fallback != nullptr && c.fixingDate() >= fallback->switchDate()) {
            requiredFixings_.addFixingDates(fallback->onCoupon(c.fixingDate())->fixingDates(),
                                            IndexNameTranslator::instance().oreName(fallback->rfrIndex()->name()),
                                            c.date());
        } else {
            visit(static_cast<FloatingRateCoupon&>(c));
        }
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

    requiredFixings_.addZeroInflationFixingDate(
        c.baseDate(), IndexNameTranslator::instance().oreName(c.index()->name()), zeroInflationIndex->interpolated(),
        zeroInflationIndex->frequency(), zeroInflationIndex->availabilityLag(), c.interpolation(), c.frequency(),
        c.date());

    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDate(), IndexNameTranslator::instance().oreName(c.index()->name()), zeroInflationIndex->interpolated(),
        zeroInflationIndex->frequency(), zeroInflationIndex->availabilityLag(), c.interpolation(), c.frequency(),
        c.date());
}

void FixingDateGetter::visit(CPICoupon& c) {
    requiredFixings_.addZeroInflationFixingDate(
        c.baseDate(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()), c.cpiIndex()->interpolated(),
        c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), c.observationInterpolation(),
        c.cpiIndex()->frequency(), c.date());

    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDate(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()), c.cpiIndex()->interpolated(),
        c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), c.observationInterpolation(),
        c.cpiIndex()->frequency(), c.date());
}

void FixingDateGetter::visit(YoYInflationCoupon& c) {
    requiredFixings_.addYoYInflationFixingDate(
        c.fixingDate(), IndexNameTranslator::instance().oreName(c.yoyIndex()->name()), c.yoyIndex()->interpolated(),
        c.yoyIndex()->frequency(), c.yoyIndex()->availabilityLag(), c.date());
}

void FixingDateGetter::visit(QuantLib::OvernightIndexedCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                    c.date());
}

void FixingDateGetter::visit(QuantExt::OvernightIndexedCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                    c.date());
}

void FixingDateGetter::visit(QuantExt::CappedFlooredOvernightIndexedCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(AverageBMACoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                    c.date());
}

void FixingDateGetter::visit(CmsSpreadCoupon& c) {
    // Enforce fixing to be added even if coupon pays on settlement.
    requiredFixings_.addFixingDate(c.fixingDate(),
                                   IndexNameTranslator::instance().oreName(c.swapSpreadIndex()->swapIndex1()->name()),
                                   c.date(), true);
    requiredFixings_.addFixingDate(c.fixingDate(),
                                   IndexNameTranslator::instance().oreName(c.swapSpreadIndex()->swapIndex2()->name()),
                                   c.date(), true);
}

void FixingDateGetter::visit(DigitalCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(StrippedCappedFlooredCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(AverageONIndexedCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                    c.date());
}

void FixingDateGetter::visit(CappedFlooredAverageONIndexedCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(EquityCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.equityCurve()->name()),
                                    c.date());
    if (c.fxIndex() != nullptr) {
        requiredFixings_.addFixingDate(c.fixingStartDate(),
                                       IndexNameTranslator::instance().oreName(c.fxIndex()->name()), c.date());
        requiredFixings_.addFixingDate(c.fixingEndDate(), IndexNameTranslator::instance().oreName(c.fxIndex()->name()),
                                       c.date());
    }
}

void FixingDateGetter::visit(FloatingRateFXLinkedNotionalCoupon& c) {
    requiredFixings_.addFixingDate(c.fxFixingDate(), IndexNameTranslator::instance().oreName(c.fxIndex()->name()),
                                   c.date());
    c.underlying()->accept(*this);
}

void FixingDateGetter::visit(FXLinkedCashFlow& c) {
    requiredFixings_.addFixingDate(c.fxFixingDate(), IndexNameTranslator::instance().oreName(c.fxIndex()->name()),
                                   c.date());
}

void FixingDateGetter::visit(AverageFXLinkedCashFlow& c) {
    requiredFixings_.addFixingDates(c.fxFixingDates(), IndexNameTranslator::instance().oreName(c.fxIndex()->name()),
                                    c.date());
}

void FixingDateGetter::visit(QuantExt::SubPeriodsCoupon1& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                    c.date());
}

void FixingDateGetter::visit(IndexedCoupon& c) {
    // the coupon's index might be null if an initial fixing is provided
    if (c.index())
        requiredFixings_.addFixingDate(c.fixingDate(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                       c.date());
    QL_REQUIRE(c.underlying(), "FixingDateGetter::visit(IndexedCoupon): underlying() is null");
    c.underlying()->accept(*this);
}

void FixingDateGetter::visit(IndexWrappedCashFlow& c) {
    // the cf's index might be null if an initial fixing is provided
    if (c.index())
        requiredFixings_.addFixingDate(c.fixingDate(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                       c.date());
    QL_REQUIRE(c.underlying(), "FixingDateGetter::visit(IndexWrappedCashFlow): underlying() is null");
    c.underlying()->accept(*this);
}

void FixingDateGetter::visit(QuantExt::NonStandardYoYInflationCoupon& c) {

    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDateNumerator(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()),
        c.cpiIndex()->interpolated(), c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), CPI::AsIndex,
        c.cpiIndex()->frequency(), c.date());
    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDateDenumerator(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()),
        c.cpiIndex()->interpolated(), c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), CPI::AsIndex,
        c.cpiIndex()->frequency(), c.date());
}

void FixingDateGetter::visit(CmbCoupon& c) {
    requiredFixings_.addFixingDate(c.fixingDate(), IndexNameTranslator::instance().oreName(c.bondIndex()->name()),
                                   c.date());
}

void FixingDateGetter::visit(EquityMarginCoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.equityCurve()->name()),
                                    c.date());
    if (c.fxIndex() != nullptr)
        requiredFixings_.addFixingDate(c.fixingStartDate(),
                                       IndexNameTranslator::instance().oreName(c.fxIndex()->name()), c.date());
}

void FixingDateGetter::visit(CommodityIndexedCashFlow& c) {
    // the ql index name is identical to the ORE index name, i.e. we do not need to call the
    // mapping function IndexNameTranslator::instance().oreName() here
    requiredFixings_.addFixingDate(c.pricingDate(), c.index()->name(), c.date());
    // if the pricing date is > future expiry, add the future expiry itself as well
    if (auto d = c.index()->expiryDate(); d != Date() && d < c.pricingDate()) {
        requiredFixings_.addFixingDate(d, c.index()->name(), d);
    }
}

void FixingDateGetter::visit(CommodityIndexedAverageCashFlow& c) {
    map<Date, boost::shared_ptr<CommodityIndex>> indices = c.indices();
    for (const auto& kv : indices) {
        // see above, the ql and ORE index names are identical
        requiredFixings_.addFixingDate(kv.first, kv.second->name(), c.date());
        // if the pricing date is > future expiry, add the future expiry itself as well
        if (auto d = kv.second->expiryDate(); d != Date() && d < kv.first) {
            requiredFixings_.addFixingDate(d, kv.second->name(), d);
        }
    }
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
        auto p = isInflationIndex(kv.first);
        if (p.first) {
            // We have an inflation index
            set<Date> newDates;
            Frequency f = p.second->frequency();
            for (const Date& d : kv.second) {
                auto period = inflationPeriod(d, f);
                if (d == period.first) {
                    // If the fixing date is the start of the inflation period, move it to the end.
                    newDates.insert(period.second);
                } else {
                    // If the fixing date is not the start of the inflation period, leave it as it is.
                    newDates.insert(d);
                }
            }
            // Update the fixings map that was passed in with the new set of dates
            kv.second = newDates;
        }
    }
}

void addMarketFixingDates(map<string, set<Date>>& fixings, const TodaysMarketParameters& mktParams,
                          const Period& iborLookback, const Period& oisLookback, const Period& bmaLookback,
                          const Period& inflationLookback, const string& configuration) {

    if (mktParams.hasConfiguration(configuration)) {

        LOG("Start adding market fixing dates for configuration '" << configuration << "'");

        boost::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

        // If there are ibor indices in the market parameters, add the lookback fixings
        // IF there are SIFMA / BMA indices, add lookback fixings for the Libor basis index
        if (mktParams.hasMarketObject(MarketObject::IndexCurve)) {

            QL_REQUIRE(iborLookback >= 0 * Days, "Ibor lookback period must be non-negative");

            DLOG("Start adding market fixing dates for interest rate indices.");

            set<Date> iborDates;
            set<Date> oisDates;
            set<Date> bmaDates;
            WeekendsOnly calendar;

            // For each of the IR indices in market parameters, insert the dates
            for (const auto& kv : mktParams.mapping(MarketObject::IndexCurve, configuration)) {
                if (isOvernightIndex(kv.first)) {
                    if (oisDates.empty()) {
                        TLOG("Generating fixing dates for overnight indices.");
                        oisDates = generateLookbackDates(oisLookback, calendar);
                    }
                    TLOG("Adding extra fixing dates for overnight index " << kv.first);
                    fixings[kv.first].insert(oisDates.begin(), oisDates.end());
                } else if (isBmaIndex(kv.first)) {
                    if (bmaDates.empty()) {
                        TLOG("Generating fixing dates for bma/sifma indices.");
                        bmaDates = generateLookbackDates(bmaLookback, calendar);
                    }
                    fixings[kv.first].insert(bmaDates.begin(), bmaDates.end());
                    if (iborDates.empty()) {
                        TLOG("Generating fixing dates for ibor indices.");
                        iborDates = generateLookbackDates(iborLookback, calendar);
                    }
                    std::set<string> liborNames;
                    for (auto const& c : conventions->get(Convention::Type::BMABasisSwap)) {
                        auto bma = boost::dynamic_pointer_cast<BMABasisSwapConvention>(c);
                        QL_REQUIRE(
                            bma, "internal error, could not cast to BMABasisSwapConvention in addMarketFixingDates()");
                        if (bma->bmaIndexName() == kv.first) {
                            liborNames.insert(bma->liborIndexName());
                        }
                    }
                    for (auto const& l : liborNames) {
                        TLOG("Adding extra fixing dates for libor index " << l << " from bma/sifma index " << kv.first);
                        fixings[l].insert(iborDates.begin(), iborDates.end());
                    }
                } else {
                    if (iborDates.empty()) {
                        TLOG("Generating fixing dates for ibor indices.");
                        iborDates = generateLookbackDates(iborLookback, calendar);
                    }
                    TLOG("Adding extra fixing dates for ibor index " << kv.first);
                    fixings[kv.first].insert(iborDates.begin(), iborDates.end());
                }
            }

            DLOG("Finished adding market fixing dates for interest rate indices.");
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
                dates.insert(lookback++);
            } while (lookback <= today);

            // Expiry months and years for which we require future contract fixings. For our purposes here, using the
            // 1st of the month does not matter. We will just use the date to get the appropriate commodity future
            // index name below when adding the dates and the "-01" will be removed (for non-daily contracts).
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
            // the dates. Skip commodity names that do not have future conventions.
            for (const auto& kv : mktParams.mapping(MarketObject::CommodityCurve, configuration)) {

                boost::shared_ptr<CommodityFutureConvention> cfc;
                if (conventions->has(kv.first)) {
                    cfc = boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(kv.first));
                }

                auto commIdx = parseCommodityIndex(kv.first, false);
                if (cfc) {
                    if (auto oppIdx = boost::dynamic_pointer_cast<OffPeakPowerIndex>(commIdx)) {
                        DLOG("Commodity " << kv.first << " is off-peak power so adding underlying daily contracts.");
                        const auto& opIndex = oppIdx->offPeakIndex();
                        const auto& pIndex = oppIdx->peakIndex();
                        for (const Date& expiry : dates) {
                            auto tmpIdx = oppIdx->clone(expiry);
                            auto opName = opIndex->clone(expiry)->name();
                            TLOG("Adding (date, id) = (" << io::iso_date(expiry) << "," << opName << ")");
                            fixings[opName].insert(expiry);
                            auto pName = pIndex->clone(expiry)->name();
                            TLOG("Adding (date, id) = (" << io::iso_date(expiry) << "," << pName << ")");
                            fixings[pName].insert(expiry);
                        }
                    } else if (cfc->contractFrequency() == Daily) {
                        DLOG("Commodity " << kv.first << " has daily frequency so adding daily contracts.");
                        for (const Date& expiry : dates) {
                            auto indexName = commIdx->clone(expiry)->name();
                            TLOG("Adding (date, id) = (" << io::iso_date(expiry) << "," << indexName << ")");
                            fixings[indexName].insert(expiry);
                        }
                    } else {
                        DLOG("Commodity " << kv.first << " is not daily so adding the monthly contracts.");
                        for (const Date& expiry : contractExpiries) {
                            auto indexName = commIdx->clone(expiry)->name();
                            TLOG("Adding extra fixing dates for commodity future " << indexName);
                            fixings[indexName].insert(dates.begin(), dates.end());
                        }
                    }
                } else {
                    // Assumption here is that we have a spot index.
                    DLOG("Commodity " << kv.first << " does not have future conventions so adding daily fixings.");
                    auto indexName = commIdx->name();
                    TLOG("Adding extra fixing dates for commodity spot " << indexName);
                    fixings[indexName].insert(dates.begin(), dates.end());
                }
            }
        }

        LOG("Finished adding market fixing dates for configuration '" << configuration << "'");
    }
}

} // namespace data
} // namespace ore
