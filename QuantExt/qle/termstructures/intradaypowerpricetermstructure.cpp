/*
 Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file qle/termstructures/pricetermstructure.cpp
    \brief Term structure of intraday power prices
*/

#include <qle/termstructures/intradaypowerpricetermstructure.hpp>
#include <qle/utilities/time.hpp>
#include <qle/utilities/intradaypower.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace QuantExt {

IntradayPowerPriceTermStructure::IntradayPowerPriceTermStructure(
    const QuantLib::Handle<PriceTermStructure>& underlying,
    const QuantLib::ext::shared_ptr<IntradayShapeTermstructure>& shape)
    : PriceTermStructure(underlying->referenceDate(), underlying->calendar(), underlying->dayCounter()),
      underlying_(underlying), shape_(shape) {
    QL_REQUIRE(!underlying_.empty(), "IntradayPowerPriceTermStructure: Underlying PriceTermStructure is empty");
    QL_REQUIRE(shape_ != nullptr,
               "IntradayPowerPriceTermStructure: Shape is required for IntradayPowerPriceTermStructure");
    registerWith(underlying_);
}

//! \name Prices
//@{
QuantLib::Real IntradayPowerPriceTermStructure::price(QuantLib::Time t, bool extrapolate) const {
    auto d = lowerDate(t, referenceDate(), dayCounter());
    return underlying_->price(t, extrapolate) * shape_->dayFactor(d);
}
QuantLib::Real IntradayPowerPriceTermStructure::price(const QuantLib::Date& d, bool extrapolate) const {
    return underlying_->price(d, extrapolate) * shape_->dayFactor(d);
}

QuantLib::Real
IntradayPowerPriceTermStructure::price(const QuantLib::Date& d,
                                       const QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadProfileWithMWh>& load,
                                       bool extrapolate) const {

    // No load given assume full day constant load
    if (load == nullptr) {
        return price(d, extrapolate);
    }

    auto underlyingPrice = underlying_->price(d, extrapolate);
    auto shapeFactor = shape_->loadWeightedIntradayShapeFactor(d, *load);
    return shapeFactor * underlyingPrice;
}

QuantLib::Real IntradayPowerPriceTermStructure::price(const QuantLib::Date& d, int deliveryStartTime,
                                                      int deliveryEndTime, bool isDSTextraHour,
                                                      bool extrapolate) const {
    std::vector<TotalLoadFactor> load;
    auto dstAdjustment = dayTimeSavingsAdjustment(d, shape_->daylightSavingsLocation());
    load.emplace_back(
        dstAdjustedTotalLoad(LoadFactor{deliveryStartTime, deliveryEndTime, 1.0, isDSTextraHour}, dstAdjustment));
    return price(d, QuantLib::ext::make_shared<IntradayPowerLoadProfileWithMWh>(load), extrapolate);
}

//@}

void IntradayPowerPriceTermStructure::update() { TermStructure::update(); }

} // namespace QuantExt
