/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/cms.hpp
    \brief builder that returns an engine to price capped floored ibor legs
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <ql/cashflows/conundrumpricer.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>

namespace ore {
namespace data {
using namespace ore::data;

//! CouponPricer Builder for CmsLeg
/*! The coupon pricers are cached by currency
 \ingroup builders
 */
class CmsCouponPricerBuilder : public CachingCouponPricerBuilder<string, const string&> {
public:
    CmsCouponPricerBuilder(const string& model, const string& engine) : CachingEngineBuilder(model, engine, {"CMS"}) {}

protected:
    virtual string keyImpl(const string& key) override { return key; }
};

class AnalyticHaganCmsCouponPricerBuilder : public CmsCouponPricerBuilder {
public:
    AnalyticHaganCmsCouponPricerBuilder() : CmsCouponPricerBuilder("Hagan", "Analytic") {}

protected:
    virtual QuantLib::ext::shared_ptr<FloatingRateCouponPricer> engineImpl(const string& key) override;
};

class NumericalHaganCmsCouponPricerBuilder : public CmsCouponPricerBuilder {
public:
    NumericalHaganCmsCouponPricerBuilder() : CmsCouponPricerBuilder("Hagan", "Numerical") {}

protected:
    virtual QuantLib::ext::shared_ptr<FloatingRateCouponPricer> engineImpl(const string& key) override;
};

class LinearTSRCmsCouponPricerBuilder : public CmsCouponPricerBuilder {
public:
    LinearTSRCmsCouponPricerBuilder() : CmsCouponPricerBuilder("LinearTSR", "LinearTSRPricer") {}

protected:
    virtual QuantLib::ext::shared_ptr<FloatingRateCouponPricer> engineImpl(const string& key) override;
};
} // namespace data
} // namespace ore
