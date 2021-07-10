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
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/fallbackiborindex.hpp>

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
Date nextValidFixingDate(Date d, const boost::shared_ptr<Index>& index, Size gap = 7) {
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

void FixingManager::initialise(const boost::shared_ptr<Portfolio>& portfolio, const boost::shared_ptr<Market>& market,
                               const std::string& configuration) {

    // loop over all cashflows, populate index map

    auto standardCashflowHandler = boost::make_shared<StandardFixingManagerCashflowHandler>();

    for (auto trade : portfolio->trades()) {
        for (auto leg : trade->legs()) {
            for (auto cf : leg) {
                bool done = false;
                for (auto& h : FixingManagerCashflowHandlerFactory::instance().handlers()) {
                    if (done)
                        break;
                    done = done || h->processCashflow(cf, fixingMap_);
                }
                if (!done)
                    standardCashflowHandler->processCashflow(cf, fixingMap_);
            }
        }
        // some trades require fixings, but don't have legs - actually we might switch to the
        // logic here for all trade types eventually, see ore ticket 1157
        if (trade->tradeType() == "ForwardRateAgreement") {
            for (auto const& m : trade->fixings(QuantLib::Date::maxDate())) {
                auto index = market->iborIndex(m.first, configuration);
                for (auto const& d : m.second) {
                    fixingMap_[*index].insert(d);
                }
            }
        }
    }

    // Now cache the original fixings so we can re-write on reset()
    for (auto m : fixingMap_) {
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
            // This date is a business day but may not be a valid fixing date in case of BMA/SIFMA
            if (!m.first->isValidFixingDate(currentFixingDate))
                currentFixingDate = nextValidFixingDate(currentFixingDate, m.first);
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

bool StandardFixingManagerCashflowHandler::processCashflow(const boost::shared_ptr<QuantLib::CashFlow>& cf,
                                                           FixingManager::FixingMap& fixingMap) {

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
            fixingMap[cmssp->swapSpreadIndex()->swapIndex1()].insert(frc->fixingDate());
            fixingMap[cmssp->swapSpreadIndex()->swapIndex2()].insert(frc->fixingDate());
            return true;
        }
        auto dcmssp = boost::dynamic_pointer_cast<DigitalCmsSpreadCoupon>(frc);
        if (dcmssp) {
            fixingMap
                [boost::dynamic_pointer_cast<CmsSpreadCoupon>(dcmssp->underlying())->swapSpreadIndex()->swapIndex1()]
                    .insert(frc->fixingDate());
            fixingMap
                [boost::dynamic_pointer_cast<CmsSpreadCoupon>(dcmssp->underlying())->swapSpreadIndex()->swapIndex2()]
                    .insert(frc->fixingDate());
            return true;
        }

        // A2 indices with native fixings, but no only on the standard fixing date
        auto on = boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(frc);
        if (on) {
            for (auto const& d : on->fixingDates()) {
                fixingMap[on->index()].insert(d);
            }
            return true;
        }
        auto avon = boost::dynamic_pointer_cast<AverageONIndexedCoupon>(frc);
        if (avon) {
            for (auto const& d : avon->fixingDates())
                fixingMap[avon->index()].insert(d);
            return true;
        }
        auto bma = boost::dynamic_pointer_cast<AverageBMACoupon>(frc);
        if (bma) {
            for (auto const& d : bma->fixingDates())
                fixingMap[bma->index()].insert(d);
            return true;
        }

        // A3 standard case
        auto fb = boost::dynamic_pointer_cast<FallbackIborIndex>(frc->index());
        if (fb != nullptr && frc->fixingDate() >= fb->switchDate()) {
            // handle ibor fallback
            return processCashflow(fb->onCoupon(frc->fixingDate()), fixingMap);
        } else {
            // handle other cases
            fixingMap[frc->index()].insert(frc->fixingDate());
        }
    }

    // B other coupon types

    boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fc =
        boost::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(cf);
    if (fc) {
        fixingMap[fc->fxIndex()].insert(fc->fxFixingDate());
        return processCashflow(fc->underlying(), fixingMap);
    }

    boost::shared_ptr<FXLinkedCashFlow> flcf = boost::dynamic_pointer_cast<FXLinkedCashFlow>(cf);
    if (flcf) {
        fixingMap[flcf->fxIndex()].insert(flcf->fxFixingDate());
        return true;
    }

    boost::shared_ptr<CPICoupon> cpc = boost::dynamic_pointer_cast<CPICoupon>(cf);
    if (cpc) {
        fixingMap[cpc->index()].insert(cpc->fixingDate());
        return true;
    }

    boost::shared_ptr<InflationCoupon> ic = boost::dynamic_pointer_cast<InflationCoupon>(cf);
    if (ic) {
        fixingMap[ic->index()].insert(ic->fixingDate());
        return true;
    }

    boost::shared_ptr<CPICashFlow> cpcf = boost::dynamic_pointer_cast<CPICashFlow>(cf);
    if (cpcf) {
        fixingMap[cpcf->index()].insert(cpcf->fixingDate());
        return true;
    }

    boost::shared_ptr<EquityCoupon> ec = boost::dynamic_pointer_cast<EquityCoupon>(cf);
    if (ec) {
        for (auto const& f : ec->fixingDates())
            fixingMap[ec->equityCurve()].insert(f);
        if (ec->fxIndex() != nullptr) {
            fixingMap[ec->fxIndex()].insert(ec->fixingStartDate());
        }
    }

    return true;
}

const std::set<boost::shared_ptr<FixingManagerCashflowHandler>>& FixingManagerCashflowHandlerFactory::handlers() const {
    return handlers_;
}

void FixingManagerCashflowHandlerFactory::addHandler(const boost::shared_ptr<FixingManagerCashflowHandler>& handler) {
    handlers_.insert(handler);
}

} // namespace analytics
} // namespace ore
