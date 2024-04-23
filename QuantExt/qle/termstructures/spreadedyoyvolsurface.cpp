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

#include <qle/termstructures/spreadedyoyvolsurface.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>

namespace QuantExt {

SpreadedYoYVolatilitySurface::SpreadedYoYVolatilitySurface(const Handle<YoYOptionletVolatilitySurface>& baseVol,
                                                           const std::vector<Date>& optionDates,
                                                           const std::vector<Real>& strikes,
                                                           const std::vector<std::vector<Handle<Quote>>>& volSpreads)
    : YoYOptionletVolatilitySurface(baseVol->settlementDays(), baseVol->calendar(), baseVol->businessDayConvention(),
                                    baseVol->dayCounter(), baseVol->observationLag(), baseVol->frequency(),
                                    baseVol->indexIsInterpolated(), baseVol->volatilityType(), baseVol->displacement()),
      baseVol_(baseVol), optionDates_(optionDates), strikes_(strikes), volSpreads_(volSpreads) {
    registerWith(baseVol_);
    optionTimes_.resize(optionDates_.size());
    volSpreadValues_ = Matrix(strikes_.size(), optionDates_.size());
    for (auto const& v : volSpreads)
        for (auto const& q : v)
            registerWith(q);
}

Date SpreadedYoYVolatilitySurface::maxDate() const { return baseVol_->maxDate(); }
Time SpreadedYoYVolatilitySurface::maxTime() const { return baseVol_->maxTime(); }
const Date& SpreadedYoYVolatilitySurface::referenceDate() const { return baseVol_->referenceDate(); }
Rate SpreadedYoYVolatilitySurface::minStrike() const { return baseVol_->minStrike(); }
Rate SpreadedYoYVolatilitySurface::maxStrike() const { return baseVol_->maxStrike(); }

Volatility SpreadedYoYVolatilitySurface::volatilityImpl(Time length, Rate strike) const {
    calculate();
    return baseVol_->volatility(length, strike) + volSpreadInterpolation_(length, strike);
}

void SpreadedYoYVolatilitySurface::performCalculations() const {
    for (Size i = 0; i < optionDates_.size(); ++i) {
        // we can not support a custom obsLag here
        if (indexIsInterpolated())
            optionTimes_[i] = timeFromReference(optionDates_[i] - observationLag());
        else
            optionTimes_[i] = timeFromReference(inflationPeriod(optionDates_[i] - observationLag(), frequency()).first);
    }
    for (Size k = 0; k < strikes_.size(); ++k) {
        for (Size i = 0; i < optionDates_.size(); ++i) {
            volSpreadValues_(k, i) = volSpreads_[i][k]->value();
        }
    }
    volSpreadInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        optionTimes_.begin(), optionTimes_.end(), strikes_.begin(), strikes_.end(), volSpreadValues_));
    volSpreadInterpolation_.enableExtrapolation();
}

void SpreadedYoYVolatilitySurface::update() {
    YoYOptionletVolatilitySurface::update();
    LazyObject::update();
}

void SpreadedYoYVolatilitySurface::deepUpdate() {
    baseVol_->update();
    update();
}

} // namespace QuantExt
