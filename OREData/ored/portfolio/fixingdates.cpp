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
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/indexparser.hpp>

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
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/bondtrscashflow.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
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
#include <qle/indexes/commoditybasisfutureindex.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/compositeindex.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>
#include <qle/indexes/genericindex.hpp>
#include <qle/indexes/offpeakpowerindex.hpp>

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
set<Date> generateLookbackDates(const Date& asof, const Period& lookbackPeriod, const Calendar& calendar) {

    set<Date> dates;
    Date lookback = calendar.advance(asof, -lookbackPeriod);
    do {
        TLOG("Adding date " << io::iso_date(lookback) << " to fixings.");
        dates.insert(lookback);
        lookback = calendar.advance(lookback, 1 * Days);
    } while (lookback <= asof);

    return dates;
}

} // namespace

namespace ore {
namespace data {

namespace {

// Return the set of dates on which a fixing will be required, if any.
RequiredFixings::FixingDates needsForecast(const Date& fixingDate, const Date& today, const bool interpolated,
                                           const Frequency frequency, const Period& availabilityLag,
                                           const bool mandatory) {

    RequiredFixings::FixingDates result;

    Date todayMinusLag = today - availabilityLag;
    Date historicalFixingKnown = inflationPeriod(todayMinusLag, frequency).first - 1;

    pair<Date, Date> lim = inflationPeriod(fixingDate, frequency);
    result.addDate(lim.first, mandatory);
    Date latestNeededDate = fixingDate;
    if (interpolated) {
        if (fixingDate > lim.first) {
            latestNeededDate += Period(frequency);
            result.addDate(lim.second + 1, mandatory);
        }
    }

    if (latestNeededDate <= historicalFixingKnown) {
        // Know that fixings are available
        return result;
    } else if (latestNeededDate > today) {
        // Know that fixings are not available
        return RequiredFixings::FixingDates();
    } else {
        // Grey area here but for now return nothing
        return RequiredFixings::FixingDates();
    }
}

// Common code for zero inflation index based coupons
void addZeroInflationDates(RequiredFixings::FixingDates& dates, const Date& fixingDate, const Date& today,
                           const bool indexInterpolated, const Frequency indexFrequency,
                           const Period& indexAvailabilityLag, const CPI::InterpolationType interpolation,
                           const Frequency f, const bool mandatory) {

    RequiredFixings::FixingDates fixingDates;

    if (interpolation == CPI::AsIndex) {
        fixingDates =
            needsForecast(fixingDate, today, indexInterpolated, indexFrequency, indexAvailabilityLag, mandatory);
    } else {
        pair<Date, Date> lim = inflationPeriod(fixingDate, f);
        fixingDates =
            needsForecast(lim.first, today, indexInterpolated, indexFrequency, indexAvailabilityLag, mandatory);
        if (interpolation == CPI::Linear) {
            auto moreDates = needsForecast(lim.second + 1, today, indexInterpolated, indexFrequency,
                                           indexAvailabilityLag, mandatory);
            fixingDates.addDates(moreDates);
        }
    }
    dates.addDates(fixingDates);
}
} // namespace

bool operator<(const RequiredFixings::FixingEntry& lhs, const RequiredFixings::FixingEntry& rhs) {
    return std::tie(lhs.indexName, lhs.fixingDate, lhs.payDate, lhs.alwaysAddIfPaysOnSettlement, lhs.mandatory) <
           std::tie(rhs.indexName, rhs.fixingDate, rhs.payDate, rhs.alwaysAddIfPaysOnSettlement, rhs.mandatory);
}

bool operator<(const RequiredFixings::InflationFixingEntry& lhs, const RequiredFixings::InflationFixingEntry& rhs) {
    return std::tie(lhs.indexName, lhs.fixingDate, lhs.payDate, lhs.alwaysAddIfPaysOnSettlement, lhs.mandatory,
                    lhs.indexInterpolated, lhs.availabilityLeg, lhs.indexFreq) <
           std::tie(rhs.indexName, rhs.fixingDate, rhs.payDate, rhs.alwaysAddIfPaysOnSettlement, rhs.mandatory,
                    rhs.indexInterpolated, rhs.availabilityLeg, rhs.indexFreq);
}

bool operator<(const RequiredFixings::ZeroInflationFixingEntry& lhs,
               const RequiredFixings::ZeroInflationFixingEntry& rhs) {
    return std::tie(lhs.indexName, lhs.fixingDate, lhs.payDate, lhs.alwaysAddIfPaysOnSettlement, lhs.mandatory,
                    lhs.indexInterpolated, lhs.availabilityLeg, lhs.indexFreq, lhs.couponFrequency,
                    lhs.couponInterpolation) < std::tie(rhs.indexName, rhs.fixingDate, rhs.payDate,
                                                        rhs.alwaysAddIfPaysOnSettlement, rhs.mandatory,
                                                        rhs.indexInterpolated, rhs.availabilityLeg, rhs.indexFreq,
                                                        rhs.couponFrequency, rhs.couponInterpolation);
}

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
        f.payDate = Date::maxDate();
        f.alwaysAddIfPaysOnSettlement = true;
        newFixingDates.insert(f);
    }
    for (auto f : zeroInflationFixingDates_) {
        f.payDate = Date::maxDate();
        f.alwaysAddIfPaysOnSettlement = true;
        newZeroInflationFixingDates.insert(f);
    }
    for (auto f : yoyInflationFixingDates_) {
        f.payDate = Date::maxDate();
        f.alwaysAddIfPaysOnSettlement = true;
        newYoYInflationFixingDates.insert(f);
    }
    fixingDates_ = newFixingDates;
    zeroInflationFixingDates_ = newZeroInflationFixingDates;
    yoyInflationFixingDates_ = newYoYInflationFixingDates;
}

