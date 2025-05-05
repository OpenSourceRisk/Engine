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

#include <qle/models/carrmadanarbitragecheck.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <numeric>
#include <sstream>
#include <algorithm>

namespace QuantExt {

CarrMadanMarginalProbability::CarrMadanMarginalProbability(const std::vector<Real>& strikes, const Real forward,
                                                           const std::vector<Real>& callPrices,
                                                           const VolatilityType volType, const Real shift)
    : strikes_(strikes), forward_(forward), callPrices_(callPrices), volType_(volType), shift_(shift) {

    // check input

    QL_REQUIRE(close_enough(shift_, 0.0) || shift_ > 0.0,
               "CarrMadanMarginalProbability: shift (" << shift_ << ") must be non-negative");

    QL_REQUIRE(strikes_.size() == callPrices_.size(), "CarrMadanMarginalProbability: strikes ("
                                                          << strikes_.size() << ") inconsistent to callPrices ("
                                                          << callPrices_.size() << ")");

    QL_REQUIRE(!strikes_.empty(), "CarrMadanMarginalProbability: input moneyness is empty");

    // build sort permutation for strikes

    std::vector<Size> perm(strikes.size());
    std::iota(perm.begin(), perm.end(), 0);
    std::sort(perm.begin(), perm.end(), [this](Size a, Size b) { return strikes_[a] < strikes_[b]; });

    // check strikes are different (and increasing for the found permutation)

    for (Size i = 1; i < strikes_.size(); ++i) {
        QL_REQUIRE(strikes_[perm[i]] > strikes_[perm[i - 1]] && !close_enough(strikes_[perm[i]], strikes_[perm[i - 1]]),
                   "CarrMadanMarginalProbability: duplicate strikes at "
                       << perm[i - 1] << ", " << perm[i] << ": " << strikes[perm[i - 1]] << ", " << strikes[perm[i]]);
    }

    QL_REQUIRE(volType_ == Normal || strikes_[perm[0]] > -shift_ || close_enough(strikes_[perm[0]], -shift_),
               "CarrMadanMarginalProbability: all input strikes (" << strikes_[perm[0]] << ") plus shift (" << shift
                                                                   << ") must be positive or zero, got "
                                                                   << strikes_[perm[0]] + shift);

    /* add strike -shift and corresponding call price (= forward + shift), if not already present
       this is only done for ShiftedLognormal vols, not for Normal */
    bool minusShiftStrikeAdded = false;
    if (volType_ == ShiftedLognormal) {
        if (!close_enough(strikes_[perm[0]], 0.0)) {
            strikes_.push_back(-shift);
            callPrices_.push_back(forward_);
            perm.insert(perm.begin(), strikes_.size() - 1);
            minusShiftStrikeAdded = true;
        } else {
            QL_REQUIRE(close_enough(callPrices_[perm[0]], forward_ + shift_),
                       "CarrMadanMarginalProbability: call price ("
                           << callPrices_.front() << ") for strike -shift (" << -shift_ << ") should match forward ("
                           << forward_ << ") + shift (" << shift_ << ") = " << forward_ + shift_);
        }
    }

    // check we have two strikes at least

    QL_REQUIRE(strikes_.size() >= 2,
               "CarrMadanMarginalProbability: at least two strikes levels required (after adding -shift)");

    // compute Q

    std::vector<Real> Q(strikes_.size() - 1);
    for (Size i = 1; i < strikes_.size(); ++i) {
        Q[i - 1] = (callPrices_[perm[i - 1]] - callPrices_[perm[i]]) / (strikes_[perm[i]] - strikes_[perm[i - 1]]);
    }

    // compute BS

    std::vector<Real> BS(strikes_.size() - 2);
    for (Size i = 1; i < strikes_.size() - 1; ++i) {
        BS[i - 1] = callPrices_[perm[i - 1]] -
                    (strikes_[perm[i + 1]] - strikes_[perm[i - 1]]) / (strikes_[perm[i + 1]] - strikes_[perm[i]]) *
                        callPrices_[perm[i]] +
                    (strikes_[perm[i]] - strikes_[perm[i - 1]]) / (strikes_[perm[i + 1]] - strikes_[perm[i]]) *
                        callPrices_[perm[i + 1]];
    }

    // perform the checks 1, 2 from the paper, and populate the set of arbitrage

    smileIsArbitrageFree_ = true;
    callSpreadArbitrage_.resize(strikes_.size());
    butterflyArbitrage_.resize(strikes_.size());

    // check 1: Q(i,j) in [0,1]

    for (Size i = 0; i < Q.size(); ++i) {
        if (Q[i] < -1.0E-10 || Q[i] > 1.0 + 1.0E-10) {
            callSpreadArbitrage_[perm[i]] = true;
            callSpreadArbitrage_[perm[i + 1]] = true;
            smileIsArbitrageFree_ = false;
        }
    }

    // check 2: BS(i,j) >= 0

    for (Size i = 0; i < BS.size(); ++i) {
        if (BS[i] < -1.0E-10) {
            butterflyArbitrage_[perm[i]] = true;
            butterflyArbitrage_[perm[i + 1]] = true;
            butterflyArbitrage_[perm[i + 2]] = true;
            smileIsArbitrageFree_ = false;
        }
    }

    // compute the density q

    q_.resize(strikes_.size());
    q_[perm[0]] = 1.0 - Q.front();
    for (Size i = 0; i < Q.size() - 1; ++i) {
        q_[perm[i + 1]] = Q[i] - Q[i + 1];
    }
    q_[perm.back()] = Q.back();

    // remove zero strike again, if not present from the start

    if (minusShiftStrikeAdded) {
        strikes_.erase(std::next(strikes_.end(), -1));
        callPrices_.erase(std::next(callPrices_.end(), -1));
        callSpreadArbitrage_.erase(std::next(callSpreadArbitrage_.begin(), perm.front()));
        butterflyArbitrage_.erase(std::next(butterflyArbitrage_.begin(), perm.front()));
        q_.erase(std::next(q_.begin(), perm.front()));
    }
}

const std::vector<Real>& CarrMadanMarginalProbability::strikes() const { return strikes_; }
Real CarrMadanMarginalProbability::forward() const { return forward_; }
const std::vector<Real>& CarrMadanMarginalProbability::callPrices() const { return callPrices_; }
VolatilityType CarrMadanMarginalProbability::volatilityType() const { return volType_; }
Real CarrMadanMarginalProbability::shift() const { return shift_; }

bool CarrMadanMarginalProbability::arbitrageFree() const { return smileIsArbitrageFree_; }

const std::vector<bool>& CarrMadanMarginalProbability::callSpreadArbitrage() const { return callSpreadArbitrage_; }
const std::vector<bool>& CarrMadanMarginalProbability::butterflyArbitrage() const { return butterflyArbitrage_; }
const std::vector<Real>& CarrMadanMarginalProbability::density() const { return q_; }

template <class CarrMadanMarginalProbabilityClass>
std::string arbitrageAsString(const CarrMadanMarginalProbabilityClass& cm) {
    std::ostringstream out;
    for (Size i = 0; i < cm.strikes().size(); ++i) {
        Size code = 0;
        if (cm.callSpreadArbitrage()[i])
            code += 1;
        if (cm.butterflyArbitrage()[i])
            code += 2;
        out << (code > 0 ? std::to_string(code) : ".");
    }
    return out.str();
}

// template instantiations
template std::string arbitrageAsString(const CarrMadanMarginalProbability& cm);
template std::string arbitrageAsString(const CarrMadanMarginalProbabilitySafeStrikes& cm);

CarrMadanSurface::CarrMadanSurface(const std::vector<Real>& times, const std::vector<Real>& moneyness, const Real spot,
                                   const std::vector<Real>& forwards, const std::vector<std::vector<Real>>& callPrices)
    : times_(times), moneyness_(moneyness), spot_(spot), forwards_(forwards), callPrices_(callPrices) {

    // checks

    QL_REQUIRE(times_.size() == callPrices_.size(),
               "CarrMadanSurface: times size (" << times_.size() << ") does not match callPrices outer vector size ("
                                                << callPrices_.size() << ")");
    QL_REQUIRE(times.size() == forwards_.size(), "CarrMadanSurface: times size (" << times_.size()
                                                                                  << ") does not match forwards size ("
                                                                                  << forwards_.size() << ")");

    QL_REQUIRE(!times_.empty(), "CarrMadanSurface: times are empty");

    for (Size i = 1; i < times_.size(); ++i) {
        QL_REQUIRE(times_[i] > times_[i - 1] && !close_enough(times_[i], times_[i - 1]),
                   "CarrMadanSurface: times not increasing at index " << (i - 1) << ", " << i << ": " << times_[i - 1]
                                                                      << ", " << times_[i]);
    }

    QL_REQUIRE(times_.front() > 0.0 || close_enough(times_.front(), 0.0),
               "CarrMadanSurface: all input times must be positive or zero, got " << times_.front());

    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(callPrices_[i].size() == moneyness_.size(), "CarrMadanSurface: callPrices at time "
                                                                   << times_[i] << "(" << callPrices_[i].size()
                                                                   << ") should match moneyness size ("
                                                                   << moneyness_.size() << ")");
    }

