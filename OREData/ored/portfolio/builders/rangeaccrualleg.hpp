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

#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace ore {
namespace data {

//! Engine Builder for RangeAccrualLeg
/*! Provides the OptionletVolatilityStructure and pricer parameters.
    Per-coupon pricers are created in makeRangeAccrualLeg using the vol surface
    to build smile sections at each coupon's expiry and payment dates.
    \ingroup builders
*/
class RangeAccrualLegEngineBuilder : public EngineBuilder {
public:
    RangeAccrualLegEngineBuilder()
        : EngineBuilder("BGM", "FloatingRateCouponPricer", {"IborRangeAccrualLeg"}) {}

    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> buildPricer(
        const std::string& index, const Date& accrualStartDate, const Date& accrualEndDate);

private:
    Handle<SwaptionVolatilityStructure> swaptionVolatilityStructure(const std::string& index);
    Real correlation(const std::string& index);
    bool withSmile(const std::string& index);
    bool byCallSpread(const std::string& index);
    Real flatVol(const std::string& index);
};

} // namespace data
} // namespace ore