RequiredFixings RequiredFixings::makeCopyWithMandatoryOverride(bool mandatory) {
    RequiredFixings result(*this);
    // we can't modify the elements of a set directly, need to make a copy and reassign
    std::set<FixingEntry> newFixingDates;
    std::set<ZeroInflationFixingEntry> newZeroInflationFixingDates;
    std::set<InflationFixingEntry> newYoYInflationFixingDates;
    for (auto f : result.fixingDates_) {
        f.mandatory = mandatory;
        newFixingDates.insert(f);
    }
    for (auto f : result.zeroInflationFixingDates_) {
        f.mandatory = mandatory;
        newZeroInflationFixingDates.insert(f);
    }
    for (auto f : yoyInflationFixingDates_) {
        f.mandatory = mandatory;
        newYoYInflationFixingDates.insert(f);
    }
    result.fixingDates_ = newFixingDates;
    result.zeroInflationFixingDates_ = newZeroInflationFixingDates;
    result.yoyInflationFixingDates_ = newYoYInflationFixingDates;
    return result;
}

RequiredFixings RequiredFixings::filteredFixingDates(const Date& settlementDate) {
    RequiredFixings rf;
    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;
    // handle the general case
    for (auto f : fixingDates_) {
        // get the data
        std::string indexName = f.indexName;
        Date fixingDate = f.fixingDate;
        Date payDate = f.payDate;
        bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
        // add to result
        if (fixingDate > d)
            continue;
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            f.payDate = Date::maxDate();
            f.alwaysAddIfPaysOnSettlement = true;
            rf.addFixingDate(f);
        }
    }

    // handle zero inflation index based coupons
    for (auto f : zeroInflationFixingDates_) {
        // get the data
        std::string indexName = f.indexName;
        Date payDate = f.payDate;
        bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
        // add to result
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            f.payDate = Date::maxDate();
            f.alwaysAddIfPaysOnSettlement = true;
            rf.addZeroInflationFixingDate(f);
        }
    }

    // handle yoy inflation index based coupons
    for (auto f : yoyInflationFixingDates_) {
        // get the data
        std::string indexName = f.indexName;
        Date payDate = f.payDate;
        bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
        // add to result
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            f.payDate = Date::maxDate();
            f.alwaysAddIfPaysOnSettlement = true;
            rf.addYoYInflationFixingDate(f);
        }
    }
    return rf;
}

