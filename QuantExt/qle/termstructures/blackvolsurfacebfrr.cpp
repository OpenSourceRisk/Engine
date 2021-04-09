/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/termstructures/blackvolsurfacebfrr.hpp>

#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

namespace detail {

SimpleDeltaInterpolatedSmile::SimpleDeltaInterpolatedSmile(
    const Real spot, const Real domDisc, const Real forDisc, const Real expiryTime, const std::vector<Real>& deltas,
    const std::vector<Real>& putVols, const std::vector<Real>& callVols, const Real atmVol,
    const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at,
    const BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation, const Real accuracy,
    const Size maxIterations)
    : spot_(spot), domDisc_(domDisc), forDisc_(forDisc), expiryTime_(expiryTime), deltas_(deltas), putVols_(putVols),
      callVols_(callVols), atmVol_(atmVol), dt_(dt), at_(at), smileInterpolation_(smileInterpolation),
      accuracy_(accuracy), maxIterations_(maxIterations) {

    /* Convert the puts, atm, calls to strikes using the given conventions and then to simple delta
       using the given conventions. Store the simple deltas as x-values for the interpolation and
       the log of the vols as y-values */

    for (Size i = 0; i < deltas_.size(); ++i) {
        BlackDeltaCalculator c(Option::Put, dt_, spot_, domDisc_, forDisc_, putVols_[i] * std::sqrt(expiryTime_));
        x_.push_back(simpleDeltaFromStrike(c.strikeFromDelta(-deltas[i])));
        y_.push_back(std::log(putVols_[i]));
    }

    BlackDeltaCalculator c(Option::Call, dt_, spot_, domDisc_, forDisc_, atmVol * std::sqrt(expiryTime_));
    x_.push_back(simpleDeltaFromStrike(c.atmStrike(at_)));
    y_.push_back(std::log(atmVol_));

    for (Size i = 0; i < deltas_.size(); ++i) {
        BlackDeltaCalculator c(Option::Call, dt_, spot_, domDisc_, forDisc_, callVols_[i] * std::sqrt(expiryTime_));
        x_.push_back(simpleDeltaFromStrike(c.strikeFromDelta(deltas[i])));
        y_.push_back(std::log(callVols_[i]));
    }

    /* Create the interpolation object */

    if (smileInterpolation_ == BlackVolatilitySurfaceBFRR::SmileInterpolation::Linear) {
        interpolation_ = boost::make_shared<LinearInterpolation>(x_.begin(), x_.end(), y_.begin());
    } else if (smileInterpolation_ == BlackVolatilitySurfaceBFRR::SmileInterpolation::Cubic) {
        interpolation_ = boost::make_shared<CubicInterpolation>(
            x_.begin(), x_.end(), y_.begin(), CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative,
            0.0, CubicInterpolation::SecondDerivative, 0.0);
    } else {
        QL_FAIL("invalid interpolation, this is unexpected");
    }
}

Real SimpleDeltaInterpolatedSmile::strikeFromDelta(const Option::Type type, const Real delta,
                                                   const DeltaVolQuote::DeltaType dt) {
    Real result = spot_ / domDisc_ * forDisc_, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(expiryTime_) * volatility(result);
        BlackDeltaCalculator c(type, dt, spot_, domDisc_, forDisc_, stddev);
        lastResult = result;
        result = c.strikeFromDelta((type == Option::Call ? 1.0 : -1.0) * delta);
    } while (std::abs(result - lastResult) > accuracy_ && ++iterations < maxIterations_);
    QL_REQUIRE(iterations < maxIterations_, "SmileDeltaInterpolatedSmile::strikeFromDelta: max iterations ("
                                                << maxIterations_ << "), no solution found for accuracy " << accuracy_
                                                << ", last iterations. " << lastResult << ", " << result);
    return result;
}

