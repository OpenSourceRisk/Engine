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
    \brief builder that provides parameters for range accrual leg pricers
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace ore {
namespace data {

//! Engine Builder for RangeAccrualLeg using BGM model with swaption vols
/*! Provides the OptionletVolatilityStructure and pricer parameters.
    Per-coupon pricers are created in makeRangeAccrualLeg using the vol surface
    to build smile sections at each coupon's expiry and payment dates.
    \ingroup builders
*/
class RangeAccrualLegEngineBuilder
    : public CachingCouponPricerBuilder<std::string, const std::string&, const QuantLib::Date&,
                                        const QuantLib::Date&> {
public:
    RangeAccrualLegEngineBuilder()
        : CachingEngineBuilder("BGM", "FloatingRateCouponPricer", {"IborRangeAccrualLeg"}) {}

protected:
    std::string keyImpl(const std::string& index, const QuantLib::Date& accrualStartDate,
                        const QuantLib::Date& accrualEndDate) override {
        return index + "/" + ore::data::to_string(accrualStartDate) + "/" + ore::data::to_string(accrualEndDate);
    }

    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> engineImpl(const std::string& index, const QuantLib::Date& accrualStartDate,
               const QuantLib::Date& accrualEndDate) override;
};

//! Engine Builder for RangeAccrualLeg using call-spread replication on optionlet vols
/*! Uses the capFloor (optionlet) volatility surface directly.
    Supports both Black (shifted-lognormal) and Bachelier (normal) vol types.
    This is the same replication approach used by the RateDigitalOption trade type.
    \ingroup builders
*/
class RangeAccrualLegCallSpreadEngineBuilder
    : public CachingCouponPricerBuilder<std::string, const std::string&> {
public:
    RangeAccrualLegCallSpreadEngineBuilder()
        : CachingEngineBuilder("Black", "CallSpreadCouponPricer", {"IborRangeAccrualLeg"}) {}

protected:
    std::string keyImpl(const std::string& index) override { return index; }

    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> engineImpl(const std::string& index) override;
};

} // namespace data
} // namespace ore
