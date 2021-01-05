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

/*! \file spreadedblackvolatilityesurfacemoneyness.hpp
 \brief Spreaded Black volatility surface based on forward moneyness
 \ingroup termstructures
 */

#pragma once

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Abstract Spreaded Black volatility surface based on moneyness (moneyness defined in subclasses)
// see non-spreaded class BlackVarianceSurfaceMoneyness for todos and other details
class SpreadedBlackVolatilitySurfaceMoneyness : public LazyObject, public BlackVolatilityTermStructure {
public:
    SpreadedBlackVolatilitySurfaceMoneyness(const Handle<BlackVolTermStructure>& referenceVol,
                                            const Handle<Quote>& spot, const std::vector<Time>& times,
                                            const std::vector<Real>& moneyness,
                                            const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                            bool stickyStrike);

    Date maxDate() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    Real minStrike() const override;
    Real maxStrike() const override;
    void update() override;

    std::vector<QuantLib::Real> moneyness() const { return moneyness_; }

protected:
    virtual Real moneyness(Time t, Real strike) const = 0;

    Handle<BlackVolTermStructure> referenceVol_;
    Handle<Quote> spot_;
    std::vector<Time> times_;
    std::vector<Real> moneyness_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    bool stickyStrike_;
    mutable Matrix data_;
    mutable Interpolation2D volSpreadSurface_;

private:
    void performCalculations() const override;
    Real blackVolImpl(Time t, Real strike) const override;

    // Shared initialisation
    void init();
};

//! Spreaded Black volatility surface based on spot moneyness
class SpreadedBlackVolatilitySurfaceMoneynessSpot : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    SpreadedBlackVolatilitySurfaceMoneynessSpot(const Handle<BlackVolTermStructure>& referenceVol,
                                                const Handle<Quote>& spot, const std::vector<Time>& times,
                                                const std::vector<Real>& moneyness,
                                                const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                                bool stickyStrike = false);

private:
    Real moneyness(Time t, Real strike) const override;
};

//! Black volatility surface based on forward moneyness
class SpreadedBlackVolatilitySurfaceMoneynessForward : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    SpreadedBlackVolatilitySurfaceMoneynessForward(const Handle<BlackVolTermStructure>& referenceVol,
                                                   const Handle<Quote>& spot, const std::vector<Time>& times,
                                                   const std::vector<Real>& moneyness,
                                                   const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                                   const Handle<YieldTermStructure>& forTS,
                                                   const Handle<YieldTermStructure>& domTS, bool stickyStrike = false);

private:
    void init();

    virtual Real moneyness(Time t, Real strike) const;
    Handle<YieldTermStructure> forTS_; // calculates fwd if StickyStrike==false
    Handle<YieldTermStructure> domTS_;
    std::vector<Real> forwards_; // cache fwd values if StickyStrike==true
    QuantLib::Interpolation forwardCurve_;
};

} // namespace QuantExt
