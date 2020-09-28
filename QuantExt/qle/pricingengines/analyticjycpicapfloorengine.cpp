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

#include <qle/pricingengines/analyticjycpicapfloorengine.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <ql/pricingengines/blackformula.hpp>

using namespace QuantExt::CrossAssetAnalytics;
using QuantLib::Real;
using QuantLib::Size;
using std::pow;
using std::sqrt;

namespace QuantExt {

AnalyticJyCpiCapFloorEngine::AnalyticJyCpiCapFloorEngine(
    const boost::shared_ptr<CrossAssetModel>& model, Size index)
    : model_(model), index_(index) {}

void AnalyticJyCpiCapFloorEngine::calculate() const {

    // Get the time to expiry
    bool interpolate = arguments_.observationInterpolation == CPI::Linear ||
        (arguments_.observationInterpolation == CPI::AsIndex && arguments_.infIndex->interpolated());
    auto zts = model_->infjy(index_)->realRate()->termStructure();
    Real t = inflationYearFraction(arguments_.infIndex->frequency(), interpolate,
        zts->dayCounter(), zts->baseDate(), arguments_.fixDate);

    // If time to expiry is non-positive, return 0. There is no valuation of possibly unsettled cashflows.
    if (t <= 0.0) {
        results_.value = 0.0;
        return;
    }

    // Section 13, Book. ZCII Cap. Note, there is a cancellation of I(0).
    const CrossAssetModel* x = model_.get();
    Size irIdx = x->ccyIndex(x->infjy(index_)->currency());
    Real k = pow(1.0 + arguments_.strike, t);
    Real f = pow(1.0 + zts->zeroRate(arguments_.fixDate), t);

    Real H_n_t = Hz(irIdx).eval(x, t);
    Real H_r_t = Hy(index_).eval(x, t);
    Real v = integral(x, P(LC(H_n_t, -1.0, Hz(irIdx)), LC(H_n_t, -1.0, Hz(irIdx)), az(irIdx), az(irIdx)), 0.0, t);
    v += integral(x, P(LC(H_r_t, -1.0, Hy(index_)), LC(H_r_t, -1.0, Hy(index_)), ay(index_), ay(index_)), 0.0, t);
    v += integral(x, P(sy(index_), sy(index_)), 0.0, t);
    v -= 2 * integral(x, P(rzy(irIdx, index_), LC(H_r_t, -1.0, Hy(index_)), LC(H_n_t, -1.0, Hz(irIdx)),
        ay(index_), az(irIdx)), 0.0, t);
    v += 2 * integral(x, P(rzy(irIdx, index_, 1), LC(H_n_t, -1.0, Hz(irIdx)), az(irIdx), sy(index_)), 0.0, t);
    v -= 2 * integral(x, P(ryy(index_, index_, 0, 1), LC(H_r_t, -1.0, Hy(index_)), ay(index_), sy(index_)), 0.0, t);

    Real discount = model_->irlgm1f(irIdx)->termStructure()->discount(arguments_.payDate);
    results_.value = arguments_.nominal * blackFormula(arguments_.type, k, f, sqrt(v), discount);
}

}
