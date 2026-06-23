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

#include <qle/termstructures/spreadedsmilesection2.hpp>

#include <qle/math/flatextrapolation.hpp>

namespace QuantExt {

SpreadedSmileSection2::SpreadedSmileSection2(const QuantLib::ext::shared_ptr<SmileSection>& base,
                                             const std::vector<Real>& volSpreads, const std::vector<Real>& strikes,
                                             const bool strikesRelativeToAtm, const Real baseAtmLevel,
                                             const Real simulatedAtmLevel, const bool stickyAbsMoney)
    : SmileSection(base->exerciseTime(), base->dayCounter(), base->volatilityType(),
                   base->volatilityType() == ShiftedLognormal ? base->shift() : 0.0),
      fwdfwd_(false), base_(base), volSpreads_(volSpreads), strikes_(strikes),
      strikesRelativeToAtm_(strikesRelativeToAtm), baseAtmLevel_(baseAtmLevel), simulatedAtmLevel_(simulatedAtmLevel),
      stickyAbsMoney_(stickyAbsMoney) {
    registerWith(base_);
    QL_REQUIRE(!strikes_.empty(), "SpreadedSmileSection2: strikes empty");
    QL_REQUIRE(strikes_.size() == volSpreads_.size(), "SpreadedSmileSection2: strike spreads ("
                                                          << strikes_.size() << ") inconsistent with vol spreads ("
                                                          << volSpreads_.size() << ")");
    if (volSpreads_.size() > 1) {
        volSpreadInterpolation_ = LinearFlat().interpolate(strikes_.begin(), strikes_.end(), volSpreads_.begin());
        volSpreadInterpolation_.enableExtrapolation();
    }
}

SpreadedSmileSection2::SpreadedSmileSection2(const QuantLib::ext::shared_ptr<SmileSection>& base,
                                             const QuantLib::ext::shared_ptr<SmileSection>& anchor,
                                             const std::vector<Real>& volSpreads, const std::vector<Real>& strikes,
                                             const bool strikesRelativeToAtm, const Real baseAtmLevel,
                                             const Real anchorBaseAtmLevel, const Real simulatedAtmLevel,
                                             const Real anchorSimulatedAtmLevel, const bool stickyAbsMoney)
    : SmileSection(base->exerciseTime() - anchor->exerciseTime(), base->dayCounter(), base->volatilityType(),
                   base->volatilityType() == ShiftedLognormal ? base->shift() : 0.0),
      fwdfwd_(true), base_(base), volSpreads_(volSpreads), strikes_(strikes),
      strikesRelativeToAtm_(strikesRelativeToAtm), baseAtmLevel_(baseAtmLevel), simulatedAtmLevel_(simulatedAtmLevel),
      stickyAbsMoney_(stickyAbsMoney), anchor_(anchor), anchorBaseAtmLevel_(anchorBaseAtmLevel),
      anchorSimulatedAtmLevel_(anchorSimulatedAtmLevel) {
    registerWith(base_);
    registerWith(anchor_);
    QL_REQUIRE(!strikes_.empty(), "SpreadedSmileSection2: strikes empty");
    QL_REQUIRE(strikes_.size() == volSpreads_.size(), "SpreadedSmileSection2: strike spreads ("
                                                          << strikes_.size() << ") inconsistent with vol spreads ("
                                                          << volSpreads_.size() << ")");
    if (volSpreads_.size() > 1) {
        volSpreadInterpolation_ = LinearFlat().interpolate(strikes_.begin(), strikes_.end(), volSpreads_.begin());
        volSpreadInterpolation_.enableExtrapolation();
    }
}

Rate SpreadedSmileSection2::minStrike() const { return base_->minStrike(); }
Rate SpreadedSmileSection2::maxStrike() const { return base_->maxStrike(); }

Rate SpreadedSmileSection2::atmLevel() const { return simulatedAtmLevel_; }

Rate SpreadedSmileSection2::getSafeAtmLevel() const {
    QL_REQUIRE(simulatedAtmLevel_ != Null<Real>(), "SpreadedSmileSection2::atmLevel(): simulatedAtmLevel_ not set.");
    return simulatedAtmLevel_;
}

Rate SpreadedSmileSection2::getSafeBaseAtmLevel() const {
    QL_REQUIRE(baseAtmLevel_ != Null<Real>() || base_->atmLevel() != Null<Real>(),
               "SpreadedSmileSection2::getSafeBaseAtmLevel(): neither baseAtmLevel_ nor base->atmLevel() provided.");
    return baseAtmLevel_ != Null<Real>() ? baseAtmLevel_ : base_->atmLevel();
}

Rate SpreadedSmileSection2::getSafeAnchorAtmLevel() const {
    QL_REQUIRE(anchorSimulatedAtmLevel_ != Null<Real>(), "SpreadedSmileSection2::atmLevel(): anchorSimulatedAtmLevel_ not set.");
    return anchorSimulatedAtmLevel_;
}

Rate SpreadedSmileSection2::getSafeAnchorBaseAtmLevel() const {
    QL_REQUIRE(anchorBaseAtmLevel_ != Null<Real>(),
               "SpreadedSmileSection2::getSafeBaseAtmLevel(): anchorBaseAtmLevel_ not provided.");
    return anchorBaseAtmLevel_ != Null<Real>() ? baseAtmLevel_ : base_->atmLevel();
}

Volatility SpreadedSmileSection2::volatilityImpl(Rate strike) const {

    // handle regular case

    if (strike == Null<Real>()) {
        strike = getSafeAtmLevel();
    }
    Real effStrike;
    if (stickyAbsMoney_) {
        effStrike = strike - (getSafeAtmLevel() - getSafeBaseAtmLevel());
    } else {
        effStrike = strike;
    }
    Real tmp;
    if (volSpreads_.size() == 1) {
        tmp= base_->volatility(effStrike) + volSpreads_.front();
    } else if (strikesRelativeToAtm_) {
        tmp= std::max(1E-8, base_->volatility(effStrike) + volSpreadInterpolation_(strike - getSafeAtmLevel()));
    } else {
        tmp= std::max(1E-8, base_->volatility(effStrike) + volSpreadInterpolation_(strike));
    }

    if(!fwdfwd_)
        return tmp;

    // handle fwd-fwd case

    Real effStrikeAnchor;
    if (stickyAbsMoney_) {
        effStrikeAnchor = strike - (getSafeAnchorAtmLevel() - getSafeAnchorBaseAtmLevel());
    } else {
        effStrikeAnchor = strike;
    }

    Real tmp2 = anchor_->volatility(effStrikeAnchor);

    return std::sqrt((tmp * tmp * base_->exerciseTime() - tmp2 * tmp2 * anchor_->exerciseTime()) / exerciseTime());
}

} // namespace QuantExt
