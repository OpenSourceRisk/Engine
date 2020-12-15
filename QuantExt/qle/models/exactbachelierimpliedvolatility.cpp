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

#include <qle/models/exactbachelierimpliedvolatility.hpp>

#include <ql/math/comparison.hpp>

#include <boost/math/distributions/normal.hpp>

namespace QuantExt {

using namespace QuantLib;

namespace {

static boost::math::normal_distribution<double> normal_dist;
Real phi(const Real x) { return boost::math::pdf(normal_dist, x); }
Real Phi(const Real x) { return boost::math::cdf(normal_dist, x); }

Real PhiTilde(const Real x) { return Phi(x) + phi(x) / x; }

Real inversePhiTilde(const Real PhiTildeStar) {
    QL_REQUIRE(PhiTildeStar < 0.0, "inversePhiTilde(" << PhiTildeStar << "): negative argument required");
    Real xbar;
    if (PhiTildeStar < -0.001882039271) {
        Real g = 1.0 / (PhiTildeStar - 0.5);
        Real xibar = (0.032114372355 - g * g * (0.016969777977 - g * g * (2.6207332461E-3 - 9.6066952861E-5 * g * g))) /
                     (1.0 - g * g * (0.6635646938 - g * g * (0.14528712196 - 0.010472855461 * g * g)));
        xbar = g * (0.3989422804014326 + xibar * g * g);
    } else {
        Real h = std::sqrt(-std::log(-PhiTildeStar));
        xbar = (9.4883409779 - h * (9.6320903635 - h * (0.58556997323 + 2.1464093351 * h))) /
               (1.0 - h * (0.65174820867 + h * (1.5120247828 + 6.6437847132E-5 * h)));
    }
    Real q = (PhiTilde(xbar) - PhiTildeStar) / phi(xbar);
    Real xstar =
        xbar + 3.0 * q * xbar * xbar * (2.0 - q * xbar * (2.0 + xbar * xbar)) /
                   (6.0 + q * xbar * (-12.0 + xbar * (6.0 * q + xbar * (-6.0 + q * xbar * (3.0 + xbar * xbar)))));
    return xstar;
}

} // namespace

Real exactBachelierImpliedVolatility(Option::Type optionType, Real strike, Real forward, Real tte, Real bachelierPrice,
                                     Real discount) {

    Real theta = optionType == Option::Call ? 1.0 : -1.0;

    // compound bechelierPrice, so that effectively discount = 1

    bachelierPrice /= discount;

    // handle case strike = forward

    if (std::abs(strike - forward) < 1E-15) {
        return bachelierPrice / (std::sqrt(tte) * phi(0.0));
    }

    // handle case strike != forward

    Real timeValue = bachelierPrice - std::max(theta * (forward - strike), 0.0);

    if (std::abs(timeValue) < 1E-15)
        return 0.0;

    QL_REQUIRE(timeValue > 0.0, "exactBachelierImpliedVolatility(theta="
                                    << theta << ",strike=" << strike << ",forward=" << forward << ",tte=" << tte
                                    << ",price=" << bachelierPrice << "): option price implies negative time value ("
                                    << timeValue << ")");

    Real PhiTildeStar = -std::abs(timeValue / (strike - forward));
    Real xstar = inversePhiTilde(PhiTildeStar);
    Real impliedVol = std::abs((strike - forward) / (xstar * std::sqrt(tte)));

    return impliedVol;
}

} // namespace QuantExt
