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

#include <qle/methods/fdmquantohelper.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

FdmQuantoHelper::FdmQuantoHelper(const ext::shared_ptr<YieldTermStructure>& rTS,
                                 const ext::shared_ptr<YieldTermStructure>& fTS,
                                 const ext::shared_ptr<BlackVolTermStructure>& fxVolTS, const Real equityFxCorrelation,
                                 const Real fxStrike, const Real initialFxSpot, const bool discounting,
                                 const bool ensureNonNegativeForwardVariance)
    : rTS_(rTS), fTS_(fTS), fxVolTS_(fxVolTS), equityFxCorrelation_(equityFxCorrelation), fxStrike_(fxStrike),
      initialFxSpot_(initialFxSpot), discounting_(discounting),
      ensureNonNegativeForwardVariance_(ensureNonNegativeForwardVariance) {
    QL_REQUIRE(fxStrike_ != Null<Real>() || initialFxSpot != Null<Real>(),
               "initialFxSpot must be given, if fxStrike is null (=atmf)");
}

Rate FdmQuantoHelper::quantoAdjustment(Volatility equityVol, Time t1, Time t2) const {
    Rate rDomestic = 0.0, rForeign = 0.0;
    if (fxStrike_ == Null<Real>() || discounting_) {
        rDomestic = rTS_->forwardRate(t1, t2, Continuous).rate();
        rForeign = fTS_->forwardRate(t1, t2, Continuous).rate();
    }
    Real k1, k2;
    if (fxStrike_ == Null<Real>()) {
        k1 = initialFxSpot_ * fTS_->discount(t1) / rTS_->discount(t1);
        k2 = initialFxSpot_ * fTS_->discount(t2) / rTS_->discount(t2);
    } else {
        k1 = k2 = fxStrike_;
    }
    Real v = ((close_enough(t2, 0.0) ? 0.0 : fxVolTS_->blackVariance(t2, k2)) -
              (close_enough(t1, 0.0) ? 0.0 : fxVolTS_->blackVariance(t1, k1))) /
             (t2 - t1);
    if (ensureNonNegativeForwardVariance_)
        v = std::max(v, 0.0);
    const Volatility fxVol = std::sqrt(v);
    return (discounting_ ? (rDomestic - rForeign) : 0.0) + equityVol * fxVol * equityFxCorrelation_;
}

Array FdmQuantoHelper::quantoAdjustment(const Array& equityVol, Time t1, Time t2) const {
    Rate rDomestic = 0.0, rForeign = 0.0;
    if (fxStrike_ == Null<Real>() || discounting_) {
        rDomestic = rTS_->forwardRate(t1, t2, Continuous).rate();
        rForeign = fTS_->forwardRate(t1, t2, Continuous).rate();
    }
    Real k1, k2;
    if (fxStrike_ == Null<Real>()) {
        k1 = initialFxSpot_ * fTS_->discount(t1) / rTS_->discount(t1);
        k2 = initialFxSpot_ * fTS_->discount(t2) / rTS_->discount(t2);
    } else {
        k1 = k2 = fxStrike_;
    }
    Real v = ((close_enough(t2, 0.0) ? 0.0 : fxVolTS_->blackVariance(t2, k2)) -
              (close_enough(t1, 0.0) ? 0.0 : fxVolTS_->blackVariance(t1, k1))) /
             (t2 - t1);
    if (ensureNonNegativeForwardVariance_)
        v = std::max(v, 0.0);
    const Volatility fxVol = std::sqrt(v);
    Array retVal(equityVol.size());
    for (Size i = 0; i < retVal.size(); ++i) {
        retVal[i] = (discounting_ ? (rDomestic - rForeign) : 0.0) + equityVol[i] * fxVol * equityFxCorrelation_;
    }

    return retVal;
}

} // namespace QuantExt
