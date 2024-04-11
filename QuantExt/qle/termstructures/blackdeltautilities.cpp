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

#include <qle/termstructures/blackdeltautilities.hpp>

#include <ql/experimental/fx/blackdeltacalculator.hpp>

namespace QuantExt {

Real getStrikeFromDelta(Option::Type optionType, Real delta, DeltaVolQuote::DeltaType dt, Real spot, Real domDiscount,
                        Real forDiscount, QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy,
                        Size maxIterations) {
    Real forward = spot / domDiscount * forDiscount;
    Real result = forward, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(vol->blackVariance(t, result));
        try {
            BlackDeltaCalculator bdc(optionType, dt, spot, domDiscount, forDiscount, stddev);
            lastResult = result;
            result = bdc.strikeFromDelta(delta);
        } catch (const std::exception& e) {
            QL_FAIL("getStrikeFromDelta("
                    << (optionType == Option::Call ? 1.0 : -1.0) * delta << ") could not be computed for spot=" << spot
                    << ", forward=" << spot / domDiscount * forDiscount << " (domRate=" << -std::log(domDiscount) / t
                    << ", forRate=" << -std::log(forDiscount) / t << "), vol=" << stddev / std::sqrt(t)
                    << ", expiry=" << t << ": " << e.what());
        }
    } while (std::abs((result - lastResult) / lastResult) > accuracy && ++iterations < maxIterations);
    QL_REQUIRE(iterations < maxIterations, "getStrikeFromDelta: max iterations ("
                                               << maxIterations << "), no solution found for accuracy " << accuracy
                                               << ", last iterations: " << lastResult << "/" << result
                                               << ", spot=" << spot << ", forward=" << spot / domDiscount * forDiscount
                                               << " (domRate=" << -std::log(domDiscount) / t
                                               << ", forRate=" << -std::log(forDiscount) / t << "), expiry=" << t);

    return result;
}

Real getAtmStrike(DeltaVolQuote::DeltaType dt, DeltaVolQuote::AtmType at, Real spot, Real domDiscount, Real forDiscount,
                  QuantLib::ext::shared_ptr<BlackVolTermStructure> vol, Real t, Real accuracy, Size maxIterations) {
    Real forward = spot / domDiscount * forDiscount;
    Real result = forward, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(vol->blackVariance(t, result));
        try {
            BlackDeltaCalculator bdc(Option::Call, dt, spot, domDiscount, forDiscount, stddev);
            lastResult = result;
            result = bdc.atmStrike(at);
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
