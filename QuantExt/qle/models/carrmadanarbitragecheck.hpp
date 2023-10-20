/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file models/carrmadanarbitragecheck.hpp
    \brief arbitrage checks based on Carr, Madan, A note on sufficient conditions for no arbitrage (2005)
    \ingroup models
*/

#pragma once

#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/types.hpp>

#include <string>
#include <vector>

namespace QuantExt {

using namespace QuantLib;

class CarrMadanMarginalProbability {
public:
    /*! The callPrices should be non-discounted */
    CarrMadanMarginalProbability(const std::vector<Real>& strikes, const Real forward,
                                 const std::vector<Real>& callPrices, const VolatilityType volType = ShiftedLognormal,
                                 const Real shift = 0.0);

    const std::vector<Real>& strikes() const;
    Real forward() const;
    const std::vector<Real>& callPrices() const;
    VolatilityType volatilityType() const;
    Real shift() const;

    bool arbitrageFree() const;

    const std::vector<bool>& callSpreadArbitrage() const;
    const std::vector<bool>& butterflyArbitrage() const;
    const std::vector<Real>& density() const;

private:
    std::vector<Real> strikes_;
    Real forward_;
    std::vector<Real> callPrices_;
    VolatilityType volType_;
    Real shift_;

    std::vector<bool> callSpreadArbitrage_, butterflyArbitrage_;
    std::vector<Real> q_;
    bool smileIsArbitrageFree_;
};

// accepts invalid forward and/or strikes (lt -shift) and performs the computation on the valid strikes only
class CarrMadanMarginalProbabilitySafeStrikes {
public:
    /*! The callPrices should be non-discounted */
    CarrMadanMarginalProbabilitySafeStrikes(const std::vector<Real>& strikes, const Real forward,
                                            const std::vector<Real>& callPrices,
                                            const VolatilityType volType = ShiftedLognormal, const Real shift = 0.0);

    const std::vector<Real>& strikes() const;
    Real forward() const;
    const std::vector<Real>& callPrices() const;
    VolatilityType volatilityType() const;
    Real shift() const;

    bool arbitrageFree() const;

    const std::vector<bool>& callSpreadArbitrage() const;
    const std::vector<bool>& butterflyArbitrage() const;
    const std::vector<Real>& density() const;

private:
    std::vector<Real> strikes_;
    Real forward_;
    std::vector<Real> callPrices_;
    VolatilityType volType_;
    Real shift_;

    std::vector<bool> validStrike_;

    std::vector<bool> callSpreadArbitrage_, butterflyArbitrage_;
    std::vector<Real> q_;
    bool smileIsArbitrageFree_;
};

class CarrMadanSurface {
public:
    /*! The moneyness is defined as K / F, K = strike, F = forward at the relevant time.
        The times and moneyness should be strictly increasing.
        The outer vectors for call prices and the calendarAbritrage() result represent times, the inner strikes. */
    CarrMadanSurface(const std::vector<Real>& times, const std::vector<Real>& moneyness, const Real spot,
                     const std::vector<Real>& forwards, const std::vector<std::vector<Real>>& callPrices);

    const std::vector<Real>& times() const;
    const std::vector<Real>& moneyness() const;
    Real spot() const;
    const std::vector<Real>& forwards() const;
    const std::vector<std::vector<Real>>& callPrices() const;

    bool arbitrageFree() const;

    const std::vector<CarrMadanMarginalProbability>& timeSlices() const;

    /* outer vector = times, length = number of result of times(),
       inner vector = strikes, length = number of strikes */
    const std::vector<std::vector<bool>>& callSpreadArbitrage() const;
    const std::vector<std::vector<bool>>& butterflyArbitrage() const;
    const std::vector<std::vector<bool>>& calendarArbitrage() const;

private:
    std::vector<Real> times_, moneyness_;
    Real spot_;
    std::vector<Real> forwards_;
    std::vector<std::vector<Real>> callPrices_;

    std::vector<CarrMadanMarginalProbability> timeSlices_;
    bool surfaceIsArbitrageFree_;
    std::vector<std::vector<bool>> callSpreadArbitrage_, butterflyArbitrage_, calendarArbitrage_;
};

template <class CarrMadanMarginalProbabilityClass>
std::string arbitrageAsString(const CarrMadanMarginalProbabilityClass& cm);

std::string arbitrageAsString(const CarrMadanSurface& cm);

} // namespace QuantExt
