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

#include <qle/processes/quantohestonprocess.hpp>
#include <ql/processes/eulerdiscretization.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/math/array.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>

using namespace QuantLib;

namespace QuantExt {

QuantoHestonProcess::QuantoHestonProcess(const QuantLib::ext::shared_ptr<HestonProcess>& process,
                                         const QuantLib::ext::shared_ptr<HestonProcess>& fxProcess,
                                         Real spotCorrelation)
    : process_(process), fxProcess_(fxProcess), spotCorrelation_(spotCorrelation),
      correlation_(Matrix(4, 4, 1.0)), eigenvalues_(4, 0.0) {
    correlation_[0][1] = correlation_[1][0] = process_->rho();
    correlation_[0][2] = correlation_[2][0] = spotCorrelation_;
    correlation_[0][3] = correlation_[3][0] = spotCorrelation_ * fxProcess_->rho();
    correlation_[1][2] = correlation_[2][1] = spotCorrelation_ * process_->rho();
    correlation_[1][3] = correlation_[3][1] = spotCorrelation_ * process_->rho() * fxProcess_->rho();
    correlation_[2][3] = correlation_[3][2] = fxProcess_->rho();

    SymmetricSchurDecomposition jd(correlation_);
    bool needsSalvaging = false;    
    for (Size k = 0; k < correlation_.rows(); ++k) {
        if (jd.eigenvalues()[k] < -1E-16)
            needsSalvaging = true;
    }
    if (needsSalvaging) {
        Matrix diagonal(correlation_.rows(), correlation_.rows(), 0.0);
        for (Size k = 0; k < jd.eigenvalues().size(); ++k) {
	    eigenvalues_[k] = jd.eigenvalues()[k];
            diagonal[k][k] = std::max<Real>(jd.eigenvalues()[k], 0.0);
	}
        correlation_ = jd.eigenvectors() * diagonal * transpose(jd.eigenvectors());
    }

    sqrtCorrelation_ = CholeskyDecomposition(correlation_, true);
    
    Matrix sqrtCorrSquared = sqrtCorrelation_ * transpose(sqrtCorrelation_);
    for (Size i = 0; i < 4; ++i) {
        for (Size j = 0; j < 4; ++j) {
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

    decorrelationMatrixEquity_ = decorrelationMatrix(process_);
    decorrelationMatrixFx_ = decorrelationMatrix(fxProcess_);
}

Matrix QuantoHestonProcess::decorrelationMatrix(const QuantLib::ext::shared_ptr<HestonProcess>& p) const {
    Matrix Q(2, 2, 0.0);
    Matrix Qinv(2, 2, 0.0);
    Real r = p->rho();
    if (p->discretization() == HestonProcess::FullTruncation ||
        p->discretization() == HestonProcess::PartialTruncation ||
	p->discretization() == HestonProcess::Reflection) {
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

Array QuantoHestonProcess::initialValues() const {
    Array ret(4);
    ret[0] = process_->initialValues()[0];
    ret[1] = process_->initialValues()[1];
    ret[2] = fxProcess_->initialValues()[0];
    ret[3] = fxProcess_->initialValues()[1];
    return ret;
}

Matrix QuantoHestonProcess::diffusion(Time t, const Array& x) const {
    QL_FAIL("QuantoHestonProcess::diffusion not implemented");
    Matrix m(4, 4, 0.0);
    return m;
}

Array QuantoHestonProcess::drift(Time t, const Array& x) const {
    QL_FAIL("QuantoHestonProcess::drift not implemented");
    Array drift(4);
    return drift;
}

Array QuantoHestonProcess::apply(const Array& x0, const Array& dx) const {
  return {x0[0] * std::exp(dx[0]),
	  x0[1] + dx[1],
	  x0[2] * std::exp(dx[2]),
	  x0[3] + dx[3]};
}

Array QuantoHestonProcess::driftAdjustment(Time t, const Array& x) const {
    const Real vol = (x[1] > 0.0)                                                ? std::sqrt(x[1])
                     : (process_->discretization() == HestonProcess::Reflection) ? Real(-std::sqrt(-x[1]))
                                                                                 : 0.0;
    const Real fxVol = (x[3] > 0.0)                                                ? std::sqrt(x[3])
                     : (process_->discretization() == HestonProcess::Reflection) ? Real(-std::sqrt(-x[3]))
                                                                                 : 0.0;
    Array adjustment = {-spotCorrelation_ * fxVol * vol,
			-spotCorrelation_ * process_->rho() * process_->sigma() * fxVol * vol,
			0.0,
			0.0};
    return adjustment;
}

Array QuantoHestonProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {
    // split state into equity and fx
    Array x0_equity(2), x0_fx(2);
    x0_equity[0] = x0[0];
    x0_equity[1] = x0[1];
    x0_fx[0] = x0[2];
    x0_fx[1] = x0[3];

    // turn 4d vector of independent dw's into correlated dw's
    Array dW_correlated = sqrtCorrelation_ * dw;

    // split correlated dw's into equity and fx parts 
    Array dW_correlated_equity(2), dW_correlated_fx(2);
    dW_correlated_equity[0] = dW_correlated[0];
    dW_correlated_equity[1] = dW_correlated[1];
    dW_correlated_fx[0] = dW_correlated[2];
    dW_correlated_fx[1] = dW_correlated[3];

    // decorrelate equity sub-system
    Array dW_decorrelated_equity = decorrelationMatrixEquity_ * dW_correlated_equity;

    // decorrelate fx sub-system
    Array dW_decorrelated_fx = decorrelationMatrixFx_ * dW_correlated_fx;

    // QuantLib Heston evolution
    Array hestonEvolution = process_->evolve(t0, x0_equity, dt, dW_decorrelated_equity);
    Array fxEvolution = fxProcess_->evolve(t0, x0_fx, dt, dW_decorrelated_fx);

    // Combine evolutions and add adjustments to the equity system
    Array a = driftAdjustment(t0, x0);
    Array evolution(4);
    evolution[0] = hestonEvolution[0] * std::exp(a[0] * dt);
    evolution[1] = hestonEvolution[1] + a[1] * dt;
    evolution[2] = fxEvolution[0];
    evolution[3] = fxEvolution[1];

    // Ensure that quanto-adjusted variance processes remain non-negative when using QE etc
    if (evolution[1] < 0 && process_->discretization() != HestonProcess::Reflection &&
        process_->discretization() != HestonProcess::FullTruncation &&
        process_->discretization() != HestonProcess::PartialTruncation) {
        evolution[1] = 0.0;
    }

    return evolution;
}
}