Real SimpleDeltaInterpolatedSmile::atmStrike(const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at) {
    Real result = spot_ / domDisc_ * forDisc_, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(expiryTime_) * volatility(result);
        BlackDeltaCalculator c(Option::Call, dt, spot_, domDisc_, forDisc_, stddev);
        lastResult = result;
        result = c.atmStrike(at);
    } while (std::abs(result - lastResult) > accuracy_ && ++iterations < maxIterations_);
    QL_REQUIRE(iterations < maxIterations_, "SmileDeltaInterpolatedSmile::atmStrikeFromDelta: max iterations ("
                                                << maxIterations_ << "), no solution found for accuracy " << accuracy_
                                                << ", last iterations. " << lastResult << ", " << result);
    return result;
}

Real SimpleDeltaInterpolatedSmile::volatility(const Real strike) {
    return std::exp((*interpolation_)(simpleDeltaFromStrike(strike)));
}

Real SimpleDeltaInterpolatedSmile::simpleDeltaFromStrike(const Real strike) const {
    CumulativeNormalDistribution Phi;
    return Phi(strike / atmVol_ * std::sqrt(expiryTime_));
}

boost::shared_ptr<SimpleDeltaInterpolatedSmile>
createSmile(const Real spot, const Real domDisc, const Real forDisc, const Real expiryTime,
            const std::vector<Real>& deltas, const std::vector<Real>& bfQuotes, const std::vector<Real>& rrQuotes,
            const Real atmVol, const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at,
            const Option::Type riskReversalInFavorOf, const bool butterflyIsBrokerStyle,
            const BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation) {

    // BlackDeltaCalculator c(Option::Type::Call, dt, spot, domDisc, forDisc, atmVol * std::sqrt(expiryTime));
    // Real k_atm = c.atmStrike(at);
    Real phirr = riskReversalInFavorOf == Option::Call ? 1.0 : -1.0;

    if (!butterflyIsBrokerStyle) {

        // butterfly is not broker style: we can directly compute the call/put strikes and vols ...

        std::vector<Real> k_c, k_p, vol_c, vol_p;

        for (Size i = 0; i < deltas.size(); ++i) {
            vol_p.push_back(atmVol + bfQuotes[i] - rrQuotes[i] / (2.0 * phirr));
            vol_c.push_back(atmVol + bfQuotes[i] + rrQuotes[i] / (2.0 * phirr));
            BlackDeltaCalculator cp(Option::Type::Put, dt, spot, domDisc, forDisc,
                                    vol_p.back() * std::sqrt(expiryTime));
            BlackDeltaCalculator cc(Option::Type::Call, dt, spot, domDisc, forDisc,
                                    vol_p.back() * std::sqrt(expiryTime));
            k_p.push_back(cp.strikeFromDelta(-deltas[i]));
            k_c.push_back(cc.strikeFromDelta(deltas[i]));
        }

        // ... and set up the interpolated smile

        return boost::make_shared<SimpleDeltaInterpolatedSmile>(spot, domDisc, forDisc, expiryTime, deltas, vol_p,
                                                                vol_c, atmVol, dt, at, smileInterpolation);

    } else {

        Real forward = spot / domDisc * forDisc;

        /* handle broker style butterflys: first determine the strikes and the (non-discounted) premiums
           of the broker butterflies */

        std::vector<Real> kb_c, kb_p, vb;

        for (Size i = 0; i < deltas.size(); ++i) {
            Real stddevb = (atmVol + bfQuotes[i]) * std::sqrt(expiryTime);
            BlackDeltaCalculator cp(Option::Type::Put, dt, spot, domDisc, forDisc, stddevb);
            BlackDeltaCalculator cc(Option::Type::Put, dt, spot, domDisc, forDisc, stddevb);
            kb_p.push_back(cp.strikeFromDelta(-deltas[i]));
            kb_c.push_back(cc.strikeFromDelta(deltas[i]));
            vb.push_back(blackFormula(Option::Put, kb_p.back(), forward, stddevb) +
                         blackFormula(Option::Call, kb_c.back(), forward, stddevb));
        }

        /* set initial guess for smile butterfly vol := broker butterfly vol*/

        Array guess(deltas.size());
        for (Size i = 0; i < deltas.size(); ++i) {
            guess[i] = std::log(bfQuotes[i]);
        }

        /* define the target function to match up the butterfly market values and smile values */

        struct TargetFunction : public QuantLib::CostFunction {
            TargetFunction(Real atmVol, Real phirr, Real spot, Real domDisc, Real forDisc, Real forward,
                           Real expiryTime, DeltaVolQuote::DeltaType dt, DeltaVolQuote::AtmType at,
                           const std::vector<Real>& rrQuotes, const std::vector<Real>& deltas,
                           const std::vector<Real>& kb_p, const std::vector<Real>& kb_c, const std::vector<Real>& vb,
                           BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation)
                : atmVol(atmVol), spot(spot), domDisc(domDisc), forDisc(forDisc), forward(forward),
                  expiryTime(expiryTime), dt(dt), at(at), rrQuotes(rrQuotes), deltas(deltas), kb_p(kb_p), kb_c(kb_c),
                  vb(vb), smileInterpolation(smileInterpolation) {}

            Real atmVol, phirr, spot, domDisc, forDisc, forward, expiryTime;
            DeltaVolQuote::DeltaType dt;
            DeltaVolQuote::AtmType at;
            const std::vector<Real>&rrQuotes, deltas, kb_p, kb_c, vb;
            BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation;

            mutable Real bestValue = QL_MAX_REAL;
            mutable boost::shared_ptr<SimpleDeltaInterpolatedSmile> bestSmile;

            Disposable<Array> values(const Array& smileBfVol) const override {

                // compute the call/put strikes and vols ....

                std::vector<Real> k_c, k_p, vol_c, vol_p;

                for (Size i = 0; i < deltas.size(); ++i) {
                    vol_p.push_back(atmVol + std::exp(smileBfVol[i]) - rrQuotes[i] / (2.0 * phirr));
                    vol_c.push_back(atmVol + std::exp(smileBfVol[i]) + rrQuotes[i] / (2.0 * phirr));
                    BlackDeltaCalculator cp(Option::Type::Put, dt, spot, domDisc, forDisc,
                                            vol_p.back() * std::sqrt(expiryTime));
                    BlackDeltaCalculator cc(Option::Type::Call, dt, spot, domDisc, forDisc,
                                            vol_p.back() * std::sqrt(expiryTime));
                    k_p.push_back(cp.strikeFromDelta(-deltas[i]));
                    k_c.push_back(cc.strikeFromDelta(deltas[i]));
                }

                // ... set up the interpolated smile ...

                auto tmpSmile = boost::make_shared<SimpleDeltaInterpolatedSmile>(
                    spot, domDisc, forDisc, expiryTime, deltas, vol_p, vol_c, atmVol, dt, at, smileInterpolation);

                // ... and price the market butterfly on the constructed smile

                std::vector<Real> vs;
                for (Size i = 0; i < deltas.size(); ++i) {
                    vs.push_back(blackFormula(Option::Put, kb_p.back(), forward,
                                              tmpSmile->volatility(k_p[i]) * std::sqrt(expiryTime)) +
                                 blackFormula(Option::Call, kb_c.back(), forward,
                                              tmpSmile->volatility(k_c[i]) * std::sqrt(expiryTime)));
                }

                // now set the target function to the relative difference of smile vs. market price

                Array result(deltas.size());
                for (Size i = 0; i < deltas.size(); ++i) {
                    result[i] = (vs[i] - vb[i]) / vb[i];
                }

                Real value =
                    std::accumulate(result.begin(), result.end(), 0.0, [](Real acc, Real x) { return acc + x * x; });

                if (value < bestValue) {
                    bestValue = value;
                    bestSmile = tmpSmile;
                }

                return result;
            }
        }; // TargetFunction declaration;

        TargetFunction targetFunction{atmVol, phirr,    spot,   domDisc, forDisc, forward, expiryTime,        dt,
                                      at,     rrQuotes, deltas, kb_p,    kb_c,    vb,      smileInterpolation};
        NoConstraint noConstraint;
        LevenbergMarquardt lm;
        EndCriteria endCriteria(100, 10, 1E-8, 1E-8, 1E-8);
        Problem problem(targetFunction, noConstraint, guess);
        lm.minimize(problem, endCriteria);
        return targetFunction.bestSmile;
    }
}

} // namespace detail

