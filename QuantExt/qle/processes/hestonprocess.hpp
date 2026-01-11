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

/*! \file hestonprocess.hpp
    \brief Extended Heston stochastic process
*/

#pragma once

#include <ql/processes/hestonprocess.hpp>

#include <complex>

typedef std::complex<QuantLib::Real> Complex;

namespace QuantExt {

using namespace QuantLib;
  
//! Extended QuantLib Heston process
/*! This class provides the Heston probability density function (PDF) and cumulative density (CDF)
    as a function of x=ln(S), i.e. integrated across variance states.

    The PDF/CDF is computed by Fourier inversion (Gil-Pelaez formula) of Schouten's stable Heston
    characteristic function phi(u):

    pdf(x) = 1/pi * \int_0^\infty Re[ exp(-i*u*x) * phi(u)] du,
    cdf(x) = 1/2 - 1/pi * \int_0^\infty 1/u * Im[ exp(-i*u*x) * phi(u)] du
    
    The numerical integration is done using Gauss-Laguerre, following
    one of QuantLib's option pricing approaches in AnalyticHestonEngine, in particular the
    default approach where the engine constructor takes the HestonModel as its only argument.
    
    Reference:
    H. Albrecher, P. Mayer, W. Schoutens and J. Tistaert,
    The Little Heston Trap
    https://perswww.kuleuven.be/~u0009713/HestonTrap.pdf
    Equation (2)

    \ingroup processes
*/
class ExtendedHestonProcess : public QuantLib::HestonProcess {
public:
    ExtendedHestonProcess(Handle<YieldTermStructure> riskFreeRate, Handle<YieldTermStructure> dividendYield,
                          Handle<Quote> s0, Real v0, Real kappa, Real theta, Real sigma, Real rho,
                          Discretization d = QuadraticExponentialMartingale)
        : HestonProcess(riskFreeRate, dividendYield, s0, v0, kappa, theta, sigma, rho, d) {}

    // Heston Characteristic function phi(u) for the random variable x=ln(S)
    // See equation (2) in https://perswww.kuleuven.be/~u0009713/HestonTrap.pdf
    Complex phi(Real u, Real time) const;

    // Gil-Pelaez formula with numerical integration using Gauss-Laguerre 
    // QuantLib's AnalyticHestonEngine uses Gauss-Laguerre integration
    // with order = 128 by default and requires order <= 192
    Real pdf(Real x, Time time, Size order) const;
    Real cdf(Real x, Time time, Size order) const;

    // Integrands for the integrations above
    Real pdfIntegrand(Real u, Real x, Real time) const;
    Real cdfIntegrand(Real u, Real x, Real time) const;
};

} // namespace QuantExt
