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

#include <ored/scripting/engines/numericlgmriskparticipationagreementengine.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/rebatedexercise.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>

namespace ore {
namespace data {
using namespace QuantExt;
    
NumericLgmRiskParticipationAgreementEngine::NumericLgmRiskParticipationAgreementEngine(
    const std::string& baseCcy, const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
    const std::map<std::string, Handle<Quote>>& fxSpots, const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
    const Real sy, const Size ny, const Real sx, const Size nx,
    const Handle<DefaultProbabilityTermStructure>& defaultCurve, const Handle<Quote>& recoveryRate,
    const Size maxGapDays, const Size maxDiscretisationPoints)
    : RiskParticipationAgreementBaseEngine(baseCcy, discountCurves, fxSpots, defaultCurve, recoveryRate, maxGapDays,
                                           maxDiscretisationPoints),
      LgmConvolutionSolver2(model, sy, ny, sx, nx) {
    registerWith(LgmConvolutionSolver2::model());
}

namespace {
RandomVariable computeIborRate(const RandomVariable& fixing, const Real spread, const Real gearing, const Real floor,
                               const Real cap, const bool nakedOption) {
    Size n = fixing.size();
    RandomVariable rate;
    if (nakedOption) {
        // compute value of embedded cap / floor
        RandomVariable floorletRate(n, 0.0), capletRate(n, 0.0);
        if (floor != -QL_MAX_REAL) {
            Real effStrike = (floor - spread) / gearing;
            floorletRate =
                RandomVariable(n, gearing) * max(RandomVariable(n, effStrike) - fixing, RandomVariable(n, 0.0));
        }
        if (cap != QL_MAX_REAL) {
            Real effStrike = (cap - spread) / gearing;
            capletRate =
                RandomVariable(n, gearing) * max(fixing - RandomVariable(n, effStrike), RandomVariable(n, 0.0));
        }
        // same logic as in StrippedCapFlooredCoupon, i.e. embedded caps / floors are considered long
        // if the leg is receiving, otherwise short, and a long collar is a long floor + short cap
        if (floor != -QL_MAX_REAL && cap != QL_MAX_REAL)
            rate = floorletRate - capletRate;
        else
            rate = floorletRate + capletRate;
    } else {
        // straight capped / floored coupon
        rate = max(min(RandomVariable(n, gearing) * fixing + RandomVariable(n, spread), RandomVariable(n, cap)),
                   RandomVariable(n, floor));
    }
    return rate;
}

class IborCouponAnalyzer {
public:
    explicit IborCouponAnalyzer(const QuantLib::ext::shared_ptr<CashFlow>& c) {
        scf_ = QuantLib::ext::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(c);
        if (scf_)
            cf_ = scf_->underlying();
        else
            cf_ = QuantLib::ext::dynamic_pointer_cast<CappedFlooredCoupon>(c);
        QuantLib::ext::shared_ptr<CashFlow> cc = c;
        if (cf_)
            cc = cf_->underlying();
        ibor_ = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(cc);
    }
    // might be nulltr if input cf is not a (capped/floored) ibor coupon
    QuantLib::ext::shared_ptr<IborCoupon> underlying() const { return ibor_; }
    // QL_MAX_REAL if not a capped/flored coupon or if no cap present
    Real cap() const {
        if (cf_ && cf_->cap() != Null<Real>())
            return cf_->cap();
        else
            return QL_MAX_REAL;
    }
    // -QL_MAX_REAL if not a capped/flored coupon or if no floor present
    Real floor() const {
        if (cf_ && cf_->floor() != Null<Real>())
            return cf_->floor();
        else
            return -QL_MAX_REAL;
    }
    // is this a stripped cf coupon?
    bool nakedOption() const { return scf_ != nullptr; }

private:
    QuantLib::ext::shared_ptr<CappedFlooredCoupon> cf_;
    QuantLib::ext::shared_ptr<StrippedCappedFlooredCoupon> scf_;
    QuantLib::ext::shared_ptr<IborCoupon> ibor_;
};

class ONCouponAnalyzer {
public:
    explicit ONCouponAnalyzer(const QuantLib::ext::shared_ptr<CashFlow>& c) {
        cfcomp_ = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(c);
        cfavg_ = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(c);
        if (cfcomp_ != nullptr)
            comp_ = cfcomp_->underlying();
        else
            comp_ = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c);
        if (cfavg_ != nullptr)
            avg_ = cfavg_->underlying();
        else
            avg_ = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c);
    }

    bool isONCoupon() const { return comp_ != nullptr || avg_ != nullptr; }
    bool isAveraging() const { return avg_ != nullptr; }

