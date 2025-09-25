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

#include <ql/experimental/fx/bachelierdeltacalculator.hpp>
#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <qle/termstructures/blackdeltautilities.hpp>

namespace QuantExt {

Real getStrikeFromDeltaBlack(Option::Type optionType, Real delta, DeltaVolQuote::DeltaType dt, Real spot,
                             Real domDiscount, Real forDiscount, QuantLib::ext::shared_ptr<BlackVolTermStructure> vol,
                             Real t, Real accuracy) {

    auto BlackTargetFct = [optionType, &delta, dt, &spot, &domDiscount, &forDiscount, &vol, &t](const Real x) {
        Real k = std::exp(x);
        Real computedDelta =
            BlackDeltaCalculator(optionType, dt, spot, domDiscount, forDiscount, std::sqrt(vol->blackVariance(t, k)))
                .deltaFromStrike(k);
        return computedDelta - delta;
    };

    auto BachelierTargetFct = [optionType, &delta, dt, &spot, &domDiscount, &forDiscount, &vol, &t](const Real x) {
        Real computedDelta = BachelierDeltaCalculator(optionType, dt, spot, domDiscount, forDiscount,
                                                      std::sqrt(vol->blackVariance(t, x)))
                                 .deltaFromStrike(x);
        return computedDelta - delta;
    };

    try {
        Real guess = std::log(BlackDeltaCalculator(optionType, dt, spot, domDiscount, forDiscount,
                                                   std::sqrt(vol->blackVariance(t, spot / domDiscount * forDiscount)))
                                  .strikeFromDelta(delta));
        Brent brent;

        auto targetFct = vol->volType() == VolatilityType::ShiftedLognormal ? BlackTargetFct : BachelierTargetFct;

        return std::exp(brent.solve(targetFct, accuracy, guess, 0.0001));
    } catch (const std::exception& e) {
        QL_FAIL("getStrikeFromDelta(" << (optionType == Option::Call ? 1.0 : -1.0) * delta
                                      << ") could not be computed for spot=" << spot << ", forward="
                                      << spot / domDiscount * forDiscount << " (domRate=" << -std::log(domDiscount) / t
                                      << ", forRate=" << -std::log(forDiscount) / t << ")");
    }
}

Real getStrikeFromDeltaBachelier(Option::Type optionType, Real delta, DeltaVolQuote::DeltaType dt, Real spot,
                                 Real domDiscount, Real forDiscount,
                                 QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy) {

    auto targetFct = [optionType, &delta, dt, &spot, &domDiscount, &forDiscount, &vol, &t](const Real k) {
        Real computedDelta = BachelierDeltaCalculator(optionType, dt, spot, domDiscount, forDiscount,
                                                      std::sqrt(vol->blackVariance(t, k)))
                                 .deltaFromStrike(k);
        return computedDelta - delta;
    };

    try {
        Real guess = BachelierDeltaCalculator(optionType, dt, spot, domDiscount, forDiscount,
                                              std::sqrt(vol->blackVariance(t, spot / domDiscount * forDiscount)))
                         .strikeFromDelta(delta);
        Brent brent;
        return brent.solve(targetFct, accuracy, guess, 0.0001);
    } catch (const std::exception& e) {
        QL_FAIL("getStrikeFromDeltaBachelier("
                << (optionType == Option::Call ? 1.0 : -1.0) * delta << ") could not be computed for spot=" << spot
                << ", forward=" << spot / domDiscount * forDiscount << " (domRate=" << -std::log(domDiscount) / t
                << ", forRate=" << -std::log(forDiscount) / t << ")");
    }
}

Real getStrikeFromDelta(Option::Type optionType, Real delta, DeltaVolQuote::DeltaType dt, Real spot, Real domDiscount,
                        Real forDiscount, QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy) {
    return vol->volType() == VolatilityType::ShiftedLognormal
               ? getStrikeFromDeltaBlack(optionType, delta, dt, spot, domDiscount, forDiscount, vol, t, accuracy)
               : getStrikeFromDeltaBachelier(optionType, delta, dt, spot, domDiscount, forDiscount, vol, t, accuracy);
}

Real getAtmStrike(DeltaVolQuote::DeltaType dt, DeltaVolQuote::AtmType at, Real spot, Real domDiscount, Real forDiscount,
                  QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy, Size maxIterations) {
    Real forward = spot / domDiscount * forDiscount;
    Real result = forward, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(vol->blackVariance(t, result));
        try {
            if(vol->volType() == VolatilityType::ShiftedLognormal) {
                BlackDeltaCalculator bdc(Option::Call, dt, spot, domDiscount, forDiscount, stddev);
                lastResult = result;
                result = bdc.atmStrike(at);
            } else {
                BachelierDeltaCalculator bdc(Option::Call, dt, spot, domDiscount, forDiscount, stddev);
                lastResult = result;
                result = bdc.atmStrike(at);
            }
        } catch (const std::exception& e) {
            QL_FAIL("getAtmStrike() could not be computed for spot="
                    << spot << ", forward=" << spot / domDiscount * forDiscount
                    << " (domRate=" << -std::log(domDiscount) / t << ", forRate=" << -std::log(forDiscount) / t
                    << "), vol=" << stddev / std::sqrt(t) << ", expiry=" << t << ": " << e.what());
        }
    } while (std::abs((result - lastResult) / lastResult) > accuracy && ++iterations < maxIterations);
    QL_REQUIRE(iterations < maxIterations, "getAtmStrike: max iterations ("
                                               << maxIterations << "), no solution found for accuracy " << accuracy
                                               << ", last iterations: " << lastResult << "/" << result
                                               << ", spot=" << spot << ", forward=" << spot / domDiscount * forDiscount
                                               << " (domRate=" << -std::log(domDiscount) / t
                                               << ", forRate=" << -std::log(forDiscount) / t << "), expiry=" << t);
    return result;
}

} // namespace QuantExt
