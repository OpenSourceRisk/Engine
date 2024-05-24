/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <boost/assign/list_of.hpp>
using boost::assign::list_of;

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/crossccybasismtmresetswap.hpp>

namespace QuantExt {

CrossCcyBasisMtMResetSwap::CrossCcyBasisMtMResetSwap(
    Real foreignNominal, const Currency& foreignCurrency, const Schedule& foreignSchedule,
    const QuantLib::ext::shared_ptr<IborIndex>& foreignIndex, Spread foreignSpread, const Currency& domesticCurrency,
    const Schedule& domesticSchedule, const QuantLib::ext::shared_ptr<IborIndex>& domesticIndex, Spread domesticSpread,
    const QuantLib::ext::shared_ptr<FxIndex>& fxIdx, bool receiveDomestic, Size foreignPaymentLag, Size domesticPaymentLag,
    boost::optional<bool> foreignIncludeSpread, boost::optional<Period> foreignLookback,
    boost::optional<Size> foreignFixingDays, boost::optional<Size> foreignRateCutoff,
    boost::optional<bool> foreignIsAveraged, boost::optional<bool> domesticIncludeSpread,
    boost::optional<Period> domesticLookback, boost::optional<Size> domesticFixingDays,
    boost::optional<Size> domesticRateCutoff, boost::optional<bool> domesticIsAveraged, const bool telescopicValueDates,
						     const bool fairSpreadLegIsForeign)
    : CrossCcySwap(3), foreignNominal_(foreignNominal), foreignCurrency_(foreignCurrency),
      foreignSchedule_(foreignSchedule), foreignIndex_(foreignIndex), foreignSpread_(foreignSpread),
      domesticCurrency_(domesticCurrency), domesticSchedule_(domesticSchedule), domesticIndex_(domesticIndex),
      domesticSpread_(domesticSpread), fxIndex_(fxIdx), receiveDomestic_(receiveDomestic),
      foreignPaymentLag_(foreignPaymentLag), domesticPaymentLag_(domesticPaymentLag),
      foreignIncludeSpread_(foreignIncludeSpread), foreignLookback_(foreignLookback),
      foreignFixingDays_(foreignFixingDays), foreignRateCutoff_(foreignRateCutoff),
      foreignIsAveraged_(foreignIsAveraged), domesticIncludeSpread_(domesticIncludeSpread),
      domesticLookback_(domesticLookback), domesticFixingDays_(domesticFixingDays),
      domesticRateCutoff_(domesticRateCutoff), domesticIsAveraged_(domesticIsAveraged),
      telescopicValueDates_(telescopicValueDates), fairSpreadLegIsForeign_(fairSpreadLegIsForeign) {
    registerWith(foreignIndex_);
    registerWith(domesticIndex_);
    registerWith(fxIndex_);
    initialize();
}

void CrossCcyBasisMtMResetSwap::initialize() {
    // Pay (foreign) leg
    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndex>(foreignIndex_)) {
        // ON leg
        if (foreignIsAveraged_ && *foreignIsAveraged_) {
            legs_[0] = QuantExt::AverageONLeg(foreignSchedule_, on)
                           .withNotional(foreignNominal_)
                           .withSpread(foreignSpread_)
                           .withPaymentLag(foreignPaymentLag_)
                           .withLookback(foreignLookback_ ? *foreignLookback_ : 0 * Days)
                           .withFixingDays(foreignFixingDays_ ? *foreignFixingDays_ : 0)
                           .withRateCutoff(foreignRateCutoff_ ? *foreignRateCutoff_ : 0)
                           .withTelescopicValueDates(telescopicValueDates_);
        } else {
            legs_[0] = QuantExt::OvernightLeg(foreignSchedule_, on)
                           .withNotionals(foreignNominal_)
                           .withSpreads(foreignSpread_)
                           .withPaymentLag(foreignPaymentLag_)
                           .includeSpread(foreignIncludeSpread_ ? *foreignIncludeSpread_ : false)
                           .withLookback(foreignLookback_ ? *foreignLookback_ : 0 * Days)
                           .withFixingDays(foreignFixingDays_ ? *foreignFixingDays_ : 0)
                           .withRateCutoff(foreignRateCutoff_ ? *foreignRateCutoff_ : 0)
                           .withTelescopicValueDates(telescopicValueDates_);
        }
    } else {
        // Ibor leg
        legs_[0] = IborLeg(foreignSchedule_, foreignIndex_)
                       .withNotionals(foreignNominal_)
                       .withSpreads(foreignSpread_)
                       .withPaymentLag(foreignPaymentLag_);
    }
    receiveDomestic_ ? payer_[0] = -1.0 : payer_[0] = +1.0;

