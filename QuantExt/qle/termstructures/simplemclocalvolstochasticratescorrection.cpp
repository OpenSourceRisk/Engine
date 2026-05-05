/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <qle/methods/multipathvariategenerator.hpp>
#include <qle/termstructures/simplemclocalvolstochasticratescorrection.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <fstream>

using namespace QuantLib;

namespace {
inline Array getProjectedArray(const Array& source, Size start, Size length) {
    QL_REQUIRE(source.size() >= start + length, "getProjectedArray(): internal errors: source size "
                                                    << source.size() << ", start" << start << ", length " << length);
    return Array(std::next(source.begin(), start), std::next(source.begin(), start + length));
}
} // namespace

namespace QuantExt {

SimpleMcLocalVolStochasticRatesCorrection::SimpleMcLocalVolStochasticRatesCorrection(
    Handle<BlackVolTermStructure> blackVol, QuantLib::Handle<QuantLib::LocalVolTermStructure> source,
    QuantLib::ext::shared_ptr<IrModel> r, QuantLib::ext::shared_ptr<IrModel> q, QuantLib::ext::shared_ptr<FxModel> S,
    QuantLib::Handle<QuantLib::YieldTermStructure> r0, QuantLib::Handle<QuantLib::YieldTermStructure> q0,
    Matrix correlation_r_q_S, Real maxTime, Real minStrike, Real maxStrike, Size timeStepsPerYear, Size nStrikes,
    Size nPaths, Real d2CdK2Threshold, Size nPasses)
    : blackVol_(std::move(blackVol)), source_(std::move(source)), r_(std::move(r)), q_(std::move(q)), S_(std::move(S)),
      r0_(std::move(r0)), q0_(std::move(q0)), correlation_r_q_S_(std::move(correlation_r_q_S)), maxTime_(maxTime),
      minStrike_(minStrike), maxStrike_(maxStrike), timeStepsPerYear_(timeStepsPerYear), nStrikes_(nStrikes),
      nPaths_(nPaths), d2CdK2Threshold_(d2CdK2Threshold), nPasses_(nPasses) {

    timeGrid_ = TimeGrid(maxTime_, static_cast<Size>(0.5 + maxTime_ * static_cast<Real>(timeStepsPerYear_)));

    atmVarianceData_.resize(timeGrid_.size());
    for (Size i = 0; i < timeGrid_.size(); ++i) {
        Real F = S_->fxSpotToday()->value() * q0_->discount(timeGrid_[i]) / r0_->discount(timeGrid_[i]);
        atmVarianceData_[i] = i == 0 ? 0.0 : blackVol_->blackVariance(timeGrid_[i], F);
    }

    correctionData_ = Matrix(nStrikes_ - 1, timeGrid_.size() - 2, 0.0);

    logStrikes_.resize(nStrikes_);

    Real k = minStrike_;
    for (Size i = 0; i < nStrikes_; ++i) {
        logStrikes_[i] = k;
        k += (maxStrike_ - minStrike_) / static_cast<Real>(nStrikes_ - 1);
    }

    logStrikesMid_.resize(nStrikes_ - 1);
    for (Size i = 0; i < nStrikes_ - 1; ++i) {
        logStrikesMid_[i] = 0.5 * (logStrikes_[i] + logStrikes_[i + 1]);
    }

    atmVariance_ = std::make_unique<LinearInterpolation>(timeGrid_.begin(), timeGrid_.end(), atmVarianceData_.begin());
    atmVariance_->enableExtrapolation();

    correction_ = std::make_unique<FlatExtrapolator2D>(QuantLib::ext::make_shared<BilinearInterpolation>(
        timeGrid_.begin(), std::next(timeGrid_.end(), -1), logStrikesMid_.begin(), logStrikesMid_.end(),
        correctionData_));
    correction_->enableExtrapolation();

    this->enableExtrapolation();
}

void SimpleMcLocalVolStochasticRatesCorrection::update() {
    TermStructure::update();
    LazyObject::update();
}

void SimpleMcLocalVolStochasticRatesCorrection::performCalculations() const {

    Size m_r = r_->m() + r_->m_aux();
    Size m_q = q_->m() + q_->m_aux();
    Size m_s = S_->m() + S_->m_aux();

    // generate variates

    MultiPathVariateGeneratorSobolBrownianBridge varGen(m_r + m_q + m_s, timeGrid_.size() - 1);

    std::vector<std::vector<Array>> variates(nPaths_);

    for (Size p = 0; p < nPaths_; ++p) {
        variates[p] = varGen.next().value;
    }

    // matrix root to generate correlated brownian shocks for the r-q-S model

    Matrix sqrtCorrelation = pseudoSqrt(correlation_r_q_S_);

    // mc simulation to determine correction term

    std::vector<Array> rState(nPaths_, r_->initialValue());
    std::vector<Array> qState(nPaths_, q_->initialValue());
    std::vector<Array> sState(nPaths_, S_->initialValue());
    std::vector<Array> sStateProv(nPaths_, S_->initialValue());
    std::vector<Array> rState_u(nPaths_, r_->initialValue());
    std::vector<Array> qState_u(nPaths_, q_->initialValue());
    std::vector<Array> sState_u(nPaths_, S_->initialValue());

    std::vector<Real> r(nPaths_);
    std::vector<Real> q(nPaths_);
    std::vector<Real> n(nPaths_);
    std::vector<Real> r_u(nPaths_);
    std::vector<Real> q_u(nPaths_);
    std::vector<Real> n_u(nPaths_);

    std::vector<Real> e1_u(logStrikes_.size() - 1);
    std::vector<Real> e2_u(logStrikes_.size() - 1);

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {

        Real d2CdK2;

        Real t0 = timeGrid_[i];
        Real t1 = timeGrid_[i + 1];
        Real dt = t1 - t0;

        for (Size pass = 0; pass < nPasses_ && i > 0; ++pass) {

            // generate path values

            for (Size p = 0; p < nPaths_; ++p) {

                Array dw = variates[p][i] * sqrtCorrelation;

                if (pass == 0) {
                    rState[p] = r_->marginalStep(t0, rState[p], dt, getProjectedArray(dw, 0, m_r));
                    qState[p] = q_->marginalStep(t0, qState[p], dt, getProjectedArray(dw, m_r, m_q));
                    r[p] = -std::log(r_->discountBond(t0, t1, rState[p], r0_)) / dt;
                    q[p] = -std::log(q_->discountBond(t0, t1, qState[p], q0_)) / dt;
                    n[p] = r_->numeraire(t1, rState[p], r0_);

                    rState_u[p] = r_->marginalStep(t0, rState_u[p], dt, Array(m_r, 0.0));
                    qState_u[p] = q_->marginalStep(t0, qState_u[p], dt, Array(m_q, 0.0));
                    r_u[p] = -std::log(r_->discountBond(t0, t1, rState_u[p], r0_)) / dt;
                    q_u[p] = -std::log(q_->discountBond(t0, t1, qState_u[p], q0_)) / dt;
                    n_u[p] = r_->numeraire(t1, rState_u[p], r0_);

                    sState_u[p] = S_->marginalStep(t0, sState_u[p], dt, getProjectedArray(dw, m_r + m_q, m_s), r_u[p],
                                                   q_u[p], FxModel::Discretization::Euler);
                }

                sStateProv[p] = S_->marginalStep(t0, sState[p], dt, getProjectedArray(dw, m_r + m_q, m_s), r[p], q[p],
                                                 FxModel::Discretization::Euler);
            }

            // iterate over strikes and accumulate results

            Real F = S_->fxSpotToday()->value() * q0_->discount(t1) / r0_->discount(t1);

            Real K_prev = logStrikes_.back();

            for (int k = logStrikes_.size() - 2; k >= 0; --k) {

                Real K = F * std::exp(logStrikes_[k] * std::sqrt(atmVarianceData_[i]));

                Real d2CdK2Count = 0.0;
                Real n1 = 0.0, n2 = 0.0;
                for (Size p = 0; p < nPaths_; ++p) {
                    if (sStateProv[p][0] > std::log(K)) {
                        n1 += (r[p] - q[p]) / n[p];
                        n2 += q[p] * (std::exp(sStateProv[p][0]) - K) / n[p];
                        if (sStateProv[p][0] < std::log(K_prev)) {
                            d2CdK2Count += 1.0 / n[p];
                        }
                    }
                }

                Real e1 = static_cast<Real>(n1) / static_cast<Real>(nPaths_);
                Real e2 = static_cast<Real>(n2) / static_cast<Real>(nPaths_);
                d2CdK2 = d2CdK2Count / static_cast<Real>(nPaths_) / (K_prev - K);

                if (pass == 0) {
                    Real n1_u = 0.0, n2_u = 0.0;
                    for (Size p = 0; p < nPaths_; ++p) {
                        if (sState_u[p][0] > std::log(K)) {
                            n1_u += (r_u[p] - q_u[p]) / n_u[p];
                            n2_u += q_u[p] * (std::exp(sState_u[p][0]) - K) / n_u[p];
                        }
                    }
                    e1_u[k] = static_cast<Real>(n1_u) / static_cast<Real>(nPaths_);
                    e2_u[k] = static_cast<Real>(n2_u) / static_cast<Real>(nPaths_);
                }

                Real Kmid = F * std::exp(logStrikesMid_[k] * std::sqrt(atmVarianceData_[i]));

                if (d2CdK2 * (K_prev - K) > d2CdK2Threshold_) {
                    correctionData_(k, i) = (-Kmid * (e1 - e1_u[k]) + (e2 - e2_u[k])) / (0.5 * Kmid * Kmid * d2CdK2);
                }

                K_prev = K;
            }

        } // loop over passes

        // evolve S with final corrected local vol

        for (Size p = 0; p < nPaths_; ++p) {
            Array dw = variates[p][i] * sqrtCorrelation;
            sState[p] = S_->marginalStep(t0, sState[p], dt, getProjectedArray(dw, m_r + m_q, m_s), r[p], q[p],
                                         FxModel::Discretization::Euler);
        }

    } // loop over time grid

    // debug output
    std::ofstream out("localvol.txt");
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        for (Size j = 0; j < logStrikesMid_.size(); ++j) {
            Real t = timeGrid_[i];
            Real F = S_->fxSpotToday()->value() * q0_->discount(t) / r0_->discount(t);
            Real K = F * std::exp(logStrikesMid_[j] * std::sqrt(atmVarianceData_[i]));
            Real blackVol = blackVol_->blackVol(t, K);
            Real localVol = source_->localVol(t, K);
            Real corr = this->localVol(t, K) - localVol;
            out << t << "," << logStrikesMid_[j] << "," << blackVol << "," << localVol << "," << corr << std::endl;
        }
        out << std::endl;
    }
    out.close();
}

Volatility SimpleMcLocalVolStochasticRatesCorrection::localVolImpl(Time t, Real strike) const {
    calculate();
    Real F = S_->fxSpotToday()->value() * q0_->discount(t) / r0_->discount(t);
    Real s = source_->localVol(t, strike);
    Real c = correction_->operator()(t, std::log(strike / F) / std::max(1E-10, std::sqrt(atmVariance_->operator()(t))));
    return std::sqrt(std::max(0.0, s * s + c));
}

} // namespace QuantExt
