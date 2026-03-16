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

/*! \file durationadjustedcms.hpp
    \brief coupon pricer builder for duration adjusted cms coupons
    \ingroup builders
*/

#pragma once

#include <qle/cashflows/durationadjustedcmscoupon.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

class DurationAdjustedCmsCouponPricerBuilder : public CachingCouponPricerBuilder<string, const string&> {
public:
    DurationAdjustedCmsCouponPricerBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"DurationAdjustedCMS"}) {}

protected:
    string keyImpl(const string& key) override { return key; }
};

class LinearTsrDurationAdjustedCmsCouponPricerBuilder : public DurationAdjustedCmsCouponPricerBuilder {
public:
    LinearTsrDurationAdjustedCmsCouponPricerBuilder()
        : DurationAdjustedCmsCouponPricerBuilder("LinearTSR", "LinearTSRPricer") {}

protected:
    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> engineImpl(const string& key) override;
};

} // namespace data
} // namespace ore
