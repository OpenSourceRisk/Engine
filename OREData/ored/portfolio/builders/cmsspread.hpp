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

/*! \file ored/portfolio/builders/cmsspread.hpp
    \brief builder that returns a cms spread coupon pricer
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <qle/cashflows/lognormalcmsspreadpricer.hpp>

namespace ore {
namespace data {
using namespace ore::data;

//! CouponPricer Builder for CmsSpreadLeg
/*! The coupon pricers are cached by currency
 \ingroup builders
 */
class CmsSpreadCouponPricerBuilder
    : public CachingCouponPricerBuilder<string, const Currency&, const string&, const string&,
                                        const QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>&> {
public:
    CmsSpreadCouponPricerBuilder() : CachingEngineBuilder("BrigoMercurio", "Analytic", {"CMSSpread"}) {}

protected:
    string keyImpl(const Currency& ccy, const string& index1, const string& index2,
                   const QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>& cmsPricer) override {

        return index1 + ":" + index2;
    }
    QuantLib::ext::shared_ptr<FloatingRateCouponPricer>
    engineImpl(const Currency& ccy, const string& index1, const string& index2,
               const QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>& cmsPricer) override;
};

} // namespace data
} // namespace ore
