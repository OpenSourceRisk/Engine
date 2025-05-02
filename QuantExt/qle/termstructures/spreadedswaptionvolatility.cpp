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

#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/spreadedsmilesection2.hpp>
#include <qle/termstructures/spreadedswaptionvolatility.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>

#include <iostream>

namespace QuantExt {

SpreadedSwaptionVolatility::SpreadedSwaptionVolatility(
    const Handle<SwaptionVolatilityStructure>& base, const std::vector<Period>& optionTenors,
    const std::vector<Period>& swapTenors, const std::vector<Real>& strikeSpreads,
    const std::vector<std::vector<Handle<Quote>>>& volSpreads,
    const QuantLib::ext::shared_ptr<SwapIndex>& baseSwapIndexBase,
    const QuantLib::ext::shared_ptr<SwapIndex>& baseShortSwapIndexBase,
    const QuantLib::ext::shared_ptr<SwapIndex>& simulatedSwapIndexBase,
    const QuantLib::ext::shared_ptr<SwapIndex>& simulatedShortSwapIndexBase, const bool stickyAbsMoney)
    : SwaptionVolatilityDiscrete(optionTenors, swapTenors, 0, base->calendar(), base->businessDayConvention(),
                                 base->dayCounter()),
      base_(base), strikeSpreads_(strikeSpreads), volSpreads_(volSpreads), baseSwapIndexBase_(baseSwapIndexBase),
      baseShortSwapIndexBase_(baseShortSwapIndexBase), simulatedSwapIndexBase_(simulatedSwapIndexBase),
      simulatedShortSwapIndexBase_(simulatedShortSwapIndexBase), stickyAbsMoney_(stickyAbsMoney) {

    QL_REQUIRE(!strikeSpreads_.empty(), "SpreadedSwaptionVolatility: empty strike spreads");
    QL_REQUIRE(!optionTenors_.empty(), "SpreadedSwaptionVolatility: empty option tenors");
    QL_REQUIRE(!swapTenors_.empty(), "SpreadedSwaptionVolatility: empty swap tenors");
    QL_REQUIRE(optionTenors.size() * swapTenors.size() == volSpreads.size(),
               "SpreadedSwaptionVolatility: optionTenors (" << optionTenors.size() << ") * swapTenors ("
                                                            << swapTenors.size() << ") inconsistent with vol spreads ("
                                                            << volSpreads.size() << ")");

    QL_REQUIRE((baseSwapIndexBase_ == nullptr && baseShortSwapIndexBase_ == nullptr &&
                simulatedSwapIndexBase_ == nullptr && simulatedShortSwapIndexBase_ == nullptr) ||
                   (baseSwapIndexBase_ != nullptr && baseShortSwapIndexBase_ != nullptr &&
                    simulatedSwapIndexBase_ != nullptr && simulatedShortSwapIndexBase_ != nullptr),
               "SpreadedSwaptionVolatility: all swap index bases must be null or non-null at the same time.");

    QL_REQUIRE(volSpreads_.size() == 1 || baseSwapIndexBase_ != nullptr,
               "SpreadedSwaptionVolatility: if vol spreads size != 1 (got " << volSpreads.size()
                                                                            << "), swap index bases must be given.");

    enableExtrapolation(base_->allowsExtrapolation());
    registerWith(base_);
    if (baseSwapIndexBase_)
        registerWith(baseSwapIndexBase_);
    if (baseShortSwapIndexBase_)
        registerWith(baseShortSwapIndexBase_);
    if (simulatedSwapIndexBase_)
        registerWith(simulatedSwapIndexBase_);
    if (simulatedShortSwapIndexBase_)
        registerWith(simulatedShortSwapIndexBase_);
    for (auto const& s : volSpreads_) {
        QL_REQUIRE(s.size() == strikeSpreads_.size(), "SpreadedSwaptionVolatility: got " << strikeSpreads_.size()
                                                                                         << " strike spreads, but "
                                                                                         << s.size() << " vol spreads");
        for (auto const& q : s)
            registerWith(q);
    }
    volSpreadValues_ = std::vector<Matrix>(strikeSpreads_.size(), Matrix(optionTenors.size(), swapTenors.size(), 0.0));
    volSpreadInterpolation_ = std::vector<Interpolation2D>(strikeSpreads_.size());
}

DayCounter SpreadedSwaptionVolatility::dayCounter() const { return base_->dayCounter(); }
Date SpreadedSwaptionVolatility::maxDate() const { return base_->maxDate(); }
Time SpreadedSwaptionVolatility::maxTime() const { return base_->maxTime(); }
const Date& SpreadedSwaptionVolatility::referenceDate() const { return base_->referenceDate(); }
Calendar SpreadedSwaptionVolatility::calendar() const { return base_->calendar(); }
Natural SpreadedSwaptionVolatility::settlementDays() const { return base_->settlementDays(); }
Rate SpreadedSwaptionVolatility::minStrike() const { return base_->minStrike(); }
Rate SpreadedSwaptionVolatility::maxStrike() const { return base_->maxStrike(); }
const Period& SpreadedSwaptionVolatility::maxSwapTenor() const { return base_->maxSwapTenor(); }
VolatilityType SpreadedSwaptionVolatility::volatilityType() const { return base_->volatilityType(); }
void SpreadedSwaptionVolatility::deepUpdate() {
    base_->update();
    update();
}
const Handle<SwaptionVolatilityStructure>& SpreadedSwaptionVolatility::baseVol() { return base_; }

Real SpreadedSwaptionVolatility::getAtmLevel(const Real optionTime, const Real swapLength,
                                             const QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase,
                                             const QuantLib::ext::shared_ptr<SwapIndex> shortSwapIndexBase) const {
    Date optionDate = optionDateFromTime(optionTime);
    Rounding rounder(0);
    Period swapTenor(static_cast<Integer>(rounder(swapLength * 12.0)), Months);
    optionDate = swapTenor > shortSwapIndexBase->tenor()
                     ? swapIndexBase->fixingCalendar().adjust(optionDate, Following)
                     : shortSwapIndexBase->fixingCalendar().adjust(optionDate, Following);
    if (swapTenor > shortSwapIndexBase->tenor()) {
        return swapIndexBase->clone(swapTenor)->fixing(optionDate);
    } else {
        return shortSwapIndexBase->clone(swapTenor)->fixing(optionDate);
    }
}

QuantLib::ext::shared_ptr<SmileSection> SpreadedSwaptionVolatility::smileSectionImpl(Time optionTime,
                                                                                     Time swapLength) const {
    calculate();
    auto baseSection = base_->smileSection(optionTime, swapLength);
    Real baseAtmLevel = Null<Real>();
    Real simulatedAtmLevel = Null<Real>();
    if (baseSection->atmLevel() == Null<Real>() && baseSwapIndexBase_) {
        baseAtmLevel = getAtmLevel(optionTime, swapLength, baseSwapIndexBase_, baseShortSwapIndexBase_);
    }
    if (simulatedSwapIndexBase_ != nullptr) {
        simulatedAtmLevel = getAtmLevel(optionTime, swapLength, simulatedSwapIndexBase_, simulatedShortSwapIndexBase_);
    }
    // interpolate vol spreads
    std::vector<Real> volSpreads(strikeSpreads_.size());
    for (Size k = 0; k < volSpreads.size(); ++k) {
        volSpreads[k] = volSpreadInterpolation_[k](swapLength, optionTime);
    }
    // create smile section
    return QuantLib::ext::make_shared<SpreadedSmileSection2>(baseSection, volSpreads, strikeSpreads_, true,
                                                             baseAtmLevel, simulatedAtmLevel, stickyAbsMoney_);
}

Volatility SpreadedSwaptionVolatility::volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
    if (baseSwapIndexBase_ == nullptr) {
        // if swap index base is not given, we assume base and this svts are atm only
        return base_->volatility(optionTime, swapLength, Null<Real>());
    }
    return smileSectionImpl(optionTime, swapLength)->volatility(strike);
}

Real SpreadedSwaptionVolatility::shiftImpl(const Date& optionDate, const Period& swapTenor) const {
    return base_->shift(optionDate, swapTenor);
}

Real SpreadedSwaptionVolatility::shiftImpl(Time optionTime, Time swapLength) const {
    return base_->shift(optionTime, swapLength);
}

void SpreadedSwaptionVolatility::performCalculations() const {
    SwaptionVolatilityDiscrete::performCalculations();
    for (Size k = 0; k < strikeSpreads_.size(); ++k) {
        for (Size i = 0; i < optionTenors_.size(); ++i) {
            for (Size j = 0; j < swapTenors_.size(); ++j) {
                Size index = i * swapTenors_.size() + j;
                QL_REQUIRE(!volSpreads_[index][k].empty(), "SpreadedSwaptionVolatility: vol spread quote at index ("
                                                               << i << "," << j << "," << k << ") is empty");
                volSpreadValues_[k](i, j) = volSpreads_[index][k]->value();
            }
        }
        volSpreadInterpolation_[k] = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
            swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(), optionTimes_.end(), volSpreadValues_[k]));
        volSpreadInterpolation_[k].enableExtrapolation();
    }
}

} // namespace QuantExt