    // construct the time slices

    surfaceIsArbitrageFree_ = true;
    for (Size i = 0; i < times_.size(); ++i) {
        std::vector<Real> strikes(moneyness_.size());
        Real f = forwards_[i];
        std::transform(moneyness_.begin(), moneyness_.end(), strikes.begin(), [&f](Real m) { return m * f; });
        timeSlices_.push_back(CarrMadanMarginalProbability(strikes, forwards_[i], callPrices_[i]));
        surfaceIsArbitrageFree_ = surfaceIsArbitrageFree_ && timeSlices_.back().arbitrageFree();
        callSpreadArbitrage_.push_back(timeSlices_.back().callSpreadArbitrage());
        butterflyArbitrage_.push_back(timeSlices_.back().butterflyArbitrage());
    }

    // check for calendar arbitrage

    calendarArbitrage_ = std::vector<std::vector<bool>>(times_.size(), std::vector<bool>(moneyness_.size()));
    for (Size i = 0; i < moneyness_.size(); ++i) {
        for (Size j = 0; j < times_.size() - 1; ++j) {
            Real c2 = callPrices_[j + 1][i] * spot_ / forwards_[j + 1];
            Real c1 = callPrices_[j][i] * spot_ / forwards_[j];
            bool af = c2 > c1 || close_enough(c1, c2);
            if (!af) {
                calendarArbitrage_[j][i] = true;
                calendarArbitrage_[j + 1][i] = true;
                surfaceIsArbitrageFree_ = false;
            }
        }
    }
}

