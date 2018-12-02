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

#include <qle/instruments/crossccybasismtmresetswap.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>

namespace QuantExt {

CrossCcyBasisMtMResetSwap::CrossCcyBasisMtMResetSwap(
    Real fgnNominal, const Currency& fgnCurrency, const Schedule& fgnSchedule,
    const boost::shared_ptr<IborIndex>& fgnIndex, Spread fgnSpread,
    const Currency& domCurrency, const Schedule& domSchedule,
    const boost::shared_ptr<IborIndex>& domIndex, Spread domSpread,
    const boost::shared_ptr<FxIndex>& fxIdx, bool invertFxIdx) 
    : CrossCcySwap(3), fgnNominal_(fgnNominal), fgnCurrency_(fgnCurrency), fgnSchedule_(fgnSchedule),
      fgnIndex_(fgnIndex), fgnSpread_(fgnSpread), domCurrency_(domCurrency), domSchedule_(domSchedule), 
      domIndex_(domIndex), domSpread_(domSpread), fxIndex_(fxIdx), invertFxIndex_(invertFxIdx) {

    registerWith(fgnIndex_);
    registerWith(domIndex_);
    registerWith(fxIndex_);
    initialize();
}

void CrossCcyBasisMtMResetSwap::initialize() {
    // Pay (foreign) leg
    legs_[0] = IborLeg(fgnSchedule_, fgnIndex_).withNotionals(fgnNominal_).withSpreads(fgnSpread_);
    payer_[0] = -1.0;
    currencies_[0] = fgnCurrency_;
    // Pay leg notional exchange at start.
    Date initialPayDate = fgnSchedule_.dates().front();
    boost::shared_ptr<CashFlow> initialPayCF(new SimpleCashFlow(-fgnNominal_, initialPayDate));
    legs_[0].insert(legs_[0].begin(), initialPayCF);
    // Pay leg notional exchange at end.
    Date finalPayDate = fgnSchedule_.dates().back();
    boost::shared_ptr<CashFlow> finalPayCF(new SimpleCashFlow(fgnNominal_, finalPayDate));
    legs_[0].push_back(finalPayCF);

    // Receive (domestic/resettable) leg
    Date initialRecDate = domSchedule_.dates().front();
    Date initRecFixingDate = fxIndex_->fixingCalendar().advance(
        initialRecDate, -static_cast<Integer>(fxIndex_->fixingDays()), Days);
    Real initFixing = fxIndex_->fixing(initRecFixingDate);
    Real domInitNominal = fgnNominal_ * (invertFxIndex_ ? (1.0 / initFixing) : initFixing);
    // start by creating a dummy vanilla floating leg
    legs_[1] = IborLeg(domSchedule_, domIndex_).withNotionals(domInitNominal).withSpreads(domSpread_);
    payer_[1] = +1.0;
    currencies_[1] = domCurrency_;
    // replace the coupons with a FloatingRateFXLinkedNotionalCoupon 
    // (skip the first coupon as it has a fixed notional)
    for (Size j = 1; j < legs_[1].size(); ++j) {
        boost::shared_ptr<FloatingRateCoupon> coupon =
            boost::dynamic_pointer_cast<FloatingRateCoupon>(legs_[1][j]);
        Date fixingDate = fxIndex_->fixingCalendar().advance(coupon->accrualStartDate(),
            -static_cast<Integer>(fxIndex_->fixingDays()), Days);
        boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon(
            new FloatingRateFXLinkedNotionalCoupon(
                fgnNominal_, fixingDate, fxIndex_, invertFxIndex_, coupon->date(),
                coupon->accrualStartDate(), coupon->accrualEndDate(), coupon->fixingDays(), coupon->index(),
                coupon->gearing(), coupon->spread(), coupon->referencePeriodStart(),
                coupon->referencePeriodEnd(), coupon->dayCounter(), coupon->isInArrears()));
        // set the same pricer
        fxLinkedCoupon->setPricer(coupon->pricer());
        legs_[1][j] = fxLinkedCoupon;
    }
    // now build a separate leg to store the domestic (resetting) notionals
    payer_[2] = +1.0;
    currencies_[2] = domCurrency_;
    for (Size j = 0; j < legs_[1].size(); j++) {
        boost::shared_ptr<Coupon> c = boost::dynamic_pointer_cast<Coupon>(legs_[1][j]);
        QL_REQUIRE(c, "Resetting XCCY - expected Coupon"); // TODO: fixed fx resetable?
        // build a pair of notional flows, one at the start and one at the end of
        // the accrual period. Both with the same FX fixing date
        if (j == 0) {
            legs_[2].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(-domInitNominal, initialRecDate)));
            legs_[2].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(domInitNominal, c->accrualEndDate())));
        }
        else {
            Date fixingDate = fxIndex_->fixingCalendar().advance(
                c->accrualStartDate(), -static_cast<Integer>(fxIndex_->fixingDays()), Days);
            legs_[2].push_back(boost::shared_ptr<CashFlow>(new FXLinkedCashFlow(
                c->accrualStartDate(), fixingDate, -fgnNominal_, fxIndex_, invertFxIndex_)));
            legs_[2].push_back(boost::shared_ptr<CashFlow>(new FXLinkedCashFlow(
                c->accrualEndDate(), fixingDate, fgnNominal_, fxIndex_, invertFxIndex_)));
        }
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

    CrossCcyBasisMtMResetSwap::arguments* arguments = 
        dynamic_cast<CrossCcyBasisMtMResetSwap::arguments*>(args);

    /* Returns here if e.g. args is CrossCcySwap::arguments which
       is the case if PricingEngine is a CrossCcySwap::engine. */
    if (!arguments)
        return;

    arguments->fgnSpread = fgnSpread_;
    arguments->fgnSpread = fgnSpread_;
}