    currencies_[0] = foreignCurrency_;
    // Pay leg notional exchange at start.
    Date initialPayDate = foreignSchedule_.dates().front();
    QuantLib::ext::shared_ptr<CashFlow> initialPayCF(new SimpleCashFlow(-foreignNominal_, initialPayDate));
    legs_[0].insert(legs_[0].begin(), initialPayCF);
    // Pay leg notional exchange at end.
    Date finalPayDate = foreignSchedule_.dates().back();
    QuantLib::ext::shared_ptr<CashFlow> finalPayCF(new SimpleCashFlow(foreignNominal_, finalPayDate));
    legs_[0].push_back(finalPayCF);

    // Receive (domestic/resettable) leg
    // start by creating a dummy vanilla floating leg
    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndex>(domesticIndex_)) {
        // ON leg
        if (domesticIsAveraged_ && *domesticIsAveraged_) {
            legs_[1] = QuantExt::AverageONLeg(domesticSchedule_, on)
                           .withNotional(0.0)
                           .withSpread(domesticSpread_)
                           .withPaymentLag(domesticPaymentLag_)
                           .withLookback(domesticLookback_ ? *domesticLookback_ : 0 * Days)
                           .withFixingDays(domesticFixingDays_ ? *domesticFixingDays_ : 0)
                           .withRateCutoff(domesticRateCutoff_ ? *domesticRateCutoff_ : 0)
                           .withTelescopicValueDates(telescopicValueDates_);
        } else {
            legs_[1] = QuantExt::OvernightLeg(domesticSchedule_, on)
                           .withNotionals(0.0)
                           .withSpreads(domesticSpread_)
                           .withPaymentLag(domesticPaymentLag_)
                           .includeSpread(domesticIncludeSpread_ ? *domesticIncludeSpread_ : false)
                           .withLookback(domesticLookback_ ? *domesticLookback_ : 0 * Days)
                           .withFixingDays(domesticFixingDays_ ? *domesticFixingDays_ : 0)
                           .withRateCutoff(domesticRateCutoff_ ? *domesticRateCutoff_ : 0)
                           .withTelescopicValueDates(telescopicValueDates_);
        }
    } else {
        // Ibor leg
        legs_[1] = IborLeg(domesticSchedule_, domesticIndex_)
                       .withNotionals(0.0)
                       .withSpreads(domesticSpread_)
                       .withPaymentLag(domesticPaymentLag_);
    }
    receiveDomestic_ ? payer_[1] = +1.0 : payer_[1] = -1.0;
    currencies_[1] = domesticCurrency_;
    // replace the coupons with a FloatingRateFXLinkedNotionalCoupon
    // (skip the first coupon as it has a fixed notional)
    for (Size j = 0; j < legs_[1].size(); ++j) {
        QuantLib::ext::shared_ptr<FloatingRateCoupon> coupon = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(legs_[1][j]);
        Date fixingDate = fxIndex_->fixingCalendar().advance(coupon->accrualStartDate(),
                                                             -static_cast<Integer>(fxIndex_->fixingDays()), Days);
        QuantLib::ext::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon(
            new FloatingRateFXLinkedNotionalCoupon(fixingDate, foreignNominal_, fxIndex_, coupon));
        legs_[1][j] = fxLinkedCoupon;
    }
    // now build a separate leg to store the domestic (resetting) notionals
    receiveDomestic_ ? payer_[2] = +1.0 : payer_[2] = -1.0;
    currencies_[2] = domesticCurrency_;
    for (Size j = 0; j < legs_[1].size(); j++) {
        QuantLib::ext::shared_ptr<Coupon> c = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs_[1][j]);
        QL_REQUIRE(c, "Resetting XCCY - expected Coupon"); // TODO: fixed fx resettable?
        // build a pair of notional flows, one at the start and one at the end of
        // the accrual period. Both with the same FX fixing date
        Date fixingDate = fxIndex_->fixingCalendar().advance(c->accrualStartDate(),
                                                             -static_cast<Integer>(fxIndex_->fixingDays()), Days);
        legs_[2].push_back(QuantLib::ext::shared_ptr<CashFlow>(
            new FXLinkedCashFlow(c->accrualStartDate(), fixingDate, -foreignNominal_, fxIndex_)));
        legs_[2].push_back(QuantLib::ext::shared_ptr<CashFlow>(
            new FXLinkedCashFlow(c->accrualEndDate(), fixingDate, foreignNominal_, fxIndex_)));
    }

    // Register the instrument with all cashflows on each leg.
    for (Size legNo = 0; legNo < legs_.size(); legNo++) {
        Leg::iterator it;
        for (it = legs_[legNo].begin(); it != legs_[legNo].end(); ++it) {
            registerWith(*it);
        }
    }
}

