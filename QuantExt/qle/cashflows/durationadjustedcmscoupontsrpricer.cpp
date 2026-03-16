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

#include <qle/cashflows/durationadjustedcmscoupon.hpp>
#include <qle/cashflows/durationadjustedcmscoupontsrpricer.hpp>

#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/math/integrals/kronrodintegral.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/atmsmilesection.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {

DurationAdjustedCmsCouponTsrPricer::DurationAdjustedCmsCouponTsrPricer(
    const Handle<SwaptionVolatilityStructure>& swaptionVol,
    const QuantLib::ext::shared_ptr<AnnuityMappingBuilder>& annuityMappingBuilder, const Real lowerIntegrationBound,
    const Real upperIntegrationBound, const QuantLib::ext::shared_ptr<Integrator>& integrator)
    : CmsCouponPricer(swaptionVol), annuityMappingBuilder_(annuityMappingBuilder),
      lowerIntegrationBound_(lowerIntegrationBound), upperIntegrationBound_(upperIntegrationBound),
      integrator_(integrator) {
    if (integrator_ == nullptr)
        integrator_ = ext::make_shared<GaussKronrodNonAdaptive>(1E-10, 5000, 1E-10);
    if (annuityMappingBuilder_ != nullptr)
        registerWith(annuityMappingBuilder_);
}

Real DurationAdjustedCmsCouponTsrPricer::swapletPrice() const {
    QL_FAIL("DurationAdjustedCmsCouponTsrPricer::swapletPrice() is not implemented");
}

Real DurationAdjustedCmsCouponTsrPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("DurationAdjustedCmsCouponTsrPricer::swapletPrice() is not implemented");
}

Real DurationAdjustedCmsCouponTsrPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("DurationAdjustedCmsCouponTsrPricer::swapletPrice() is not implemented");
}

Rate DurationAdjustedCmsCouponTsrPricer::swapletRate() const {
    return capletRate(swapRate_) - floorletRate(swapRate_) +
           (coupon_->gearing() * swapRate_ + coupon_->spread()) * durationAdjustment_;
}

Rate DurationAdjustedCmsCouponTsrPricer::capletRate(Rate effectiveCap) const {
    if (coupon_->fixingDate() <= today_) {
        return durationAdjustment_ * coupon_->gearing() * std::max(swapRate_ - effectiveCap, 0.0);
    } else {
        return durationAdjustment_ * coupon_->gearing() * optionletRate(Option::Call, effectiveCap);
    }
}

Rate DurationAdjustedCmsCouponTsrPricer::floorletRate(Rate effectiveFloor) const {
    if (coupon_->fixingDate() <= today_) {
        return durationAdjustment_ * coupon_->gearing() * std::max(effectiveFloor - swapRate_, 0.0);
    } else {
        return durationAdjustment_ * coupon_->gearing() * optionletRate(Option::Put, effectiveFloor);
    }
}

void DurationAdjustedCmsCouponTsrPricer::initialize(const FloatingRateCoupon& coupon) {

    coupon_ = dynamic_cast<const DurationAdjustedCmsCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "DurationAdjustedCmsCoupon needed");
    today_ = QuantLib::Settings::instance().evaluationDate();
    durationAdjustment_ = coupon_->durationAdjustment();

    if (coupon_->fixingDate() > today_) {
        Handle<YieldTermStructure> discountCurve;
        if (coupon_->swapIndex()->exogenousDiscount())
            discountCurve = coupon_->swapIndex()->discountingTermStructure();
        else
            discountCurve = coupon_->swapIndex()->forwardingTermStructure();
        auto swap = coupon_->swapIndex()->underlyingSwap(coupon_->fixingDate());
        swapRate_ = swap->fairRate();
        forwardAnnuity_ = 1.0E4 * std::fabs(swap->fixedLegBPS()) / discountCurve->discount(coupon_->date());
        smileSection_ = swaptionVolatility()->smileSection(coupon_->fixingDate(), coupon_->swapIndex()->tenor());
        // if the smile section does not have an ATM level, we add one
        if (smileSection_->atmLevel() == Null<Real>())
            smileSection_ = ext::make_shared<AtmSmileSection>(smileSection_, swapRate_);
        annuityMapping_ =
            annuityMappingBuilder_->build(today_, coupon_->fixingDate(), coupon_->date(), *swap, discountCurve);
    } else {
        swapRate_ = coupon_->swapIndex()->fixing(coupon_->fixingDate());
    }
}