const std::vector<Real>& CarrMadanSurface::times() const { return times_; }
const std::vector<Real>& CarrMadanSurface::moneyness() const { return moneyness_; }
Real CarrMadanSurface::spot() const { return spot_; }
const std::vector<Real>& CarrMadanSurface::forwards() const { return forwards_; }
const std::vector<std::vector<Real>>& CarrMadanSurface::callPrices() const { return callPrices_; }

bool CarrMadanSurface::arbitrageFree() const { return surfaceIsArbitrageFree_; }
const std::vector<std::vector<bool>>& CarrMadanSurface::calendarArbitrage() const { return calendarArbitrage_; }

const std::vector<std::vector<bool>>& CarrMadanSurface::callSpreadArbitrage() const { return callSpreadArbitrage_; }
const std::vector<std::vector<bool>>& CarrMadanSurface::butterflyArbitrage() const { return butterflyArbitrage_; }

const std::vector<CarrMadanMarginalProbability>& CarrMadanSurface::timeSlices() const { return timeSlices_; }

std::string arbitrageAsString(const CarrMadanSurface& cm) {
    std::ostringstream out;
    for (Size j = 0; j < cm.times().size(); ++j) {
        for (Size i = 0; i < cm.moneyness().size(); ++i) {
            Size code = 0;
            if (cm.timeSlices()[j].callSpreadArbitrage()[i])
                code += 1;
            if (cm.timeSlices()[j].butterflyArbitrage()[i])
                code += 2;
            if (cm.calendarArbitrage()[j][i])
                code += 4;
            out << (code > 0 ? std::to_string(code) : ".");
        }
        out << "\n";
    }
    return out.str();
}