std::map<std::string, RequiredFixings::FixingDates>
RequiredFixings::fixingDatesIndices(const Date& settlementDate) const {

    // If settlement date is an empty date, update to evaluation date.
    Date d = settlementDate == Date() ? Settings::instance().evaluationDate() : settlementDate;

    std::map<std::string, FixingDates> result;

    // handle the general case
    for (auto const& f : fixingDates_) {
        // get the data
        std::string indexName = f.indexName;
        Date fixingDate = f.fixingDate;
        Date payDate = f.payDate;
        bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
        // add to result
        if (fixingDate > d)
            continue;
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            result[indexName].addDate(fixingDate, f.mandatory);
        }
    }

    // handle zero inflation index based coupons
    for (auto const& f : zeroInflationFixingDates_) {
        // get the data
        std::string indexName = f.indexName;
        Date fixingDate = f.fixingDate;
        Date payDate = f.payDate;
        bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
        bool indexInterpolated = f.indexInterpolated;
        Frequency indexFrequency = f.indexFreq;
        Period indexAvailabilityLag = f.availabilityLeg;
        CPI::InterpolationType couponInterpolation = f.couponInterpolation;
        Frequency couponFrequency = f.couponFrequency;
        // add to result
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            RequiredFixings::FixingDates tmp;
            addZeroInflationDates(tmp, fixingDate, d, indexInterpolated, indexFrequency, indexAvailabilityLag,
                                  couponInterpolation, couponFrequency, f.mandatory);
            if (!tmp.empty())
                result[indexName].addDates(tmp);
        }
    }

    // handle yoy inflation index based coupons
    for (auto const& f : yoyInflationFixingDates_) {
        // get the data
        std::string indexName = f.indexName;
        Date fixingDate = f.fixingDate;
        Date payDate = f.payDate;
        bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
        bool indexInterpolated = f.indexInterpolated;
        Frequency indexFrequency = f.indexFreq;
        Period indexAvailabilityLag = f.availabilityLeg;
        // add to result
        SimpleCashFlow dummyCf(0.0, payDate);
        if (!dummyCf.hasOccurred(d) || (alwaysAddIfPaysOnSettlement && dummyCf.date() == d)) {
            auto fixingDates =
                needsForecast(fixingDate, d, indexInterpolated, indexFrequency, indexAvailabilityLag, f.mandatory);
            if (!fixingDates.empty())
                result[indexName].addDates(fixingDates);
            // Add the previous year's date(s) also if any.
            for (const auto& [d, mandatory] : fixingDates) {
                Date previousYear = d - 1 * Years;
                result[indexName].addDate(previousYear, mandatory);
            }
        }
    }

    return result;
}

void RequiredFixings::addFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                    const QuantLib::Date& payDate, const bool alwaysAddIfPaysOnSettlement,
                                    const bool mandatory) {
    fixingDates_.insert(
        {indexName, fixingDate, payDate, payDate == Date::maxDate() || alwaysAddIfPaysOnSettlement, mandatory});
}

void RequiredFixings::addFixingDate(const FixingEntry& fixingEntry) { fixingDates_.insert(fixingEntry); }

void RequiredFixings::addFixingDates(const std::vector<std::pair<QuantLib::Date, bool>>& fixingDates,
                                     const std::string& indexName, const QuantLib::Date& payDate,
                                     const bool alwaysAddIfPaysOnSettlement) {
    for (auto const& [date, mandatory] : fixingDates) {
        fixingDates_.insert({indexName, date, payDate, alwaysAddIfPaysOnSettlement, mandatory});
    }
}

void RequiredFixings::addFixingDates(const std::vector<QuantLib::Date>& fixingDates, const std::string& indexName,
                                     const QuantLib::Date& payDate, const bool alwaysAddIfPaysOnSettlement,
                                     const bool mandatory) {
    for (auto const& date : fixingDates) {
        fixingDates_.insert({indexName, date, payDate, alwaysAddIfPaysOnSettlement, mandatory});
    }
}

void RequiredFixings::addZeroInflationFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                                 const bool indexInterpolated, const Frequency indexFrequency,
                                                 const Period& indexAvailabilityLag,
                                                 const CPI::InterpolationType couponInterpolation,
                                                 const Frequency couponFrequency, const QuantLib::Date& payDate,
                                                 const bool alwaysAddIfPaysOnSettlement, const bool mandatory) {
    ZeroInflationFixingEntry entry;
    entry.indexName = indexName;
    entry.fixingDate = fixingDate;
    entry.payDate = payDate;
    entry.alwaysAddIfPaysOnSettlement = alwaysAddIfPaysOnSettlement;
    entry.mandatory = mandatory;
    entry.indexInterpolated = indexInterpolated;
    entry.indexFreq = indexFrequency;
    entry.availabilityLeg = indexAvailabilityLag;
    entry.couponFrequency = couponFrequency;
    entry.couponInterpolation = couponInterpolation;
    addZeroInflationFixingDate(entry);
}

void RequiredFixings::addZeroInflationFixingDate(const ZeroInflationFixingEntry& fixingEntry) {
    zeroInflationFixingDates_.insert(fixingEntry);
}

