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

#include <qle/instruments/balanceguaranteedswap.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

BalanceGuaranteedSwap::BalanceGuaranteedSwap(
    const VanillaSwap::Type type, const std::vector<std::vector<Real>>& trancheNominals,
    const Schedule& nominalSchedule, const Size referencedTranche, const Schedule& fixedSchedule,
    const std::vector<Real>& fixedRate, const DayCounter& fixedDayCount, const Schedule& floatingSchedule,
    const QuantLib::ext::shared_ptr<IborIndex>& iborIndex, const std::vector<Real>& gearing, const std::vector<Real>& spread,
    const std::vector<Real>& cappedRate, const std::vector<Real>& flooredRate, const DayCounter& floatingDayCount,
    boost::optional<BusinessDayConvention> paymentConvention)
    : Swap(2), type_(type), trancheNominals_(trancheNominals), nominalSchedule_(nominalSchedule),
      referencedTranche_(referencedTranche), fixedSchedule_(fixedSchedule), fixedRate_(fixedRate),
      fixedDayCount_(fixedDayCount), floatingSchedule_(floatingSchedule), iborIndex_(iborIndex), gearing_(gearing),
      spread_(spread), cappedRate_(cappedRate), flooredRate_(flooredRate), floatingDayCount_(floatingDayCount) {

    // checks

    if (paymentConvention)
        paymentConvention_ = *paymentConvention;
    else
        paymentConvention_ = floatingSchedule_.businessDayConvention();

    QL_REQUIRE(nominalSchedule.size() > 0, "Nominal schedule size is zero");
    QL_REQUIRE(nominalSchedule.hasTenor(), "Nominal schedule needs a tenor");
    QL_REQUIRE(nominalSchedule.tenor().frequency() != QuantLib::OtherFrequency,
               "Nominal schedule tenor (" << nominalSchedule.tenor() << ") not allowed, corresponds to OtherFrequency");
    QL_REQUIRE(fixedSchedule.size() > 0, "Fixed schedule size is zero");
    QL_REQUIRE(floatingSchedule.size() > 0, "Floating schedule size is zero");
    QL_REQUIRE(fixedSchedule_.size() - 1 == fixedRate_.size(),
               "Fixed schedule size (" << fixedSchedule_.size() << ") does not match fixed rate size ("
                                       << fixedRate_.size() << ")");

    QL_REQUIRE(floatingSchedule_.size() - 1 == gearing_.size(),
               "Floating schedule size (" << floatingSchedule_.size() << ") does not match gearing size ("
                                          << gearing_.size() << ")");
    QL_REQUIRE(floatingSchedule_.size() - 1 == spread_.size(),
               "Floating schedule size (" << floatingSchedule_.size() << ") does not match spread size ("
                                          << spread_.size() << ")");
    QL_REQUIRE(floatingSchedule_.size() - 1 == cappedRate_.size(),
               "Floating schedule size (" << floatingSchedule_.size() << ") does not match spread size ("
                                          << cappedRate_.size() << ")");
    QL_REQUIRE(floatingSchedule_.size() - 1 == flooredRate_.size(),
               "Floating schedule size (" << floatingSchedule_.size() << ") does not match spread size ("
                                          << flooredRate_.size() << ")");
    QL_REQUIRE((floatingSchedule.size() - 1) % (fixedSchedule.size() - 1) == 0,
               "fixed schedule size - 1 (" << fixedSchedule.size() - 1 << ") must divide floating schedule size - 1 ("
                                           << floatingSchedule.size() - 1 << ")");

    QL_REQUIRE(trancheNominals.size() > 0, "trancheNominals must be non-empty");
    QL_REQUIRE(referencedTranche < trancheNominals.size(),
               "referencedTranche (" << referencedTranche << ") out of range 0..." << (trancheNominals.size() - 1));
    for (Size i = 0; i < trancheNominals.size(); ++i) {
        QL_REQUIRE(trancheNominals[i].size() == nominalSchedule.size() - 1,
                   "Tranche nominals at " << i << " (" << trancheNominals.size() << ") do not match nominal schedule ("
                                          << nominalSchedule.size() << ")");
    }

    // build the fixed and floating nominal for the swap
    std::vector<Real> fixedNominal, floatingNominal;
    for (Size i = 0; i < fixedSchedule.size() - 1; ++i) {
        fixedNominal.push_back(trancheNominal(referencedTranche_, fixedSchedule[i]));
    }
    // derive floating nominal schedule from fixed to ensure they match
    Size ratio = (floatingSchedule.size() - 1) / (fixedSchedule.size() - 1); // we know there is no remainder
    floatingNominal.resize(fixedNominal.size() * ratio);
    Size i = 0; // remove in C++14 and instead write [i=0, &fixedNotional] in the capture below
    std::generate(floatingNominal.begin(), floatingNominal.end(),
                  [i, &fixedNominal, ratio]() mutable { return fixedNominal[i++ / ratio]; });

    // if the gearing is zero then the ibor leg will be set up with fixed
    // coupons which makes trouble here in this context. We therefore use
    // a dirty trick and enforce the gearing to be non zero.

    // TODO do we need this?

    std::vector<Real> gearingTmp(gearing_);
    for (Size i = 0; i < gearingTmp.size(); ++i) {
        if (close(gearingTmp[i], 0.0))
            gearingTmp[i] = QL_EPSILON;
    }

    legs_[0] = FixedRateLeg(fixedSchedule_)
                   .withNotionals(fixedNominal)
                   .withCouponRates(fixedRate_, fixedDayCount_)
                   .withPaymentAdjustment(paymentConvention_);

    legs_[1] = IborLeg(floatingSchedule_, iborIndex_)
                   .withNotionals(floatingNominal)
                   .withPaymentDayCounter(floatingDayCount_)
                   .withPaymentAdjustment(paymentConvention_)
                   .withSpreads(spread_)
                   .withGearings(gearingTmp)
                   .withCaps(cappedRate_)
                   .withFloors(flooredRate_);

    for (Leg::const_iterator i = legs_[1].begin(); i < legs_[1].end(); ++i)
        registerWith(*i);

    auto cpnPricer = QuantLib::ext::make_shared<BlackIborCouponPricer>();
    setCouponPricer(legs_[1], cpnPricer);

    switch (type_) {
    case VanillaSwap::Payer:
        payer_[0] = -1.0;
        payer_[1] = +1.0;
        break;
    case VanillaSwap::Receiver:
        payer_[0] = +1.0;
        payer_[1] = -1.0;
        break;
    default:
        QL_FAIL("Unknown vanilla swap type");
    }
}

