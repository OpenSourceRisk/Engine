/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/rangeaccrualleg.hpp
    \brief builder that returns a pricer for range accrual legs
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <ql/cashflows/couponpricer.hpp>

namespace ore {
namespace data {

//! CouponPricer Builder for RangeAccrualLeg
/*! The coupon pricers are cached by index name
    \ingroup builders
*/
class RangeAccrualLegEngineBuilder : public CachingCouponPricerBuilder<string, const string&> {
public:
    RangeAccrualLegEngineBuilder()
        : CachingEngineBuilder("BlackOrBachelier", "RangeAccrualPricer", {"RangeAccrualLeg"}) {}

protected:
    string keyImpl(const string& index) override { return index; }
    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> engineImpl(const string& index) override;
};

} // namespace data
} // namespace ore
