/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file kienitzlawsonswaynesabrpdedensity.hpp
    \brief Adaption of VBA code by Jörg Kienitz, 2017, to create a SABR density with PDE methods

    \ingroup termstructures
*/

#pragma once

#include <ql/types.hpp>
#include <ql/math/array.hpp>

namespace QuantExt {

using QuantLib::Real;
using QuantLib::Size;
using QuantLib::Array;

class KienitzLawsonSwayneSabrPdeDensity {
public:
    KienitzLawsonSwayneSabrPdeDensity(const Real alpha, const Real beta, const Real nu, const Real rho,
                                      const Real forward, const Real expiryTime, const Real displacement,
                                      const Size zSteps, const Size tSteps, const Real nStdDev);

    // inspectors
    Real alpha() const { return alpha_; }
    Real beta() const { return beta_; }
    Real nu() const { return nu_; }
    Real rho() const { return rho_; }
    Real forward() const { return forward_; }
    Real expiryTime() const { return expiryTime_; }
    Real displacement() const { return displacement_; }
    Size zSteps() const { return zSteps_; }
    Size tSteps() const { return tSteps_; }
    Real nStdDev() const { return nStdDev_; }

    // vanilla prices
    std::vector<Real> callPrices(const std::vector<Real>& strikes) const;
    std::vector<Real> putPrices(const std::vector<Real>& strikes) const;

    // strike at which a given cumulative probability is hit
    Real inverseCumulative(const Real p) const;

    // results
    const Array& sabrProb() const { return SABR_Prob_Vec_; }
    Real zMin() const { return zMin_; }
    Real zMax() const { return zMax_; }
    Real hh() const { return hh_; }
    Real pL() const { return pL_; }
    Real pR() const { return pR_; }
    Real theta0() const { return theta0_; }

    // variable transformations
    Real yf(const Real f) const;
    Real fy(const Real y) const;
    Real yz(const Real z) const;
    Real zy(const Real y) const;

private:
    void calculate();
    // VBA helper functions
    void PDE_method(const Array& fm, const Array& Ccm, const Array& Gamma_Vec, const Real dt, const Size Nj);
    void solveTimeStep_LS(const Array& fm, const Array& cm, const Array& Em, const Real dt, const Array& PP_in,
                          const Real PL_in, const Real PR_in, const Size n, Array& PP_out, Real& PL_out, Real& PR_out);
    void tridag(const Array& a, const Array& b, const Array& c, const Array& R, Array& u, const Size n,
                const bool firstLastRZero);
    // inputs
    const Real alpha_, beta_, nu_, rho_, forward_, expiryTime_, displacement_;
    const Size zSteps_, tSteps_;
    const Real nStdDev_;
    // outputs
    Real zMin_, zMax_, hh_, pL_, pR_, theta0_;
    Array SABR_Prob_Vec_, SABR_Cum_Prob_Vec_;
};

} // namespace QuantExt
