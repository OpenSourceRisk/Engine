/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/simulation/fixingmanager.hpp>
#include <ored/utilities/flowanalysis.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/genericindex.hpp>
#include <qle/utilities/inflation.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/experimental/coupons/digitalcmsspreadcoupon.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace ore {
namespace analytics {

inline Date nextValidFixingDate(Date d, const QuantLib::ext::shared_ptr<Index>& index, Size gap = 7) {
    Date adjusted = d;
    for (Size i = 0; i <= gap; ++i, ++adjusted) {
        if (index->isValidFixingDate(adjusted))
            return adjusted;
    }
    QL_FAIL("FixingManager::nextValidFixingDate(): no valid fixing date found for index "
            << index->name() << " within gap " << gap << " from start date" << io::iso_date(d));
}

FixingManager::~FixingManager() { reset(); }

FixingManager::FixingManager(Date anchor, Mode mode)
    : anchor_(std::move(anchor)), mode_(mode), fixingsEnd_(anchor_), modifiedFixingHistory_(false) {}

void FixingManager::initialise(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                               const QuantLib::ext::shared_ptr<Market>& market, const std::string& configuration) {

    for (auto const& [tradeId, t] : portfolio->trades()) {

        auto r = t->requiredFixings();
        r.unsetPayDates();
        for (auto const& [name, fixingDates] : r.fixingDatesIndices(QuantLib::Date::maxDate())) {

            std::set<Date> dates;
            for (const auto& [d, _] : fixingDates) {
                dates.insert(d);
            }

            try {
                auto rawIndex = parseIndex(name);
                // dnamic pointer casts should be fine here, since initialise() is only called to init the manager
                if (auto index = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(rawIndex)) {
                    fixingMap_[*market->equityCurve(index->familyName(), configuration)].insert(dates.begin(),
                                                                                                dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<BondIndex>(rawIndex)) {
                    QL_FAIL("FixingManager: BondIndex not handled");
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<CommodityIndex>(rawIndex)) {
                    // for comm indices with non-daily expiries the expiry date's day of month is 1 always
                    Date safeExpiryDate = index->expiryDate();
                    if (safeExpiryDate != Date() && !index->keepDays()) {
                        safeExpiryDate = Date::endOfMonth(safeExpiryDate);
                    }
                    fixingMap_[index->clone(safeExpiryDate, safeExpiryDate,
                                            market->commodityPriceCurve(index->underlyingName(), configuration))]
                        .insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<FxIndex>(rawIndex)) {
                    fixingMap_[*market->fxIndex(name, configuration)].insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<GenericIndex>(rawIndex)) {
                    QL_FAIL("FixingManager: GenericIndex not handled");
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<ConstantMaturityBondIndex>(rawIndex)) {
                    QL_FAIL("FixingManager: ConstantMaturityBondIndex not handled");
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<IborIndex>(rawIndex)) {
                    fixingMap_[*market->iborIndex(name, configuration)].insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<SwapIndex>(rawIndex)) {
                    fixingMap_[*market->swapIndex(name, configuration)].insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(rawIndex)) {
                    fixingMap_[*market->zeroInflationIndex(name, configuration)].insert(dates.begin(), dates.end());
                }
            } catch (const std::exception& e) {
                StructuredTradeErrorMessage(t, "FixingManager: no fixings are added for index '" + name + "'", e.what())
                    .log();
            }
            TLOG("Added " << dates.size() << " fixing dates for '" << name << "'");
        }
    }

    for (auto const& m : fixingMap_) {
        QL_DEPRECATED_DISABLE_WARNING
        fixingCache_[m.first] = IndexManager::instance().getHistory(m.first->name());
        QL_DEPRECATED_ENABLE_WARNING
    }
}

void FixingManager::update(const Date& d) {
    if (d == Null<Date>())
        return;

    QL_REQUIRE(d >= anchor_, "FixingManager::update(): given date "
                                 << d << " must be later or equal than the manager's anchor date (" << anchor_ << ")");
    if (!fixingMap_.empty()) {
        if (d < fixingsEnd_) {
            reset();
        }
        if (d > fixingsEnd_) {
            applyFixings(fixingsEnd_, d);
        }
    }
    fixingsEnd_ = d;
}

void FixingManager::reset() {
    QL_DEPRECATED_DISABLE_WARNING
    if (modifiedFixingHistory_) {
        for (auto const& [index, ts] : fixingCache_)
            IndexManager::instance().setHistory(index->name(), ts);
        modifiedFixingHistory_ = false;
    }
    QL_DEPRECATED_ENABLE_WARNING
    fixingsEnd_ = anchor_;
}

void FixingManager::applyFixings(const Date& start, const Date& end) {
    if (mode_ == Mode::BackwardFlat)
        applyFixingsBackwardFlat(start, end);
    else if (mode_ == Mode::Projected)
        applyFixingsProjected(start, end);
    else {
        QL_FAIL("FixingManager::applyFixing(): mode (" << static_cast<int>(mode_) << ") not handled. Internal error.");
    }
}

namespace {

void getStartEndCurrentDate(const QuantLib::ext::shared_ptr<Index>& index, Date& fixStart, Date& fixEnd,
                            Date& currentFixingDate) {
    // TODO: avoid dnamic pointer casts here, since applyFixing() is called frequently during MC exposure simulation
    if (auto zii = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(index)) {
        fixStart = inflationPeriod(fixStart - simulationLag(zii->zeroInflationTermStructure()), zii->frequency()).first;
        fixEnd = inflationPeriod(fixEnd - simulationLag(zii->zeroInflationTermStructure()), zii->frequency()).first + 1;
        currentFixingDate = fixEnd;
    } else if (auto yii = QuantLib::ext::dynamic_pointer_cast<YoYInflationIndex>(index)) {
        fixStart = inflationPeriod(fixStart - simulationLag(yii->yoyInflationTermStructure()), yii->frequency()).first;
        fixEnd = inflationPeriod(fixEnd - simulationLag(yii->yoyInflationTermStructure()), yii->frequency()).first + 1;
        currentFixingDate = fixEnd;
    } else {
        currentFixingDate = index->fixingCalendar().adjust(fixEnd, Following);
        if (!index->isValidFixingDate(currentFixingDate))
            currentFixingDate = nextValidFixingDate(currentFixingDate, index);
    }
}

Rate getFixing(const QuantLib::ext::shared_ptr<Index>& index, const Date& currentFixingDate) {
    Rate currentFixing;
    if (auto comm = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(index);
        comm != nullptr && comm->expiryDate() < currentFixingDate) {
        currentFixing = comm->priceCurve()->price(currentFixingDate);
    } else {
        currentFixing = index->fixing(currentFixingDate);
    }
    return currentFixing;
}

} // namespace

void FixingManager::applyFixingsBackwardFlat(const Date& start, const Date& end) {

    Date today = Settings::instance().evaluationDate();

   QL_REQUIRE(end == today,
               "FixingManager::applyFixing(): mode backward flat requires end date ("
                   << end << ") = today (" << today << "). Internal error, check orchestration.");

    for (auto const& [index, dates] : fixingMap_) {

        Date fixStart = start;
        Date fixEnd = end;
        Date currentFixingDate;

        getStartEndCurrentDate(index, fixStart, fixEnd, currentFixingDate);

        auto l = dates.lower_bound(fixStart);
        auto h = dates.lower_bound(fixEnd);

        if (!dates.empty() && l != h) {

            Rate currentFixing = getFixing(index, currentFixingDate);

            TimeSeries<Real> history;
            for (auto const& d : dates) {
                if (d >= fixStart && d < fixEnd) {
                    if (index->isValidFixingDate(d)) {
                        history[d] = currentFixing;
                        modifiedFixingHistory_ = true;
                    }
                }
                if (d >= fixEnd)
                    break;
            }
            index->addFixings(history, true);
        }
    }
}

void FixingManager::applyFixingsProjected(const Date& start, const Date& end) {

    Date today = Settings::instance().evaluationDate();

    QL_REQUIRE(start == today, "FixingManager::applyFixing(): mode Projected requires start date ("
                                   << start << ") = today (" << today << "). Internal error, check orchestration.");

    std::map<Date, std::set<QuantLib::ext::shared_ptr<Index>, detail::IndexComparator>> dateMap;

    for(auto const& [index, dates] : fixingMap_) {

        Date fixStart = start;
        Date fixEnd = end;
        Date currentFixingDate;

        getStartEndCurrentDate(index, fixStart, fixEnd, currentFixingDate);

        auto l = dates.lower_bound(fixStart);
        auto h = dates.lower_bound(fixEnd);

        if (!dates.empty() && l != h) {
            for (auto const& d : dates) {
                if (d >= fixStart && d < fixEnd) {
                    if (index->isValidFixingDate(d)) {
                        dateMap[d].insert(index);
                    }
                }
                if (d >= fixEnd)
                    break;
            }
        }
    }

    for (auto const& [d, indices] : dateMap) {

        if (d < today)
            continue;

        Settings::instance().evaluationDate() = d;

        for (auto const& index : indices) {
            index->addFixing(d, getFixing(index, d), true);
            modifiedFixingHistory_ = true;
        }
    }

    Settings::instance().evaluationDate() = end;
}

} // namespace analytics
} // namespace ore