BlackVolatilitySurfaceBFRR::BlackVolatilitySurfaceBFRR(
    Date referenceDate, const std::vector<Date>& dates, const std::vector<Real>& deltas,
    const std::vector<std::vector<Real>>& bfQuotes, const std::vector<std::vector<Real>>& rrQuotes,
    const std::vector<Real>& atmQuotes, const DayCounter& dayCounter, const Calendar& calendar,
    const Handle<Quote>& spot, const Size spotDays, const Calendar spotCalendar,
    const Handle<YieldTermStructure>& domesticTS, const Handle<YieldTermStructure>& foreignTS,
    const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at, const Period& switchTenor,
    const DeltaVolQuote::DeltaType ltdt, const DeltaVolQuote::AtmType ltat, const Option::Type riskReversalInFavorOf,
    const bool butterflyIsBrokerStyle, const SmileInterpolation smileInterpolation)
    : BlackVolatilityTermStructure(referenceDate, calendar, Following, dayCounter), dates_(dates), deltas_(deltas),
      bfQuotes_(bfQuotes), rrQuotes_(rrQuotes), atmQuotes_(atmQuotes), spot_(spot), spotDays_(spotDays),
      spotCalendar_(spotCalendar), domesticTS_(domesticTS), foreignTS_(foreignTS), dt_(dt), at_(at),
      switchTenor_(switchTenor), ltdt_(ltdt), ltat_(ltat), riskReversalInFavorOf_(riskReversalInFavorOf),
      butterflyIsBrokerStyle_(butterflyIsBrokerStyle), smileInterpolation_(smileInterpolation) {

    // checks

    QL_REQUIRE(!dates_.empty(), "BlackVolatilitySurfaceBFRR: no expiry dates given");
    QL_REQUIRE(!deltas_.empty(), "BlackVolatilitySurfaceBFRR: no deltas given");

    for (Size i = 0; i < deltas_.size() - 1; ++i) {
        QL_REQUIRE(deltas_[i + 1] > deltas_[i] && !close_enough(deltas_[i], deltas_[i + 1]),
                   "BlackVolatilitySurfaceBFRR: deltas are not strictly ascending at index " << i << ": " << deltas_[i]
                                                                                             << ", " << deltas_[i + 1]);
    }

    QL_REQUIRE(bfQuotes_.size() == dates_.size(), "BlackVolatilitySurfaceBFRR: bfQuotes ("
                                                      << bfQuotes_.size() << ") mismatch with expiry dates ("
                                                      << dates_.size() << ")");
    QL_REQUIRE(rrQuotes_.size() == dates_.size(), "BlackVolatilitySurfaceBFRR: rrQuotes ("
                                                      << rrQuotes_.size() << ") mismatch with expiry dates ("
                                                      << dates_.size() << ")");
    QL_REQUIRE(atmQuotes_.size() == dates_.size(), "BlackVolatilitySurfaceBFRR: atmQuotes ("
                                                       << atmQuotes_.size() << ") mismatch with expiry dates ("
                                                       << dates_.size() << ")");
    for (auto const& q : bfQuotes) {
        QL_REQUIRE(q.size() == deltas_.size(), "BlackVolatilitySurfaceBFRR: bfQuotes inner vector ("
                                                   << q.size() << ") mismatch with deltas (" << deltas_.size() << ")");
    }
    for (auto const& q : rrQuotes) {
        QL_REQUIRE(q.size() == deltas_.size(), "BlackVolatilitySurfaceBFRR: bfQuotes inner vector ("
                                                   << q.size() << ") mismatch with deltas (" << deltas_.size() << ")");
    }

    // register with observables

    registerWith(spot_);
    registerWith(domesticTS_);
    registerWith(foreignTS_);

    // resize cache for smiles on expiry dates

    smiles_.resize(expiryTimes_.size());

    // initialise

    init();
}

