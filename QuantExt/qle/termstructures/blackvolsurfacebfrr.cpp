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

Real transformVol(const Real v) { return std::log(v); }
Real untransformVol(const Real w) { return std::exp(w); }

SimpleDeltaInterpolatedSmile::SimpleDeltaInterpolatedSmile(
    const Real spot, const Real domDisc, const Real forDisc, const Real expiryTime, const std::vector<Real>& deltas,
    const std::vector<Real>& putVols, const std::vector<Real>& callVols, const Real atmVol,
    const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at,
    const BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation, const Real accuracy,
    const Size maxIterations)
    : spot_(spot), domDisc_(domDisc), forDisc_(forDisc), expiryTime_(expiryTime), deltas_(deltas), putVols_(putVols),
      callVols_(callVols), atmVol_(atmVol), dt_(dt), at_(at), smileInterpolation_(smileInterpolation),
      accuracy_(accuracy), maxIterations_(maxIterations) {

    forward_ = spot_ / domDisc_ * forDisc_;

    std::vector<Real> x, y;

    /* Convert the puts, atm, calls to strikes using the given conventions and then to simple delta
       using the given conventions. Store the simple deltas as x-values for the interpolation and
       the log of the vols as y-values */

    for (Size i = 0; i < deltas_.size(); ++i) {
        try {
            BlackDeltaCalculator c(Option::Put, dt_, spot_, domDisc_, forDisc_, putVols_[i] * std::sqrt(expiryTime_));
            x.push_back(simpleDeltaFromStrike(c.strikeFromDelta(-deltas[i])));
            y.push_back(transformVol(putVols_[i]));
        } catch (const std::exception& e) {
            QL_FAIL("SimpleDeltaInterpolatedSmile: strikeFromDelta("
                    << -deltas[i] << ") could not be computed for spot=" << spot_
                    << ", forward=" << spot_ / domDisc_ * forDisc_ << " (domRate=" << -std::log(domDisc_) / expiryTime_
                    << ", forRate=" << -std::log(forDisc) / expiryTime_ << "), putVol=" << putVols_[i]
                    << ", expiry=" << expiryTime_ << ": " << e.what());
        }
    }

    try {
        BlackDeltaCalculator c(Option::Call, dt_, spot_, domDisc_, forDisc_, atmVol * std::sqrt(expiryTime_));
        x.push_back(simpleDeltaFromStrike(c.atmStrike(at_)));
        y.push_back(transformVol(atmVol_));
    } catch (const std::exception& e) {
        QL_FAIL("SimpleDeltaIinterpolatedSmile: atmStrike could not be computed for spot="
                << spot_ << ", forward=" << spot_ / domDisc_ * forDisc_
                << " (domRate=" << -std::log(domDisc_) / expiryTime_ << ", forRate=" << -std::log(forDisc) / expiryTime_
                << "), atmVol=" << atmVol << ", expiry=" << expiryTime_ << ": " << e.what());
    }

    for (Size i = deltas_.size(); i > 0; --i) {
        try {
            BlackDeltaCalculator c(Option::Call, dt_, spot_, domDisc_, forDisc_,
                                   callVols_[i - 1] * std::sqrt(expiryTime_));
            x.push_back(simpleDeltaFromStrike(c.strikeFromDelta(deltas[i - 1])));
            y.push_back(transformVol(callVols_[i - 1]));
        } catch (const std::exception& e) {
            QL_FAIL("SimpleDeltaInterpolatedSmile: strikeFromDelta("
                    << deltas[i - 1] << ") could not be computed for spot=" << spot_
                    << ", forward=" << spot_ / domDisc_ * forDisc_ << " (domRate=" << -std::log(domDisc_) / expiryTime_
                    << ", forRate=" << -std::log(forDisc) / expiryTime_ << "), callVol=" << callVols_[i - 1]
                    << ", expiry=" << expiryTime_ << ": " << e.what());
        }
    }

    /* sort the strikes */

    std::vector<Size> perm(x.size());
    std::iota(perm.begin(), perm.end(), 0);
    std::sort(perm.begin(), perm.end(), [&x](Size a, Size b) { return x[a] < x[b]; });
    for (Size i = 0; i < perm.size(); ++i) {
        x_.push_back(x[perm[i]]);
        y_.push_back(y[perm[i]]);
    }

    /* check the strikes are not (numerically) identical */

    for (Size i = 0; i < x_.size() - 1; ++i) {
        QL_REQUIRE(!close_enough(x_[i], x_[i + 1]), "SmileDeltaInterpolatedSmile: interpolation points x["
                                                        << i << "] = x[" << (i + 1) << "] = " << x_[i]
                                                        << " are numerically identical.");
    }

    /* Create the interpolation object */

    if (smileInterpolation_ == BlackVolatilitySurfaceBFRR::SmileInterpolation::Linear) {
        interpolation_ = QuantLib::ext::make_shared<LinearInterpolation>(x_.begin(), x_.end(), y_.begin());
    } else if (smileInterpolation_ == BlackVolatilitySurfaceBFRR::SmileInterpolation::Cubic) {
        interpolation_ = QuantLib::ext::make_shared<CubicInterpolation>(
            x_.begin(), x_.end(), y_.begin(), CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative,
            0.0, CubicInterpolation::SecondDerivative, 0.0);
    } else {
        QL_FAIL("invalid interpolation, this is unexpected");
    }

    interpolation_->enableExtrapolation();
}