    const std::vector<Date>& fixingDates() const {
        if (comp_ != nullptr)
            return comp_->fixingDates();
        else if (avg_ != nullptr)
            return avg_->fixingDates();
        else {
            QL_FAIL("internal error, requested fixingDates from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    const std::vector<Date>& valueDates() const {
        if (comp_ != nullptr)
            return comp_->valueDates();
        else if (avg_ != nullptr)
            return avg_->valueDates();
        else {
            QL_FAIL("internal error, requested valueDates from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    const std::vector<Real>& dt() const {
        if (comp_ != nullptr)
            return comp_->dt();
        else if (avg_ != nullptr)
            return avg_->dt();
        else {
            QL_FAIL("internal error, requested dt from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex() const {
        if (comp_ != nullptr)
            return comp_->overnightIndex();
        else if (avg_ != nullptr)
            return avg_->overnightIndex();
        else {
            QL_FAIL("internal error, requested overnightIndex from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    Real gearing() const {
        if (comp_ != nullptr)
            return comp_->gearing();
        else if (avg_ != nullptr)
            return avg_->gearing();
        else {
            QL_FAIL("internal error, requested gearing from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    Real spread() const {
        if (comp_ != nullptr)
            return comp_->spread();
        else if (avg_ != nullptr)
            return avg_->spread();
        else {
            QL_FAIL("internal error, requested spread from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    Real nominal() const {
        if (comp_ != nullptr)
            return comp_->nominal();
        else if (avg_ != nullptr)
            return avg_->nominal();
        else {
            QL_FAIL("internal error, requested nominal from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    Real cap() const {
        if (cfcomp_ != nullptr && cfcomp_->cap() != Null<Real>())
            return cfcomp_->cap();
        else if (cfavg_ != nullptr && cfavg_->cap() != Null<Real>())
            return cfavg_->cap();
        else
            return QL_MAX_REAL;
    }

    Real floor() const {
        if (cfcomp_ != nullptr && cfcomp_->floor() != Null<Real>())
            return cfcomp_->floor();
        else if (cfavg_ != nullptr && cfavg_->floor() != Null<Real>())
            return cfavg_->floor();
        else
            return -QL_MAX_REAL;
    }

    bool localCapFloor() const {
        if (cfcomp_ != nullptr)
            return cfcomp_->localCapFloor();
        else if (cfavg_ != nullptr)
            return cfavg_->localCapFloor();
        else
            return false;
    }

    bool nakedOption() const {
        if (cfcomp_ != nullptr)
            return cfcomp_->nakedOption();
        else if (cfavg_ != nullptr)
            return cfavg_->nakedOption();
        else
            return false;
    }

    Size rateCutoff() const {
        if (comp_ != nullptr)
            return comp_->rateCutoff();
        else if (avg_ != nullptr)
            return avg_->rateCutoff();
        else {
            QL_FAIL("internal error, requested rateCutoff from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    bool includeSpread() const {
        if (comp_ != nullptr)
            return comp_->includeSpread();
        else if (cfavg_ != nullptr)
            return cfavg_->includeSpread();
        return false;
    }

    const Period& lookback() const {
        if (comp_ != nullptr)
            return comp_->lookback();
        else if (avg_ != nullptr)
            return avg_->lookback();
        else {
            QL_FAIL("internal error, requested lookback from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    Real accrualPeriod() const {
        if (comp_ != nullptr)
            return comp_->accrualPeriod();
        else if (avg_ != nullptr)
            return avg_->accrualPeriod();
        else {
            QL_FAIL("internal error, requested accrualPeriod from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    DayCounter dayCounter() const {
        if (comp_ != nullptr)
            return comp_->dayCounter();
        else if (avg_ != nullptr)
            return avg_->dayCounter();
        else {
            QL_FAIL("internal error, requested dayCounter from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

    const Date& accrualStartDate() const {
        if (comp_ != nullptr)
            return comp_->accrualStartDate();
        else if (avg_ != nullptr)
            return avg_->accrualStartDate();
        else {
            QL_FAIL("internal error, requested accrualStartDate from ONCouponAnalyzer, but no on coupon is given.");
        }
    }

private:
    QuantLib::ext::shared_ptr<QuantExt::OvernightIndexedCoupon> comp_;
    QuantLib::ext::shared_ptr<QuantExt::AverageONIndexedCoupon> avg_;
    QuantLib::ext::shared_ptr<QuantExt::CappedFlooredOvernightIndexedCoupon> cfcomp_;
    QuantLib::ext::shared_ptr<QuantExt::CappedFlooredAverageONIndexedCoupon> cfavg_;
};

Size getEventIndex(const std::vector<Date>& eventDates, const Date& d) {
    auto ed = std::find(eventDates.begin(), eventDates.end(), d);
    QL_REQUIRE(ed != eventDates.end(), "internal error, can not find event date for " << d);
    return std::distance(eventDates.begin(), ed);
}

int getLatestRelevantCallIndex(const Date& accrualStart, const std::vector<Date>& callDates,
                               const std::vector<Date>& eventDates) {
    auto it = std::upper_bound(callDates.begin(), callDates.end(), accrualStart);
    if (it == callDates.end())
        return std::numeric_limits<int>::max();
    else if (it == callDates.begin())
        return -1;
    else
        return static_cast<int>(getEventIndex(eventDates, *std::next(it, -1)));
}

RandomVariable& getRv(std::map<int, RandomVariable>& map, const int index, const Size size) {
    return map.insert(std::make_pair(index, RandomVariable(size, 0.0))).first->second;
}

} // namespace

Real NumericLgmRiskParticipationAgreementEngine::protectionLegNpv() const {

    // check underlying legs are in the same ccy

    for (auto const& ccy : arguments_.underlyingCcys) {
        QL_REQUIRE(
            ccy == arguments_.underlyingCcys.front(),
            "NumericLgmRiskParticipationAgreementEngine::protectionLegNpv(): underlying ccys must all be the same, got "
                << ccy << ", " << arguments_.underlyingCcys.front());
    }

    // the option dates will be the mid points of the grid intervals

    std::vector<Date> optionDates;
    for (Size i = 0; i < gridDates_.size() - 1; ++i) {
        optionDates.push_back(gridDates_[i] + (gridDates_[i + 1] - gridDates_[i]) / 2);
    }

    // collect simulation dates for coupons:
    // - Ibor: these are future fixing dates resp. payment dates of an already fixed Ibor coupon
    // - OIS : max( today , first fixing date )
    // - Fixed: pay date

    std::vector<Date> couponDates;
    for (auto const& l : arguments_.underlying) {
        for (auto const& c : l) {
            // already paid => skip
            if (c->date() <= referenceDate_)
                continue;
            IborCouponAnalyzer iborCouponAnalyzer(c);
            ONCouponAnalyzer onCouponAnalyzer(c);
            if (iborCouponAnalyzer.underlying() != nullptr) {
                if (iborCouponAnalyzer.underlying()->fixingDate() >= referenceDate_) {
                    // Ibor Coupon with future fixing date
                    couponDates.push_back(iborCouponAnalyzer.underlying()->fixingDate());
                } else {
                    // Ibor Coupon with past fixing date
                    couponDates.push_back(c->date());
                }
            } else if (onCouponAnalyzer.isONCoupon()) {
                if (onCouponAnalyzer.fixingDates().empty())
                    continue;
                couponDates.push_back(std::max(onCouponAnalyzer.fixingDates().front(), referenceDate_));
            } else if (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c) != nullptr ||
                       QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(c) != nullptr) {
                couponDates.push_back(c->date());
            } else {
                QL_FAIL("NumericLgmRiskParticipationAgreementEngine: unsupported coupon type  when constructing event "
                        "dates, only (capped/floored) Ibor, OIS, Fixed, SimpleCashFlow supported");
            }
        }
    }

    // collect future call dates and associated rebates (if applicable)

    std::vector<Date> callDates;
    std::vector<Real> callRebates;
    std::vector<Date> callRebatePayDates;
    if (arguments_.exercise) {
        Size idx = 0;
        for (auto const& d : arguments_.exercise->dates()) {
            if (d > referenceDate_) {
                callDates.push_back(d);
                if (auto r = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(arguments_.exercise)) {
                    callRebates.push_back(r->rebate(idx));
                    callRebatePayDates.push_back(r->rebatePaymentDate(idx));
                } else {
                    callRebates.push_back(0);
                    callRebatePayDates.push_back(d);
                }
            }
            ++idx;
        }
    }

    // build event dates = optionDates + couponDates + exercise dates for callables / swaptions

    std::set<Date> uniqueDates;
    uniqueDates.insert(optionDates.begin(), optionDates.end());
    uniqueDates.insert(couponDates.begin(), couponDates.end());
    uniqueDates.insert(callDates.begin(), callDates.end());

    std::vector<Date> eventDates(uniqueDates.begin(), uniqueDates.end());

    // build event times

    std::vector<Real> eventTimes;
    for (auto const& d : eventDates) {
        eventTimes.push_back(discountCurves_[baseCcy_]->timeFromReference(d));
    }

    // build objects and collect information needed for rollback

    std::vector<std::vector<Real>> fixedCoupons(eventDates.size());
    std::vector<std::vector<int>> fixedCouponsLatestRelevantCallEventIndex(eventDates.size());

    std::vector<std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>> floatingIndices(eventDates.size());
    std::vector<std::vector<Real>> floatingGearings(eventDates.size());
    std::vector<std::vector<Real>> floatingSpreads(eventDates.size());
    std::vector<std::vector<Real>> floatingCaps(eventDates.size());
    std::vector<std::vector<Real>> floatingFloors(eventDates.size());
    std::vector<std::vector<Real>> floatingMultipliers(eventDates.size());
    std::vector<std::vector<Real>> payTimes(eventDates.size());
    std::vector<std::vector<bool>> nakedOption(eventDates.size());
    std::vector<std::vector<bool>> onIsAveraging(eventDates.size());
    std::vector<std::vector<std::vector<Date>>> onFixingDates(eventDates.size());
    std::vector<std::vector<std::vector<Date>>> onValueDates(eventDates.size());
    std::vector<std::vector<std::vector<Real>>> onDt(eventDates.size());
    std::vector<std::vector<Size>> onRateCutoff(eventDates.size());
    std::vector<std::vector<bool>> onIncludeSpread(eventDates.size());
    std::vector<std::vector<Period>> onLookback(eventDates.size());
    std::vector<std::vector<bool>> onLocalCapFloor(eventDates.size());
    std::vector<std::vector<int>> floatingCouponsLatestRelevantCallEventIndex(eventDates.size());

    std::vector<Size> optionDateIndex(eventDates.size());

    std::vector<Size> callDateIndex(eventDates.size());
    std::vector<Real> callRebateAmount(eventDates.size());
    std::vector<Real> callRebatePayTime(eventDates.size());

    // trapped coupons w.r.t. optionDates, these are coupons that are missing on the event
    // date although they are relevant, i.e.
    // - Ibor Coupons with fixingDate < eventDate < paymentDate
    // - ON Coupons with first fixingDate < eventDate < paymentDate
    // We memorise the original event index of these coupons, so that we can include them
    // in the npv as seen from the event date
    std::vector<std::set<Size>> trappedCouponIndex(eventDates.size());

    // trapped coupons w.r.t. call dates, these are coupons that are missing on the event
    // date although they are relevant, i.e.
    // - Ibor Coupons with fixingDate < eventDate <= accrualStart
    // - ON Coupons with first fixingDate < eventDate <= accrualStart
    // Again we memorise the original event index of these coupons, so that we can include them
    // in the exercise into npv as seen from the event date
    std::vector<std::set<Size>> trappedCouponIndexCall(eventDates.size());

    for (Size i = 0; i < eventDates.size(); ++i) {

        // set option date index (or null if it is not an option date)

        auto od = std::find(optionDates.begin(), optionDates.end(), eventDates[i]);
        if (od != optionDates.end()) {
            optionDateIndex[i] = std::distance(optionDates.begin(), od);
            // on option dates, search for trapped coupons (see above for definition)
            for (auto const& l : arguments_.underlying) {
                for (auto const& c : l) {
                    IborCouponAnalyzer iborCouponAnalyzer(c);
                    ONCouponAnalyzer onCouponAnalyzer(c);
                    if (iborCouponAnalyzer.underlying() != nullptr) {
                        if (iborCouponAnalyzer.underlying()->fixingDate() >= referenceDate_ &&
                            iborCouponAnalyzer.underlying()->fixingDate() < eventDates[i] &&
                            eventDates[i] < c->date()) {
                            trappedCouponIndex[i].insert(
                                getEventIndex(eventDates, iborCouponAnalyzer.underlying()->fixingDate()));
                        }
                    } else if (onCouponAnalyzer.isONCoupon()) {
                        if (onCouponAnalyzer.fixingDates().empty())
                            continue;
                        Date d = std::max(onCouponAnalyzer.fixingDates().front(), referenceDate_);
                        if (d < eventDates[i] && eventDates[i] < c->date()) {
                            trappedCouponIndex[i].insert(getEventIndex(eventDates, d));
                        }
                    }
                    // fixed coupons and simple cashflows are not relevant here
                }
            }
        } else {
            optionDateIndex[i] = Null<Size>();
        }

        // set exercise date index for callables / swaptions

        auto cd = std::find(callDates.begin(), callDates.end(), eventDates[i]);
        if (cd != callDates.end()) {
            callDateIndex[i] = std::distance(callDates.begin(), cd);
            // on call dates, search for trapped coupons, trapped coupons 2 (see above for definition)
            for (auto const& l : arguments_.underlying) {
                for (auto const& c : l) {
                    IborCouponAnalyzer iborCouponAnalyzer(c);
                    ONCouponAnalyzer onCouponAnalyzer(c);
                    if (iborCouponAnalyzer.underlying() != nullptr) {
                        if (iborCouponAnalyzer.underlying()->fixingDate() >= referenceDate_ &&
                            iborCouponAnalyzer.underlying()->fixingDate() < eventDates[i] &&
                            eventDates[i] <= iborCouponAnalyzer.underlying()->accrualStartDate()) {
                            trappedCouponIndexCall[i].insert(
                                getEventIndex(eventDates, iborCouponAnalyzer.underlying()->fixingDate()));
                        }
                    } else if (onCouponAnalyzer.isONCoupon()) {
                        if (onCouponAnalyzer.fixingDates().empty())
                            continue;
                        Date d = std::max(onCouponAnalyzer.fixingDates().front(), referenceDate_);
                        if (d < eventDates[i] && eventDates[i] <= onCouponAnalyzer.accrualStartDate()) {
                            trappedCouponIndexCall[i].insert(getEventIndex(eventDates, d));
                        }
                    }
                    // fixed coupons and simple cashflows are not relevant here
                }
            }
        } else {
            callDateIndex[i] = Null<Size>();
        }

        // loop over coupons and fill vectors used in rollback for the current event date

        auto underlyingPayer = arguments_.underlyingPayer.begin();
        for (auto const& l : arguments_.underlying) {
            bool isPayer = *(underlyingPayer++);
            for (auto const& c : l) {
                // already paid => skip
                if (c->date() <= referenceDate_)
                    continue;
                IborCouponAnalyzer iborCouponAnalyzer(c);
                ONCouponAnalyzer onCouponAnalyzer(c);
                if (iborCouponAnalyzer.underlying() != nullptr) {
                    if (iborCouponAnalyzer.underlying()->fixingDate() >= referenceDate_ &&
                        iborCouponAnalyzer.underlying()->fixingDate() == eventDates[i]) {
                        // Ibor coupon with future fixing
                        floatingIndices[i].push_back(iborCouponAnalyzer.underlying()->iborIndex());
                        floatingGearings[i].push_back(iborCouponAnalyzer.underlying()->gearing());
                        floatingSpreads[i].push_back(iborCouponAnalyzer.underlying()->spread());
                        floatingMultipliers[i].push_back(iborCouponAnalyzer.underlying()->nominal() *
                                                         iborCouponAnalyzer.underlying()->accrualPeriod() *
                                                         (isPayer ? -1.0 : 1.0));
                        floatingCaps[i].push_back(iborCouponAnalyzer.cap());
                        floatingFloors[i].push_back(iborCouponAnalyzer.floor());
                        nakedOption[i].push_back(iborCouponAnalyzer.nakedOption());
                        payTimes[i].push_back(discountCurves_[baseCcy_]->timeFromReference(c->date()));
                    } else if (iborCouponAnalyzer.underlying()->fixingDate() < referenceDate_ &&
                               c->date() == eventDates[i]) {
                        // already fixed Ibor coupon => treat as fixed coupon
                        fixedCoupons[i].push_back((isPayer ? -1.0 : 1.0) * c->amount());
                        fixedCouponsLatestRelevantCallEventIndex[i].push_back(getLatestRelevantCallIndex(
                            iborCouponAnalyzer.underlying()->accrualStartDate(), callDates, eventDates));
                    } else {
                        continue;
                    }
                    // on coupon specifics, not relevant for ibor, but need to populate them as well
                    onFixingDates[i].push_back(std::vector<Date>());
                    onValueDates[i].push_back(std::vector<Date>());
                    onDt[i].push_back(std::vector<Real>());
                    onRateCutoff[i].push_back(0);
                    onIncludeSpread[i].push_back(false);
                    onLookback[i].push_back(0 * Days);
                    onLocalCapFloor[i].push_back(false);
                    onIsAveraging[i].push_back(false);
                    floatingCouponsLatestRelevantCallEventIndex[i].push_back(getLatestRelevantCallIndex(
                        iborCouponAnalyzer.underlying()->accrualStartDate(), callDates, eventDates));
                } else if (onCouponAnalyzer.isONCoupon()) {
                    if (onCouponAnalyzer.fixingDates().empty())
                        continue;
                    // ON coupon (avg, comp, with or without cf)
                    Date d = std::max(onCouponAnalyzer.fixingDates().front(), referenceDate_);
                    if (d == eventDates[i]) {
                        floatingIndices[i].push_back(onCouponAnalyzer.overnightIndex());
                        floatingGearings[i].push_back(onCouponAnalyzer.gearing());
                        floatingSpreads[i].push_back(onCouponAnalyzer.spread());
                        floatingMultipliers[i].push_back(onCouponAnalyzer.nominal() * onCouponAnalyzer.accrualPeriod() *
                                                         (isPayer ? -1.0 : 1.0));
                        floatingCaps[i].push_back(onCouponAnalyzer.cap());
                        floatingFloors[i].push_back(onCouponAnalyzer.floor());
                        nakedOption[i].push_back(onCouponAnalyzer.nakedOption());
                        onLocalCapFloor[i].push_back(onCouponAnalyzer.localCapFloor());
                        onFixingDates[i].push_back(onCouponAnalyzer.fixingDates());
                        onValueDates[i].push_back(onCouponAnalyzer.valueDates());
                        onDt[i].push_back(onCouponAnalyzer.dt());
                        onRateCutoff[i].push_back(onCouponAnalyzer.rateCutoff());
                        onIncludeSpread[i].push_back(onCouponAnalyzer.includeSpread());
                        onLookback[i].push_back(onCouponAnalyzer.lookback());
                        onIsAveraging[i].push_back(onCouponAnalyzer.isAveraging());
                        payTimes[i].push_back(discountCurves_[baseCcy_]->timeFromReference(c->date()));
                        floatingCouponsLatestRelevantCallEventIndex[i].push_back(
                            getLatestRelevantCallIndex(onCouponAnalyzer.accrualStartDate(), callDates, eventDates));
                    }
                } else if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c)) {
                    if (c->date() == eventDates[i]) {
                        // genuine fixed rate coupon
                        fixedCoupons[i].push_back((isPayer ? -1.0 : 1.0) * cpn->amount());
                        fixedCouponsLatestRelevantCallEventIndex[i].push_back(
                            getLatestRelevantCallIndex(cpn->accrualStartDate(), callDates, eventDates));
                    }
                } else if (QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(c) != nullptr) {
                    if (c->date() == eventDates[i]) {
                        // simple cash flow
                        fixedCoupons[i].push_back((isPayer ? -1.0 : 1.0) * c->amount());
                        fixedCouponsLatestRelevantCallEventIndex[i].push_back(
                            getLatestRelevantCallIndex(c->date(), callDates, eventDates));
                    }
                } else {
                    QL_FAIL("NumericLgmRiskParticipationAgreementEngine: unsupported coupon type when collecting "
                            "coupon data, only (capped/floored) Ibor, OIS, Fixed, SimpleCashFlow supported");
                }
            }
        }

    } // loop over event dates

    // setup the vectorised LGM model which we use for the calculations below

    LgmVectorised lgm(model()->parametrization());

    // rollback

    std::map<int, RandomVariable> underlyingPv;
    underlyingPv[0] = RandomVariable(gridSize(), 0.0);

    RandomVariable swaptionPv(gridSize(), 0.0);

    std::vector<Real> optionPv(optionDates.size(), 0.0);

    for (int i = static_cast<int>(eventDates.size()) - 1; i >= 0; --i) {

        // get states for current event dates

        auto states = stateGrid(eventTimes[i]);

        // rollback underlying PV to current event date, if we are not on the latest event date

        if (i < static_cast<int>(eventDates.size()) - 1) {
            for (auto& u : underlyingPv) {
                u.second = rollback(u.second, eventTimes[i + 1], eventTimes[i]);
            }
        }

        // move relevant PV components to index 0

        for (auto& u : underlyingPv) {
            if (u.first != 0 && i <= u.first) {
                underlyingPv[0] += u.second;
                u.second = RandomVariable(gridSize(), 0.0);
            }
        }
        // rollback swaption PV

        if (i < static_cast<int>(eventDates.size()) - 1) {
            swaptionPv = rollback(swaptionPv, eventTimes[i + 1], eventTimes[i]);
        }

        // loop over floating coupons with fixingDate == eventDate and add them to the underlyingPv

        for (Size k = 0; k < floatingIndices[i].size(); ++k) {
            RandomVariable rate;
            if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(floatingIndices[i][k])) {
                if (onIsAveraging[i][k]) {
                    rate =
                        lgm.averagedOnRate(on, onFixingDates[i][k], onValueDates[i][k], onDt[i][k], onRateCutoff[i][k],
                                           onIncludeSpread[i][k], floatingSpreads[i][k], floatingGearings[i][k],
                                           onLookback[i][k], floatingCaps[i][k], floatingFloors[i][k],
                                           onLocalCapFloor[i][k], nakedOption[i][k], eventTimes[i], states);
                } else {
                    rate = lgm.compoundedOnRate(on, onFixingDates[i][k], onValueDates[i][k], onDt[i][k],
                                                onRateCutoff[i][k], onIncludeSpread[i][k], floatingSpreads[i][k],
                                                floatingGearings[i][k], onLookback[i][k], floatingCaps[i][k],
                                                floatingFloors[i][k], onLocalCapFloor[i][k], nakedOption[i][k],
                                                eventTimes[i], states);
                }
            } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(floatingIndices[i][k])) {
                rate = computeIborRate(lgm.fixing(ibor, eventDates[i], eventTimes[i], states), floatingSpreads[i][k],
                                       floatingGearings[i][k], floatingFloors[i][k], floatingCaps[i][k],
                                       nakedOption[i][k]);
            } else {
                QL_FAIL("NumericLgmRiskParticipationAgreementEngine: unexpected index, should be IborIndex or "
                        "OvernightIndex");
            }
            auto tmp = rate * RandomVariable(gridSize(), floatingMultipliers[i][k]) *
                       lgm.reducedDiscountBond(eventTimes[i], payTimes[i][k], states,
                                               discountCurves_[arguments_.underlyingCcys[0]]);
            getRv(underlyingPv, floatingCouponsLatestRelevantCallEventIndex[i][k], gridSize()) += tmp;
        }

        // compute trapped coupon pvs

        RandomVariable trappedCouponPv(gridSize(), 0.0), trappedCouponPvCall(gridSize(), 0.0);

        if (optionDateIndex[i] != Null<Size>()) {

            // loop over floating coupons with fixingDate < eventDate < paymentDate, add them to
            // a temporary pv component for this event date ("trapped coupons")
            // assume that the unkonwn past fixing is the same as on the event date (modeling assumption)

            for (auto const& t : trappedCouponIndex[i]) {
                for (Size k = 0; k < floatingIndices[t].size(); ++k) {
                    if (payTimes[t][k] > eventTimes[i] && !QuantLib::close_enough(eventTimes[i], payTimes[t][k])) {
                        RandomVariable rate;
                        if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(floatingIndices[t][k])) {
                            if (onIsAveraging[t][k]) {
                                rate = lgm.averagedOnRate(
                                    on, onFixingDates[t][k], onValueDates[t][k], onDt[t][k], onRateCutoff[t][k],
                                    onIncludeSpread[t][k], floatingSpreads[t][k], floatingGearings[t][k],
                                    onLookback[t][k], floatingCaps[t][k], floatingFloors[t][k], onLocalCapFloor[t][k],
                                    nakedOption[t][k], eventTimes[i], states);
                            } else {
                                rate = lgm.compoundedOnRate(
                                    on, onFixingDates[t][k], onValueDates[t][k], onDt[t][k], onRateCutoff[t][k],
                                    onIncludeSpread[t][k], floatingSpreads[t][k], floatingGearings[t][k],
                                    onLookback[t][k], floatingCaps[t][k], floatingFloors[t][k], onLocalCapFloor[t][k],
                                    nakedOption[t][k], eventTimes[i], states);
                            }
                        } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(floatingIndices[t][k])) {
                            rate = computeIborRate(
                                lgm.fixing(ibor, ibor->fixingCalendar().adjust(eventDates[i]), eventTimes[i], states),
                                floatingSpreads[t][k], floatingGearings[t][k], floatingFloors[t][k], floatingCaps[t][k],
                                nakedOption[t][k]);
                        }
                        trappedCouponPv += rate * RandomVariable(gridSize(), floatingMultipliers[t][k]) *
                                           lgm.reducedDiscountBond(eventTimes[i], payTimes[t][k], states,
                                                                   discountCurves_[arguments_.underlyingCcys[0]]);
                    }
                }
            }
        }

        if (callDateIndex[i] != Null<Size>()) {

            // same for call dates: compute temp npv for coupons with fixingDate < eventDate <= accrualStartDate

            for (auto const& t : trappedCouponIndexCall[i]) {
                for (Size k = 0; k < floatingIndices[t].size(); ++k) {
                    if (payTimes[t][k] > eventTimes[i] && !QuantLib::close_enough(eventTimes[i], payTimes[t][k])) {
                        RandomVariable rate;
                        if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(floatingIndices[t][k])) {
                            if (onIsAveraging[t][k]) {
                                rate = lgm.averagedOnRate(
                                    on, onFixingDates[t][k], onValueDates[t][k], onDt[t][k], onRateCutoff[t][k],
                                    onIncludeSpread[t][k], floatingSpreads[t][k], floatingGearings[t][k],
                                    onLookback[t][k], floatingCaps[t][k], floatingFloors[t][k], onLocalCapFloor[t][k],
                                    nakedOption[t][k], eventTimes[i], states);
                            } else {
                                rate = lgm.compoundedOnRate(
                                    on, onFixingDates[t][k], onValueDates[t][k], onDt[t][k], onRateCutoff[t][k],
                                    onIncludeSpread[t][k], floatingSpreads[t][k], floatingGearings[t][k],
                                    onLookback[t][k], floatingCaps[t][k], floatingFloors[t][k], onLocalCapFloor[t][k],
                                    nakedOption[t][k], eventTimes[i], states);
                            }
                        } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(floatingIndices[t][k])) {
                            rate = computeIborRate(
                                lgm.fixing(ibor, ibor->fixingCalendar().adjust(eventDates[i]), eventTimes[i], states),
                                floatingSpreads[t][k], floatingGearings[t][k], floatingFloors[t][k], floatingCaps[t][k],
                                nakedOption[t][k]);
                        }
                        trappedCouponPvCall += rate * RandomVariable(gridSize(), floatingMultipliers[t][k]) *
                                               lgm.reducedDiscountBond(eventTimes[i], payTimes[t][k], states,
                                                                       discountCurves_[arguments_.underlyingCcys[0]]);
                    }
                }
            }

            // on a call date, update swaption value

            Real rt = discountCurves_[baseCcy_]->timeFromReference(callRebatePayDates[callDateIndex[i]]);
            auto callRebateValue =
                RandomVariable(gridSize(), callRebates[callDateIndex[i]]) *
                lgm.reducedDiscountBond(eventTimes[i], rt, states, discountCurves_[arguments_.underlyingCcys[0]]);
            RandomVariable callMultiplier(gridSize(), arguments_.exerciseIsLong ? 1.0 : -1.0);
            RandomVariable nakedOptionMultiplier(gridSize(), arguments_.nakedOption ? 1.0 : -1.0);

            swaptionPv = callMultiplier *
                         max(callMultiplier * swaptionPv,
                             callMultiplier * (nakedOptionMultiplier * (underlyingPv[0] + trappedCouponPvCall)) +
                                 callRebateValue);
        }

        // Handle Premium
        // If PremiumDate > EventDate we  include them in swaptionPv

        for (int j = 0; j < arguments_.premium.size(); j++) {
            Real premiumAmount = 0;
            if (arguments_.exerciseIsLong) {
                premiumAmount = - arguments_.premium[j]->amount();
            } else {
                premiumAmount = arguments_.premium[j]->amount();
            }

            if (arguments_.premium[j]->date() > eventDates[i]) {
                swaptionPv += RandomVariable(gridSize(), premiumAmount) /
                             lgm.numeraire(eventTimes[i], states, discountCurves_[arguments_.underlyingCcys[0]]);
            }
        }
        
        // compute positive pv as of event date

        RandomVariable tmp = swaptionPv;
        if (!arguments_.nakedOption) {
            tmp += trappedCouponPv;
            for (auto const& u : underlyingPv)
                tmp += u.second;
        }

        RandomVariable currentPositivePv = max(tmp, RandomVariable(gridSize(), 0.0));

        // if on an option date, compute the option pv (as of t0)

        if (optionDateIndex[i] != Null<Size>()) {
            optionPv[optionDateIndex[i]] = rollback(currentPositivePv, eventTimes[i], 0.0).at(0);
        }

        // loop over fixed coupons with payment == eventDate and add them to the underlying pv

        for (Size k = 0; k < fixedCoupons[i].size(); ++k) {
            RandomVariable tmp = RandomVariable(gridSize(), fixedCoupons[i][k]) /
                                 lgm.numeraire(eventTimes[i], states, discountCurves_[arguments_.underlyingCcys[0]]);
            getRv(underlyingPv, fixedCouponsLatestRelevantCallEventIndex[i][k], gridSize()) += tmp;
        }

    } // loop over event dates

    // roll back to t = 0

    for (auto& u : underlyingPv) {
        if (u.first != 0)
            underlyingPv[0] += u.second;
    }

    underlyingPv[0] = rollback(underlyingPv[0], eventTimes[0], 0.0);

    swaptionPv = rollback(swaptionPv, eventTimes[0], 0.0);

    // compute a CVA using the option pvs

    QL_REQUIRE(!fxSpots_[arguments_.underlyingCcys[0]].empty(),
               "NumericLgmRiskParticipationAgreementEngine::protectionLegNpv(): empty fx spot for ccy pair "
                   << arguments_.underlyingCcys[0] + baseCcy_);

    Real cva = 0.0;
    for (Size i = 0; i < gridDates_.size() - 1; ++i) {
        Real pd = defaultCurve_->defaultProbability(gridDates_[i], gridDates_[i + 1]);
        cva += pd * (1.0 - effectiveRecoveryRate_) * optionPv[i] * fxSpots_[arguments_.underlyingCcys[0]]->value();
    }

    // set additional results (grid and option pvs)

    results_.additionalResults["OptionNpvs"] = optionPv;
    results_.additionalResults["OptionExerciseDates"] = optionDates;
    results_.additionalResults["UnderlyingNpv"] = underlyingPv.at(0).at(0);
    results_.additionalResults["SwaptionNpv"] = swaptionPv.at(0);
    results_.additionalResults["FXSpot"] = fxSpots_[arguments_.underlyingCcys[0]]->value();

    // return result

    return arguments_.participationRate * cva;
}

} // namespace data
} // namespace ore
