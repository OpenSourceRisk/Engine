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

#include <ql/processes/hestonprocess.hpp>

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
*/
  
class MultiAssetQuantoHestonProcess : public StochasticProcess {
public:
    MultiAssetQuantoHestonProcess(const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& processes,
				  const std::vector<Size>& fxProcessIndices,
				  const Matrix& spotCorrelation);

    Size size() const override;
    Size factors() const override;
    Array initialValues() const override;
    Matrix diffusion(Time t, const Array& x) const override;
    Array apply(const Array& x0, const Array& dx) const override;
    Array driftAdjustment(Time t, const Array& x) const;
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
    Matrix decorrelationMatrix(const QuantLib::ext::shared_ptr<HestonProcess>& p) const;
  
    std::vector<QuantLib::ext::shared_ptr<HestonProcess>> processes_;
    std::vector<Size> fxProcessIndices_;
    Matrix spotCorrelation_;
    Matrix correlation_;
    Matrix sqrtCorrelation_;
    bool salvaging_;
    std::vector<Real> eigenvalues_;
    std::vector<Matrix> decorrelationMatrices_;
};

} // namespace QuantExt