void BalanceGuaranteedSwap::setupArguments(PricingEngine::arguments* args) const {
    Swap::setupArguments(args);
    auto arguments = dynamic_cast<BalanceGuaranteedSwap::arguments*>(args);

    // QL_REQUIRE(arguments != nullptr, "BalanceGuaranteedSwap::setupArguments(): wrong argument type");

    // allow for swap engine
    if (arguments == nullptr)
        return;

    arguments->type = type_;
    arguments->trancheNominals = trancheNominals_;
    arguments->trancheNominalDates = nominalSchedule_.dates();
    arguments->trancheNominalFrequency = nominalSchedule_.tenor().frequency();
    arguments->referencedTranche = referencedTranche_;
    arguments->fixedRate = fixedRate_;
    arguments->iborIndex = iborIndex();
    arguments->cappedRate = cappedRate_;
    arguments->flooredRate = flooredRate_;

    const Leg& fixedCoupons = fixedLeg();

    arguments->fixedResetDates = arguments->fixedPayDates = std::vector<Date>(fixedCoupons.size());
    arguments->fixedCoupons = std::vector<Real>(fixedCoupons.size());

    for (Size i = 0; i < fixedCoupons.size(); ++i) {
        auto coupon = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(fixedCoupons[i]);
        QL_REQUIRE(coupon != nullptr, "BalanceGuaranteedSwap::setupArguments(): expected fixed rate coupon");
        arguments->fixedPayDates[i] = coupon->date();
        arguments->fixedResetDates[i] = coupon->accrualStartDate();
        arguments->fixedCoupons[i] = coupon->amount();
    }

    const Leg& floatingCoupons = floatingLeg();

    arguments->floatingResetDates = arguments->floatingPayDates = arguments->floatingFixingDates =
        std::vector<Date>(floatingCoupons.size());
    arguments->floatingAccrualTimes = std::vector<Time>(floatingCoupons.size());
    arguments->floatingSpreads = std::vector<Spread>(floatingCoupons.size());
    arguments->floatingGearings = std::vector<Real>(floatingCoupons.size());
    arguments->floatingCoupons = std::vector<Real>(floatingCoupons.size());

    for (Size i = 0; i < floatingCoupons.size(); ++i) {
        auto coupon = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(floatingCoupons[i]);
        QL_REQUIRE(coupon != nullptr, "BalanceGuaranteedSwap::setupArguments(): expected fixed rate coupon");
        arguments->floatingResetDates[i] = coupon->accrualStartDate();
        arguments->floatingPayDates[i] = coupon->date();
        arguments->floatingFixingDates[i] = coupon->fixingDate();
        arguments->floatingAccrualTimes[i] = coupon->accrualPeriod();
        arguments->floatingSpreads[i] = coupon->spread();
        arguments->floatingGearings[i] = coupon->gearing();
        try {
            arguments->floatingCoupons[i] = coupon->amount();
        } catch (const std::exception&) {
            arguments->floatingCoupons[i] = Null<Real>();
        }
    }

    arguments->fixedLeg = legs_[0];
    arguments->floatingLeg = legs_[1];
}

Real BalanceGuaranteedSwap::trancheNominal(const Size trancheIndex, const Date& d) {
    QL_REQUIRE(trancheIndex < trancheNominals_.size(), "BalanceGuaranteedSwap::trancheNominal(): tranceIndex ("
                                                           << trancheIndex << ") out of range 0..."
                                                           << trancheNominals_.size() - 1);
    if (d < nominalSchedule_.dates().front() || d >= nominalSchedule_.dates().back())
        return 0.0;
    Size l = std::upper_bound(nominalSchedule_.dates().begin(), nominalSchedule_.dates().end(), d) -
             nominalSchedule_.dates().begin();
    return trancheNominals_[trancheIndex][l - 1];
}

void BalanceGuaranteedSwap::setupExpired() const { Swap::setupExpired(); }

void BalanceGuaranteedSwap::fetchResults(const PricingEngine::results* r) const {
    Swap::fetchResults(r);
    auto results = dynamic_cast<const BalanceGuaranteedSwap::results*>(r);

    // QL_REQUIRE(results != nullptr, "BalanceGuaranteedSwap::fetchResult(): wrong result type");

    // allow for swap engine
    if (results == nullptr)
        return;
}

void BalanceGuaranteedSwap::arguments::validate() const {
    Swap::arguments::validate();
    // TODO
}

void BalanceGuaranteedSwap::results::reset() { Swap::results::reset(); }

} // namespace QuantExt