void RequiredFixings::addYoYInflationFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                                const bool indexInterpolated, const Frequency indexFrequency,
                                                const Period& indexAvailabilityLag, const QuantLib::Date& payDate,
                                                const bool alwaysAddIfPaysOnSettlement, const bool mandatory) {
    InflationFixingEntry entry;
    entry.indexName = indexName;
    entry.fixingDate = fixingDate;
    entry.payDate = payDate;
    entry.alwaysAddIfPaysOnSettlement = alwaysAddIfPaysOnSettlement;
    entry.mandatory = mandatory;
    entry.indexInterpolated = indexInterpolated;
    entry.indexFreq = indexFrequency;
    entry.availabilityLeg = indexAvailabilityLag;
    addYoYInflationFixingDate(entry);
}

void RequiredFixings::addYoYInflationFixingDate(const InflationFixingEntry& fixingEntry) {
    yoyInflationFixingDates_.insert(fixingEntry);
}

std::ostream& operator<<(std::ostream& out, const ore::data::RequiredFixings::FixingEntry& f) {

    std::string indexName = f.indexName;
    Date fixingDate = f.fixingDate;
    Date payDate = f.payDate;
    bool alwaysAddIfPaysOnSettlement = f.alwaysAddIfPaysOnSettlement;
    bool mandatory = f.mandatory;
    out << indexName << " " << QuantLib::io::iso_date(fixingDate) << " " << QuantLib::io::iso_date(payDate) << " "
        << std::boolalpha << alwaysAddIfPaysOnSettlement << " " << std::boolalpha << mandatory << "\n";
    return out;
}

std::ostream& operator<<(std::ostream& out, const std::set<ore::data::RequiredFixings::FixingEntry>& entries) {
    for (auto const& f : entries) {
        out << f;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const std::set<ore::data::RequiredFixings::InflationFixingEntry>& entries) {
    for (auto const& f : entries) {
        out << f;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out,
                         const std::set<ore::data::RequiredFixings::ZeroInflationFixingEntry>& entries) {
    for (auto const& f : entries) {
        out << f;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const RequiredFixings& requiredFixings) {
    out << "IndexName FixingDate PayDate AlwaysAddIfPaysOnSettlement\n";
    out << requiredFixings.fixingDates_;
    out << requiredFixings.zeroInflationFixingDates_;
    out << requiredFixings.yoyInflationFixingDates_;
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
    if (auto bma = QuantLib::ext::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(c.index())) {
        // Handle bma indices which we allow in IborCoupon as an approximation to BMA
        // coupons. For these we allow fixing dates that are invalid as BMA fixing dates
        // and adjust these dates to the last valid BMA fixing date in the BMAIndexWrapper.
        // It is this adjusted date that we want to record here.
        // Enforce fixing to be added even if coupon pays on settlement.
        requiredFixings_.addFixingDate(bma->adjustedFixingDate(c.fixingDate()),
                                       IndexNameTranslator::instance().oreName(c.index()->name()), c.date(), true);
    } else {
        auto fallback = QuantLib::ext::dynamic_pointer_cast<FallbackIborIndex>(c.index());
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
    auto zeroInflationIndex = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(c.index());
    QL_REQUIRE(zeroInflationIndex, "Expected CPICashFlow to have an index of type ZeroInflationIndex");

    QL_DEPRECATED_DISABLE_WARNING
    bool isInterpolated = c.interpolation() == QuantLib::CPI::Linear ||
                          (c.interpolation() == QuantLib::CPI::AsIndex && zeroInflationIndex->interpolated());
    QL_DEPRECATED_ENABLE_WARNING

    requiredFixings_.addZeroInflationFixingDate(
        c.baseDate(), IndexNameTranslator::instance().oreName(c.index()->name()), isInterpolated,
        zeroInflationIndex->frequency(), zeroInflationIndex->availabilityLag(), c.interpolation(), c.frequency(),
        c.date());

    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDate(), IndexNameTranslator::instance().oreName(c.index()->name()), isInterpolated,
        zeroInflationIndex->frequency(), zeroInflationIndex->availabilityLag(), c.interpolation(), c.frequency(),
        c.date());
}

void FixingDateGetter::visit(CPICoupon& c) {

    QL_DEPRECATED_DISABLE_WARNING
    bool isInterpolated = c.observationInterpolation() == QuantLib::CPI::Linear ||
                          (c.observationInterpolation() == QuantLib::CPI::AsIndex && c.cpiIndex()->interpolated());
    QL_DEPRECATED_ENABLE_WARNING

    requiredFixings_.addZeroInflationFixingDate(
        c.baseDate(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()), isInterpolated,
        c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), c.observationInterpolation(),
        c.cpiIndex()->frequency(), c.date());

    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDate(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()), isInterpolated,
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
    auto fallback = QuantLib::ext::dynamic_pointer_cast<FallbackOvernightIndex>(c.index());
    string indexName;
    if (fallback && c.fixingDate() >= fallback->switchDate())
        indexName = fallback->rfrIndex()->name();
    else
        indexName = c.index()->name();
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(indexName), c.date());
}

void FixingDateGetter::visit(QuantExt::CappedFlooredOvernightIndexedCoupon& c) { c.underlying()->accept(*this); }

void FixingDateGetter::visit(AverageBMACoupon& c) {
    requiredFixings_.addFixingDates(c.fixingDates(), IndexNameTranslator::instance().oreName(c.index()->name()),
                                    c.date());
}

void FixingDateGetter::visit(CappedFlooredAverageBMACoupon& c) { c.underlying()->accept(*this); }

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

    bool isInterpolated = c.isInterpolated();
    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDateNumerator(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()), isInterpolated,
        c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), CPI::Flat, c.cpiIndex()->frequency(), c.date());
    requiredFixings_.addZeroInflationFixingDate(
        c.fixingDateDenumerator(), IndexNameTranslator::instance().oreName(c.cpiIndex()->name()), isInterpolated,
        c.cpiIndex()->frequency(), c.cpiIndex()->availabilityLag(), CPI::Flat, c.cpiIndex()->frequency(), c.date());
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