Real SimpleDeltaInterpolatedSmile::strikeFromDelta(const Option::Type type, const Real delta,
                                                   const DeltaVolQuote::DeltaType dt) {
    Real result = forward_, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(expiryTime_) * volatility(result);
        try {
            BlackDeltaCalculator c(type, dt, spot_, domDisc_, forDisc_, stddev);
            lastResult = result;
            result = c.strikeFromDelta((type == Option::Call ? 1.0 : -1.0) * delta);
        } catch (const std::exception& e) {
            QL_FAIL("SimpleDeltaInterpolatedSmile::strikeFromDelta("
                    << (type == Option::Call ? 1.0 : -1.0) * delta << ") could not be computed for spot=" << spot_
                    << ", forward=" << spot_ / domDisc_ * forDisc_ << " (domRate=" << -std::log(domDisc_) / expiryTime_
                    << ", forRate=" << -std::log(forDisc_) / expiryTime_ << "), vol=" << stddev / std::sqrt(expiryTime_)
                    << ", expiry=" << expiryTime_ << ": " << e.what());
        }
    } while (std::abs((result - lastResult) / lastResult) > accuracy_ && ++iterations < maxIterations_);
    QL_REQUIRE(iterations < maxIterations_,
               "SmileDeltaInterpolatedSmile::strikeFromDelta("
                   << (type == Option::Call ? 1.0 : -1.0) * delta << "): max iterations (" << maxIterations_
                   << "), no solution found for accuracy " << accuracy_ << ", last iterations: " << lastResult << "/"
                   << result << ", spot=" << spot_ << ", forward=" << spot_ / domDisc_ * forDisc_
                   << " (domRate=" << -std::log(domDisc_) / expiryTime_
                   << ", forRate=" << -std::log(forDisc_) / expiryTime_ << "), expiry=" << expiryTime_);
    return result;
}

