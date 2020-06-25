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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

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

//! Initialise the manager-

void FixingManager::initialise(const boost::shared_ptr<Portfolio>& portfolio) {

    // loop over all cashflows, populate index map

    for (auto trade : portfolio->trades()) {
        for (auto leg : trade->legs()) {
            for (auto cf : leg) {
                processCashFlows(cf);
            }
            // processLegs(leg);
        }
    }

    // Now cache the original fixings so we can re-write on reset()
    for (auto m : fixingMap_) {
        fixingCache_[m.first] = IndexManager::instance().getHistory(m.first->name());
    }
}

void FixingManager::processCashFlows(const boost::shared_ptr<QuantLib::CashFlow> cf) {

    // For any coupon type that requires fixings, it must be handled here
    // Most coupons are based off a floating rate coupon and their single index
    // will be captured in section A.
    //
    // Other more exotic coupons (inflation, CMS spreads, etc) are captured on a
    // case by case basis in section B.
    //
    // In all cases we want to add dates to the fixingMap_ map.

    // A floating rate coupons

    // extract underlying from cap/floored coupons
    boost::shared_ptr<FloatingRateCoupon> frc;
    auto cfCpn = boost::dynamic_pointer_cast<CappedFlooredCoupon>(cf);
    if (cfCpn)
        frc = cfCpn->underlying();
    else
        frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(cf);

    if (frc) {
        // A1 indices with fixings derived from underlying indices
        auto cmssp = boost::dynamic_pointer_cast<CmsSpreadCoupon>(frc);
        if (cmssp) {
            fixingMap_[cmssp->swapSpreadIndex()->swapIndex1()].insert(frc->fixingDate());
            fixingMap_[cmssp->swapSpreadIndex()->swapIndex2()].insert(frc->fixingDate());
            return;
        }
        auto dcmssp = boost::dynamic_pointer_cast<DigitalCmsSpreadCoupon>(frc);
        if (dcmssp) {
            fixingMap_
                [boost::dynamic_pointer_cast<CmsSpreadCoupon>(dcmssp->underlying())->swapSpreadIndex()->swapIndex1()]
                    .insert(frc->fixingDate());
            fixingMap_
                [boost::dynamic_pointer_cast<CmsSpreadCoupon>(dcmssp->underlying())->swapSpreadIndex()->swapIndex2()]
                    .insert(frc->fixingDate());
            return;
        }

        // A2 indices with native fixings, but no only on the standard fixing date
        auto on = boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(frc);
        if (on) {
            for (auto const& d : on->fixingDates())
                fixingMap_[on->index()].insert(d);
            return;
        }
        auto avon = boost::dynamic_pointer_cast<AverageONIndexedCoupon>(frc);
        if (avon) {
            for (auto const& d : avon->fixingDates())
                fixingMap_[avon->index()].insert(d);
            return;
        }
        auto bma = boost::dynamic_pointer_cast<AverageBMACoupon>(frc);
        if (bma) {
            for (auto const& d : bma->fixingDates())
                fixingMap_[bma->index()].insert(d);
            return;
        }

        // A3 standard case
        fixingMap_[frc->index()].insert(frc->fixingDate());
    }

    // B other coupon types

    boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fc =
        boost::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(cf);
    if (fc) {
        fixingMap_[fc->index()].insert(fc->fixingDate());
        fixingMap_[fc->fxIndex()].insert(fc->fxFixingDate());
        return;
    }

    boost::shared_ptr<FXLinkedCashFlow> flcf = boost::dynamic_pointer_cast<FXLinkedCashFlow>(cf);
    if (flcf) {
        fixingMap_[flcf->fxIndex()].insert(flcf->fxFixingDate());
        return;
    }

    boost::shared_ptr<CPICoupon> cpc = boost::dynamic_pointer_cast<CPICoupon>(cf);
    if (cpc) {
        fixingMap_[cpc->index()].insert(cpc->fixingDate());
        return;
    }

    boost::shared_ptr<InflationCoupon> ic = boost::dynamic_pointer_cast<InflationCoupon>(cf);
    if (ic) {
        fixingMap_[ic->index()].insert(ic->fixingDate());
        return;
    }

    boost::shared_ptr<CPICashFlow> cpcf = boost::dynamic_pointer_cast<CPICashFlow>(cf);
    if (cpcf) {
        fixingMap_[cpcf->index()].insert(cpcf->fixingDate());
        return;
    }
}

//! Update fixings to date d
void FixingManager::update(Date d) {
    if (!fixingMap_.empty()) {
        QL_REQUIRE(d >= fixingsEnd_, "Can't go back in time, fixings must be reset."
                                     " Update date "
                                         << d << " but current fixings go to " << fixingsEnd_);
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
        boost::shared_ptr<ZeroInflationIndex> zii = boost::dynamic_pointer_cast<ZeroInflationIndex>(m.first);
        boost::shared_ptr<YoYInflationIndex> yii = boost::dynamic_pointer_cast<YoYInflationIndex>(m.first);
        set<Date> fixingDates;
        Date currentFixingDate;
        Date fixStart = start;
        Date fixEnd = end;
        if (zii) { // for inflation indices we just only add a fixing for the first date in the month
            fixStart =
                inflationPeriod(fixStart - zii->zeroInflationTermStructure()->observationLag(), zii->frequency()).first;
            fixEnd =
                inflationPeriod(fixEnd - zii->zeroInflationTermStructure()->observationLag(), zii->frequency()).first +
                1;
            currentFixingDate = fixEnd;
            for (auto const& d : m.second) {
                fixingDates.insert(inflationPeriod(d, zii->frequency()).first);
            }
        } else if (yii) {
            fixStart =
                inflationPeriod(fixStart - yii->yoyInflationTermStructure()->observationLag(), yii->frequency()).first;
            fixEnd =
                inflationPeriod(fixEnd - yii->yoyInflationTermStructure()->observationLag(), yii->frequency()).first +
                1;
            currentFixingDate = fixEnd;
            for (auto const& d : m.second) {
                fixingDates.insert(inflationPeriod(d, yii->frequency()).first);
            }
        } else {
            currentFixingDate = m.first->fixingCalendar().adjust(fixEnd, Following);
            fixingDates = m.second;
        }

        // Add we have a coupon between start and asof.
        bool needFixings = false;
        for (auto const& d : fixingDates) {
            if (d >= fixStart && d < fixEnd) {
                needFixings = true;
                break;
            }
        }

        if (needFixings) {
            Rate currentFixing = m.first->fixing(currentFixingDate);
            // if we read the fixing from an inverted FxIndex we have to undo the inversion
            if (auto f = boost::dynamic_pointer_cast<FxIndex>(m.first)) {
                if (f->inverseIndex())
                    currentFixing = 1.0 / currentFixing;
            }
            TimeSeries<Real> history;
            for (auto const& d : fixingDates) {
                if (d >= fixStart && d < fixEnd) {
                    history[d] = currentFixing;
                    modifiedFixingHistory_ = true;
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
