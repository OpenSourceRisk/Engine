/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/termstructures/blackvolsurfaceproxy.hpp>

namespace QuantExt {

BlackVolatilitySurfaceProxy::BlackVolatilitySurfaceProxy(const boost::shared_ptr<BlackVolTermStructure>& proxySurface,
    const Handle<Quote>& spot, const Handle<Quote>& proxySpot)
    : BlackVolatilityTermStructure(0, proxySurface->calendar(), proxySurface->businessDayConvention(), proxySurface->dayCounter()),
    proxySurface_(proxySurface), spot_(spot), proxySpot_(proxySpot) {

    QL_REQUIRE(!spot.empty(), "No spot handle provided");
    QL_REQUIRE(!proxySpot.empty(), "No proxy spot handle provided");

    if (proxySurface->allowsExtrapolation())
        this->enableExtrapolation();

    registerWith(proxySurface);
    registerWith(spot);
    registerWith(proxySpot);
}

Volatility BlackVolatilitySurfaceProxy::blackVolImpl(Time t, Real strike) const {
    Real adjustedStrike = strike * proxySpot_->value() / spot_->value();
    return proxySurface_->blackVol(t, adjustedStrike);
}

} // namespace QuantExt