CarrMadanMarginalProbabilitySafeStrikes::CarrMadanMarginalProbabilitySafeStrikes(const std::vector<Real>& strikes,
                                                                                 const Real forward,
                                                                                 const std::vector<Real>& callPrices,
                                                                                 const VolatilityType volType,
                                                                                 const Real shift)
    : strikes_(strikes), forward_(forward), callPrices_(callPrices), volType_(volType), shift_(shift) {

    QL_REQUIRE(strikes_.size() == callPrices_.size(), "CarrMadanMarginalProbabilitySafeStrikes: strike size ("
                                                          << strikes_.size() << ") must match callPrices size ("
                                                          << callPrices_.size() << ")");

    // handle edge cases (no strikes given, invalid forward given)

    if (strikes_.empty()) {
        smileIsArbitrageFree_ = true;
        return;
    }

    if (volType == ShiftedLognormal && forward < -shift && !close_enough(forward, -shift)) {
        smileIsArbitrageFree_ = true;
        callSpreadArbitrage_ = std::vector<bool>(strikes_.size(), false);
        butterflyArbitrage_ = std::vector<bool>(strikes_.size(), false);
        q_ = std::vector<Real>(strikes_.size(), 0.0);
        return;
    }

    // identify the strikes that are not valid (i.e. < -shift)

    validStrike_.resize(strikes_.size(), true);
    for (Size i = 0; i < strikes_.size(); ++i) {
        if (volType_ == ShiftedLognormal && (strikes_[i] < -shift && !close_enough(strikes[i], -shift))) {
            validStrike_[i] = false;
        }
    }

    // build input for regular CM class

    std::vector<Real> regStrikes;
    std::vector<Real> regCallPrices;

    for (Size i = 0; i < validStrike_.size(); ++i) {
        if (validStrike_[i]) {
            regStrikes.push_back(strikes_[i]);
            regCallPrices.push_back(callPrices_[i]);
        }
    }

    // check if we have at least one strike > -shift

    bool haveNonBoundaryStrike = false;
    if (volType == Normal) {
        haveNonBoundaryStrike = true;
    } else {
        for (auto const& k : regStrikes) {
            if (volType_ == ShiftedLognormal && k > -shift && !close_enough(k, -shift)) {
                haveNonBoundaryStrike = true;
            }
        }
    }

    // if no such strike exists, the result is trivial

    if (!haveNonBoundaryStrike) {
        smileIsArbitrageFree_ = true;
        callSpreadArbitrage_ = std::vector<bool>(1, false);
        butterflyArbitrage_ = std::vector<bool>(1, false);
        q_ = std::vector<Real>(1, 1.0);
    }

    // otherwise we call the regular CM class on the regular strikes

    CarrMadanMarginalProbability cm(regStrikes, forward_, regCallPrices, volType_, shift_);

    // the results are set for the regular strikes and filled with reasonable values for invalid strike positions

    smileIsArbitrageFree_ = cm.arbitrageFree();
    callSpreadArbitrage_ = std::vector<bool>(strikes_.size(), false);
    butterflyArbitrage_ = std::vector<bool>(strikes_.size(), false);
    q_ = std::vector<Real>(strikes_.size(), 0.0);

    for (Size i = 0; i < validStrike_.size(); ++i) {
        if (validStrike_[i]) {
            callSpreadArbitrage_[i] = cm.callSpreadArbitrage()[i];
            butterflyArbitrage_[i] = cm.butterflyArbitrage()[i];
            q_[i] = cm.density()[i];
        }
    }
}

const std::vector<Real>& CarrMadanMarginalProbabilitySafeStrikes::strikes() const { return strikes_; }
Real CarrMadanMarginalProbabilitySafeStrikes::forward() const { return forward_; }
const std::vector<Real>& CarrMadanMarginalProbabilitySafeStrikes::callPrices() const { return callPrices_; }
VolatilityType CarrMadanMarginalProbabilitySafeStrikes::volatilityType() const { return volType_; }
Real CarrMadanMarginalProbabilitySafeStrikes::shift() const { return shift_; }

bool CarrMadanMarginalProbabilitySafeStrikes::arbitrageFree() const { return smileIsArbitrageFree_; }

const std::vector<bool>& CarrMadanMarginalProbabilitySafeStrikes::callSpreadArbitrage() const {
    return callSpreadArbitrage_;
}
const std::vector<bool>& CarrMadanMarginalProbabilitySafeStrikes::butterflyArbitrage() const {
    return butterflyArbitrage_;
}
const std::vector<Real>& CarrMadanMarginalProbabilitySafeStrikes::density() const { return q_; }

} // namespace QuantExt
