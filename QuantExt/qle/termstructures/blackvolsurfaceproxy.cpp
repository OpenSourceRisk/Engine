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

using namespace QuantLib;

namespace QuantExt {

BlackVolatilitySurfaceProxy::BlackVolatilitySurfaceProxy(
    const QuantLib::ext::shared_ptr<BlackVolTermStructure>& proxySurface, const QuantLib::ext::shared_ptr<EqFxIndexBase>& index,
    const QuantLib::ext::shared_ptr<EqFxIndexBase>& proxyIndex, const QuantLib::ext::shared_ptr<BlackVolTermStructure>& fxSurface,
    const QuantLib::ext::shared_ptr<FxIndex>& fxIndex, const QuantLib::ext::shared_ptr<CorrelationTermStructure>& correlation)
    : BlackVolatilityTermStructure(0, proxySurface->calendar(), proxySurface->businessDayConvention(),
                                   proxySurface->dayCounter()),
      proxySurface_(proxySurface), index_(index), proxyIndex_(proxyIndex), fxSurface_(fxSurface), 
      fxIndex_(fxIndex), correlation_(correlation) {

    if (proxySurface->allowsExtrapolation())
        this->enableExtrapolation();

    registerWith(proxySurface);
    registerWith(index);
    registerWith(proxyIndex);
}

Volatility BlackVolatilitySurfaceProxy::blackVolImpl(Time t, Real strike) const {

    t = std::max(t, 1E-6);

    Volatility vol;
    if (fxSurface_) {
        // Get the Fx forward value at time t, and hence the ATM vol
        Real fxForward = fxIndex_->forecastFixing(t);
        Volatility fxVol = fxSurface_->blackVol(t, fxForward);

        // Get the ATM vol for the proxy surface
        Volatility proxyAtmVol = proxySurface_->blackVol(t, proxyIndex_->forecastFixing(t));
        
        // Convert the atm vol for this surface using sqrt( sigma^2 + sigma_X^2 + 2 rho sigma sigma_X)
        Volatility atmVol = sqrt(proxyAtmVol * proxyAtmVol + fxVol * fxVol + 2 * correlation_->correlation(t) * proxyAtmVol * fxVol);

        // Get the moneyness in this surface of the strike requested
        Real forward = index_->forecastFixing(t);
        Real moneyness = std::log(strike / forward) / (atmVol * sqrt(t));

        // Find the strike in the proxy surface that gives the same moneyness
        Real proxyForward = proxyIndex_->forecastFixing(t);
        Real proxyStrike = proxyForward * exp(moneyness * proxyAtmVol * sqrt(t));

        // Look up the vol in the proxy surface for the proxyStrike and convert using sqrt( sigma^2 + sigma_X^2 + 2 rho sigma sigma_X)
        Volatility proxyVol = proxySurface_->blackVol(t, proxyStrike);        
        vol = sqrt(proxyVol * proxyVol + fxVol * fxVol + 2 * correlation_->correlation(t) * proxyVol * fxVol);
        
    } else {
        Real adjustedStrike = strike * proxyIndex_->forecastFixing(t) / index_->forecastFixing(t);
        vol = proxySurface_->blackVol(t, adjustedStrike);
    } 
    return vol;
}

Rate BlackVolatilitySurfaceProxy::minStrike() const {
    return proxySurface_->minStrike() * index_->forecastFixing(0.0) / proxyIndex_->forecastFixing(0.0);
}

Rate BlackVolatilitySurfaceProxy::maxStrike() const {
    return proxySurface_->maxStrike() * index_->forecastFixing(0.0) / proxyIndex_->forecastFixing(0.0);
}

} // namespace QuantExt