void FixingDateGetter::visit(CommodityCashFlow& c) {
    auto indices = c.indices();
    for (const auto& [pricingDate, index] : indices) {
        // todays fixing is not mandatory, we will fallback to estimate it if its not there.
        bool isTodaysFixing = Settings::instance().evaluationDate() == pricingDate;
        if (auto powerIndex = QuantLib::ext::dynamic_pointer_cast<OffPeakPowerIndex>(index)) {
            // if powerindex, we need the offpeak index fixing and the peak index fixings
            requiredFixings_.addFixingDate(pricingDate, powerIndex->offPeakIndex()->name(), c.date(), false,
                                           !isTodaysFixing);
            bool isOffPeakDay = powerIndex->peakCalendar().isHoliday(pricingDate);
            requiredFixings_.addFixingDate(pricingDate, powerIndex->peakIndex()->name(), c.date(), false,
                                           isOffPeakDay && !isTodaysFixing);
            // if the pricing date is > future expiry, add the future expiry itself as well
            if (auto d = index->expiryDate(); d != Date() && d < pricingDate) {
                requiredFixings_.addFixingDate(d, powerIndex->offPeakIndex()->name(), c.date(), false, !isTodaysFixing);
                requiredFixings_.addFixingDate(d, powerIndex->peakIndex()->name(), c.date(), false,
                                               isOffPeakDay && !isTodaysFixing);
            }
        } else {
            requiredFixings_.addFixingDate(pricingDate, index->name(), c.date(), false, !isTodaysFixing);
            // if the pricing date is > future expiry, add the future expiry itself as well
            if (auto d = index->expiryDate(); d != Date() && d < pricingDate) {
                requiredFixings_.addFixingDate(d, index->name(), c.date(), false, !isTodaysFixing);
            }
        } 
        if (auto baseFutureIndex = QuantLib::ext::dynamic_pointer_cast<CommodityBasisFutureIndex>(index)) {
            RequiredFixings tmpFixings;
            FixingDateGetter baseCashflowGetter(tmpFixings);
            baseFutureIndex->baseCashflow(c.date())->accept(baseCashflowGetter);
            auto optionalFixings = tmpFixings.makeCopyWithMandatoryOverride(false);
            requiredFixings_.addData(optionalFixings);
        }
    }
}

void FixingDateGetter::visit(BondTRSCashFlow& bc) {
    if (bc.initialPrice() == Null<Real>() || requireFixingStartDates_) {
        requiredFixings_.addFixingDate(bc.fixingStartDate(), bc.index()->name(), bc.date());
    }
    requiredFixings_.addFixingDate(bc.fixingEndDate(), bc.index()->name(), bc.date());
    if (bc.fxIndex()) {
        requiredFixings_.addFixingDate(bc.fxIndex()->fixingCalendar().adjust(bc.fixingStartDate(), Preceding),
                                       IndexNameTranslator::instance().oreName(bc.fxIndex()->name()), bc.date());
        requiredFixings_.addFixingDate(bc.fxIndex()->fixingCalendar().adjust(bc.fixingEndDate(), Preceding),
                                       IndexNameTranslator::instance().oreName(bc.fxIndex()->name()), bc.date());
    }
}

