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

/*! \file ored/portfolio/builders/equity.hpp
    \brief builder that returns a equity coupon pricer
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <qle/cashflows/equitycouponpricer.hpp>

namespace ore {
namespace data {
using namespace ore::data;

//! CouponPricer Builder for EquityLeg
/*! The coupon pricers are cached by equity name and currency
 \ingroup builders
 */
//! Base class for EquityCouponPricerBuilder
class EquityCouponPricerBuilderBase
    : public CachingEquityCouponPricerBuilder<string, const Currency&, const string&, const string&> {
public:
    EquityCouponPricerBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"EquityLeg"}) {}

protected:
    virtual string keyImpl(const Currency& ccy, const string& equityName, const string& fxIndex) override {
        return equityName;
    }
};

//! Stardard Equity Coupon Pricer Builder
class EquityCouponPricerBuilder : public EquityCouponPricerBuilderBase {
public:
    EquityCouponPricerBuilder() : EquityCouponPricerBuilderBase("DiscountedCashflows", "EquityCouponPricer") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::EquityCouponPricer> engineImpl(const Currency& ccy, const string& equityName,
                                                                       const string& fxIndex) override;
};

//! Black Quanto Equity Coupon Pricer Builder for Convexity Adjustment
class BlackQuantoEquityCouponPricerBuilder : public EquityCouponPricerBuilderBase {
public:
    BlackQuantoEquityCouponPricerBuilder()
        : EquityCouponPricerBuilderBase("BlackQuantoAdjusted", "BlackQuantoEquityCouponPricer") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::EquityCouponPricer> engineImpl(const Currency& ccy, const string& equityName,
                                                                       const string& fxIndex) override;

private:
    void parseEngineParameters();

    mutable bool parametersInitialised_ = false;
    mutable bool applyConvexityAdjustment_ = false;
    mutable std::set<std::string> quantoCurrencyPairs_;
};

} // namespace data
} // namespace ore
