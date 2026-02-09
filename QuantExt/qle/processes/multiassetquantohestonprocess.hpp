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

/*! \file multiassetquantohestonprocess.hpp
    \brief multi-asset Heston state processes with quanto drift adjustments
    \ingroup processes
*/

#ifndef quantext_multiassetquantohestonprocess_hpp
#define quantext_multiassetquantohestonprocess_hpp

#include <ql/processes/hestonprocess.hpp>
#include <qle/processes/ptdhestonprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Multi-Asset Quanto Heston Process 
/*!
  This class models a system of n Heston processes for EQ/COM indices (possibly quanto) and FX indices, 
  generalising the Quanto Heston Process that covers two processes, one EQ/COM process and
  one FX process.

  The Heston processes are provided in no particular order. The second input vector establishes the
  connection between an EQ/COM process and the associated FX process: fxProcessIndices[i] points to the
  position of the fx process in the vector of Heston processes that is used to quanto-adjust the
  Heston process at position i. An index value of Null<Size>() indicates that the Heston process does
  not need adjustment, since it is an FX process or a domestic EQ/COM process.

  As a third parameter we pass the matrix of spot-spot correlations. The full parsimonious correlation
  matrix is built internally at construction, with spot-variance and variance-variance cross correlations
  computed using the partial independence assumption.

  \warning The drift adjustment is added to the Heston evolution over each time interval.
  This is consistent only with the simple Euler (full/partial truncation or reflection)
  schemes in the Heston process, and needs sufficiently large number of time steps.
  The other HestonProcess schemes (QE etc) can be used at own risk, but we actually need
  a new "large-step" scheme similar to QE that incorporates the quanto drift component
  proportional to sqrt(V). The evolve method allows all schemes. In case of the non-Euler
  schemes it enforces positive variance by reflection.
*/
  
class MultiAssetQuantoHestonProcess : public StochasticProcess {
public:
    MultiAssetQuantoHestonProcess(const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& processes,
				  const std::vector<Size>& fxProcessIndices,
				  const Matrix& spotCorrelation);

    MultiAssetQuantoHestonProcess(const std::vector<QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonProcess>>& processes,
				  const std::vector<Size>& fxProcessIndices,
				  const Matrix& spotCorrelation);

    Size size() const override;
    Size factors() const override;
    Array initialValues() const override;
    Matrix diffusion(Time t, const Array& x) const override;
    Array apply(const Array& x0, const Array& dx) const override;
    Array drift(Time t, const Array& x) const override;
    Array evolve(Time t0, const Array& x0, Time dt, const Array& dw) const override;

    Matrix spotCorrelation() { return spotCorrelation_; }
    Matrix parsimoniousCorrelation() { return correlation_; }
    //! Allow checking whether the parsimonious correlation matrix is positive definite
    bool parsimoniousCorrelationNeededSalvaging() { return salvaging_; }
    std::vector<Real> parsimoniousEigenvalues() { return eigenvalues_; }
    // Cholesky decomposition of the parsimonious correlation matrix
    Matrix sqrtCorrelation() { return sqrtCorrelation_; }

private:
    //! Updates correlations (Salvaging) if required
    bool checkCorrelations(Matrix& C);
    Matrix decorrelationMatrix(const Real& rho, const HestonProcess::Discretization& discretization) const;
    // Adjustment with constant parameter Heston processes
    Array driftAdjustment(Time t, const Array& x) const;
    // Adjustment with piecewise constant time-dependent Heston parameters, evaluate at segment midpoint
    Array ptdDriftAdjustment(Time tm, const Array& x) const;
    // Find the index of the next grid point with grid_[idx] > t
    Size locate(Time t) const;

    Size size_; // Dimensions of the resulting stochastic process, 2 * models
    Size models_; // number of input processes resp. models
    std::vector<QuantLib::ext::shared_ptr<HestonProcess>> processes_;
    std::vector<QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonProcess>> ptdProcesses_;
    std::vector<Size> fxProcessIndices_;
    Matrix spotCorrelation_;
    Array initialValues_;

    // constant paramter processes
    Matrix correlation_;
    Matrix sqrtCorrelation_;
    bool salvaging_;
    std::vector<Real> eigenvalues_;
    std::vector<Matrix> decorrelationMatrices_;

    // piecewise processes
    bool isPTD_;
    TimeGrid grid_;
    Size segments_; // number of piecewise segments, grid_.size() - 1
    std::vector<Matrix> ptdCorrelations_;
    std::vector<Matrix> ptdSqrtCorrelations_;
    std::vector<std::vector<Matrix>> ptdDecorrelationMatrices_;
};

} // namespace QuantExt

#endif
