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

/*! \file durationadjustedcmscoupontsrpricer.hpp
    \brief tsr coupon pricer for duration adjusted cms coupon
*/

#pragma once

#include <qle/models/annuitymapping.hpp>

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
        const QuantLib::ext::shared_ptr<AnnuityMappingBuilder>& annuityMappingBuilder, const Real lowerIntegrationBound = -0.3,
        const Real upperIntegrationBound = 0.3,
        const QuantLib::ext::shared_ptr<Integrator>& integrator = QuantLib::ext::shared_ptr<Integrator>());

    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;

private:
    void initialize(const FloatingRateCoupon& coupon) override;
    Real optionletRate(Option::Type type, Real effectiveStrike) const;

    QuantLib::ext::shared_ptr<AnnuityMappingBuilder> annuityMappingBuilder_;
    Real lowerIntegrationBound_, upperIntegrationBound_;
    QuantLib::ext::shared_ptr<Integrator> integrator_;

    const DurationAdjustedCmsCoupon* coupon_;
    Date today_;
    Real swapRate_, durationAdjustment_, forwardAnnuity_;
    QuantLib::ext::shared_ptr<SmileSection> smileSection_;
    QuantLib::ext::shared_ptr<AnnuityMapping> annuityMapping_;
};

} // namespace QuantExt
