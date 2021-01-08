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

SpreadedSwaptionVolatility::SpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& base,
                                                       const std::vector<Period>& optionTenors,
                                                       const std::vector<Period>& swapTenors,
                                                       const std::vector<Real>& strikeSpreads,
                                                       const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                                       const boost::shared_ptr<SwapIndex>& swapIndexBase,
                                                       const boost::shared_ptr<SwapIndex>& shortSwapIndexBase)
    : SwaptionVolatilityDiscrete(optionTenors, swapTenors, 0, base->calendar(), base->businessDayConvention(),
                                 base->dayCounter()),
      base_(base), strikeSpreads_(strikeSpreads), volSpreads_(volSpreads), swapIndexBase_(swapIndexBase),
      shortSwapIndexBase_(shortSwapIndexBase) {
    enableExtrapolation(base_->allowsExtrapolation());
    registerWith(base_);
    QL_REQUIRE((swapIndexBase_ == nullptr && shortSwapIndexBase_ == nullptr) ||
                   (swapIndexBase_ != nullptr && shortSwapIndexBase_ != nullptr),
               "SpreadedSwaptionVolatility: swapIndexBase and shortSwapIndexBase must be both null or non-null");
    QL_REQUIRE(!strikeSpreads_.empty(), "SpreadedSwaptionVolatility: empty strike spreads");
    QL_REQUIRE(!optionTenors_.empty(), "SpreadedSwaptionVolatility: empty option tenors");
    QL_REQUIRE(!swapTenors_.empty(), "SpreadedSwaptionVolatility: empty swap tenors");
    QL_REQUIRE(optionTenors.size() * swapTenors.size() == volSpreads.size(),
               "SpreadedSwaptionVolatility: optionTenors (" << optionTenors.size() << ") * swapTenors ("
                                                            << swapTenors.size() << ") inconsistent with vol spreads ("
                                                            << volSpreads.size() << ")");
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
boost::shared_ptr<SmileSection> SpreadedSwaptionVolatility::smileSectionImpl(Time optionTime, Time swapLength) const {
    calculate();
    auto baseSection = base_->smileSection(optionTime, swapLength);
    Real atmLevel = Null<Real>();
    if (baseSection->atmLevel() == Null<Real>() && strikeSpreads_.size() > 1) {
        // determine atm level if it is required (more than one strike spread) and the base section does not have it
        QL_REQUIRE(swapIndexBase_ != nullptr,
                   "SpreadedSwaptionVolatility::smileSectionImpl: require swapIndexBase, since the base vol smile "
                   "section does not provide an atm level and we have more than one strike spread given ("
                       << strikeSpreads_.size() << ")");
        Date optionDate = optionDateFromTime(optionTime);
        Rounding rounder(0);
        Period swapTenor(static_cast<Integer>(rounder(swapLength * 12.0)), Months);
        optionDate = swapTenor > shortSwapIndexBase_->tenor()
                         ? swapIndexBase_->fixingCalendar().adjust(optionDate, Following)
                         : shortSwapIndexBase_->fixingCalendar().adjust(optionDate, Following);
        if (swapTenor > shortSwapIndexBase_->tenor()) {
            atmLevel = swapIndexBase_->clone(swapTenor)->fixing(optionDate);
        } else {
            atmLevel = shortSwapIndexBase_->clone(swapTenor)->fixing(optionDate);
        }
    }
    // interpolate vol spreads
    std::vector<Real> volSpreads(strikeSpreads_.size());
    for (Size k = 0; k < volSpreads.size(); ++k) {
        volSpreads[k] = volSpreadInterpolation_[k](swapLength, optionTime);
    }
    // create smile section
    return boost::make_shared<SpreadedSmileSection2>(base_->smileSection(optionTime, swapLength), volSpreads,
                                                     strikeSpreads_, true, atmLevel);
}

Volatility SpreadedSwaptionVolatility::volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
    if (strike == Null<Real>()) {
        calculate();
        // support input strike null, interpret this as atm
        std::vector<Real> volSpreads(strikeSpreads_.size());
        for (Size k = 0; k < volSpreads.size(); ++k) {
            volSpreads[k] = volSpreadInterpolation_[k](swapLength, optionTime);
        }
        Real volSpread;
        if (volSpreads.size() > 1) {
            auto spreadInterpolation =
                LinearFlat().interpolate(strikeSpreads_.begin(), strikeSpreads_.end(), volSpreads.begin());
            volSpread = spreadInterpolation(0.0);
        } else {
            volSpread = volSpreads.front();
        }
        return base_->volatility(optionTime, swapLength, strike) + volSpread;
    } else {
        // if input strike != null, use smile section implementation
        return smileSectionImpl(optionTime, swapLength)->volatility(strike);
    }
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
        volSpreadInterpolation_[k] = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
            swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(), optionTimes_.end(), volSpreadValues_[k]));
        volSpreadInterpolation_[k].enableExtrapolation();
    }
}

} // namespace QuantExt
