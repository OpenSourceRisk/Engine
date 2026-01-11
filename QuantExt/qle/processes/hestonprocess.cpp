/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/processes/hestonprocess.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/math/integrals/gaussianquadratures.hpp>
#include <ql/math/integrals/segmentintegral.hpp>

using namespace QuantLib;

namespace QuantExt {
using namespace QuantLib;

class PDFIntegrand {
public:
    PDFIntegrand(Real x, Time time, const ExtendedHestonProcess& p) : x_(x), time_(time), p_(p) {}
    Real operator()(Real u) const { return p_.pdfIntegrand(u, x_, time_); }

private:
    Real x_, time_;
    ExtendedHestonProcess p_;
};

class CDFIntegrand {
public:
    CDFIntegrand(Real x, Time time, const ExtendedHestonProcess& p) : x_(x), time_(time), p_(p) {}
    Real operator()(Real u) const { return p_.cdfIntegrand(u, x_, time_); }

private:
    Real x_, time_;
    ExtendedHestonProcess p_;
};

Real ExtendedHestonProcess::pdfIntegrand(Real u, Real x, Real time) const {
    Complex i(0.0, 1.0);
    return std::real(std::exp(-i * u * x) * phi(u, time));
}

Real ExtendedHestonProcess::cdfIntegrand(Real u, Real x, Real time) const {
    Complex i(0.0, 1.0);
    return std::imag(std::exp(-i * u * x) * phi(u, time)) / u;
}

Real ExtendedHestonProcess::pdf(Real x, Time time, Size order) const {
    PDFIntegrand func(x, time, *this);
    GaussLaguerreIntegration integrator(order);
    return integrator(func) / M_PI;
}

Real ExtendedHestonProcess::cdf(Real x, Time time, Size order) const {
    CDFIntegrand func(x, time, *this);
    GaussLaguerreIntegration integrator(order);
    return 0.5 - integrator(func) / M_PI;
}

Complex ExtendedHestonProcess::phi(Real u, Real time) const {
    Real spot = s0()->value();
    Real forward = spot * dividendYield()->discount(time) / riskFreeRate()->discount(time);
    Real mu = std::log(forward / spot) / time;

    Real k = kappa();
    Real t = theta();
    Real s = sigma();
    Real s2 = s*s;
    Real r = rho();

    Complex i(0.0, 1.0);
    Complex xi = k - i * s * r * u;
    Complex d = std::sqrt(xi * xi + s2 * (u*u + i * u));
    Complex g1 = (xi + d) / (xi - d);
    Complex g2 = 1.0 / g1;
    Complex cf = std::exp(i * u * mu * time
			  + (k * t) / s2 * ((xi - d) * time - 2.0 * std::log((1.0 - g2 * std::exp(-d * time)) / (1.0 - g2)))
			  + v0() / s2 * (xi - d) * (1.0 - std::exp(-d * time)) / (1.0 - g2 * std::exp(-d * time)));
    return cf;
}

} // namespace QuantExt