void CrossCcyBasisMtMResetSwap::setupArguments(PricingEngine::arguments* args) const {

    CrossCcySwap::setupArguments(args);

    CrossCcyBasisMtMResetSwap::arguments* arguments = dynamic_cast<CrossCcyBasisMtMResetSwap::arguments*>(args);

    /* Returns here if e.g. args is CrossCcySwap::arguments which
       is the case if PricingEngine is a CrossCcySwap::engine. */
    if (!arguments)
        return;

    arguments->domesticSpread = domesticSpread_;
    arguments->foreignSpread = foreignSpread_;
}

void CrossCcyBasisMtMResetSwap::fetchResults(const PricingEngine::results* r) const {

    CrossCcySwap::fetchResults(r);

    const CrossCcyBasisMtMResetSwap::results* results = dynamic_cast<const CrossCcyBasisMtMResetSwap::results*>(r);
    if (results) {
        /* If PricingEngine::results are of type
           CrossCcyBasisSwap::results */
        fairForeignSpread_ = results->fairForeignSpread;
        fairDomesticSpread_ = results->fairDomesticSpread;
    } else {
        /* If not, e.g. if the engine is a CrossCcySwap::engine */
        fairForeignSpread_ = Null<Spread>();
        fairDomesticSpread_ = Null<Spread>();
    }

    /* Calculate the fair pay and receive spreads if they are null */
    static Spread basisPoint = 1.0e-4;
    if (fairForeignSpread_ == Null<Spread>()) {
        if (legBPS_[0] != Null<Real>())
            fairForeignSpread_ = foreignSpread_ - NPV_ / (legBPS_[0] / basisPoint);
    }
    if (fairDomesticSpread_ == Null<Spread>()) {
        if (legBPS_[1] != Null<Real>())
            fairDomesticSpread_ = domesticSpread_ - NPV_ / (legBPS_[1] / basisPoint);
    }
}

void CrossCcyBasisMtMResetSwap::setupExpired() const {
    CrossCcySwap::setupExpired();
    fairForeignSpread_ = Null<Spread>();
    fairDomesticSpread_ = Null<Spread>();
}

void CrossCcyBasisMtMResetSwap::arguments::validate() const {
    CrossCcySwap::arguments::validate();
    QL_REQUIRE(foreignSpread != Null<Spread>(), "Pay spread cannot be null");
    QL_REQUIRE(domesticSpread != Null<Spread>(), "Rec spread cannot be null");
}

void CrossCcyBasisMtMResetSwap::results::reset() {
    CrossCcySwap::results::reset();
    fairForeignSpread = Null<Spread>();
    fairDomesticSpread = Null<Spread>();
}
} // namespace QuantExt
