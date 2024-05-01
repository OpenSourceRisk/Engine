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
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <qle/utilities/inflation.hpp>

namespace ore {
namespace data {
//! CouponPricer Builder for Capped/Floored CPI Inflation Leg
/*! The coupon pricers are cached by index name
\ingroup builders
*/
class CapFlooredCpiLegCouponEngineBuilder : public CachingInflationCouponPricerBuilder<string, const string&> {
public:
    CapFlooredCpiLegCouponEngineBuilder()
        : CachingEngineBuilder("Black", "BlackAnalytic", {"CappedFlooredCpiLegCoupons"}) {}

protected:
    virtual string keyImpl(const string& indexName) override { return indexName; }
    virtual QuantLib::ext::shared_ptr<QuantLib::InflationCouponPricer> engineImpl(const string& indexName) override {
        Handle<QuantLib::CPIVolatilitySurface> vol =
            market_->cpiInflationCapFloorVolatilitySurface(indexName, configuration(MarketContext::pricing));
        Handle<ZeroInflationIndex> cpiIndex =
            market_->zeroInflationIndex(indexName, configuration(MarketContext::pricing));
        std::string ccyCode = cpiIndex->currency().code();
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccyCode, configuration(MarketContext::pricing));

         bool useLastFixingDate =
            parseBool(engineParameter("useLastFixingDate", std::vector<std::string>(), false, "false"));

         bool isLogNormal = QuantExt::ZeroInflation::isCPIVolSurfaceLogNormal(vol.currentLink());

         if (isLogNormal) {
             return QuantLib::ext::make_shared<QuantExt::BlackCPICouponPricer>(vol, discountCurve, useLastFixingDate);
         } else {
             return QuantLib::ext::make_shared<QuantExt::BachelierCPICouponPricer>(vol, discountCurve, useLastFixingDate);
         }

        
    }
};

class CapFlooredCpiLegCashFlowEngineBuilder : public CachingInflationCashFlowPricerBuilder<string, const string&> {
public:
    CapFlooredCpiLegCashFlowEngineBuilder()
        : CachingEngineBuilder("Black", "BlackAnalytic", {"CappedFlooredCpiLegCashFlows"}) {}

protected:
    virtual string keyImpl(const string& indexName) override { return indexName; }
    virtual QuantLib::ext::shared_ptr<QuantExt::InflationCashFlowPricer> engineImpl(const string& indexName) override {
        Handle<QuantLib::CPIVolatilitySurface> vol =
            market_->cpiInflationCapFloorVolatilitySurface(indexName, configuration(MarketContext::pricing));
        Handle<ZeroInflationIndex> cpiIndex =
            market_->zeroInflationIndex(indexName, configuration(MarketContext::pricing));
        std::string ccyCode = cpiIndex->currency().code();
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccyCode, configuration(MarketContext::pricing));

        bool useLastFixingDate =
            parseBool(engineParameter("useLastFixingDate", std::vector<std::string>(), false, "false"));

        bool isLogNormal = QuantExt::ZeroInflation::isCPIVolSurfaceLogNormal(vol.currentLink());

        if (isLogNormal) {
            return QuantLib::ext::make_shared<QuantExt::BlackCPICashFlowPricer>(vol, discountCurve, useLastFixingDate);
        } else {
            return QuantLib::ext::make_shared<QuantExt::BachelierCPICashFlowPricer>(vol, discountCurve, useLastFixingDate);
        }
    }
};
} // namespace data
} // namespace ore
