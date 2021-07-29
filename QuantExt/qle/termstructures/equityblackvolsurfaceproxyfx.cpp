/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/termstructures/equityblackvolsurfaceproxyfx.hpp>

namespace QuantExt {

EquityBlackVolatilitySurfaceProxyFx::EquityBlackVolatilitySurfaceProxyFx(
    const boost::shared_ptr<BlackVolTermStructure>& proxySurface, const boost::shared_ptr<EquityIndex>& index,
    const boost::shared_ptr<EquityIndex>& proxyIndex)
    : BlackVolatilityTermStructure(0, proxySurface->calendar(), proxySurface->businessDayConvention(),
        proxySurface->dayCounter()),
    proxySurface_(proxySurface), index_(index), proxyIndex_(proxyIndex) {

    if (proxySurface->allowsExtrapolation())
        this->enableExtrapolation();

    registerWith(proxySurface);
    registerWith(index);
    registerWith(proxyIndex);
}

Volatility EquityBlackVolatilitySurfaceProxyFx::blackVolImpl(Time t, Real strike) const {
    Real adjustedStrike = strike * proxyIndex_->forecastFixing(t) / index_->forecastFixing(t);
    return proxySurface_->blackVol(t, adjustedStrike);
}

Rate EquityBlackVolatilitySurfaceProxyFx::minStrike() const {
    return proxySurface_->minStrike() * index_->equitySpot()->value() / proxyIndex_->equitySpot()->value();
}

Rate EquityBlackVolatilitySurfaceProxyFx::maxStrike() const {
    return proxySurface_->maxStrike() * index_->equitySpot()->value() / proxyIndex_->equitySpot()->value();
}

} // namespace QuantExt
