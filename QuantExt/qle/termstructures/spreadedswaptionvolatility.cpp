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
#include <qle/termstructures/spreadedswaptionvolatility.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>

namespace QuantExt {

SpreadedSwaptionSmileSection::SpreadedSwaptionSmileSection(const boost::shared_ptr<SmileSection>& base,
                                                           const std::vector<Real>& strikeSpreads,
                                                           const std::vector<Real>& volSpreads, const Real atmLevel)
    : SmileSection(base->exerciseTime(), base->dayCounter(), base->volatilityType(),
                   base->volatilityType() == ShiftedLognormal ? base_->shift() : 0.0),
      base_(base), strikeSpreads_(strikeSpreads), volSpreads_(volSpreads), atmLevel_(atmLevel) {
    QL_REQUIRE(strikeSpreads_.size() == volSpreads.size(),
               "SpreadedSwaptionSmileSection: strike spreads ("
                   << strikeSpreads.size() << ") inconsistent with vol spreads (" << volSpreads.size() << ")");
    if (atmLevel_ != Null<Real>() && volSpreads_.size() > 1) {
        volSpreadInterpolation_ =
            LinearFlat().interpolate(strikeSpreads_.begin(), strikeSpreads_.end(), volSpreads_.begin());
    } else {
        QL_REQUIRE(strikeSpreads_.size() == 1 && close_enough(strikeSpreads_.front(), 0.0),
                   "SpreadedSwaptionSmileSection: if no atm level is given, exactly one strike spread 0 must be given");
    }
}

Rate SpreadedSwaptionSmileSection::minStrike() const { return base_->minStrike(); }
Rate SpreadedSwaptionSmileSection::maxStrike() const { return base_->maxStrike(); }
Rate SpreadedSwaptionSmileSection::atmLevel() const {
    return atmLevel_ != Null<Real>() ? atmLevel_ : base_->atmLevel();
}

Volatility SpreadedSwaptionSmileSection::volatilityImpl(Rate strike) const {
    if (atmLevel_ == Null<Real>() || volSpreads_.size() == 1)
        return base_->volatility(strike) + volSpreads_.front();
    else
        return base_->volatility(strike) + volSpreadInterpolation_(strike - atmLevel_);
}

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
    QL_REQUIRE(swapIndexBase != nullptr || strikeSpreads.size() == 1 && close_enough(strikeSpreads.front(), 0.0),
               "SpreadedSwaptionVolatility: if no swapIndexBase is given, exactly one strike spread = 0 is allowed");
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
    atmLevelValues_ = Matrix(optionTenors.size(), swapTenors.size());
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
    std::vector<Real> volSpreads(strikeSpreads_.size());
    for (Size k = 0; k < volSpreads.size(); ++k) {
        volSpreads[k] = volSpreadInterpolation_[k](swapLength, optionTime);
    }
    return boost::make_shared<SpreadedSwaptionSmileSection>(
        base_->smileSection(optionTime, swapLength), strikeSpreads_, volSpreads,
        swapIndexBase_ == nullptr ? Null<Real>() : atmLevelInterpolation_(swapLength, optionTime));
}

Volatility SpreadedSwaptionVolatility::volatilityImpl(Time optionTime, Time swapLength, Rate strike) const {
    return smileSectionImpl(optionTime, swapLength)->volatility(strike);
}

void SpreadedSwaptionVolatility::performCalculations() const {
    SwaptionVolatilityDiscrete::performCalculations();
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        for (Size j = 0; j < swapTenors_.size(); ++j) {
            Real atm = Null<Real>();
            if (swapIndexBase_ == nullptr) {
                if (swapTenors_[j] > shortSwapIndexBase_->tenor()) {
                    atm = swapIndexBase_->clone(swapTenors_[j])->fixing(optionDateFromTenor(optionTenors_[i]));
                } else {
                    atm = shortSwapIndexBase_->clone(swapTenors_[j])->fixing(optionDateFromTenor(optionTenors_[i]));
                }
            } else {
                atmLevelValues_[i][j] = atm;
            }
        }
    }
    atmLevelInterpolation_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(), optionTimes_.end(), atmLevelValues_));
    for (Size k = 0; k < strikeSpreads_.size(); ++k) {
        for (Size i = 0; i < optionTenors_.size(); ++i) {
            for (Size j = 0; j < swapTenors_.size(); ++j) {
                Size index = i * swapTenors_.size() + j;
                volSpreadValues_[k](i, j) = volSpreads_[index][k]->value();
            }
        }
        volSpreadInterpolation_[k] = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
            swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(), optionTimes_.end(), volSpreadValues_[k]));
        volSpreadInterpolation_[k].enableExtrapolation();
    }
}

} // namespace QuantExt
