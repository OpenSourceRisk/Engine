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

/*! \file spreadedblackvolatilitysurfacestddevs.hpp
 \brief Spreaded Black volatility surface in terms of standardised moneyness
 */

#pragma once

#include <qle/indexes/eqfxindexbase.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/termstructures/spreadedblackvolatilitysurfacemoneyness.hpp>

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Spreaded Black volatility surface based on standardised moneyness
class SpreadedBlackVolatilitySurfaceStdDevs : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    // referenceVol must support strike values Null<Real>(), to be interpreted as its atm level
    SpreadedBlackVolatilitySurfaceStdDevs(const Handle<BlackVolTermStructure>& referenceVol, const Handle<Quote>& spot,
                                          const std::vector<Time>& times, const std::vector<Real>& stdDevs,
                                          const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                          const boost::shared_ptr<EqFxIndexBase>& index, bool stickyStrike = false);

private:
    Real moneyness(Time t, Real strike) const override;
    boost::shared_ptr<EqFxIndexBase> index_;
    std::vector<Real> forwards_; // cache fwd values if StickyStrike==true
    Interpolation forwardCurve_;
};

} // namespace QuantExt
