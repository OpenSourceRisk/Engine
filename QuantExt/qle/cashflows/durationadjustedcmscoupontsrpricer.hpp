/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.
 All rights reserved.
 */

/*! \file durationadjustedcmscoupontsrpricer.hpp
    \brief tsr coupon pricer for duration adjusted cms coupon
*/

#pragma once

#include <orepbase/qle/models/annuitymapping.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

class DurationAdjustedCmsCoupon;

class DurationAdjustedCmsCouponTsrPricer : public CmsCouponPricer {
public:
    DurationAdjustedCmsCouponTsrPricer(
        const Handle<SwaptionVolatilityStructure>& swaptionVol,
        const boost::shared_ptr<AnnuityMappingBuilder>& annuityMappingBuilder, const Real lowerIntegrationBound = -0.3,
        const Real upperIntegrationBound = 0.3,
        const boost::shared_ptr<Integrator>& integrator = boost::shared_ptr<Integrator>());

    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;

private:
    void initialize(const FloatingRateCoupon& coupon) override;
    Real optionletRate(Option::Type type, Real effectiveStrike) const;

    boost::shared_ptr<AnnuityMappingBuilder> annuityMappingBuilder_;
    Real lowerIntegrationBound_, upperIntegrationBound_;
    boost::shared_ptr<Integrator> integrator_;

    const DurationAdjustedCmsCoupon* coupon_;
    Date today_;
    Real swapRate_, durationAdjustment_, forwardAnnuity_;
    boost::shared_ptr<SmileSection> smileSection_;
    boost::shared_ptr<AnnuityMapping> annuityMapping_;
};

} // namespace QuantExt