void CrossCcyBasisMtMResetSwap::fetchResults(const PricingEngine::results* r) const {

    CrossCcySwap::fetchResults(r);

    const CrossCcyBasisMtMResetSwap::results* results = 
        dynamic_cast<const CrossCcyBasisMtMResetSwap::results*>(r);
    if (results) {
        /* If PricingEngine::results are of type
           CrossCcyBasisSwap::results */
        fairFgnSpread_ = results->fairFgnSpread;
        fairDomSpread_ = results->fairDomSpread;
    } else {
        /* If not, e.g. if the engine is a CrossCcySwap::engine */
        fairFgnSpread_ = Null<Spread>();
        fairDomSpread_ = Null<Spread>();
    }

    /* Calculate the fair pay and receive spreads if they are null */
    static Spread basisPoint = 1.0e-4;
    if (fairFgnSpread_ == Null<Spread>()) {
        if (legBPS_[0] != Null<Real>())
            fairFgnSpread_ = fgnSpread_ - NPV_ / (legBPS_[0] / basisPoint);
    }
    if (fairDomSpread_ == Null<Spread>()) {
        if (legBPS_[1] != Null<Real>())
            fairDomSpread_ = domSpread_ - NPV_ / (legBPS_[1] / basisPoint);
    }
}

void CrossCcyBasisMtMResetSwap::setupExpired() const {
    CrossCcySwap::setupExpired();
    fairFgnSpread_ = Null<Spread>();
    fairDomSpread_ = Null<Spread>();
}

void CrossCcyBasisMtMResetSwap::arguments::validate() const {
    CrossCcySwap::arguments::validate();
    QL_REQUIRE(fgnSpread != Null<Spread>(), "Pay spread cannot be null");
    QL_REQUIRE(domSpread != Null<Spread>(), "Rec spread cannot be null");
}

void CrossCcyBasisMtMResetSwap::results::reset() {
    CrossCcySwap::results::reset();
    fairFgnSpread = Null<Spread>();
    fairDomSpread = Null<Spread>();
}
} // namespace QuantExt