Real DurationAdjustedCmsCouponTsrPricer::optionletRate(Option::Type optionType, Real strike) const {

    Real effectiveLowerIntegrationBound = lowerIntegrationBound_;
    Real effectiveUpperIntegrationBound = upperIntegrationBound_;

    // adjust the lower integration bound if the vol type is lognormal

    if (swaptionVolatility()->volatilityType() == ShiftedLognormal) {
        effectiveLowerIntegrationBound = std::max(
            lowerIntegrationBound_, -swaptionVolatility()->shift(coupon_->fixingDate(), coupon_->swapIndex()->tenor()));
    }

    // the payoff function and its 1st and 2nd derivatives vanish for arguments > strike (call) resp. < strike (put)

    if (optionType == Option::Call)
        effectiveLowerIntegrationBound = std::max(effectiveLowerIntegrationBound, strike);
    else
        effectiveUpperIntegrationBound = std::min(effectiveUpperIntegrationBound, strike);

    Real integral = 0.0;
    Real omega = optionType == Option::Call ? 1.0 : -1.0;

    // compute the relevant integral

    if (effectiveLowerIntegrationBound < effectiveUpperIntegrationBound &&
        !close_enough(effectiveLowerIntegrationBound, effectiveUpperIntegrationBound)) {

        auto f = [this, strike, omega](const Real S) -> Real {
            return std::max(omega * (S - strike), 0.0) * this->durationAdjustment_;
        };

        auto fp = [this, strike, omega](const Real S) -> Real {
            if (this->coupon_->duration() == 0) {
                return omega * (omega * S > omega * strike ? 1.0 : 0.0);
            } else {
                Real dp = 0.0;
                for (Size i = 0; i < this->coupon_->duration(); ++i) {
                    dp -= static_cast<Real>(i + 1) / std::pow(1.0 + this->swapRate_, i + 2);
                }
                return this->durationAdjustment_ * omega * (omega * S > omega * strike ? 1.0 : 0.0) +
                       dp * std::max(omega * (S - strike), 0.0);
            }
        };

        auto fpp = [this, strike, omega](const Real S) -> Real {
            if (this->coupon_->duration() == 0) {
                return 0.0;
            } else {
                Real dp1 = 0.0, dp2 = 0.0;
                for (Size i = 0; i < this->coupon_->duration(); ++i) {
                    dp1 -= static_cast<Real>(i + 1) / std::pow(1.0 + this->swapRate_, i + 2);
                    dp2 += static_cast<Real>(i + 1) * static_cast<Real>(i + 2) / std::pow(1.0 + this->swapRate_, i + 3);
                }
                return dp1 * omega * (omega * S > omega * strike ? 1.0 : 0.0) +
                       dp2 * std::max(omega * (S - strike), 0.0);
            }
        };

        auto integrand = [this, &f, &fp, &fpp](const Real S) -> Real {
            Real f2 = 0.0;
            if (this->coupon_->duration() != 0) {
                f2 = this->annuityMapping_->map(S) * fpp(S);
            }
            Real f1 = 2.0 * fp(S) * this->annuityMapping_->mapPrime(S);
            Real f0 = 0.0;
            if (!this->annuityMapping_->mapPrime2IsZero())
                f0 = annuityMapping_->mapPrime2(S) * f(S);
            return (f0 + f1 + f2) * this->smileSection_->optionPrice(S, S < swapRate_ ? Option::Put : Option::Call);
        };

        // calculate two integrals up to and from the fair swap rate => is that better than a single integral?
        Real tmpBound;
        tmpBound = std::min(effectiveUpperIntegrationBound, swapRate_);
        if (tmpBound > effectiveLowerIntegrationBound)
            integral += (*integrator_)(integrand, effectiveLowerIntegrationBound, tmpBound);
        tmpBound = std::max(effectiveLowerIntegrationBound, swapRate_);
        if (effectiveUpperIntegrationBound > tmpBound)
            integral += (*integrator_)(integrand, tmpBound, effectiveUpperIntegrationBound);

        integral = (*integrator_)(integrand, effectiveLowerIntegrationBound, effectiveUpperIntegrationBound);
    }

    // add payoff function * annuityMapping at fair swap rate and Dirac delta contributions from integral

    Real singularTerms =
        annuityMapping_->map(swapRate_) * durationAdjustment_ * std::max(omega * (swapRate_ - strike), 0.0) +
        annuityMapping_->map(strike) * durationAdjustment_ *
            smileSection_->optionPrice(strike, strike < swapRate_ ? Option::Put : Option::Call);

    return forwardAnnuity_ * (integral + singularTerms) / durationAdjustment_;
}

} // namespace QuantExt
