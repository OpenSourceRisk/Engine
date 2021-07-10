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

#include <iostream>

namespace QuantExt {

SpreadedSmileSection2::SpreadedSmileSection2(const boost::shared_ptr<SmileSection>& base,
                                             const std::vector<Real>& volSpreads, const std::vector<Real>& strikes,
                                             const bool strikesRelativeToAtm, const Real atmLevel)
    : SmileSection(base->exerciseTime(), base->dayCounter(), base->volatilityType(),
                   base->volatilityType() == ShiftedLognormal ? base->shift() : 0.0),
      base_(base), volSpreads_(volSpreads), strikes_(strikes), strikesRelativeToAtm_(strikesRelativeToAtm),
      atmLevel_(atmLevel) {
    registerWith(base_);
    QL_REQUIRE(!strikes_.empty(), "SpreadedSwaptionSmileSection: strikes empty");
    QL_REQUIRE(strikes_.size() == volSpreads_.size(), "SpreadedSwaptionSmileSection: strike spreads ("
                                                          << strikes_.size() << ") inconsistent with vol spreads ("
                                                          << volSpreads_.size() << ")");
    QL_REQUIRE(!strikesRelativeToAtm_ || strikes.size() == 1 || atmLevel != Null<Real>(),
               "SpreadedSmileSection2: if strikes are relative to atm and more than one strike is given, the atm level "
               "is required");
    if (volSpreads_.size() > 1) {
        volSpreadInterpolation_ = LinearFlat().interpolate(strikes_.begin(), strikes_.end(), volSpreads_.begin());
        volSpreadInterpolation_.enableExtrapolation();
    }
}

Rate SpreadedSmileSection2::minStrike() const { return base_->minStrike(); }
Rate SpreadedSmileSection2::maxStrike() const { return base_->maxStrike(); }
Rate SpreadedSmileSection2::atmLevel() const { return atmLevel_ != Null<Real>() ? atmLevel_ : base_->atmLevel(); }

Volatility SpreadedSmileSection2::volatilityImpl(Rate strike) const {
    if (volSpreads_.size() == 1)
        return base_->volatility(strike) + volSpreads_.front();
    else if (strikesRelativeToAtm_) {
        Real f = atmLevel();
        QL_REQUIRE(f != Null<Real>(), "SpreadedSmileSection2: atm level required");
        return base_->volatility(strike) + volSpreadInterpolation_(strike - f);
    } else {
        return base_->volatility(strike) + volSpreadInterpolation_(strike);
    }
}

} // namespace QuantExt
