/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file durationadjustedcms.hpp
    \brief coupon pricer builder for duration adjuted cms coupons
    \ingroup builders
*/

#pragma once

#include <orepbase/qle/cashflows/durationadjustedcmscoupon.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {
using namespace ore::data;

class DurationAdjustedCmsCouponPricerBuilder : public CachingCouponPricerBuilder<string, const Currency&> {
public:
    DurationAdjustedCmsCouponPricerBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"DurationAdjustedCMS"}) {}

protected:
    string keyImpl(const Currency& ccy) override { return ccy.code(); }
};

class LinearTsrDurationAdjustedCmsCouponPricerBuilder : public DurationAdjustedCmsCouponPricerBuilder {
public:
    LinearTsrDurationAdjustedCmsCouponPricerBuilder()
        : DurationAdjustedCmsCouponPricerBuilder("LinearTSR", "LinearTSRPricer") {}

protected:
    boost::shared_ptr<FloatingRateCouponPricer> engineImpl(const Currency& ccy) override;
};

} // namespace data
} // namespace ore
