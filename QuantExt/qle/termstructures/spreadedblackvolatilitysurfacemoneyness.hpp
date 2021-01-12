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

/*! \file spreadedblackvolatilitysurfacemoneyness.hpp
 \brief Spreaded Black volatility surface based on moneyness
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
class SpreadedBlackVolatilitySurfaceMoneyness : public LazyObject, public BlackVolatilityTermStructure {
public:
    /* The smile dynamics is defined in terms of the forward curve defined by the spot and moving term structure.
       The sticky spot and term structures define the sticky forward curve of the reference vol instead, which
       should not react to the moving forward curve. */
    SpreadedBlackVolatilitySurfaceMoneyness(const Handle<BlackVolTermStructure>& referenceVol,
                                            const Handle<Quote>& movingSpot, const std::vector<Time>& times,
                                            const std::vector<Real>& moneyness,
                                            const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                            const Handle<Quote>& stickySpot,
                                            const Handle<YieldTermStructure>& stickyDividendTs,
                                            const Handle<YieldTermStructure>& stickyRiskFreeTs,
                                            const Handle<YieldTermStructure>& movingDividendTs,
                                            const Handle<YieldTermStructure>& movingRiskFreeTs, bool stickyStrike);

    Date maxDate() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    Real minStrike() const override;
    Real maxStrike() const override;
    void update() override;

    const std::vector<QuantLib::Real>& moneyness() const;

protected:
    virtual Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const = 0;
    virtual Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const = 0;

    Handle<BlackVolTermStructure> referenceVol_;
    Handle<Quote> movingSpot_;
    std::vector<Time> times_;
    std::vector<Real> moneyness_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    Handle<Quote> stickySpot_;
    Handle<YieldTermStructure> stickyDividendTs_;
    Handle<YieldTermStructure> stickyRiskFreeTs_;
    Handle<YieldTermStructure> movingDividendTs_;
    Handle<YieldTermStructure> movingRiskFreeTs_;
    bool stickyStrike_;

    mutable Matrix data_;
    mutable Interpolation2D volSpreadSurface_;

private:
    void performCalculations() const override;
    Real blackVolImpl(Time t, Real strike) const override;
};

//! Spreaded Black volatility surface based on spot moneyness
class SpreadedBlackVolatilitySurfaceMoneynessSpot : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

//! Black volatility surface based on forward moneyness
class SpreadedBlackVolatilitySurfaceMoneynessForward : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

//! Spreaded Black volatility surface based on spot log moneyness
class SpreadedBlackVolatilitySurfaceLogMoneynessSpot : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

//! Black volatility surface based on forward log moneyness
class SpreadedBlackVolatilitySurfaceLogMoneynessForward : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

//! Black volatility surface based on std devs (standardised log moneyness)
class SpreadedBlackVolatilitySurfaceStdDevs : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

//! Spreaded Black volatility surface based on absolute spot moneyness
class SpreadedBlackVolatilitySurfaceMoneynessSpotAbsolute : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

//! Spreaded Black volatility surface based on absolute forward moneyness
class SpreadedBlackVolatilitySurfaceMoneynessForwardAbsolute : public SpreadedBlackVolatilitySurfaceMoneyness {
public:
    using SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness;

private:
    Real strikeFromMoneyness(Time t, Real moneyness, const bool stickyReference) const override;
    Real moneynessFromStrike(Time t, Real strike, const bool stickyReference) const override;
};

} // namespace QuantExt