void FixingDateGetter::visit(TRSCashFlow& bc) {
    vector<QuantLib::ext::shared_ptr<Index>> indexes;
    vector<QuantLib::ext::shared_ptr<FxIndex>> fxIndexes;

    if (auto e = QuantLib::ext::dynamic_pointer_cast<QuantExt::CompositeIndex>(bc.index())) {
        indexes = e->indices();
        fxIndexes = e->fxConversion();

        // Dividends date can require FX fixings for conversion, add any required fixing
        std::vector<std::pair<QuantLib::Date, std::string>> fixings =
            e->dividendFixingDates(bc.fixingStartDate(), bc.fixingEndDate());

        for (const auto& f : fixings)
            requiredFixings_.addFixingDate(f.first, ore::data::IndexNameTranslator::instance().oreName(f.second));
    } else {
        indexes.push_back(bc.index());
    }

    // always add the top level fxIndex, for a CompositeIndex we may need to convert underlyings to the CompositeIndex
    // ccy and then to the leg currency
    fxIndexes.push_back(bc.fxIndex());
    if (additionalFxIndex_)
        fxIndexes.push_back(additionalFxIndex_);

    for (const auto& ind : indexes) {
        if (ind) {
            auto startDate = ind->fixingCalendar().adjust(bc.fixingStartDate(), Preceding);
            auto endDate = ind->fixingCalendar().adjust(bc.fixingEndDate(), Preceding);

            auto gi = QuantLib::ext::dynamic_pointer_cast<QuantExt::GenericIndex>(ind);

            if (!gi || gi->expiry() == Date() || startDate < gi->expiry()) {
                if (bc.initialPrice() == Null<Real>() || requireFixingStartDates_)
                    requiredFixings_.addFixingDate(startDate, IndexNameTranslator::instance().oreName(ind->name()),
                                                   bc.date());
            }

            if (!gi || gi->expiry() == Date() || endDate < gi->expiry())
                requiredFixings_.addFixingDate(endDate, IndexNameTranslator::instance().oreName(ind->name()),
                                               bc.date());
        }
    }

    for (const auto& fx : fxIndexes) {
        if (fx) {
            requiredFixings_.addFixingDate(fx->fixingCalendar().adjust(bc.fixingStartDate(), Preceding),
                                           IndexNameTranslator::instance().oreName(fx->name()), bc.date());
            requiredFixings_.addFixingDate(fx->fixingCalendar().adjust(bc.fixingEndDate(), Preceding),
                                           IndexNameTranslator::instance().oreName(fx->name()), bc.date());

            // also add using the underlyingIndex calendar, as FX Conversion is done within a CompositeIndex
            // for a basket of underlyings
            requiredFixings_.addFixingDate(bc.index()->fixingCalendar().adjust(bc.fixingStartDate(), Preceding),
                                           IndexNameTranslator::instance().oreName(fx->name()), bc.date(), false);
            requiredFixings_.addFixingDate(bc.index()->fixingCalendar().adjust(bc.fixingEndDate(), Preceding),
                                           IndexNameTranslator::instance().oreName(fx->name()), bc.date(), false);
        }
    }
}

void addToRequiredFixings(const QuantLib::Leg& leg, const QuantLib::ext::shared_ptr<FixingDateGetter>& fixingDateGetter) {
    for (auto const& c : leg) {
        QL_REQUIRE(c, "addToRequiredFixings(), got null cashflow, this is unexpected");
        c->accept(*fixingDateGetter);
    }
}

void amendInflationFixingDates(std::map<std::string, RequiredFixings::FixingDates>& fixings) {
    // Loop over indices and amend any that are of type InflationIndex
    for (auto& [indexName, fixingDates] : fixings) {
        auto [isInfIndex, infIndex] = isInflationIndex(indexName);
        if (isInfIndex) {
            // We have an inflation index
            RequiredFixings::FixingDates amendedFixingDates;
            Frequency f = infIndex->frequency();
            for (const auto& [d, mandatory] : fixingDates) {
                auto period = inflationPeriod(d, f);
                if (d == period.first) {
                    // If the fixing date is the start of the inflation period, move it to the end.
                    amendedFixingDates.addDate(period.second, mandatory);
                } else {
                    // If the fixing date is not the start of the inflation period, leave it as it is.
                    amendedFixingDates.addDate(d, mandatory);
                }
            }
            // Update the fixings map that was passed in with the new set of dates
            fixings[indexName] = amendedFixingDates;
        }
    }
}

