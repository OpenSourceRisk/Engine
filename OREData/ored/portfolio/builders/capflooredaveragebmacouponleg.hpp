/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/capflooredaveragebmacouponleg.hpp
    \brief builder that returns an engine to price capped floored avg BMA legs
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <ql/cashflows/couponpricer.hpp>

namespace ore {
namespace data {

//! CouponPricer Builder for CapFlooredAVerageBMACouponLeg
/*! The coupon pricers are cached by index / rate comp period
 \ingroup builders
 */
class CapFlooredAverageBMACouponLegEngineBuilder
    : public CachingCouponPricerBuilder<string, const std::string&, const QuantLib::Period&> {
public:
    CapFlooredAverageBMACouponLegEngineBuilder()
        : CachingEngineBuilder("BlackOrBachelier", "BlackAverageBMACouponPricer", {"CapFlooredAverageBMACouponLeg"}) {}

protected:
    string keyImpl(const string& index, const QuantLib::Period& rateComputationPeriod) override;
    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> engineImpl(const string& index,
                                                           const QuantLib::Period& rateComputationPeriod) override;
};
} // namespace data
} // namespace ore