Real SimpleDeltaInterpolatedSmile::atmStrike(const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at) {
    Real result = forward_, lastResult;
    Size iterations = 0;
    do {
        Real stddev = std::sqrt(expiryTime_) * volatility(result);
        try {
            BlackDeltaCalculator c(Option::Call, dt, spot_, domDisc_, forDisc_, stddev);
            lastResult = result;
            result = c.atmStrike(at);
        } catch (const std::exception& e) {
            QL_FAIL("SimpleDeltaInterpolatedSmile::atmStrike() could not be computed for spot="
                    << spot_ << ", forward=" << spot_ / domDisc_ * forDisc_ << " (domRate="
                    << -std::log(domDisc_) / expiryTime_ << ", forRate=" << -std::log(forDisc_) / expiryTime_
                    << "), vol=" << stddev / std::sqrt(expiryTime_) << ", expiry=" << expiryTime_ << ": " << e.what());
        }
    } while (std::abs((result - lastResult) / lastResult) > accuracy_ && ++iterations < maxIterations_);
    QL_REQUIRE(iterations < maxIterations_,
               "SmileDeltaInterpolatedSmile::atmStrike(): max iterations ("
                   << maxIterations_ << "), no solution found for accuracy " << accuracy_
                   << ", last iterations: " << lastResult << "/" << result << ", spot=" << spot_
                   << ", forward=" << spot_ / domDisc_ * forDisc_ << " (domRate=" << -std::log(domDisc_) / expiryTime_
                   << ", forRate=" << -std::log(forDisc_) / expiryTime_ << "), expiry=" << expiryTime_);
    return result;
}

Real SimpleDeltaInterpolatedSmile::volatilityAtSimpleDelta(const Real simpleDelta) {
    Real tmp = untransformVol((*interpolation_)(simpleDelta));
    QL_REQUIRE(std::isfinite(tmp), "SimpleDeltaInterpolatedSmile::volatilityAtSimpleDelta() non-finite result ("
                                       << tmp << ") for simple delta " << simpleDelta);
    return tmp;
}

Real SimpleDeltaInterpolatedSmile::volatility(const Real strike) {
    Real tmp = untransformVol((*interpolation_)(simpleDeltaFromStrike(strike)));
    if (!std::isfinite(tmp)) {
        std::ostringstream os;
        for (Size i = 0; i < x_.size(); ++i) {
            os << "(" << x_[i] << "," << y_[i] << ")";
        }
        QL_FAIL("SimpleDeltaInterpolatedSmile::volatility() non-finite result ("
                << tmp << ") for strike " << strike << ", simple delta is " << simpleDeltaFromStrike(strike)
                << ", interpolated value is " << (*interpolation_)(simpleDeltaFromStrike(strike))
                << ", interpolation data point are " << os.str());
    }
    return tmp;
}

Real SimpleDeltaInterpolatedSmile::simpleDeltaFromStrike(const Real strike) const {
    if (close_enough(strike, 0.0))
        return 0.0;
    CumulativeNormalDistribution Phi;
    return Phi(std::log(strike / forward_) / (atmVol_ * std::sqrt(expiryTime_)));
}

