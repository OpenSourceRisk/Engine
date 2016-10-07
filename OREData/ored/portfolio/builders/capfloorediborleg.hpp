/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/portfolio/builders/capfloorediborleg.hpp
    \brief builder that returns an engine to price capped floored ibor legs
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>

#include <ql/cashflows/couponpricer.hpp>

namespace openriskengine {
namespace data {

//! CouponPricer Builder for CapFlooredIborLeg
/*! The coupon pricers are cached by currency
 */
class CapFlooredIborLegEngineBuilder : public CachingCouponPricerBuilder<string, const Currency&> {
public:
    CapFlooredIborLegEngineBuilder() : CachingEngineBuilder("BlackOrBachelier", "BlackIborCouponPricer") {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }
    virtual boost::shared_ptr<FloatingRateCouponPricer> engineImpl(const Currency& ccy) override;
};
}
}
