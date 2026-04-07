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

/*! \file quantohestonprocess.hpp
    \brief state process for the Heston model with quanto drift adjustments
    \ingroup processes
*/

#ifndef quantext_quantohestonprocess_hpp
#define quantext_quantohestonprocess_hpp

#include <ql/processes/hestonprocess.hpp>

namespace QuantExt {
using namespace QuantLib;
  
//! Quanto-adjusted Heston process
/*! The stadard Heston process implements
  \f[
  \begin{array}{rcl}
  dS(t, S)  &=& \mu S dt + \sqrt{V} S dW_1 \\
  dV(t, S)  &=& \kappa (\theta - V) dt + \sigma \sqrt{V} dW_2 \\
  dW_1 dW_2 &=& \rho dt
  \end{array}
  \f]
  
  The Quanto Heston process applies quanto drift adjustments to spot process mu and
  variance process theta:

  \f[
  \begin{align*}
  \mu &\rightarrow \mu - \rho_{SX}\:\sqrt{V}\:\sqrt{V_X} \\
  \theta &\rightarrow \theta - 1/\kappa\: \rho_{VX}\: \sigma\:\sqrt{V}\:\sqrt{V_X}
  \end{align*}
  \f]

  The (constant) correlations \f$\rho_{SX}\f$ and \f$\rho_{VX}\f$ are passed to the constructor,
  the FX process volatility \f$\sqrt{V_X}\f$ (possibly constant, determinstic, local or
  stochastic) that is "external" to this Heston system, needs to be updated before
  evolve is called.

  It is the developer's responsibility to ensure that
  - the passed correlations \f$\rho_{SX}\f$ and \f$\rho_{VX}\f$ are consistent with the correlation
  matrix that drives the overall system of processes that includes the FX process and
  this Heston system
  - the Wiener increments passed to this Heston system are nevertheless independent,
  i.e. properly decorrelated before passing to the evolve method; note that the
  decorrelation method depends on the chosen discretisation
  
  By default the FX volatility is zero so that this quanto process evolve function
  has the same result as the domestic currency Heston process.

  The QuantoHestonProcess class  modelling the foreign currency EQ/COM index and FX,
  respectively . Its dimesnion is hence 4.

  Process order: EQ Spot (SE), EQ Variance (VE), FX Spot (SX), FX Variance (VX)
  Correlations: Parsimonious, with single spot-spot correlation to be passed
                    ES     EV     XS     XV
           SE  [ [   1    SE-VE  SE-SX  SE-VX ],
           VE    [ SE-VE    1    VE-SX  VE-VX ]
           SX    [ SE-SX  VE-SX    1    SX-VX ]
           VX    [ SE-VX  VE-VX  SX-VX    1   ] ]
  SE-SX: rho_spot, to be provided  
  SE-VE: internal Heston process correlation rho_e
  SX-VX: internal FX Heston process correlation rho_x
  SE-VX: rho_spot * rho_x
  SX-VE: rho_spot * rho_e
  VX-VE: rho_spot * rho_e * rho_x

  \warning The drift adjustment is added to the Heston evolution over each time interval.
  This is consistent only with the simple Euler (full/partial truncation or reflection)
  schemes in the Heston process, and needs sufficiently large number of time steps.
  The other HestonProcess schemes (QE etc) can be used at own risk, but we actually need
  a new "large-step" scheme similar to QE that incorporates the quanto drift component
  proportional to sqrt(V). The evolve method allows all schemes. In case of the non-Euler
  schemes it enforces positive variance by reflection.  
*/
  
class QuantoHestonProcess : public StochasticProcess {
public:
    QuantoHestonProcess(const QuantLib::ext::shared_ptr<HestonProcess>& process,
			const QuantLib::ext::shared_ptr<HestonProcess>& fxProcess,
			Real spotCorrelation);

    Size size() const override { return process_->size() + fxProcess_->size(); }
    Size factors() const override { return process_->factors() + fxProcess_->factors(); }
    Array initialValues() const override;
    Matrix diffusion(Time t, const Array& x) const override;
    Array apply(const Array& x0, const Array& dx) const override;
    Array driftAdjustment(Time t, const Array& x) const;
    Array drift(Time t, const Array& x) const override;
    Array evolve(Time t0, const Array& x0, Time dt, const Array& dw) const override;

    Matrix parsimoniousCorrelation() { return correlation_; }
    //! Allow checking whether the parsimonious correlation matrix is positive definite
    std::vector<Real> parsimoniousEigenvalues() { return eigenvalues_; }
    // Cholesky decomposition of the  
    Matrix sqrtCorrelation() { return sqrtCorrelation_; }

private:
    Matrix decorrelationMatrix(const QuantLib::ext::shared_ptr<HestonProcess>& p) const;
  
    QuantLib::ext::shared_ptr<HestonProcess> process_;
    QuantLib::ext::shared_ptr<HestonProcess> fxProcess_;
    Real spotCorrelation_;
    Matrix correlation_;
    Matrix sqrtCorrelation_;
    std::vector<Real> eigenvalues_;
    Matrix decorrelationMatrixEquity_;
    Matrix decorrelationMatrixFx_;
};

} // namespace QuantExt

#endif
