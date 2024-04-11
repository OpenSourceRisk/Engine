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

#include <qle/termstructures/spreadedoptionletvolatility2.hpp>
#include <qle/termstructures/spreadedsmilesection2.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>

namespace QuantExt {

SpreadedOptionletVolatility2::SpreadedOptionletVolatility2(const Handle<OptionletVolatilityStructure>& baseVol,
                                                           const std::vector<Date>& optionDates,
                                                           const std::vector<Real>& strikes,
                                                           const std::vector<std::vector<Handle<Quote>>>& volSpreads)
    : baseVol_(baseVol), optionDates_(optionDates), strikes_(strikes), volSpreads_(volSpreads) {
    registerWith(baseVol_);

    QL_REQUIRE(!optionDates_.empty(), "SpreadedOptionletVolatility2(): optionDates are empty");
    QL_REQUIRE(!strikes_.empty(), "SpreadedOptionletVolatility2(): strikes are empty");

    // add an artificial option date if we only have one to ensure the interpolation is working
    if (optionDates_.size() == 1) {
        optionDates_.push_back(optionDates_.back() + 1);
        volSpreads_.push_back(volSpreads_.back());
    }

    // add an artificial strike if we only have one to ensure the interpolation is working
    if (strikes_.size() == 1) {
        strikes_.push_back(strikes_.back() + 0.01);
        for (auto& v : volSpreads_)
            v.push_back(v.back());
    }

    optionTimes_.resize(optionDates_.size());
    volSpreadValues_ = Matrix(strikes_.size(), optionDates_.size());
    for (auto const& v : volSpreads_)
        for (auto const& q : v)
            registerWith(q);
}

DayCounter SpreadedOptionletVolatility2::dayCounter() const { return baseVol_->dayCounter(); }
Date SpreadedOptionletVolatility2::maxDate() const { return baseVol_->maxDate(); }
Time SpreadedOptionletVolatility2::maxTime() const { return baseVol_->maxTime(); }
const Date& SpreadedOptionletVolatility2::referenceDate() const { return baseVol_->referenceDate(); }
Calendar SpreadedOptionletVolatility2::calendar() const { return baseVol_->calendar(); }
Natural SpreadedOptionletVolatility2::settlementDays() const { return baseVol_->settlementDays(); }
BusinessDayConvention SpreadedOptionletVolatility2::businessDayConvention() const {
    return baseVol_->businessDayConvention();
}
Rate SpreadedOptionletVolatility2::minStrike() const { return baseVol_->minStrike(); }
Rate SpreadedOptionletVolatility2::maxStrike() const { return baseVol_->maxStrike(); }
VolatilityType SpreadedOptionletVolatility2::volatilityType() const { return baseVol_->volatilityType(); }
Real SpreadedOptionletVolatility2::displacement() const { return baseVol_->displacement(); }

QuantLib::ext::shared_ptr<SmileSection> SpreadedOptionletVolatility2::smileSectionImpl(Time optionTime) const {
    calculate();
    std::vector<Real> volSpreads(strikes_.size());
    for (Size k = 0; k < strikes_.size(); ++k) {
        volSpreads[k] = volSpreadInterpolation_(optionTime, strikes_[k]);
    }
    return QuantLib::ext::make_shared<SpreadedSmileSection2>(baseVol_->smileSection(optionTime), volSpreads, strikes_);
}

Volatility SpreadedOptionletVolatility2::volatilityImpl(Time optionTime, Rate strike) const {
    return smileSectionImpl(optionTime)->volatility(strike);
}

void SpreadedOptionletVolatility2::performCalculations() const {
    for (Size i = 0; i < optionDates_.size(); ++i)
        optionTimes_[i] = timeFromReference(optionDates_[i]);
    for (Size k = 0; k < strikes_.size(); ++k) {
        for (Size i = 0; i < optionDates_.size(); ++i) {
            QL_REQUIRE(!volSpreads_[i][k].empty(), "SpreadedOptionletVolatility2::performCalculations(): volSpread at "
                                                       << i << ", " << k << " is empty");
            volSpreadValues_(k, i) = volSpreads_[i][k]->value();
        }
    }
    volSpreadInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        optionTimes_.begin(), optionTimes_.end(), strikes_.begin(), strikes_.end(), volSpreadValues_));
    volSpreadInterpolation_.enableExtrapolation();
}

void SpreadedOptionletVolatility2::update() {
    OptionletVolatilityStructure::update();
    LazyObject::update();
}

void SpreadedOptionletVolatility2::deepUpdate() {
    baseVol_->update();
    update();
}

} // namespace QuantExt
