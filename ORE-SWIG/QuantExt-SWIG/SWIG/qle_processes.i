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

#ifndef qle_processes_i
#define qle_processes_i

%include date.i
%include types.i
%include calendars.i
%include daycounters.i
%include indexes.i
%include termstructures.i
%include scheduler.i
%include vectors.i
%include cashflows.i
%include qle_indexes.i

%shared_ptr(QuantExt::CrossAssetModel)
namespace QuantExt {
class CrossAssetModel;
}

%shared_ptr(QuantExt::ExtendedHestonProcess)
namespace QuantExt {
class ExtendedHestonProcess : public HestonProcess {
public:
    ExtendedHestonProcess(Handle<YieldTermStructure> riskFreeRate,
			  Handle<YieldTermStructure> dividendYield,
			  Handle<Quote> s0,
			  Real v0,
			  Real kappa,
			  Real theta,
			  Real sigma,
			  Real rho,
			  Discretization d = QuadraticExponentialMartingale);

    // Heston Characteristic function phi(u) for the random variable x=ln(S)
    // See equation (2) in https://perswww.kuleuven.be/~u0009713/HestonTrap.pdf
    std::complex<Real> phi(Real u, Real time) const;

    // Gil-Pelaez formula with numerical integration using Gauss-Laguerre
    // QuantLib's AnalyticHestonEngine uses Gauss-Laguerre integration
    // with order = 128 by default and requires order <= 192
    Real pdf(Real x, Time time, Size order) const;
    Real cdf(Real x, Time time, Size order) const;

    // Integrand for the integration above
    Real pdfIntegrand(Real u, Real x, Real time) const;
    Real cdfIntegrand(Real u, Real x, Real time) const;
};
}

%shared_ptr(QuantExt::CrossAssetStateProcess)
%nodefaultctor QuantExt::CrossAssetStateProcess;
namespace QuantExt {
class CrossAssetStateProcess : public StochasticProcess {
public:
    Size size() const override;
    Size factors() const override;
    Array initialValues() const override;
    Array drift(Time t, const Array& x) const override;
    Matrix diffusion(Time t, const Array& x) const override;
    Array evolve(Time t0, const Array& x0, Time dt, const Array& dw) const override;
    void resetCache(const Size timeSteps) const;
};
}

%shared_ptr(QuantExt::IrLgm1fStateProcess)
namespace QuantExt {
class IrLgm1fStateProcess : public StochasticProcess1D {
public:
    IrLgm1fStateProcess(const QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization>& parametrization);
    Real x0() const override;
    Real drift(Time t, Real x) const override;
    Real diffusion(Time t, Real x) const override;
    Real expectation(Time t0, Real x0, Time dt) const override;
    Real stdDeviation(Time t0, Real x0, Time dt) const override;
    Real variance(Time t0, Real x0, Time dt) const override;
    void resetCache(const Size timeSteps) const;
};
}

#endif
