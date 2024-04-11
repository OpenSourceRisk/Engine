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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/genericindex.hpp>

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

// Search for a valid fixing date maximum gap days larger than d, the only relevant case for this so far is BMA/SIFMA
Date nextValidFixingDate(Date d, const QuantLib::ext::shared_ptr<Index>& index, Size gap = 7) {
    Date adjusted = d;
    for (Size i = 0; i <= gap; ++i) {
        adjusted = d + i;
        if (index->isValidFixingDate(adjusted))
            return adjusted;
    }
    QL_FAIL("no valid fixing date found for index " << index->name() << " within gap from " << io::iso_date(d));
}

FixingManager::FixingManager(Date today) : today_(today), fixingsEnd_(today), modifiedFixingHistory_(false) {}

//! Initialise the manager-

void FixingManager::initialise(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<Market>& market,
                               const std::string& configuration) {

    // populate the map "Index -> set of required fixing dates", where the index on the LHS is linked to curves
    for (auto const& [tradeId,t] : portfolio->trades()) {
        auto r = t->requiredFixings();
        r.unsetPayDates();
        for (auto const& [name, fixingDates] : r.fixingDatesIndices(QuantLib::Date::maxDate())) {
            std::set<Date> dates;
            for (const auto& [d, _] : fixingDates) {
                dates.insert(d);
            }
            try {
                auto rawIndex = parseIndex(name);
                if (auto index = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(rawIndex)) {
                    
                    fixingMap_[*market->equityCurve(index->familyName(), configuration)].insert(dates.begin(),
                                                                                                dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<BondIndex>(rawIndex)) {
                    QL_FAIL("BondIndex not handled");
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<CommodityIndex>(rawIndex)) {
                    // for comm indices with non-daily expiries the expiry date's day of month is 1 always
                    Date safeExpiryDate = index->expiryDate();
                    if (safeExpiryDate != Date() && !index->keepDays()) {
                        safeExpiryDate = Date::endOfMonth(safeExpiryDate);
                    }
                    fixingMap_[index->clone(safeExpiryDate,
                                            market->commodityPriceCurve(index->underlyingName(), configuration))]
                        .insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<FxIndex>(rawIndex)) {
                    fixingMap_[*market->fxIndex(index->oreName(), configuration)].insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<GenericIndex>(rawIndex)) {
                    QL_FAIL("GenericIndex not handled");
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<ConstantMaturityBondIndex>(rawIndex)) {
                    QL_FAIL("ConstantMaturityBondIndex not handled");
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<IborIndex>(rawIndex)) {
                    fixingMap_[*market->iborIndex(name, configuration)].insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<SwapIndex>(rawIndex)) {
                    fixingMap_[*market->swapIndex(name, configuration)].insert(dates.begin(), dates.end());
                } else if (auto index = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(rawIndex)) {
                    fixingMap_[*market->zeroInflationIndex(name, configuration)].insert(dates.begin(), dates.end());
                }
            } catch (const std::exception& e) {
                ALOG("FixingManager: error " << e.what() << " - no fixings are added for '" << name << "'");
            }
            TLOG("Added " << dates.size() << " fixing dates for '" << name << "'");
        }
    }

    // Now cache the original fixings so we can re-write on reset()
    for (auto const& m : fixingMap_) {
        fixingCache_[m.first] = IndexManager::instance().getHistory(m.first->name());
    }
}

//! Update fixings to date d
void FixingManager::update(Date d) {
    if (!fixingMap_.empty()) {
        QL_REQUIRE(d >= fixingsEnd_, "Can't go back in time, fixings must be reset."
                                     " Update date "
                                         << d << " but current fixings go to " << fixingsEnd_);
        if (d > fixingsEnd_)
            applyFixings(fixingsEnd_, d);
    }
    fixingsEnd_ = d;
}

//! Reset fixings to t0 (today)
void FixingManager::reset() {
    if (modifiedFixingHistory_) {
        for (auto& kv : fixingCache_)
            IndexManager::instance().setHistory(kv.first->name(), kv.second);
        modifiedFixingHistory_ = false;
    }
    fixingsEnd_ = today_;
}

void FixingManager::applyFixings(Date start, Date end) {
    // Loop over all indices
    for (auto const& m : fixingMap_) {
        Date fixStart = start;
        Date fixEnd = end;
        Date currentFixingDate;
        if (auto zii = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(m.first)) {
            fixStart =
                inflationPeriod(fixStart - zii->zeroInflationTermStructure()->observationLag(), zii->frequency()).first;
            fixEnd =
                inflationPeriod(fixEnd - zii->zeroInflationTermStructure()->observationLag(), zii->frequency()).first +
                1;
            currentFixingDate = fixEnd;
        } else if (auto yii = QuantLib::ext::dynamic_pointer_cast<YoYInflationIndex>(m.first)) {
            fixStart =
                inflationPeriod(fixStart - yii->yoyInflationTermStructure()->observationLag(), yii->frequency()).first;
            fixEnd =
                inflationPeriod(fixEnd - yii->yoyInflationTermStructure()->observationLag(), yii->frequency()).first +
                1;
            currentFixingDate = fixEnd;
        } else {
            currentFixingDate = m.first->fixingCalendar().adjust(fixEnd, Following);
            // This date is a business day but may not be a valid fixing date in case of BMA/SIFMA
            if (!m.first->isValidFixingDate(currentFixingDate))
                currentFixingDate = nextValidFixingDate(currentFixingDate, m.first);
        }

        // Add we have a coupon between start and asof.
        bool needFixings = false;
        for (auto const& d : m.second) {
            if (d >= fixStart && d < fixEnd) {
                needFixings = true;
                break;
            }
        }

        if (needFixings) {
            Rate currentFixing;
            if (auto comm = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(m.first);
                comm != nullptr && comm->expiryDate() < currentFixingDate) {
                currentFixing = comm->priceCurve()->price(currentFixingDate);
            } else {
                currentFixing = m.first->fixing(currentFixingDate);
            }
            // if we read the fixing from an inverted FxIndex we have to undo the inversion
            TimeSeries<Real> history;
            for (auto const& d : m.second) {
                if (d >= fixStart && d < fixEnd) {
                    // Fixing dates include the valuation grid dates which might not be valid fixing dates (BMA/SIFMA)
                    bool valid = m.first->isValidFixingDate(d);
                    if (valid) {
                        history[d] = currentFixing;
                        modifiedFixingHistory_ = true;
                    }
                }
                if (d >= fixEnd)
                    break;
            }
            m.first->addFixings(history, true);
        }
    }
}

} // namespace analytics
} // namespace ore
