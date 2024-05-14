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

#pragma once

#include <ql/termstructures/volatility/smilesection.hpp>

namespace QuantExt {

class AtmAdjustedSmileSection : public QuantLib::SmileSection {
public:
    AtmAdjustedSmileSection(const QuantLib::ext::shared_ptr<QuantLib::SmileSection>& base, const QuantLib::Real baseAtmLevel,
                            const QuantLib::Real targetAtmLevel)
        : SmileSection(), base_(base), baseAtmLevel_(baseAtmLevel), targetAtmLevel_(targetAtmLevel) {}
    QuantLib::Real minStrike() const override { return base_->minStrike(); }
    QuantLib::Real maxStrike() const override { return base_->maxStrike(); }
    QuantLib::Real atmLevel() const override { return targetAtmLevel_; }
    const QuantLib::Date& exerciseDate() const override { return base_->exerciseDate(); }
    QuantLib::VolatilityType volatilityType() const override { return base_->volatilityType(); }
    QuantLib::Rate shift() const override { return base_->shift(); }
    const QuantLib::Date& referenceDate() const override { return base_->referenceDate(); }
    QuantLib::Time exerciseTime() const override { return base_->exerciseTime(); }
    const QuantLib::DayCounter& dayCounter() const override { return base_->dayCounter(); }

private:
    QuantLib::Volatility volatilityImpl(QuantLib::Rate strike) const override {
        if (strike == QuantLib::Null<QuantLib::Real>()) {
            return base_->volatility(baseAtmLevel_);
        }
        // just a moneyness, but no vol adjustment, so this is only suitable for normal vols
        return base_->volatility(strike + baseAtmLevel_ - targetAtmLevel_);
    };

    QuantLib::ext::shared_ptr<QuantLib::SmileSection> base_;
    QuantLib::Real baseAtmLevel_;
    QuantLib::Real targetAtmLevel_;
};

} // namespace QuantExt
