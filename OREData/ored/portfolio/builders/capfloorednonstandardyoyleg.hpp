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

/*! \file ored/portfolio/builders/capflooredyoyleg.hpp
\brief builder that returns an engine to price capped floored yoy inflation legs
\ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <qle/cashflows/nonstandardinflationcouponpricer.hpp>

namespace ore {
namespace data {
//! CouponPricer Builder for Capped/Floored YoY Inflation Leg
/*! The coupon pricers are cached by index name
\ingroup builders
*/
class CapFlooredNonStandardYoYLegEngineBuilder : public CachingInflationCouponPricerBuilder<string, const string&> {
public:
    CapFlooredNonStandardYoYLegEngineBuilder()
        : CachingEngineBuilder("CapFlooredNonStdYYModel", "CapFlooredNonStdYYCouponPricer", {"CapFlooredNonStdYYLeg"}) {
    }

protected:
    virtual string keyImpl(const string& indexName) override { return indexName; }
    virtual QuantLib::ext::shared_ptr<QuantLib::InflationCouponPricer> engineImpl(const string& indexName) override {
        QuantLib::ext::shared_ptr<YoYOptionletVolatilitySurface> vol =
            market_->yoyCapFloorVol(indexName, configuration(MarketContext::pricing)).currentLink();
        Handle<YoYInflationIndex> index = market_->yoyInflationIndex(indexName, configuration(MarketContext::pricing));
        Handle<YieldTermStructure> yts = market_->discountCurve(index->currency().code());
        if (vol->volatilityType() == VolatilityType::ShiftedLognormal && vol->displacement() == 0.0)
            return QuantLib::ext::make_shared<QuantExt::NonStandardBlackYoYInflationCouponPricer>(
                Handle<YoYOptionletVolatilitySurface>(vol), yts);
        else if (vol->volatilityType() == VolatilityType::ShiftedLognormal && vol->displacement() != 0.0)
            return QuantLib::ext::make_shared<QuantExt::NonStandardUnitDisplacedBlackYoYInflationCouponPricer>(
                Handle<YoYOptionletVolatilitySurface>(vol), yts);
        else if (vol->volatilityType() == VolatilityType::Normal)
            return QuantLib::ext::make_shared<QuantExt::NonStandardBachelierYoYInflationCouponPricer>(
                Handle<YoYOptionletVolatilitySurface>(vol), yts);
        else
            QL_FAIL("Unknown VolatilityType of YoYOptionletVolatilitySurface");
    }
};
} // namespace data
} // namespace ore
