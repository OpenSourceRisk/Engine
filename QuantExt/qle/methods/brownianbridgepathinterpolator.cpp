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

#include <qle/methods/brownianbridgepathinterpolator.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {
RandomVariable createNewVariate(const Size p, const MersenneTwisterUniformRng& mt) {
    InverseCumulativeNormal icn;
    RandomVariable r(p);
    for (Size i = 0; i < p; ++i) {
        r.set(i, icn(mt.nextReal()));
    }
    return r;
}
} // namespace

void interpolateVariatesWithBrownianBridge(const std::vector<QuantLib::Real>& times,
                                           std::vector<std::vector<QuantExt::RandomVariable>>& variates,
                                           const Size seed) {

    // check inputs

    for (Size i = 1; i < times.size(); ++i) {
        QL_REQUIRE(times[i] > times[i - 1] && !QuantLib::close_enough(times[i], times[i - 1]),
                   "interpolateVariatesWithBrownianBridge():: times must be ascending, got "
                       << times[i - 1] << " at " << i - 1 << " and " << times[i] << " at " << i);
    }

    QL_REQUIRE(variates.size() == times.size(), "interpolateVariatesWithBrownianBridge(): variates vector size ("
                                                    << variates.size() << ") must match times vector size ("
                                                    << times.size() << ")");

    Size d = Null<Size>(), p = Null<Size>();
    for (Size i = 0; i < times.size(); ++i) {
        if (variates[i].empty())
            continue;
        if (d == Null<Size>()) {
            d = variates[i].size();
            p = variates[i].front().size();
            QL_REQUIRE(p > 0,
                       "interpolatedVariatesWithBrownianBridge(): found RandomVariable of size 0 at time step " << i);
        } else {
            QL_REQUIRE(variates[i].size() == d,
                       "interpolateVariatesWithBrownianBridge(): variates dimension at time step "
                           << i << " (" << variates[i].size() << ") is expected to be " << d);
            for (Size j = 0; j < d; ++j) {
                QL_REQUIRE(variates[i][j].size() == p, "interpolateVariatesWithBrownianBridge(): variate at time step "
                                                           << i << " dimension " << j << " has "
                                                           << variates[i][j].size() << " paths, expected " << p);
            }
        }
    }

    QL_REQUIRE(d != Null<Size>(),
               "interpolateVariatesWithBrownianBridge(): expected at least one time step with non-empty variate");

    // we construct a Wiener process W(t) from the initially given variates by adding the given variates up
    // on a unit distance time grid 1, 2, 3, 4, ... which we call the "standardised time grid".

    std::vector<std::vector<QuantExt::RandomVariable>> paths(times.size(), std::vector<QuantExt::RandomVariable>(d));
    std::vector<Real> standardisedTimes(times.size(), Null<Real>());

    Size counter = 0;
    Size lastTimeStep = Null<Size>();

    std::vector<bool> initialised(times.size(), false);
    for (Size i = 0; i < times.size(); ++i) {
        if (variates[i].empty())
            continue;
        standardisedTimes[i] = static_cast<Real>(++counter);
        for (Size j = 0; j < d; ++j) {
            paths[i][j] = lastTimeStep == Null<Size>() ? variates[i][j] : paths[lastTimeStep][j] + variates[i][j];
        }
        lastTimeStep = i;
        initialised[i] = true;
    }

    // next we fill in the missing times and create interpolated variates using a Brownian bridge, this will generate
    // intermediate times on the unit distance grid from above as a scaled version of the original times

    MersenneTwisterUniformRng mt(seed);

    for (Size i = 0; i < times.size(); ++i) {
        if (initialised[i])
            continue;
        // we interpolate between time indices l and r
        Size l = i + 1, r = i;
        while (l > 0 && variates[--l].empty())
            ;
        while (r < variates.size() - 1 && variates[++r].empty())
            ;
        if (variates[r].empty()) {
            // if there is no right point to interpolate we just continue the path with new variates ...
            QL_REQUIRE(!variates[l].empty(),
                       "interpolateVariatesWithBrownianBridge(): internal error, expected variates["
                           << l << "] to be non-empty");
            for (Size k = l + 1; k <= r; ++k) {
                for (Size j = 0; j < d; ++j) {
                    paths[k][j] = paths[k - 1][j] + createNewVariate(p, mt);
                }
                standardisedTimes[k] = standardisedTimes[k - 1] + 1.0;
                initialised[k] = true;
            }
        } else {
            // ... otherwise we interpolate using a Brownian bridge, this relies on Theorem 14.2 in
            // Mark Joshi, "More Mathematical Finance", Pilot Whale Press
            RandomVariable t, T;
            // compute the standardised times first
            if (variates[l].empty()) {
                QL_REQUIRE(l == 0, "interpolateVariatesWithBrownianBridge(): internal error, expected l==0, got " << l);
                for (Size k = l; k < r; ++k) {
                    standardisedTimes[k] = times[k] / times[r];
                }
            } else {
                for (Size k = l; k < r; ++k) {
                    standardisedTimes[k] = standardisedTimes[l] + (times[k] - times[l]) / (times[r] - times[l]);
                }
            }
            // now interpolate the path
            for (Size k = l; k < r; ++k) {
                if (variates[l].empty() && k == l) {
                    // interpolate between 0 and the first non-empty point
                    t = RandomVariable(p, standardisedTimes[k]);
                    T = RandomVariable(p, standardisedTimes[r]);
                } else if (k > l) {
                    // interpolate between two non-empty points
                    t = RandomVariable(p, standardisedTimes[k] - standardisedTimes[k - 1]);
                    T = RandomVariable(p, standardisedTimes[r] - standardisedTimes[k - 1]);
                } else {
                    // k=l and variates[l] non empty => nothing to do
                    continue;
                }
                RandomVariable stdDev = sqrt(t * (T - t) / T);
                for (Size j = 0; j < d; ++j) {
                    RandomVariable expectedValue =
                        ((T - t) * (k == l ? RandomVariable(p, 0.0) : paths[k - 1][j]) + t * paths[r][j]) / T;
                    paths[k][j] = expectedValue + stdDev * createNewVariate(p, mt);
                }
                initialised[k] = true;
            }
        }
    }

    // finally we calcuate the sequenctial differences on the paths and rescale them to a unit distance grid
    // in order to get the desired N(0,1) variates on the refined time grid

    for (Size i = 0; i < times.size(); ++i) {
        RandomVariable scaling(p, std::sqrt(1.0 / (standardisedTimes[i] - (i == 0 ? 0.0 : standardisedTimes[i - 1]))));
        if (variates[i].empty())
            variates[i] = std::vector<RandomVariable>(d, RandomVariable(p));
        for (Size j = 0; j < d; ++j) {
            variates[i][j] = (i == 0 ? paths[i][j] : paths[i][j] - paths[i - 1][j]) * scaling;
        }
    }

    // results are set in variates, exit without returning anything

    return;
}

} // namespace QuantExt
