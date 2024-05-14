/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/models/crossassetanalytics.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>

#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

AnalyticDkCpiCapFloorEngine::AnalyticDkCpiCapFloorEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                                         const Size index, const Real baseCPI)
    : model_(model), index_(index), baseCPI_(baseCPI) {}

void AnalyticDkCpiCapFloorEngine::calculate() const {

    QL_DEPRECATED_DISABLE_WARNING
    bool interpolate = arguments_.observationInterpolation == CPI::Linear ||
                       (arguments_.observationInterpolation == CPI::AsIndex && arguments_.index->interpolated());
    QL_DEPRECATED_ENABLE_WARNING

    Real t = inflationYearFraction(arguments_.index->frequency(), interpolate,
                                   model_->infdk(index_)->termStructure()->dayCounter(),
                                   model_->infdk(index_)->termStructure()->baseDate(), arguments_.fixDate);

    if (t <= 0.0) {
        // option is expired, we do not value any possibly non settled
        // flows, i.e. set the npv to zero in this case
        results_.value = 0.0;
        return;
    }

    // Book, 13.37, 13.38

    Real k = std::pow(1.0 + arguments_.strike, t);
    Real kTilde = k * arguments_.baseCPI;
    Real nTilde = arguments_.nominal / arguments_.baseCPI;

    Real m = baseCPI_ *
             std::pow(1.0 + model_->infdk(index_)->termStructure()->zeroRate(arguments_.fixDate, Period(0, Days)), t);
    m = arguments_.index->fixing(arguments_.fixDate);
    Real Ht = Hy(index_).eval(*model_, t);
    Real v = Ht * Ht * zetay(index_).eval(*model_, t) -
             2.0 * Ht * integral(*model_, P(Hy(index_), ay(index_), ay(index_)), 0.0, t) +
             integral(*model_, P(Hy(index_), Hy(index_), ay(index_), ay(index_)), 0.0, t);

    Size irIdx = model_->ccyIndex(model_->infdk(index_)->currency());
    Real discount = model_->irlgm1f(irIdx)->termStructure()->discount(arguments_.payDate);

    results_.value = nTilde * blackFormula(arguments_.type, kTilde, m, std::sqrt(v), discount);

} // calculate()

} // namespace QuantExt
