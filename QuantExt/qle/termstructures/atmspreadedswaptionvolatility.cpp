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

#include <qle/termstructures/atmspreadedswaptionvolatility.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>

namespace QuantExt {

AtmSpreadedSmileSection::AtmSpreadedSmileSection(const boost::shared_ptr<SmileSection>& base, const Real spread)
    : SmileSection(base->exerciseTime(), base->dayCounter(), base->volatilityType(),
                   base->volatilityType() == ShiftedLognormal ? base_->shift() : 0.0),
      base_(base), spread_(spread) {}

Rate AtmSpreadedSmileSection::minStrike() const { return base_->minStrike(); }
Rate AtmSpreadedSmileSection::maxStrike() const { return base_->maxStrike(); }
Rate AtmSpreadedSmileSection::atmLevel() const { return base_->atmLevel(); }
Volatility AtmSpreadedSmileSection::volatilityImpl(Rate strike) const { return base_->volatility(strike) + spread_; }

AtmSpreadedSwaptionVolatility::AtmSpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& base,
                                                             const std::vector<Period>& optionTenors,
                                                             const std::vector<Period>& swapTenors,
                                                             const std::vector<std::vector<Handle<Quote>>>& spreads)
    : SwaptionVolatilityDiscrete(optionTenors, swapTenors, 0, base->calendar(), base->businessDayConvention(),
                                 base->dayCounter()),
      base_(base), optionTenors_(optionTenors), swapTenors_(swapTenors), spreads_(spreads) {
    enableExtrapolation(base_->allowsExtrapolation());
    registerWith(base_);
    for (auto const& s : spreads_)
        for (auto const& q : s)
            registerWith(q);
    QL_REQUIRE(!spreads_.empty(), "AtmSpreadedSwaptionVolatility: no spreads given");
    spreadValues_ = Matrix(spreads.size(), spreads.front().size());
}

DayCounter AtmSpreadedSwaptionVolatility::dayCounter() const { return base_->dayCounter(); }
Date AtmSpreadedSwaptionVolatility::maxDate() const { return base_->maxDate(); }
Time AtmSpreadedSwaptionVolatility::maxTime() const { return base_->maxTime(); }
const Date& AtmSpreadedSwaptionVolatility::referenceDate() const { return base_->referenceDate(); }
Calendar AtmSpreadedSwaptionVolatility::calendar() const { return base_->calendar(); }
Natural AtmSpreadedSwaptionVolatility::settlementDays() const { return base_->settlementDays(); }
Rate AtmSpreadedSwaptionVolatility::minStrike() const { return base_->minStrike(); }
Rate AtmSpreadedSwaptionVolatility::maxStrike() const { return base_->maxStrike(); }
const Period& AtmSpreadedSwaptionVolatility::maxSwapTenor() const { return base_->maxSwapTenor(); }
VolatilityType AtmSpreadedSwaptionVolatility::volatilityType() const { return base_->volatilityType(); }
void AtmSpreadedSwaptionVolatility::deepUpdate() {
    base_->update();
    update();
}
const Handle<SwaptionVolatilityStructure>& AtmSpreadedSwaptionVolatility::baseVol() { return base_; }
boost::shared_ptr<SmileSection> AtmSpreadedSwaptionVolatility::smileSectionImpl(Time optionTime,
                                                                                Time swapLength) const {
    calculate();
    return boost::make_shared<AtmSpreadedSmileSection>(base_->smileSection(optionTime, swapLength),
                                                       spread_(optionTime, swapLength));
}

Volatility AtmSpreadedSwaptionVolatility::volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
    calculate();
    return base_->volatility(optionTime, swapLength, strike) + spread_(optionTime, swapLength);
}

void AtmSpreadedSwaptionVolatility::performCalculations() const {
    SwaptionVolatilityDiscrete::performCalculations();
    for (Size i = 0; i < spreads_.size(); ++i) {
        for (Size j = 0; j < spreads_.front().size(); ++j) {
            spreadValues_[i][j] = spreads_[i][j]->value();
        }
    }
    spread_ = FlatExtrapolator2D(ext::make_shared<BilinearInterpolation>(
        swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(), optionTimes_.end(), spreadValues_));
    spread_.enableExtrapolation();
}

} // namespace QuantExt
