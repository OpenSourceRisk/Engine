/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ql/termstructures/volatility/smilesection.hpp>

namespace QuantExt {

class AtmAdjustedSmileSection : public QuantLib::SmileSection {
public:
    AtmAdjustedSmileSection(const boost::shared_ptr<SmileSection>& base, const Real baseAtmLevel,
                            const Real targetAtmLevel)
        : SmileSection(), base_(base), baseAtmLevel_(baseAtmLevel), targetAtmLevel_(targetAtmLevel) {}
    Real minStrike() const override { return base_->minStrike(); }
    Real maxStrike() const override { return base_->maxStrike(); }
    Real atmLevel() const override { return targetAtmLevel_; }
    const Date& exerciseDate() const override { return base_->exerciseDate(); }
    VolatilityType volatilityType() const override { return base_->volatilityType(); }
    Rate shift() const override { return base_->shift(); }
    const Date& referenceDate() const override { return base_->referenceDate(); }
    Time exerciseTime() const override { return base_->exerciseTime(); }
    const DayCounter& dayCounter() const override { return base_->dayCounter(); }

private:
    Volatility volatilityImpl(Rate strike) const override {
        // just a moneyness, but no vol adjustment, so this is only suitable for normal vols
        return base_->volatility(strike + baseAtmLevel_ - targetAtmLevel_);
    };

    boost::shared_ptr<SmileSection> base_;
    Real baseAtmLevel_;
    Real targetAtmLevel_;
};

} // namespace QuantExt
