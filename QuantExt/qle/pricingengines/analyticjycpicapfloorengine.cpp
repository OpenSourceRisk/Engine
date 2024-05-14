/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/pricingengines/analyticjycpicapfloorengine.hpp>

using namespace QuantExt::CrossAssetAnalytics;
using QuantLib::DiscountFactor;
using QuantLib::Real;
using QuantLib::SimpleCashFlow;
using QuantLib::Size;
using std::max;
using std::pow;
using std::sqrt;

namespace QuantExt {

AnalyticJyCpiCapFloorEngine::AnalyticJyCpiCapFloorEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index)
    : model_(model), index_(index) {}

void AnalyticJyCpiCapFloorEngine::calculate() const {

    // If the pay date has occurred, nothing to do.
    SimpleCashFlow cf(0.0, arguments_.payDate);
    if (cf.hasOccurred()) {
        results_.value = 0.0;
        return;
    }

    // Discount factor to pay date will be needed below.
    Size irIdx = model_->ccyIndex(model_->infjy(index_)->currency());
    DiscountFactor df = model_->irlgm1f(irIdx)->termStructure()->discount(arguments_.payDate);

    QL_DEPRECATED_DISABLE_WARNING
    bool interpolate = arguments_.observationInterpolation == CPI::Linear ||
                       (arguments_.observationInterpolation == CPI::AsIndex && arguments_.index->interpolated());
    QL_DEPRECATED_ENABLE_WARNING
    // Get the time to expiry. This determines if we use the JY model or look for an inflation index fixing.
    auto zts = model_->infjy(index_)->realRate()->termStructure();
    Real t = inflationYearFraction(arguments_.index->frequency(), interpolate, zts->dayCounter(), zts->baseDate(),
                                   arguments_.fixDate);

    // If time to expiry is non-positive, we return the discounted value of the settled amount.
    // CPICapFloor should really have its own day counter for going from strike rate to k. We use t here.
    Real k = pow(1.0 + arguments_.strike, t);
    if (t <= 0.0) {
        auto cpiAtExpiry = arguments_.index->fixing(arguments_.fixDate);
        if (arguments_.type == Option::Call) {
            results_.value = max(cpiAtExpiry / arguments_.baseCPI - k, 0.0);
        } else {
            results_.value = max(k - cpiAtExpiry / arguments_.baseCPI, 0.0);
        }
        results_.value *= arguments_.nominal * df;
        return;
    }

    // Section 13, Book. ZCII Cap. Note that there is a difference between the base CPI value associated with the
    // inflation term structures and used as a starting point in the CAM evolution vs. the contractual base CPI in
    // the CPICapFloor instrument. The former is curveBaseCpi below and the latter is arguments_.baseCPI.

    // Calculate the variance, \Sigma^2_I in the book.
    Real H_n_t = Hz(irIdx).eval(*model_, t);
    Real H_r_t = Hy(index_).eval(*model_, t);
    Real v = integral(*model_, P(LC(H_n_t, -1.0, Hz(irIdx)), LC(H_n_t, -1.0, Hz(irIdx)), az(irIdx), az(irIdx)), 0.0, t);
    v += integral(*model_, P(LC(H_r_t, -1.0, Hy(index_)), LC(H_r_t, -1.0, Hy(index_)), ay(index_), ay(index_)), 0.0, t);
    v += integral(*model_, P(sy(index_), sy(index_)), 0.0, t);
    v -= 2 *
         integral(*model_,
                  P(rzy(irIdx, index_), LC(H_r_t, -1.0, Hy(index_)), LC(H_n_t, -1.0, Hz(irIdx)), ay(index_), az(irIdx)),
                  0.0, t);
    v += 2 * integral(*model_, P(rzy(irIdx, index_, 1), LC(H_n_t, -1.0, Hz(irIdx)), az(irIdx), sy(index_)), 0.0, t);
    v -= 2 *
         integral(*model_, P(ryy(index_, index_, 0, 1), LC(H_r_t, -1.0, Hy(index_)), ay(index_), sy(index_)), 0.0, t);

    // Calculate the forward CPI, F_I(0,T) in the book.
    Real fwd = arguments_.index->fixing(arguments_.fixDate);

    // Get adjusted nominal and strike, \tilde{N} and \tilde{K} from the book.
    Real adjNominal = arguments_.nominal / arguments_.baseCPI;
    Real adjStrike = k * arguments_.baseCPI;

    results_.value = adjNominal * blackFormula(arguments_.type, adjStrike, fwd, sqrt(v), df);
}

} // namespace QuantExt
