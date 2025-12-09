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

using namespace QuantLib;

namespace QuantExt {

MultiAssetQuantoHestonProcess::MultiAssetQuantoHestonProcess(
    const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& processes, const std::vector<Size>& fxProcessIndices,
    const Matrix& spotCorrelation)
    : processes_(processes), fxProcessIndices_(fxProcessIndices), spotCorrelation_(spotCorrelation) {

    QL_REQUIRE(processes.size() > 0, "empty processes vector in MultiAssetQuantoHestonProcess");
    QL_REQUIRE(processes.size() == fxProcessIndices.size(), "vector size mismatch in MultiAssetQuantoHestonProcess");
    QL_REQUIRE(processes.size() == spotCorrelation.rows(), "matrix size mismatch in MultiAssetQuantoHestonProcess");

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
    
    Size dim = 2 * processes_.size();
    correlation_ = Matrix(dim, dim, 0.0);
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

    // Ensure positive semi-definite, but without normalisation
    SymmetricSchurDecomposition jd(correlation_);
    salvaging_ = false;
    for (Size k = 0; k < correlation_.rows(); ++k) {
        if (jd.eigenvalues()[k] < -1E-16)
            salvaging_ = true;
    }
    if (salvaging_) {
        Matrix diagonal(correlation_.rows(), correlation_.rows(), 0.0);
        for (Size k = 0; k < jd.eigenvalues().size(); ++k) {
            eigenvalues_[k] = jd.eigenvalues()[k];
            diagonal[k][k] = std::max<Real>(jd.eigenvalues()[k], 0.0);
        }
        correlation_ = jd.eigenvectors() * diagonal * transpose(jd.eigenvectors());
    }

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
    for (Size i = 0; i < processes_.size(); ++i)
        decorrelationMatrices_.push_back(decorrelationMatrix(processes_[i]));
}

Size MultiAssetQuantoHestonProcess::size() const { return 2 * processes_.size(); }

Size MultiAssetQuantoHestonProcess::factors() const { return 2 * processes_.size(); }

Matrix MultiAssetQuantoHestonProcess::decorrelationMatrix(const QuantLib::ext::shared_ptr<HestonProcess>& p) const {
    // QuantLib's Heston::evolve uses dW(correlated) = Q * dW(independent)
    // with matrix
    // QL = [ [ 1, 0 ], [ r, sqrt(1 - r^2) ] ] resp.
    // QU = [ [ sqrt(1 - r^2), r ], [ 0, 1 ] ],
    // depending on the chosen discretization method, to convert independent variates into correlated variates.
    // Since we generate correlated variates for the multi-asset Heston model we need to decorrelate each
    // Heston subsystem before passing variates into the Heston::evolve method, i.e. using the inverse of Q.
    Matrix Q(2, 2, 0.0);
    Matrix Qinv(2, 2, 0.0);
    Real r = p->rho();
    if (p->discretization() == HestonProcess::FullTruncation ||
        p->discretization() == HestonProcess::PartialTruncation || p->discretization() == HestonProcess::Reflection) {
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

    // Double check with numerical inversion
    Matrix Qi = inverse(Q);
    for (Size i = 0; i < 2; ++i) {
        for (Size j = 0; j < 2; ++j) {
            QL_REQUIRE(fabs(Qi[i][j] - Qinv[i][j]) < 1e-6, "error inverting matrix Q");
        }
    }

    return Qinv;
}

Array MultiAssetQuantoHestonProcess::initialValues() const {
    Array ret(size());
    for (Size i = 0; i < processes_.size(); ++i) {
        ret[2 * i] = processes_[i]->initialValues()[0];
        ret[2 * i + 1] = processes_[i]->initialValues()[1];
    }
    return ret;
}

Matrix MultiAssetQuantoHestonProcess::diffusion(Time t, const Array& x) const {
    QL_FAIL("QuantoHestonProcess::diffusion not implemented");
    Matrix m(size(), size(), 0.0);
    return m;
}

Array MultiAssetQuantoHestonProcess::drift(Time t, const Array& x) const {
    QL_FAIL("MultiAssetQuantoHestonProcess::drift not implemented");
    Array drift(size(), 0.0);
    return drift;
}

Array MultiAssetQuantoHestonProcess::apply(const Array& x0, const Array& dx) const {
    Array ret(size(), 0.0);
    for (Size i = 0; i < processes_.size(); ++i) {
        ret[2 * i] = x0[2 * i] * std::exp(dx[2 * i]);
        ret[2 * i + 1] = x0[2 * i] + dx[2 * i + 1];
    }
    return ret;
}

Array MultiAssetQuantoHestonProcess::driftAdjustment(Time t, const Array& x) const {
    Array adjustment(size(), 0.0);
    for (Size i = 0; i < processes_.size(); ++i) {
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

Array MultiAssetQuantoHestonProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {

    Array a = driftAdjustment(t0, x0);

    // turn vector of independent dw's into correlated dw's
    Array dW_correlated = sqrtCorrelation_ * dw;

    Array x0_heston(2, 0.0);
    Array dW_heston_correlated(2, 0.0);
    Array dW_heston_decorrelated(2, 0.0);
    Array hestonEvolution(2, 0.0);

    // adjusted evolution
    Array evolution(size());

    for (Size i = 0; i < processes_.size(); ++i) {
        // split state into 2d states of individual Heston processes
        x0_heston[0] = x0[2 * i];
        x0_heston[1] = x0[2 * i + 1];

	// split correlated dw's into 2d dw's per Heston process
        dW_heston_correlated[0] = dW_correlated[2 * i];
        dW_heston_correlated[1] = dW_correlated[2 * i + 1];

	// decorrelate equity sub-systems
        dW_heston_decorrelated = decorrelationMatrices_[i] * dW_heston_correlated;

	// QuantLib Heston evolution
        hestonEvolution = processes_[i]->evolve(t0, x0_heston, dt, dW_heston_decorrelated);

	// Combine evolutions and add adjustments to the equity system
        evolution[2 * i] = hestonEvolution[0] * std::exp(a[2 * i] * dt);
        evolution[2 * i + 1] = hestonEvolution[1] + a[2 * i + 1] * dt;
    }
    return evolution;
}
}
