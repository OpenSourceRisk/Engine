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

/*! \file fdmquantohelper.hpp
    \brief extended version of the QuantLib class, see the documentation for details
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/patterns/observable.hpp>

namespace QuantLib {
class YieldTermStructure;
class BlackVolTermStructure;
} // namespace QuantLib

namespace QuantExt {
using namespace QuantLib;

/*! As the ql class, but fxStrike can be null in which case the atmf level is used as a strike, more precisely
    we compute

    forward vol = sqrt( ( V(t2, k2) - V(t1, k1) ) / (t2-t1) )

    where k1, k2 are the atmf levels at t1 and t2. If fxStrike = null, the initialFxSpot value must be given.

    If discounting = false, the adjustment will omit the rTS and fTS terms.

   If ensureNonNegativeForwardVariance is true, the forward variances from the input vol ts are floored at zero. */
class FdmQuantoHelper : public Observable {
public:
    FdmQuantoHelper(const ext::shared_ptr<YieldTermStructure>& rTS, const ext::shared_ptr<YieldTermStructure>& fTS,
                    const ext::shared_ptr<BlackVolTermStructure>& fxVolTS, const Real equityFxCorrelation,
                    const Real fxStrike, Real initialFxSpot = Null<Real>(), const bool discounting = true,
                    const bool ensureNonNegativeForwardVariance = false);

    Rate quantoAdjustment(Volatility equityVol, Time t1, Time t2) const;
    Array quantoAdjustment(const Array& equityVol, Time t1, Time t2) const;

private:
    const ext::shared_ptr<YieldTermStructure> rTS_, fTS_;
    const ext::shared_ptr<BlackVolTermStructure> fxVolTS_;
    const Real equityFxCorrelation_, fxStrike_, initialFxSpot_;
    const bool discounting_, ensureNonNegativeForwardVariance_;
};

} // namespace QuantExt
