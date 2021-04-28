/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/instruments/crossccyfixfloatmtmresetswap.hpp>

using namespace QuantLib;

namespace QuantExt {

    CrossCcyFixFloatMtMResetSwap::CrossCcyFixFloatMtMResetSwap(
    Real Nominal, const Currency& fixedCurrency, const Schedule& fixedSchedule,
    QuantLib::Rate fixedRate, const QuantLib::DayCounter& fixedDayCount, const QuantLib::BusinessDayConvention& fixedPaymentBdc,
    QuantLib::Natural fixedPaymentLag, const QuantLib::Calendar& fixedPaymentCalendar, const Currency& floatCurrency,
    const Schedule& floatSchedule, const boost::shared_ptr<IborIndex>& floatIndex, Spread floatSpread,
    const boost::shared_ptr<FxIndex>& fxIdx, bool receiveFixed)
    : CrossCcySwap(3), nominal_(Nominal), fixedCurrency_(fixedCurrency),
    fixedSchedule_(fixedSchedule), fixedRate_(fixedRate), fixedDayCount_(fixedDayCount), 
    fixedPaymentBdc_(fixedPaymentBdc), fixedPaymentLag_(fixedPaymentLag), fixedPaymentCalendar_(fixedPaymentCalendar),
    floatCurrency_(floatCurrency), floatSchedule_(floatSchedule), floatIndex_(floatIndex),
    floatSpread_(floatSpread), fxIndex_(fxIdx), receiveFixed_(receiveFixed) {

    registerWith(floatIndex_);
    registerWith(fxIndex_);
    initialize();
}

void CrossCcyFixFloatMtMResetSwap::initialize() {
    // Pay (foreign) leg
    legs_[0] = IborLeg(foreignSchedule_, foreignIndex_).withNotionals(foreignNominal_).withSpreads(foreignSpread_);
    receiveDomestic_ ? payer_[0] = -1.0 : payer_[0] = +1.0;

    currencies_[0] = foreignCurrency_;
    // Pay leg notional exchange at start.
    Date initialPayDate = foreignSchedule_.dates().front();
    boost::shared_ptr<CashFlow> initialPayCF(new SimpleCashFlow(-foreignNominal_, initialPayDate));
    legs_[0].insert(legs_[0].begin(), initialPayCF);
    // Pay leg notional exchange at end.
    Date finalPayDate = foreignSchedule_.dates().back();
    boost::shared_ptr<CashFlow> finalPayCF(new SimpleCashFlow(foreignNominal_, finalPayDate));
    legs_[0].push_back(finalPayCF);

    // Receive (domestic/resettable) leg
    // start by creating a dummy vanilla floating leg
    legs_[1] = IborLeg(domesticSchedule_, domesticIndex_).withNotionals(0).withSpreads(domesticSpread_);
    receiveDomestic_ ? payer_[1] = +1.0 : payer_[1] = -1.0;
    currencies_[1] = domesticCurrency_;
    // replace the coupons with a FloatingRateFXLinkedNotionalCoupon
    // (skip the first coupon as it has a fixed notional)
    for (Size j = 0; j < legs_[1].size(); ++j) {
        boost::shared_ptr<FloatingRateCoupon> coupon = boost::dynamic_pointer_cast<FloatingRateCoupon>(legs_[1][j]);
        Date fixingDate = fxIndex_->fixingCalendar().advance(coupon->accrualStartDate(),
            -static_cast<Integer>(fxIndex_->fixingDays()), Days);
        boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon(
            new FloatingRateFXLinkedNotionalCoupon(fixingDate, foreignNominal_, fxIndex_, coupon));
        legs_[1][j] = fxLinkedCoupon;
    }
    // now build a separate leg to store the domestic (resetting) notionals
    receiveDomestic_ ? payer_[2] = +1.0 : payer_[2] = -1.0;
    currencies_[2] = domesticCurrency_;
    for (Size j = 0; j < legs_[1].size(); j++) {
        boost::shared_ptr<Coupon> c = boost::dynamic_pointer_cast<Coupon>(legs_[1][j]);
        QL_REQUIRE(c, "Resetting XCCY - expected Coupon"); // TODO: fixed fx resetable?
        // build a pair of notional flows, one at the start and one at the end of
        // the accrual period. Both with the same FX fixing date
        Date fixingDate = fxIndex_->fixingCalendar().advance(c->accrualStartDate(),
            -static_cast<Integer>(fxIndex_->fixingDays()), Days);
        legs_[2].push_back(boost::shared_ptr<CashFlow>(
            new FXLinkedCashFlow(c->accrualStartDate(), fixingDate, -foreignNominal_, fxIndex_)));
        legs_[2].push_back(boost::shared_ptr<CashFlow>(
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

void CrossCcyFixFloatMtMResetSwap::setupArguments(PricingEngine::arguments* args) const {

    CrossCcySwap::setupArguments(args);

    CrossCcyFixFloatMtMResetSwap::arguments* arguments = dynamic_cast<CrossCcyFixFloatMtMResetSwap::arguments*>(args);

    /* Returns here if e.g. args is CrossCcySwap::arguments which
        is the case if PricingEngine is a CrossCcySwap::engine. */
    if (!arguments)
        return;

    arguments->domesticSpread = domesticSpread_;
    arguments->foreignSpread = foreignSpread_;
}

void CrossCcyFixFloatMtMResetSwap::fetchResults(const PricingEngine::results* r) const {

    CrossCcySwap::fetchResults(r);

    const CrossCcyFixFloatMtMResetSwap::results* results = dynamic_cast<const CrossCcyFixFloatMtMResetSwap::results*>(r);
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

void CrossCcyFixFloatMtMResetSwap::setupExpired() const {
    CrossCcySwap::setupExpired();
    fairForeignSpread_ = Null<Spread>();
    fairDomesticSpread_ = Null<Spread>();
}

void CrossCcyFixFloatMtMResetSwap::arguments::validate() const {
    CrossCcySwap::arguments::validate();
    QL_REQUIRE(foreignSpread != Null<Spread>(), "Pay spread cannot be null");
    QL_REQUIRE(domesticSpread != Null<Spread>(), "Rec spread cannot be null");
}

void CrossCcyFixFloatMtMResetSwap::results::reset() {
    CrossCcySwap::results::reset();
    fairForeignSpread = Null<Spread>();
    fairDomesticSpread = Null<Spread>();
}
} // namespace QuantExt
