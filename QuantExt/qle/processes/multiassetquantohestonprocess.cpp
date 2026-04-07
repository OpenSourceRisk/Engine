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

#include <qle/processes/multiassetquantohestonprocess.hpp>
#include <ql/processes/eulerdiscretization.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/math/array.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>

#include <iostream>

using namespace QuantLib;

namespace QuantExt {

MultiAssetQuantoHestonProcess::MultiAssetQuantoHestonProcess(
    const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& processes, const std::vector<Size>& fxProcessIndices,
    const Matrix& spotCorrelation)
    : size_(2 * processes.size()), models_(processes.size()), processes_(processes),
      fxProcessIndices_(fxProcessIndices), spotCorrelation_(spotCorrelation), initialValues_(size_, 0.0),
      isPTD_(false), segments_(1) {

    QL_REQUIRE(processes.size() > 0, "empty processes vector in MultiAssetQuantoHestonProcess");
    QL_REQUIRE(processes.size() == fxProcessIndices.size(), "vector size mismatch in MultiAssetQuantoHestonProcess");
    QL_REQUIRE(processes.size() == spotCorrelation.rows(), "matrix size mismatch in MultiAssetQuantoHestonProcess");

    for (Size i = 0; i < processes_.size(); ++i) {
        initialValues_[2 * i] = processes_[i]->initialValues()[0];
        initialValues_[2 * i + 1] = processes_[i]->initialValues()[1];
    }

    // Build the parsimonious correlation matrix, expanding the spot-spot correlation matrix following
    //    Dimitroff, Lorenz, Szimayer: A Parsimonious Multi-Asset Heston Model,
    //    https://ssrn.com/abstract=1435199, Eqn (16)
    // to twice the size, including the variance factors, i.e. including
    // - spot-variance correlation within each Heston "sub-system",
    // - spot-variance correlation across different sub-systems
    // - variance-variance correlations across different sub-systems
    // with block matrices C_i along the diagonal and C_ij off diagonal.
    // Eqn (16):      
    //       (1,    r_i )                  ( 1,     r_j       )
    // C_i = (          )    C_ij = r_ij * (                  )
    //       (r_i,  1   )                  ( r_i,   r_i * r_j )
    
    correlation_ = Matrix(size_, size_, 0.0);
    for (Size i = 0; i < processes_.size(); ++i) {
        Real ri = processes_[i]->rho();
        for (Size j = 0; j < processes_.size(); ++j) {
            if (i == j) {
                correlation_[2 * i][2 * i] = 1.0;
                correlation_[2 * i][2 * i + 1] = ri;
                correlation_[2 * i + 1][2 * i] = ri;
                correlation_[2 * i + 1][2 * i + 1] = 1.0;
            } else {
                Real rij = spotCorrelation_[i][j];
                Real rj = processes_[j]->rho();
                correlation_[2 * i][2 * j] = rij;
                correlation_[2 * i][2 * j + 1] = rij * rj;
                correlation_[2 * i + 1][2 * j] = rij * ri;
                correlation_[2 * i + 1][2 * j + 1] = rij * ri * rj;
            }
        }
    }

    salvaging_ = checkCorrelations(correlation_);
    
    // Used to generate correlated variates based on the parsimonious large correlation matrix
    sqrtCorrelation_ = CholeskyDecomposition(correlation_, true);

    // Consistency check
    Matrix sqrtCorrSquared = sqrtCorrelation_ * transpose(sqrtCorrelation_);
    for (Size i = 0; i < correlation_.rows(); ++i) {
        for (Size j = 0; j < correlation_.rows(); ++j) {
            // FIXME: tolerances?
            QL_REQUIRE(fabs(correlation_[i][j] - correlation_[j][i]) < 1e-9,
                       "correlation matrix not symmetric for i=" << i << ", j=" << j << ": " << std::setprecision(6)
                                                                 << correlation_[i][j] << " vs " << correlation_[j][i]);
            QL_REQUIRE(fabs(correlation_[i][j] - sqrtCorrSquared[i][j]) < 1e-6,
                       "correlation matrix square root problem for i=" << i << ", j=" << j << ": "
                                                                       << std::setprecision(6) << correlation_[i][j]
                                                                       << " vs " << sqrtCorrSquared[i][j]);
        }
    }

    // Used below to pass decorrelated spot/variance variates into the Heston processes,
    // such that the Heston processes recover the correlated external variates.
    decorrelationMatrices_.clear();
    decorrelationMatrices_ = std::vector<Matrix>(processes_.size());
    for (Size i = 0; i < processes_.size(); ++i) {
        Real r = processes_[i]->rho();
        decorrelationMatrices_[i] = decorrelationMatrix(r, processes_[i]->discretization());
    }
}

MultiAssetQuantoHestonProcess::MultiAssetQuantoHestonProcess(
    const std::vector<QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonProcess>>& processes,
    const std::vector<Size>& fxProcessIndices, const Matrix& spotCorrelation)
    : size_(2 * processes.size()), models_(processes.size()), ptdProcesses_(processes),
      fxProcessIndices_(fxProcessIndices), spotCorrelation_(spotCorrelation), initialValues_(size_, 0.0), isPTD_(true) {

    QL_REQUIRE(processes.size() > 0, "MultiAssetQuantoHestonProcess called with empty processes vector");
    QL_REQUIRE(processes.size() == fxProcessIndices.size(), "vector size mismatch in MultiAssetQuantoHestonProcess");
    QL_REQUIRE(processes.size() == spotCorrelation.rows(), "matrix size mismatch in MultiAssetQuantoHestonProcess");

    grid_ = ptdProcesses_.front()->model()->timeGrid();
    QL_REQUIRE(grid_.size() > 1, "MultiAssetQuantoHestonProcess expects at least two time grid point");
    for (auto p : ptdProcesses_) {
        QL_REQUIRE(grid_.size() == p->model()->timeGrid().size(),
                   "time grid sizes do not match in MultiAssetQuantoHestonProcess ctor");
        for (Size k = 1; k < grid_.size(); ++k) {
            QL_REQUIRE(close_enough(grid_[k], p->model()->timeGrid()[k]),
                       "time grids do not match in MultiAssetQuantoHestonProcess ctor");
        }
    }
    // Recall that the grid starts with t = 0, so we need to cover grid_.size() - 1 segments    
    segments_ = grid_.size() - 1;

    for (Size i = 0; i < ptdProcesses_.size(); ++i) {
        initialValues_[2 * i] = ptdProcesses_[i]->initialValues()[0];
        initialValues_[2 * i + 1] = ptdProcesses_[i]->initialValues()[1];
    }
    
    ptdCorrelations_.clear();
    ptdSqrtCorrelations_.clear();
    ptdDecorrelationMatrices_.clear();

    std::vector<bool> sal(segments_, false);
    ptdCorrelations_ = std::vector<Matrix>(segments_, Matrix(size_, size_, 0.0));
    ptdSqrtCorrelations_ = std::vector<Matrix>(segments_);
    ptdDecorrelationMatrices_ = std::vector<std::vector<Matrix>>(segments_, std::vector<Matrix>(models_));

    for (Size k = 0; k < segments_; ++k) {
        // Like the AnalyticPTDHestonEngine we set t to the segment's mid point, to stay away from jump dates
        Real t = 0.5 * (grid_[k+1] + grid_[k]);

	Matrix& C = ptdCorrelations_[k];

	for (Size i = 0; i < ptdProcesses_.size(); ++i) {
	   Real ri = ptdProcesses_[i]->model()->rho(t);
	   for (Size j = 0; j < ptdProcesses_.size(); ++j) {
	       if (i == j) {
		   C[2 * i][2 * i] = 1.0;
		   C[2 * i][2 * i + 1] = ri;
		   C[2 * i + 1][2 * i] = ri;
		   C[2 * i + 1][2 * i + 1] = 1.0;
	       } else {
		   Real rij = spotCorrelation_[i][j];
		   Real rj = ptdProcesses_[j]->model()->rho(t);
		   C[2 * i][2 * j] = rij;
		   C[2 * i][2 * j + 1] = rij * rj;
		   C[2 * i + 1][2 * j] = rij * ri;
		   C[2 * i + 1][2 * j + 1] = rij * ri * rj;
	       }
	   }
	}
	if (checkCorrelations(C))
  	    salvaging_ = true;

        ptdSqrtCorrelations_[k] = CholeskyDecomposition(C, true);

	// Consistency checks
	Matrix sqrtCorrSquared = ptdSqrtCorrelations_[k] * transpose(ptdSqrtCorrelations_[k]);
	for (Size i = 0; i < size_; ++i) {
	    for (Size j = 0; j < size_; ++j) {
	        // FIXME: tolerances?
	        QL_REQUIRE(fabs(ptdCorrelations_[k][i][j] - ptdCorrelations_[k][j][i]) < 1e-9,
			   "correlation matrix not symmetric for i=" << i << ", j=" << j << ": "
			   << std::setprecision(6) << ptdCorrelations_[k][i][j] << " vs " << ptdCorrelations_[k][j][i]);
		QL_REQUIRE(fabs(ptdCorrelations_[k][i][j] - sqrtCorrSquared[i][j]) < 1e-6,
			   "correlation matrix square root problem for i=" << i << ", j=" << j << ": "
			   << std::setprecision(6) << ptdCorrelations_[k][i][j] << " vs " << sqrtCorrSquared[i][j]);
	    }
	}

        for (Size i = 0; i < ptdProcesses_.size(); ++i) {
	    Real r = ptdProcesses_[i]->model()->rho(t);
            ptdDecorrelationMatrices_[k][i] = decorrelationMatrix(r, ptdProcesses_[i]->discretization());
        }	
    }
}

bool MultiAssetQuantoHestonProcess::checkCorrelations(Matrix& C) {
    // Ensure positive semi-definite, but without normalisation
    SymmetricSchurDecomposition jd(C);
    bool updated = false;
    for (Size k = 0; k < C.rows(); ++k) {
        if (jd.eigenvalues()[k] < -1E-16)
            updated = true;
    }
    if (updated) {
        Matrix diagonal(C.rows(), C.rows(), 0.0);
        for (Size k = 0; k < jd.eigenvalues().size(); ++k) {
            eigenvalues_[k] = jd.eigenvalues()[k];
            diagonal[k][k] = std::max<Real>(jd.eigenvalues()[k], 0.0);
        }
        C = jd.eigenvectors() * diagonal * transpose(jd.eigenvectors());
    }
    return updated;
}

Matrix MultiAssetQuantoHestonProcess::decorrelationMatrix(const Real& r, const HestonProcess::Discretization& discretization) const {
    Matrix Q(2, 2, 0.0);
    Matrix Qinv(2, 2, 0.0);
    if (discretization == HestonProcess::FullTruncation ||
        discretization == HestonProcess::PartialTruncation ||
	discretization == HestonProcess::Reflection) {
        // QuantLib uses lower triangular decomposition, QL
        Q[0][0] = 1.0;
        Q[0][1] = 0.0;
        Q[1][0] = r;
        Q[1][1] = std::sqrt(1.0 - r * r);
        Qinv[0][0] = 1.0;
        Qinv[0][1] = 0.0;
        Qinv[1][0] = -r / std::sqrt(1.0 - r * r);
        Qinv[1][1] = 1.0 / std::sqrt(1.0 - r * r);
    } else {
        // QuantLib uses upper triangular decomposition, QU
        Q[0][0] = std::sqrt(1.0 - r * r);
        Q[0][1] = r;
        Q[1][0] = 0;
        Q[1][1] = 1;
        Qinv[0][0] = 1.0 / std::sqrt(1.0 - r * r);
        Qinv[0][1] = -r / std::sqrt(1.0 - r * r);
        Qinv[1][0] = 0;
        Qinv[1][1] = 1;
    }

    // double check with numerical inversion

    Matrix Qi = inverse(Q);
    for (Size i = 0; i < 2; ++i) {
        for (Size j = 0; j < 2; ++j) {
            QL_REQUIRE(fabs(Qi[i][j] - Qinv[i][j]) < 1e-6, "error inverting matrix Q");
        }
    }

    return Qinv;
}
  
Size MultiAssetQuantoHestonProcess::size() const { return size_; }

Size MultiAssetQuantoHestonProcess::factors() const { return size_; }

Array MultiAssetQuantoHestonProcess::initialValues() const { return initialValues_; }

Matrix MultiAssetQuantoHestonProcess::diffusion(Time t, const Array& x) const {
    QL_FAIL("QuantoHestonProcess::diffusion not implemented");
    Matrix m(size_, size_, 0.0);
    return m;
}

Array MultiAssetQuantoHestonProcess::drift(Time t, const Array& x) const {
    QL_FAIL("MultiAssetQuantoHestonProcess::drift not implemented");
    Array drift(size_, 0.0);
    return drift;
}

Array MultiAssetQuantoHestonProcess::apply(const Array& x0, const Array& dx) const {
    Array ret(size_, 0.0);
    for (Size i = 0; i < models_; ++i) {
        ret[2 * i] = x0[2 * i] * std::exp(dx[2 * i]);
        ret[2 * i + 1] = x0[2 * i] + dx[2 * i + 1];
    }
    return ret;
}

Array MultiAssetQuantoHestonProcess::driftAdjustment(Time t, const Array& x) const {
    Array adjustment(size(), 0.0);
    for (Size i = 0; i < models_; ++i) {
        if (fxProcessIndices_[i] == Null<Size>())
            continue;
        Size j = fxProcessIndices_[i];
        auto fxProcess = processes_[j];
        auto eqProcess = processes_[i];
        const Real eqVol = (x[2 * i + 1] > 0.0) ? std::sqrt(x[2 * i + 1])
                           : (eqProcess->discretization() == HestonProcess::Reflection)
                               ? Real(-std::sqrt(-x[2 * i + 1]))
                               : 0.0;
        const Real fxVol = (x[2 * j + 1] > 0.0) ? std::sqrt(x[2 * j + 1])
                           : (fxProcess->discretization() == HestonProcess::Reflection)
                               ? Real(-std::sqrt(-x[2 * j + 1]))
                               : 0.0;
        adjustment[2 * i] = -spotCorrelation_[i][j] * fxVol * eqVol;
        adjustment[2 * i + 1] = -spotCorrelation_[i][j] * eqProcess->rho() * eqProcess->sigma() * fxVol * eqVol;
    }
    return adjustment;
}

Array MultiAssetQuantoHestonProcess::ptdDriftAdjustment(Time tm, const Array& x) const {
    Array adjustment(size(), 0.0);
    for (Size i = 0; i < models_; ++i) {
        if (fxProcessIndices_[i] == Null<Size>())
            continue;
        Size j = fxProcessIndices_[i];
        auto fxProcess = ptdProcesses_[j];
        auto eqProcess = ptdProcesses_[i];
        const Real eqVol = (x[2 * i + 1] > 0.0) ? std::sqrt(x[2 * i + 1])
                           : (eqProcess->discretization() == HestonProcess::Reflection)
                               ? Real(-std::sqrt(-x[2 * i + 1]))
                               : 0.0;
        const Real fxVol = (x[2 * j + 1] > 0.0) ? std::sqrt(x[2 * j + 1])
                           : (fxProcess->discretization() == HestonProcess::Reflection)
                               ? Real(-std::sqrt(-x[2 * j + 1]))
                               : 0.0;
        adjustment[2 * i] = -spotCorrelation_[i][j] * fxVol * eqVol;
        adjustment[2 * i + 1] = -spotCorrelation_[i][j] * eqProcess->model()->rho(tm) * eqProcess->model()->sigma(tm) * fxVol * eqVol;
    }
    return adjustment;
}
  
Size MultiAssetQuantoHestonProcess::locate(Time t) const {
    QL_REQUIRE(t <= grid_.back() && t >= grid_.front(),
	       "time t " << t << " out of range [" << grid_.front() << ", " << grid_.back()
	       << " in MultiAssetQuantoHestonProcess::locate");
    // Locate the segment that contains time t: idx is the index of the closest grid point larger than t
    // We add QL_EPSILON here to ensure that t=0 is within the first segment
    Size idx = std::lower_bound(grid_.begin(), grid_.end(), t + QL_EPSILON) - grid_.begin();
    QL_REQUIRE(idx >= 1 && idx < grid_.size(),
               "next index " << idx << " out of range in MultiAssetQuantoHestonProcess::locate for t=" << t);
    return idx;
}

Array MultiAssetQuantoHestonProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {

    // Index of the closest grid point larger than t
    Size idx = isPTD_ ? locate(t0) : 1;
    // k0 is the index for picking up the correlation matrices relevant for this segment
    Size k0 = idx - 1;
    // mid point of the segment that contains t0 (for the piecewise case)
    Real tm = t0;
    if (isPTD_) {
        Real t1 = grid_[idx - 1];
        Real t2 = grid_[idx];
        tm = 0.5 * (t1 + t2);
    }

    Array a = isPTD_ ? ptdDriftAdjustment(tm, x0) : driftAdjustment(t0, x0);

    // turn array of independent dw's into correlated dw's, using the current sqrt correlation matrix
    Matrix SM = isPTD_ ? ptdSqrtCorrelations_[k0] : sqrtCorrelation_;
    Array dW_correlated = SM * dw;

    Array x0_heston(2, 0.0);
    Array dW_heston_correlated(2, 0.0);
    Array dW_heston_decorrelated(2, 0.0);
    Array hestonEvolution(2, 0.0);

    // adjusted evolution
    Array evolution(size());

    for (Size i = 0; i < models_; ++i) {
        // split state into 2d states of individual Heston processes
        x0_heston[0] = x0[2 * i];
        x0_heston[1] = x0[2 * i + 1];

	// split correlated dw's into 2d dw's per Heston process
        dW_heston_correlated[0] = dW_correlated[2 * i];
        dW_heston_correlated[1] = dW_correlated[2 * i + 1];

	// decorrelate equity sub-systems, using the current decorrelation matrix
        Matrix DM = isPTD_ ? ptdDecorrelationMatrices_[k0][i] : decorrelationMatrices_[i];
        dW_heston_decorrelated = DM * dW_heston_correlated;

        // QuantLib Heston evolution
        hestonEvolution = isPTD_ ? ptdProcesses_[i]->evolve(t0, x0_heston, dt, dW_heston_decorrelated)
                                 : processes_[i]->evolve(t0, x0_heston, dt, dW_heston_decorrelated);

        // Combine evolutions and add adjustments to the equity system
        evolution[2 * i] = hestonEvolution[0] * std::exp(a[2 * i] * dt);
        evolution[2 * i + 1] = hestonEvolution[1] + a[2 * i + 1] * dt;

	// Ensure that quanto-adjusted variance processes remain non-negative when using QE etc
        HestonProcess::Discretization discretization =
            isPTD_ ? ptdProcesses_[i]->discretization() : processes_[i]->discretization();
        if (evolution[2 * i + 1] < 0 &&
	    discretization != HestonProcess::Reflection &&
            discretization != HestonProcess::FullTruncation &&
	    discretization != HestonProcess::PartialTruncation) {
            evolution[2 * i + 1] = 0.0;
        }
    }
    return evolution;
}
}