void addMarketFixingDates(const Date& asof, map<string, RequiredFixings::FixingDates>& fixings, const TodaysMarketParameters& mktParams,
                          const Period& iborLookback, const Period& oisLookback, const Period& bmaLookback,
                          const Period& inflationLookback) {

    for (auto const& [configuration, _] : mktParams.configurations()) {

        LOG("Start adding market fixing dates for configuration '" << configuration << "'");

        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

        // If there are ibor indices in the market parameters, add the lookback fixings
        // IF there are SIFMA / BMA indices, add lookback fixings for the Libor basis index
        if (mktParams.hasMarketObject(MarketObject::IndexCurve)) {

            QL_REQUIRE(iborLookback >= 0 * Days, "Ibor lookback period must be non-negative");

            DLOG("Start adding market fixing dates for interest rate indices.");

            set<Date> iborDates;
            set<Date> oisDates;
            set<Date> bmaDates;
            WeekendsOnly calendar;

            std::set<std::string> indices;
            for (auto const& [i, _] : mktParams.mapping(MarketObject::IndexCurve, configuration)) {
                indices.insert(i);
            }
            for (auto const& [i, _] : mktParams.mapping(MarketObject::YieldCurve, configuration)) {
                QuantLib::ext::shared_ptr<IborIndex> dummy;
                if (tryParseIborIndex(i, dummy))
                    indices.insert(i);
            }
            for (auto const& [_, s] : mktParams.mapping(MarketObject::DiscountCurve, configuration)) {
                auto spec = parseCurveSpec(s);
                QuantLib::ext::shared_ptr<IborIndex> dummy;
                if (tryParseIborIndex(spec->curveConfigID(), dummy))
                    indices.insert(spec->curveConfigID());
            }

            // For each of the IR indices in market parameters, insert the dates
            for (const auto& i : indices) {
                if (isOvernightIndex(i)) {
                    if (oisDates.empty()) {
                        TLOG("Generating fixing dates for overnight indices.");
                        oisDates = generateLookbackDates(asof, oisLookback, calendar);
                    }
                    TLOG("Adding extra fixing dates for overnight index " << i);
                    fixings[i].addDates(oisDates, false);
                    
                } else if (isBmaIndex(i)) {
                    if (bmaDates.empty()) {
                        TLOG("Generating fixing dates for bma/sifma indices.");
                        bmaDates = generateLookbackDates(asof, bmaLookback, calendar);
                    }
                    fixings[i].addDates(bmaDates, false);
                    if (iborDates.empty()) {
                        TLOG("Generating fixing dates for ibor indices.");
                        iborDates = generateLookbackDates(asof, iborLookback, calendar);
                    }
                    std::set<string> liborNames;
                    for (auto const& c : conventions->get(Convention::Type::BMABasisSwap)) {
                        auto bma = QuantLib::ext::dynamic_pointer_cast<BMABasisSwapConvention>(c);
                        QL_REQUIRE(
                            bma, "internal error, could not cast to BMABasisSwapConvention in addMarketFixingDates()");
                        if (bma->bmaIndexName() == i) {
                            liborNames.insert(bma->liborIndexName());
                        }
                    }
                    for (auto const& l : liborNames) {
                        TLOG("Adding extra fixing dates for libor index " << l << " from bma/sifma index " << i);
                        fixings[l].addDates(iborDates, false);
                    }
                } else {
                    if (iborDates.empty()) {
                        TLOG("Generating fixing dates for ibor indices.");
                        iborDates = generateLookbackDates(asof, iborLookback, calendar);
                    }
                    TLOG("Adding extra fixing dates for ibor index " << i);
                    fixings[i].addDates(iborDates, false);
                }
            }

            DLOG("Finished adding market fixing dates for interest rate indices.");
        }

        // If there are inflation indices in the market parameters, add the lookback fixings
        if (mktParams.hasMarketObject(MarketObject::ZeroInflationCurve) ||
            mktParams.hasMarketObject(MarketObject::YoYInflationCurve)) {

            QL_REQUIRE(inflationLookback >= 0 * Days, "Inflation lookback period must be non-negative");

            // Dates that will be used for each of the inflation indices
            Date lookback = NullCalendar().advance(asof, -inflationLookback);
            lookback = Date(1, lookback.month(), lookback.year());
            set<Date> dates;
            do {
                TLOG("Adding date " << io::iso_date(lookback) << " to fixings for inflation indices");
                dates.insert(lookback);
                lookback = NullCalendar().advance(lookback, 1 * Months);
            } while (lookback <= asof);

            // For each of the inflation indices in market parameters, insert the dates
            if (mktParams.hasMarketObject(MarketObject::ZeroInflationCurve)) {
                for (const auto& kv : mktParams.mapping(MarketObject::ZeroInflationCurve, configuration)) {
                    TLOG("Adding extra fixing dates for (zero) inflation index " << kv.first);
                    fixings[kv.first].addDates(dates, false);
                }
            }

            if (mktParams.hasMarketObject(MarketObject::YoYInflationCurve)) {
                for (const auto& kv : mktParams.mapping(MarketObject::YoYInflationCurve, configuration)) {
                    TLOG("Adding extra fixing dates for (yoy) inflation index " << kv.first);
                    fixings[kv.first].addDates(dates, false);
                }
            }
        }

        // If there are commodity curves, add "fixings" for this month and two previous months. We add "fixings" for
        // future contracts with expiry from two months hence to two months prior.
        if (mktParams.hasMarketObject(MarketObject::CommodityCurve)) {

            // "Fixing" dates for commodities.
            Period commodityLookback = 2 * Months;
            Date lookback = asof - commodityLookback;
            lookback = Date(1, lookback.month(), lookback.year());
            set<Date> dates;
            do {
                TLOG("Adding date " << io::iso_date(lookback) << " to fixings for commodities");
                dates.insert(lookback++);
            } while (lookback <= asof);

            // Expiry months and years for which we require future contract fixings. For our purposes here, using the
            // 1st of the month does not matter. We will just use the date to get the appropriate commodity future
            // index name below when adding the dates and the "-01" will be removed (for non-daily contracts).
            Size numberMonths = 2;
            vector<Date> contractExpiries;
            Date startContract = asof - numberMonths * Months;
            Date endContract = asof + numberMonths * Months;
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

                QuantLib::ext::shared_ptr<CommodityFutureConvention> cfc;
                if (conventions->has(kv.first)) {
                    cfc = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(kv.first));
                }

                auto commIdx = parseCommodityIndex(kv.first, false);
                if (cfc) {
                    if (auto oppIdx = QuantLib::ext::dynamic_pointer_cast<OffPeakPowerIndex>(commIdx)) {
                        DLOG("Commodity " << kv.first << " is off-peak power so adding underlying daily contracts.");
                        const auto& opIndex = oppIdx->offPeakIndex();
                        const auto& pIndex = oppIdx->peakIndex();
                        for (const Date& expiry : dates) {
                            auto tmpIdx = oppIdx->clone(expiry);
                            auto opName = opIndex->clone(expiry)->name();
                            TLOG("Adding (date, id) = (" << io::iso_date(expiry) << "," << opName << ")");
                            fixings[opName].addDate(expiry, false);
                            auto pName = pIndex->clone(expiry)->name();
                            TLOG("Adding (date, id) = (" << io::iso_date(expiry) << "," << pName << ")");
                            fixings[pName].addDate(expiry, false);
                        }
                    } else if (cfc->contractFrequency() == Daily) {
                        DLOG("Commodity " << kv.first << " has daily frequency so adding daily contracts.");
                        for (const Date& expiry : dates) {
                            auto indexName = commIdx->clone(expiry)->name();
                            TLOG("Adding (date, id) = (" << io::iso_date(expiry) << "," << indexName << ")");
                            fixings[indexName].addDate(expiry, false);
                        }
                    } else {
                        DLOG("Commodity " << kv.first << " is not daily so adding the monthly contracts.");
                        for (const Date& expiry : contractExpiries) {
                            auto indexName = commIdx->clone(expiry)->name();
                            TLOG("Adding extra fixing dates for commodity future " << indexName);
                            fixings[indexName].addDates(dates, false);
                        }
                    }
                } else {
                    // Assumption here is that we have a spot index.
                    DLOG("Commodity " << kv.first << " does not have future conventions so adding daily fixings.");
                    auto indexName = commIdx->name();
                    TLOG("Adding extra fixing dates for commodity spot " << indexName);
                    fixings[indexName].addDates(dates, false);
                }
            }
        }

        LOG("Finished adding market fixing dates for configuration '" << configuration << "'");
    }
}

} // namespace data
} // namespace ore
