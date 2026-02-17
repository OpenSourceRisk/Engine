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

#include <qle/termstructures/simplemclocalvolstochasticratescorrection.hpp>

#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/pricingengines/blackformula.hpp>

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
    Handle<BlackVolTermStructure> blackVol, Handle<LocalVolTermStructure> source, ext::shared_ptr<IrModel> r,
    ext::shared_ptr<IrModel> q, ext::shared_ptr<FxModel> S, Handle<YieldTermStructure> r0,
    Handle<YieldTermStructure> q0, std::function<Array(Real, Real)> dwGenerator, std::vector<Real> times,
    std::vector<Real> logStrikes, Size nPaths)
    : blackVol_(blackVol), source_(std::move(source)), r_(std::move(r)), q_(std::move(q)), S_(std::move(S)),
      r0_(std::move(r0)), q0_(std::move(q0)), dwGenerator_(std::move(dwGenerator)), times_(std::move(times)),
      logStrikes_(std::move(logStrikes)), nPaths_(nPaths) {

    QL_REQUIRE(!times_.empty(), "SimpleMcLocalVolStochasticRatesCorrection: times are empty");
    QL_REQUIRE(!logStrikes_.empty(), "SimpleMcLocalVolStochasticRatesCorrection: strikes are empty");

    if (!close_enough(times_.front(), 0.0))
        times_.insert(times_.begin(), 0.0);

    correctionData_ = Matrix(logStrikes_.size(), times_.size(), 0.0);
}

void SimpleMcLocalVolStochasticRatesCorrection::update() {
    TermStructure::update();
    LazyObject::update();
}

void SimpleMcLocalVolStochasticRatesCorrection::performCalculations() const {

    Size m_r = r_->m() + r_->m_aux();
    Size m_q = q_->m() + q_->m_aux();
    Size m_s = S_->m() + S_->m_aux();

    std::vector<Array> rState(nPaths_, r_->initialValue());
    std::vector<Array> qState(nPaths_, q_->initialValue());
    std::vector<Array> sState(nPaths_, S_->initialValue());

    std::vector<Real> r(nPaths_);
    std::vector<Real> q(nPaths_);
    std::vector<Real> n(nPaths_);

    correction_ = std::make_unique<FlatExtrapolator2D>(QuantLib::ext::make_shared<BilinearInterpolation>(
        times_.begin(), times_.end(), logStrikes_.begin(), logStrikes_.end(), correctionData_));

    for (Size i = 1; i < times_.size(); ++i) {

        Real t0 = times_[i - 1];
        Real t1 = times_[i];
        Real dt = t1 - t0;

        // generate path values

        for (Size p = 0; p < nPaths_; ++p) {

            Array dw = dwGenerator_(t0, dt);

            rState[p] = r_->marginalStep(t0, rState[p], dt, getProjectedArray(dw, 0, m_r));
            qState[p] = q_->marginalStep(t0, qState[p], dt, getProjectedArray(dw, m_r, m_q));
            sState[p] = S_->marginalStep(t0, sState[p], dt, getProjectedArray(dw, m_r + m_q, m_s), r[p], q[p]);

            r[p] = r_->shortRate(t0, rState[p], r0_);
            q[p] = q_->shortRate(t0, qState[p], q0_);

            n[p] = r_->numeraire(t0, getProjectedArray(rState[p], 0, m_r), r0_);
        }

        // sort path data by S

        std::vector<Size> pm(nPaths_);
        std::iota(pm.begin(), pm.end(), 0);
        std::sort(pm.begin(), pm.end(), [&sState](Size i, Size j) { return sState[i][0] > sState[j][0]; });

        // iterate over strikes and accumulate results

        Real F = S_->fxSpotToday()->value() * q0_->discount(t1) / r0_->discount(t1);

        Real r0 = r0_->forwardRate(t0, t0, Continuous);
        Real q0 = q0_->forwardRate(t0, t0, Continuous);
        Real dsc = r0_->discount(t0);

        Real e1 = 0.0, e2 = 0.0;

        Size p = 0;
        Real K_prev;

        for (int k = logStrikes_.size() - 1; k >= 0; --k) {

            Real K = F * std::exp(logStrikes_[k]);

            if (k < logStrikes_.size() - 1) {
                e2 += K_prev - K;
            }

            Real n1 = 0.0, n2 = 0.0;

            while (sState[pm[p]][0] > logStrikes_[k] + std::log(F)) {

                n1 += (r[pm[p]] - q[pm[p]]) / n[pm[p]];
                n2 += q[pm[p]] * (std::exp(sState[pm[p]][0]) - K) / n[pm[p]];

                ++p;
            }

            e1 += static_cast<Real>(n1) / static_cast<Real>(nPaths_);
            e2 += static_cast<Real>(n2) / static_cast<Real>(nPaths_);

            K_prev = K;

            constexpr Real eps = 1E-4;
            Real volK = blackVol_->blackVol(t0, K);
            Real volKm = blackVol_->blackVol(t0, K - eps);
            Real volKp = blackVol_->blackVol(t0, K + eps);
            Real C = blackFormula(Option::Type::Call, K, F, volK, dsc);
            Real Cm = blackFormula(Option::Type::Call, K - eps, F, volKm, dsc);
            Real Cp = blackFormula(Option::Type::Call, K + eps, F, volKp, dsc);
            Real dCdK = (Cp - Cm) / (2.0 * eps);
            Real d2CdK2 = (Cp - 2.0 * Cm + Cm) / (eps * eps);

            correctionData_(k, i - 1) =
                std::sqrt(((-K * e1 + e2) - (K * dCdK * (r0 - q0) + q0 * C)) / (0.5 * K * K * d2CdK2));
        }

        correction_->update();
    }
}

Volatility SimpleMcLocalVolStochasticRatesCorrection::localVolImpl(Time t, Real strike) const {
    return std::max(0.0, source_->localVol(t, strike) + correction_->operator()(t, strike));
}

} // namespace QuantExt
