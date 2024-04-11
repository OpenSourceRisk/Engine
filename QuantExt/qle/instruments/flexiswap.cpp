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

#include <qle/instruments/flexiswap.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

FlexiSwap::FlexiSwap(const VanillaSwap::Type type, const std::vector<Real>& fixedNominal,
                     const std::vector<Real>& floatingNominal, const Schedule& fixedSchedule,
                     const std::vector<Real>& fixedRate, const DayCounter& fixedDayCount,
                     const Schedule& floatingSchedule, const QuantLib::ext::shared_ptr<IborIndex>& iborIndex,
                     const std::vector<Real>& gearing, const std::vector<Spread>& spread,
                     const std::vector<Real>& cappedRate, const std::vector<Real>& flooredRate,
                     const DayCounter& floatingDayCount, const std::vector<Real>& lowerNotionalBound,
                     const QuantLib::Position::Type optionPosition, const std::vector<bool>& notionalCanBeDecreased,
                     boost::optional<BusinessDayConvention> paymentConvention)
    : Swap(2), type_(type), fixedNominal_(fixedNominal), floatingNominal_(floatingNominal),
      fixedSchedule_(fixedSchedule), fixedRate_(fixedRate), fixedDayCount_(fixedDayCount),
      floatingSchedule_(floatingSchedule), iborIndex_(iborIndex), gearing_(gearing), spread_(spread),
      cappedRate_(cappedRate), flooredRate_(flooredRate), floatingDayCount_(floatingDayCount),
      lowerNotionalBound_(lowerNotionalBound), optionPosition_(optionPosition),
      notionalCanBeDecreased_(notionalCanBeDecreased) {

    if (paymentConvention)
        paymentConvention_ = *paymentConvention;
    else
        paymentConvention_ = floatingSchedule_.businessDayConvention();

    QL_REQUIRE(floatingNominal.size() > 0, "FloatingNominal size is zero");
    QL_REQUIRE(fixedNominal.size() > 0, "FixedNominal size is zero");
    QL_REQUIRE(floatingNominal.size() % fixedNominal.size() == 0,
               "Fixed nominal size (" << fixedNominal.size() << ") must divide floating nominal size ("
                                      << floatingNominal.size() << ")");
    QL_REQUIRE(fixedNominal_.size() == fixedRate_.size(), "Fixed nominal size (" << fixedNominal_.size()
                                                                                 << ") does not match fixed rate size ("
                                                                                 << fixedRate_.size() << ")");
    QL_REQUIRE(fixedNominal_.size() == fixedSchedule_.size() - 1,
               "Fixed nominal size (" << fixedNominal_.size() << ") does not match schedule size ("
                                      << fixedSchedule_.size() << ") - 1");
    QL_REQUIRE(fixedNominal_.size() == lowerNotionalBound_.size(),
               "Fixed nominal size (" << fixedNominal_.size() << ") does not match lowerNotionalBound size ("
                                      << lowerNotionalBound_.size() << ")");

    QL_REQUIRE(floatingNominal_.size() == floatingSchedule_.size() - 1,
               "Floating nominal size (" << floatingNominal_.size() << ") does not match schedule size ("
                                         << floatingSchedule_.size() << ") - 1");

    QL_REQUIRE(floatingNominal_.size() == gearing_.size(),
               "Floating nominal size (" << floatingNominal_.size() << ") does not match gearing size ("
                                         << gearing_.size() << ")");
    QL_REQUIRE(floatingNominal_.size() == spread_.size(), "Floating nominal size (" << floatingNominal_.size()
                                                                                    << ") does not match spread size ("
                                                                                    << spread_.size() << ")");
    QL_REQUIRE(floatingNominal_.size() == cappedRate_.size(),
               "Floating nominal size (" << floatingNominal_.size() << ") does not match spread size ("
                                         << cappedRate_.size() << ")");
    QL_REQUIRE(floatingNominal_.size() == flooredRate_.size(),
               "Floating nominal size (" << floatingNominal_.size() << ") does not match spread size ("
                                         << flooredRate_.size() << ")");

    QL_REQUIRE(notionalCanBeDecreased.empty() || notionalCanBeDecreased_.size() == fixedSchedule.size() - 1,
               "notionalCanBeDecreased (" << notionalCanBeDecreased_.size() << ") must match number of fixed periods ("
                                          << fixedSchedule.size() - 1 << ")");

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
                   .withNotionals(fixedNominal_)
                   .withCouponRates(fixedRate_, fixedDayCount_)
                   .withPaymentAdjustment(paymentConvention_);

    legs_[1] = IborLeg(floatingSchedule_, iborIndex_)
                   .withNotionals(floatingNominal_)
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

void FlexiSwap::setupArguments(PricingEngine::arguments* args) const {
    Swap::setupArguments(args);
    auto arguments = dynamic_cast<FlexiSwap::arguments*>(args);

    // QL_REQUIRE(arguments != nullptr, "FlexiSwap::setupArguments(): wrong argument type");

    // allow for swap engine
    if (arguments == nullptr)
        return;

    arguments->type = type_;
    arguments->fixedNominal = fixedNominal_;
    arguments->floatingNominal = floatingNominal_;
    arguments->fixedRate = fixedRate_;
    arguments->iborIndex = iborIndex();
    arguments->cappedRate = cappedRate_;
    arguments->flooredRate = flooredRate_;
    arguments->lowerNotionalBound = lowerNotionalBound_;
    arguments->optionPosition = optionPosition_;
    arguments->notionalCanBeDecreased =
        notionalCanBeDecreased_.empty() ? std::vector<bool>(fixedNominal_.size(), true) : notionalCanBeDecreased_;

    const Leg& fixedCoupons = fixedLeg();

    arguments->fixedResetDates = arguments->fixedPayDates = std::vector<Date>(fixedCoupons.size());
    arguments->fixedCoupons = std::vector<Real>(fixedCoupons.size());

    for (Size i = 0; i < fixedCoupons.size(); ++i) {
        auto coupon = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(fixedCoupons[i]);
        QL_REQUIRE(coupon != nullptr, "FlexiSwap::setupArguments(): expected fixed rate coupon");
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
        QL_REQUIRE(coupon != nullptr, "FlexiSwap::setupArguments(): expected fixed rate coupon");
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
}

Real FlexiSwap::underlyingValue() const {
    calculate();
    QL_REQUIRE(underlyingValue_ != Null<Real>(), "FlexiSwap: underlying value not provided");
    return underlyingValue_;
}

void FlexiSwap::setupExpired() const {
    Swap::setupExpired();
    underlyingValue_ = 0.0;
}

void FlexiSwap::fetchResults(const PricingEngine::results* r) const {
    Swap::fetchResults(r);
    auto results = dynamic_cast<const FlexiSwap::results*>(r);

    // QL_REQUIRE(results != nullptr, "FlexiSwap::fetchResult(): wrong result type");

    // allow for swap engine
    if (results == nullptr)
        return;

    underlyingValue_ = results->underlyingValue;
}

void FlexiSwap::arguments::validate() const {
    Swap::arguments::validate();
    Size ratio = floatingNominal.size() / fixedNominal.size(); // we know there is no remainder
    bool hasOptionality = false;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < fixedNominal.size(); ++i) {
        for (Size j = 0; j < ratio; ++j) {
            QL_REQUIRE(QuantLib::close_enough(fixedNominal[i], floatingNominal[i * ratio + j]),
                       "FlexiSwap::arguments::validate(): fixedNominal["
                           << i << "] = " << fixedNominal[i] << " does not match floatingNominal[" << i * ratio + j
                           << "] = " << floatingNominal[i * ratio + j] << ", ratio is " << ratio);
        }
        QL_REQUIRE(lowerNotionalBound[i] < fixedNominal[i] ||
                       QuantLib::close_enough(lowerNotionalBound[i], fixedNominal[i]),
                   "FlexiSwap::arguments::validate(): lowerNotionalBound[" << i << "] = " << lowerNotionalBound[i]
                                                                           << " must be leq fixedNominal[" << i
                                                                           << "] = " << fixedNominal[i]);
        if (floatingResetDates[ratio * i] > today && notionalCanBeDecreased[i])
            hasOptionality = hasOptionality || !QuantLib::close_enough(lowerNotionalBound[i], fixedNominal[i]);
        if (i > 0 && hasOptionality) {
            QL_REQUIRE(lowerNotionalBound[i] < lowerNotionalBound[i - 1] ||
                           QuantLib::close_enough(lowerNotionalBound[i], lowerNotionalBound[i - 1]),
                       "FlexiSwap::arguments::validate(): lowerNotionalBound["
                           << i - 1 << "] = " << lowerNotionalBound[i - 1] << " < lowerNotionalBound[" << i << "] = "
                           << lowerNotionalBound[i] << ", not allowed, since optionality has kicked in already");
            QL_REQUIRE(fixedNominal[i] < fixedNominal[i - 1] ||
                           QuantLib::close_enough(fixedNominal[i], fixedNominal[i - 1]),
                       "FlexiSwap::arguments::validate(): fixedNominal["
                           << i - 1 << "] = " << fixedNominal[i - 1] << " < fixedNominal[" << i
                           << "] = " << fixedNominal[i] << ", not allowed, since optionality has kicked in already");
        }
    }
}

void FlexiSwap::results::reset() {
    Swap::results::reset();
    underlyingValue = Null<Real>();
}

} // namespace QuantExt