QuantLib::ext::shared_ptr<SimpleDeltaInterpolatedSmile>
createSmile(const Real spot, const Real domDisc, const Real forDisc, const Real expiryTime,
            const std::vector<Real>& deltas, const std::vector<Real>& bfQuotes, const std::vector<Real>& rrQuotes,
            const Real atmVol, const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at,
            const Option::Type riskReversalInFavorOf, const bool butterflyIsBrokerStyle,
            const BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation) {

    Real phirr = riskReversalInFavorOf == Option::Call ? 1.0 : -1.0;
    QuantLib::ext::shared_ptr<SimpleDeltaInterpolatedSmile> resultSmile;

    if (!butterflyIsBrokerStyle) {

        // butterfly is not broker style: we can directly compute the call/put vols ...

        std::vector<Real> vol_p, vol_c;

        for (Size i = 0; i < deltas.size(); ++i) {
            QL_REQUIRE(atmVol + bfQuotes[i] - 0.5 * std::abs(rrQuotes[i]) > 0.0,
                       "createSmile: atmVol ("
                           << atmVol << ") + bf (" << bfQuotes[i] << ") - rr (" << rrQuotes[i]
                           << ") must be positive when creating smile from smile bf quotes, tte=" << expiryTime);
            vol_p.push_back(atmVol + bfQuotes[i] - 0.5 * phirr * rrQuotes[i]);
            vol_c.push_back(atmVol + bfQuotes[i] + 0.5 * phirr * rrQuotes[i]);
        }

        // ... and set up the interpolated smile

        resultSmile = QuantLib::ext::make_shared<SimpleDeltaInterpolatedSmile>(
            spot, domDisc, forDisc, expiryTime, deltas, vol_p, vol_c, atmVol, dt, at, smileInterpolation);

    } else {

        Real forward = spot / domDisc * forDisc;

        /* handle broker style butterflys: first determine the strikes and the (non-discounted) premiums
           of the broker butterflies */

        std::vector<Real> kb_c, kb_p, vb;

        for (Size i = 0; i < deltas.size(); ++i) {
            Real stddevb = (atmVol + bfQuotes[i]) * std::sqrt(expiryTime);
            QL_REQUIRE(stddevb > 0.0,
                       "createSmile: atmVol ("
                           << atmVol << ") + bf (" << bfQuotes[i]
                           << ") must be positive when creating smile from broker bf quotes, tte=" << expiryTime);
            BlackDeltaCalculator cp(Option::Type::Put, dt, spot, domDisc, forDisc, stddevb);
            BlackDeltaCalculator cc(Option::Type::Call, dt, spot, domDisc, forDisc, stddevb);
            kb_p.push_back(cp.strikeFromDelta(-deltas[i]));
            kb_c.push_back(cc.strikeFromDelta(deltas[i]));
            vb.push_back(blackFormula(Option::Put, kb_p.back(), forward, stddevb) +
                         blackFormula(Option::Call, kb_c.back(), forward, stddevb));
        }

        /* set initial guess for smile butterfly vol := broker butterfly vol
           we optimise in z = log( bf - 0.5 * abs(rr) + atmVol ) */

        Array guess(deltas.size());
        for (Size i = 0; i < deltas.size(); ++i) {
            guess[i] = std::log(std::max(0.0001, bfQuotes[i] - 0.5 * std::abs(rrQuotes[i]) + atmVol));
        }

        /* define the target function to match up the butterfly market values and smile values */

        struct TargetFunction : public QuantLib::CostFunction {
            TargetFunction(Real atmVol, Real phirr, Real spot, Real domDisc, Real forDisc, Real forward,
                           Real expiryTime, DeltaVolQuote::DeltaType dt, DeltaVolQuote::AtmType at,
                           const std::vector<Real>& rrQuotes, const std::vector<Real>& deltas,
                           const std::vector<Real>& kb_p, const std::vector<Real>& kb_c, const std::vector<Real>& vb,
                           BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation)
                : atmVol(atmVol), phirr(phirr), spot(spot), domDisc(domDisc), forDisc(forDisc), forward(forward),
                  expiryTime(expiryTime), dt(dt), at(at), rrQuotes(rrQuotes), deltas(deltas), kb_p(kb_p), kb_c(kb_c),
                  vb(vb), smileInterpolation(smileInterpolation) {}

            Real atmVol, phirr, spot, domDisc, forDisc, forward, expiryTime;
            DeltaVolQuote::DeltaType dt;
            DeltaVolQuote::AtmType at;
            const std::vector<Real>&rrQuotes, deltas, kb_p, kb_c, vb;
            BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation;

            mutable Real bestValue = QL_MAX_REAL;
            mutable QuantLib::ext::shared_ptr<SimpleDeltaInterpolatedSmile> bestSmile;

            Array values(const Array& x) const override {

                constexpr Real large_error = 1E6;

                Array smileBfVol(x.size());
                for (Size i = 0; i < x.size(); ++i)
                    smileBfVol[i] = std::exp(x[i]) + 0.5 * std::abs(rrQuotes[i]) - atmVol;

                // compute the call/put vols ....

                std::vector<Real> vol_c, vol_p;

                for (Size i = 0; i < deltas.size(); ++i) {
                    vol_p.push_back(atmVol + smileBfVol[i] - 0.5 * phirr * rrQuotes[i]);
                    vol_c.push_back(atmVol + smileBfVol[i] + 0.5 * phirr * rrQuotes[i]);
                    QL_REQUIRE(vol_p.back() > 0.0, " createSmile: internal error: put vol = "
                                                       << vol_p.back() << " during broker bf fitting");
                    QL_REQUIRE(vol_c.back() > 0.0, " createSmile: internal error: call vol = "
                                                       << vol_c.back() << " during broker bf fitting");
                }

                // ... set up the interpolated smile ...

                QuantLib::ext::shared_ptr<SimpleDeltaInterpolatedSmile> tmpSmile;
                try {
                    tmpSmile = QuantLib::ext::make_shared<SimpleDeltaInterpolatedSmile>(
                        spot, domDisc, forDisc, expiryTime, deltas, vol_p, vol_c, atmVol, dt, at, smileInterpolation);
                } catch (...) {
                    // if we run into a problem we return max error and continue with the optimization
                    return Array(deltas.size(), large_error);
                }

                // ... and price the market butterfly on the constructed smile

                std::vector<Real> vs;
                for (Size i = 0; i < deltas.size(); ++i) {
                    Real pvol, cvol;
                    try {
                        pvol = tmpSmile->volatility(kb_p[i]);
                        cvol = tmpSmile->volatility(kb_c[i]);
                    } catch (...) {
                        // as above, if there is a problem, return max error
                        return Array(deltas.size(), large_error);
                    }
                    vs.push_back(blackFormula(Option::Put, kb_p[i], forward, pvol * std::sqrt(expiryTime)) +
                                 blackFormula(Option::Call, kb_c[i], forward, cvol * std::sqrt(expiryTime)));
                }

                // now set the target function to the relative difference of smile vs. market price

                Array result(deltas.size());
                for (Size i = 0; i < deltas.size(); ++i) {
                    result[i] = (vs[i] - vb[i]) / vb[i];
                    if (!std::isfinite(result[i]))
                        result[i] = large_error;
                }

                Real value = std::sqrt(std::accumulate(result.begin(), result.end(), 0.0,
                                                       [](Real acc, Real x) { return acc + x * x; })) /
                             result.size();

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

        QL_REQUIRE(targetFunction.bestValue < 0.01, "createSmile at expiry "
                                                        << expiryTime << " failed: target function value ("
                                                        << problem.functionValue() << ") not close to zero");

        resultSmile = targetFunction.bestSmile;
    }

    // sanity check of result smile before return it

    static const std::vector<Real> samplePoints = {0.01, 0.05, 0.1, 0.2, 0.5, 0.8, 0.9, 0.95, 0.99};
    for (auto const& simpleDelta : samplePoints) {
        Real vol = resultSmile->volatilityAtSimpleDelta(simpleDelta);
        QL_REQUIRE(vol > 0.0001 && vol < 5.0, "createSmile at expiry " << expiryTime << ": volatility at simple delta "
                                                                       << simpleDelta << " (" << vol
                                                                       << ") is not plausible.");
    }

    return resultSmile;
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
}

const std::vector<bool>& BlackVolatilitySurfaceBFRR::smileHasError() const {
    calculate();
    return smileHasError_;
}

const std::vector<std::string>& BlackVolatilitySurfaceBFRR::smileErrorMessage() const {
    calculate();
    return smileErrorMessage_;
}

void BlackVolatilitySurfaceBFRR::performCalculations() const {

    // calculate switch time

    switchTime_ = switchTenor_ == 0 * Days ? QL_MAX_REAL : timeFromReference(optionDateFromTenor(switchTenor_));

    // calculate times associated to expiry dates

    expiryTimes_.clear();
    settlementDates_.clear();
    for (auto const& d : dates_) {
        expiryTimes_.push_back(timeFromReference(d));
        settlementDates_.push_back(spotCalendar_.advance(d, spotDays_ * Days));
    }

    // resize cache for smiles on expiry dates

    smiles_.resize(expiryTimes_.size());
    smileHasError_.resize(expiryTimes_.size());
    smileErrorMessage_.resize(expiryTimes_.size());

    // calculate discount factors for spot settlement date and the settlement lag

    Date settlDate = spotCalendar_.advance(referenceDate(), spotDays_ * Days);
    settlDomDisc_ = domesticTS_->discount(settlDate);
    settlForDisc_ = foreignTS_->discount(settlDate);
    settlLag_ = timeFromReference(settlDate);

    // clear caches

    clearCaches();

    // reset deltas to use

    currentDeltas_ = deltas_;
}

void BlackVolatilitySurfaceBFRR::update() {
    BlackVolatilityTermStructure::update();
    LazyObject::update();
}

void BlackVolatilitySurfaceBFRR::clearCaches() const {
    std::fill(smiles_.begin(), smiles_.end(), nullptr);
    std::fill(smileHasError_.begin(), smileHasError_.end(), false);
    std::fill(smileErrorMessage_.begin(), smileErrorMessage_.end(), std::string());
    cachedInterpolatedSmiles_.clear();
}

Volatility BlackVolatilitySurfaceBFRR::blackVolImpl(Time t, Real strike) const {

    calculate();

    /* minimum supported time is 1D, i.e. if t is smaller, we return the vol at 1D */

    t = std::max(t, 1.0 / 365.0);

    t = t <= expiryTimes_.back() ? t : expiryTimes_.back();

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

    /* the smiles at the indices might have errors, then go to the next ones without errors */

    while (index_m != Null<Size>() && index_m > 0 && smileHasError_[index_m])
        --index_m;

    while (index_p != Null<Size>() && index_p < expiryTimes_.size() - 1 && smileHasError_[index_p])
        ++index_p;

    if (index_m != Null<Size>() && smileHasError_[index_m])
        index_m = Null<Size>();

    if (index_p != Null<Size>() && smileHasError_[index_p])
        index_p = Null<Size>();

    if (index_m == Null<Size>() && index_p == Null<Size>()) {

        /* no valid smiles, try to remove the smallest delta until there is only one delta left, then throw an error */

        if (currentDeltas_.size() <= 1) {
            QL_FAIL("BlackVolatilitySurfaceBFRR::blackVolImpl(" << t << "," << strike
                                                                << "): no valid smiles, check the market data input.");
        }

        currentDeltas_.erase(currentDeltas_.begin());
        clearCaches();
        return blackVolImpl(t, strike);
    }

    /* build the smiles on the indices, if we do not have them yet */

    if (index_m != Null<Size>() && smiles_[index_m] == nullptr) {
        DeltaVolQuote::AtmType at;
        DeltaVolQuote::DeltaType dt;
        if (expiryTimes_[index_m] < switchTime_ && !close_enough(switchTime_, expiryTimes_[index_m])) {
            at = at_;
            dt = dt_;
        } else {
            at = ltat_;
            dt = ltdt_;
        }
        try {
            smiles_[index_m] = detail::createSmile(
                spot_->value(), domesticTS_->discount(settlementDates_[index_m]) / settlDomDisc_,
                foreignTS_->discount(settlementDates_[index_m]) / settlForDisc_, expiryTimes_[index_m], currentDeltas_,
                bfQuotes_[index_m], rrQuotes_[index_m], atmQuotes_[index_m], dt, at, riskReversalInFavorOf_,
                butterflyIsBrokerStyle_, smileInterpolation_);
        } catch (const std::exception& e) {
            smileHasError_[index_m] = true;
            smileErrorMessage_[index_m] = e.what();
            return blackVolImpl(t, strike);
        }
    }

    if (index_p != Null<Size>() && smiles_[index_p] == nullptr) {
        DeltaVolQuote::AtmType at;
        DeltaVolQuote::DeltaType dt;
        if (expiryTimes_[index_p] < switchTime_ && !close_enough(switchTime_, expiryTimes_[index_p])) {
            at = at_;
            dt = dt_;
        } else {
            at = ltat_;
            dt = ltdt_;
        }
        try {
            smiles_[index_p] = detail::createSmile(
                spot_->value(), domesticTS_->discount(settlementDates_[index_p]) / settlDomDisc_,
                foreignTS_->discount(settlementDates_[index_p]) / settlForDisc_, expiryTimes_[index_p], currentDeltas_,
                bfQuotes_[index_p], rrQuotes_[index_p], atmQuotes_[index_p], dt, at, riskReversalInFavorOf_,
                butterflyIsBrokerStyle_, smileInterpolation_);
        } catch (const std::exception& e) {
            smileHasError_[index_p] = true;
            smileErrorMessage_[index_p] = e.what();
            return blackVolImpl(t, strike);
        }
    }

    /* Choose a consistent set of smile conventions for all maturities, we follow Clark, 4.2.3 and set
       delta type = forward delta, with pa if the original (short term) delta convention has pa.
       The atm type ist set to delta neutral. */

    DeltaVolQuote::DeltaType dt_c =
        dt_ == (DeltaVolQuote::Spot || dt_ == DeltaVolQuote::Fwd) ? DeltaVolQuote::Fwd : DeltaVolQuote::PaFwd;
    DeltaVolQuote::AtmType at_c = DeltaVolQuote::AtmDeltaNeutral;

    /* find the vols on both smiles for the artificial smile conventions */

    Real atmVol_m = 0.0, atmVol_p = 0.0;
    std::vector<Real> putVols_m, callVols_m, putVols_p, callVols_p;

    if (index_m != Null<Size>()) {
        try {
            atmVol_m = smiles_[index_m]->volatility(smiles_[index_m]->atmStrike(dt_c, at_c));
            for (auto const& d : currentDeltas_) {
                putVols_m.push_back(
                    smiles_[index_m]->volatility(smiles_[index_m]->strikeFromDelta(Option::Put, d, dt_c)));
                callVols_m.push_back(
                    smiles_[index_m]->volatility(smiles_[index_m]->strikeFromDelta(Option::Call, d, dt_c)));
            }
        } catch (const std::exception& e) {
            smileHasError_[index_m] = true;
            smileErrorMessage_[index_m] = e.what();
            return blackVolImpl(t, strike);
        }
    }

    if (index_p != Null<Size>()) {
        try {
            atmVol_p = smiles_[index_p]->volatility(smiles_[index_p]->atmStrike(dt_c, at_c));
            for (auto const& d : currentDeltas_) {
                putVols_p.push_back(
                    smiles_[index_p]->volatility(smiles_[index_p]->strikeFromDelta(Option::Put, d, dt_c)));
                callVols_p.push_back(
                    smiles_[index_p]->volatility(smiles_[index_p]->strikeFromDelta(Option::Call, d, dt_c)));
            }
        } catch (const std::exception& e) {
            smileHasError_[index_p] = true;
            smileErrorMessage_[index_p] = e.what();
            return blackVolImpl(t, strike);
        }
    }

    /* find the interpolated vols */

    Real atmVol_i;
    std::vector<Real> putVols_i, callVols_i;

    if (index_p == Null<Size>()) {
        // extrapolate beyond last expiry
        atmVol_i = atmVol_m;
        putVols_i = putVols_m;
        callVols_i = callVols_m;
        QL_REQUIRE(atmVol_i > 0.0, "BlackVolatilitySurfaceBFRR: negative front-extrapolated atm vol " << atmVol_i);
        for (Size i = 0; i < currentDeltas_.size(); ++i) {
            QL_REQUIRE(putVols_i[i] > 0.0,
                       "BlackVolatilitySurfaceBFRR: negative front-extrapolated put vol " << putVols_i[i]);
            QL_REQUIRE(callVols_i[i] > 0.0,
                       "BlackVolatilitySurfaceBFRR: negative front-extrapolated call vol " << callVols_i[i]);
        }
    } else if (index_m == Null<Size>()) {
        // extrapolate before first expiry
        atmVol_i = atmVol_p;
        putVols_i = putVols_p;
        callVols_i = callVols_p;
        QL_REQUIRE(atmVol_i > 0.0, "BlackVolatilitySurfaceBFRR: negative back-extrapolated atm vol " << atmVol_i);
        for (Size i = 0; i < currentDeltas_.size(); ++i) {
            QL_REQUIRE(putVols_i[i] > 0.0,
                       "BlackVolatilitySurfaceBFRR: negative back-extrapolated put vol " << putVols_i[i]);
            QL_REQUIRE(callVols_i[i] > 0.0,
                       "BlackVolatilitySurfaceBFRR: negative back-extrapolated call vol " << callVols_i[i]);
        }
    } else {
        // interpolate between two expiries
        Real a = (t - expiryTimes_[index_m]) / (expiryTimes_[index_p] - expiryTimes_[index_m]);
        atmVol_i = (1.0 - a) * atmVol_m + a * atmVol_p;
        QL_REQUIRE(atmVol_i > 0.0, "BlackVolatilitySurfaceBFRR: negative atm vol "
                                       << atmVol_i << " = " << (1.0 - a) << " * " << atmVol_m << " + " << a << " * "
                                       << atmVol_p);
        for (Size i = 0; i < currentDeltas_.size(); ++i) {
            putVols_i.push_back((1.0 - a) * putVols_m[i] + a * putVols_p[i]);
            callVols_i.push_back((1.0 - a) * callVols_m[i] + a * callVols_p[i]);
            QL_REQUIRE(putVols_i.back() > 0.0, "BlackVolatilitySurfaceBFRR: negative put vol for delta="
                                                   << currentDeltas_[i] << ", " << putVols_i.back() << " = "
                                                   << (1.0 - a) << " * " << putVols_m[i] << " + " << a << " * "
                                                   << putVols_p[i]);
            QL_REQUIRE(callVols_i.back() > 0.0, "BlackVolatilitySurfaceBFRR: negative call vol for delta="
                                                    << currentDeltas_[i] << ", " << callVols_i.back() << " = "
                                                    << (1.0 - a) << " * " << callVols_m[i] << " + " << a << " * "
                                                    << callVols_p[i]);
        }
    }

    /* build a new smile using the interpolated vols and artificial conventions
       (querying the dom / for TS at t + settlLag_ is not entirely correct, because
        - of possibly different dcs in the curves and the vol ts and
        - because settLag_ is the time from today
          to today's settlement date,
        but the best we can realistically do in this context, because we don't have a date
        corresponding to t) */

    QuantLib::ext::shared_ptr<detail::SimpleDeltaInterpolatedSmile> smile;

    try {
        smile = QuantLib::ext::make_shared<detail::SimpleDeltaInterpolatedSmile>(
            spot_->value(), domesticTS_->discount(t + settlLag_) / settlDomDisc_,
            foreignTS_->discount(t + settlLag_) / settlForDisc_, t, currentDeltas_, putVols_i, callVols_i, atmVol_i,
            dt_c, at_c, smileInterpolation_);
    } catch (const std::exception& e) {
        // the interpolated smile failed to build => mark the "m" smile as a failure if available and retry, otherwise
        // market the "p" smile as a failure and retry
        Size failureIndex = index_m != Null<Size>() ? index_m : index_p;
        smileHasError_[failureIndex] = true;
        smileErrorMessage_[failureIndex] = e.what();
        return blackVolImpl(t, strike);
    }

    /* store the new smile in the cache */

    cachedInterpolatedSmiles_[t] = smile;

    /* return the volatility from the smile we just created */

    return smile->volatility(strike);
}

} // namespace QuantExt