void BlackVolatilitySurfaceBFRR::init() {

    // calcluate switch time

    switchTime_ = switchTenor_ == 0 * Days ? QL_MAX_REAL : timeFromReference(optionDateFromTenor(switchTenor_));

    // calculate times associated to expiry dates

    for (auto const& d : dates_)
        expiryTimes_.push_back(timeFromReference(d));

    // calculate discount factors for spot settlement date

    Date settlDate = spotCalendar_.advance(referenceDate(), spotDays_ * Days);
    settlDomDisc_ = domesticTS_->discount(settlDate);
    settlForDisc_ = foreignTS_->discount(settlDate);

    // clear caches

    std::fill(smiles_.begin(), smiles_.end(), nullptr);
    cachedInterpolatedSmiles_.clear();
}

void BlackVolatilitySurfaceBFRR::update() {
    BlackVolatilityTermStructure::update();
    init();
}

Volatility BlackVolatilitySurfaceBFRR::blackVolImpl(Time t, Real strike) const {

    /* minimum supported time is 1D, i.e. if t is smaller, we return the vol at 1D */

    t = std::max(t, 1.0 / 365.0);

    /* if we have cached the interpolated smile at t, we use that */

    auto s = cachedInterpolatedSmiles_.find(t);
    if (s != cachedInterpolatedSmiles_.end()) {
        return s->second->volatility(strike);
    }

    /* find the indices ip and im such that t_im <= t  < t_ip, im will be null if t < first expiry,
       ip will be null if t >= last expiry */

    Size index_p = std::upper_bound(expiryTimes_.begin(), expiryTimes_.end(), t) - expiryTimes_.begin();
    Size index_m = index_p == 0 ? Null<Size>() : index_p - 1;
    if (index_p == expiryTimes_.size())
        index_p = Null<Size>();

    /* build the smiles on the indices, if we do not have them yet */

    if (smiles_[index_m] == nullptr) {
        DeltaVolQuote::AtmType at;
        DeltaVolQuote::DeltaType dt;
        if (expiryTimes_[index_m] < switchTime_ || close_enough(expiryTimes_[index_m], switchTime_)) {
            at = at_;
            dt = dt_;
        } else {
            at = ltat_;
            dt = ltdt_;
        }
        smiles_[index_m] =
            detail::createSmile(spot_->value(), domesticTS_->discount(expiryTimes_[index_m]) / settlDomDisc_,
                                foreignTS_->discount(expiryTimes_[index_m]) / settlForDisc_, expiryTimes_[index_m],
                                deltas_, bfQuotes_[index_m], rrQuotes_[index_m], atmQuotes_[index_m], dt, at,
                                riskReversalInFavorOf_, butterflyIsBrokerStyle_, smileInterpolation_);
    }

    if (smiles_[index_p] == nullptr) {
        DeltaVolQuote::AtmType at;
        DeltaVolQuote::DeltaType dt;
        if (expiryTimes_[index_p] < switchTime_ || close_enough(expiryTimes_[index_p], switchTime_)) {
            at = at_;
            dt = dt_;
        } else {
            at = ltat_;
            dt = ltdt_;
        }
        smiles_[index_p] =
            detail::createSmile(spot_->value(), domesticTS_->discount(expiryTimes_[index_p]) / settlDomDisc_,
                                foreignTS_->discount(expiryTimes_[index_p]) / settlForDisc_, expiryTimes_[index_m],
                                deltas_, bfQuotes_[index_p], rrQuotes_[index_p], atmQuotes_[index_p], dt, at,
                                riskReversalInFavorOf_, butterflyIsBrokerStyle_, smileInterpolation_);
    }

    /* choose a consistent set of smile conventions for all maturities, we follow Clark, 4.2.3 and set
       delta type = forward delta, with pa if the original (short term) delta convention has pa
       atm type = delta neutral */

    DeltaVolQuote::DeltaType dt_c =
        dt_ == DeltaVolQuote::Spot || dt_ == DeltaVolQuote::Fwd ? DeltaVolQuote::Fwd : DeltaVolQuote::PaFwd;
    DeltaVolQuote::AtmType at_c = DeltaVolQuote::AtmDeltaNeutral;

    /* find the strikes and vols on both smiles for the artificial smile conventions */

    Real atmStrike_m, atmStrike_p;
    std::vector<Real> putStrikes_m, callStrikes_m, putStrikes_p, callStrikes_p;
    Real atmVol_m, atmVol_p;
    std::vector<Real> putVols_m, callVols_m, putVols_p, callVols_p;

    if (index_m != Null<Size>()) {
        atmStrike_m = smiles_[index_m]->atmStrike(dt_c, at_c);
        atmVol_m = smiles_[index_m]->volatility(atmStrike_m);
        for (auto const& d : deltas_) {
            putStrikes_m.push_back(smiles_[index_m]->strikeFromDelta(Option::Call, d, dt_c));
            callStrikes_m.push_back(smiles_[index_m]->strikeFromDelta(Option::Put, d, dt_c));
            putVols_m.push_back(smiles_[index_m]->volatility(putStrikes_m.back()));
            callVols_m.push_back(smiles_[index_m]->volatility(callStrikes_m.back()));
        }
    }

    if (index_p != Null<Size>()) {
        atmStrike_p = smiles_[index_p]->atmStrike(dt_c, at_c);
        atmVol_p = smiles_[index_p]->volatility(atmStrike_p);
        for (auto const& d : deltas_) {
            putStrikes_p.push_back(smiles_[index_p]->strikeFromDelta(Option::Call, d, dt_c));
            callStrikes_p.push_back(smiles_[index_p]->strikeFromDelta(Option::Put, d, dt_c));
            putVols_p.push_back(smiles_[index_p]->volatility(putStrikes_p.back()));
            callVols_m.push_back(smiles_[index_p]->volatility(callStrikes_p.back()));
        }
    }

    /* find the interpolated vols */

    Real atmVol_i;
    std::vector<Real> putVols_i, callVols_i;

    if (index_p == Null<Size>()) {
        // extrapolate beyond last expiry
        Real dt = std::sqrt(t / expiryTimes_[index_m]);
        atmVol_i = atmVol_m * dt;
        for (Size i = 0; i < deltas_.size(); ++i) {
            putVols_i.push_back(putVols_m[i] * dt);
            callVols_i.push_back(callVols_m[i] * dt);
        }
    } else if (index_m == Null<Size>()) {
        // extrapolate before first expiry
        atmVol_i = atmVol_p;
        putVols_i = putVols_p;
        callVols_i = callVols_p;
    } else {
        // interpolate between two expiries
        Real a = (t - expiryTimes_[index_m]) / (expiryTimes_[index_p] - expiryTimes_[index_m]);
        atmVol_i = (1.0 - a) * atmVol_m + a * atmVol_p;
        for (Size i = 0; i < deltas_.size(); ++i) {
            putVols_i.push_back((1.0 - a) * putVols_m[i] + a * putVols_p[i]);
            callVols_i.push_back((1.0 - a) * callVols_m[i] * a * callVols_p[i]);
        }
    }

    /* build a new smile using the interpolated vols and artifical conventions */

    auto smile = boost::make_shared<detail::SimpleDeltaInterpolatedSmile>(
        spot_->value(), domesticTS_->discount(t) / settlDomDisc_, foreignTS_->discount(t) / settlForDisc_, t, deltas_,
        putVols_i, callVols_i, atmVol_i, dt_c, at_c, smileInterpolation_);

    /* store the new smile in the cache */

    cachedInterpolatedSmiles_[t] = smile;

    /* return the volatility from the smile we just created */

    return smile->volatility(strike);
}

} // namespace QuantExt
