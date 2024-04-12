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

#include <qle/termstructures/spreadedcpivolatilitysurface.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

SpreadedCPIVolatilitySurface::SpreadedCPIVolatilitySurface(const Handle<QuantExt::CPIVolatilitySurface>& baseVol,
                                                           const std::vector<Date>& optionDates,
                                                           const std::vector<Real>& strikes,
                                                           const std::vector<std::vector<Handle<Quote>>>& volSpreads)
    : QuantExt::CPIVolatilitySurface(baseVol->settlementDays(), baseVol->calendar(), baseVol->businessDayConvention(),
                                     baseVol->dayCounter(), baseVol->observationLag(), baseVol->frequency(),
                                     baseVol->indexIsInterpolated(), baseVol->capFloorStartDate(),
                                     baseVol->volatilityType(), baseVol->displacement()),
      baseVol_(baseVol), optionDates_(optionDates), strikes_(strikes), volSpreads_(volSpreads) {
    registerWith(baseVol_);
    optionTimes_.resize(optionDates_.size());
    volSpreadValues_ = Matrix(strikes_.size(), optionDates_.size());
    for (auto const& v : volSpreads)
        for (auto const& q : v)
            registerWith(q);
}

Date SpreadedCPIVolatilitySurface::maxDate() const { return baseVol_->maxDate(); }
Time SpreadedCPIVolatilitySurface::maxTime() const { return baseVol_->maxTime(); }
const Date& SpreadedCPIVolatilitySurface::referenceDate() const { return baseVol_->referenceDate(); }
Rate SpreadedCPIVolatilitySurface::minStrike() const { return baseVol_->minStrike(); }
Rate SpreadedCPIVolatilitySurface::maxStrike() const { return baseVol_->maxStrike(); }

Volatility SpreadedCPIVolatilitySurface::volatilityImpl(Time length, Rate strike) const {
    calculate();
    return baseVol_->volatility(length, strike) + volSpreadInterpolation_(length, strike);
}

void SpreadedCPIVolatilitySurface::performCalculations() const {
    for (Size i = 0; i < optionDates_.size(); ++i) {
        optionTimes_[i] = fixingTime(optionDates_[i]);
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

void SpreadedCPIVolatilitySurface::update() {
    CPIVolatilitySurface::update();
    LazyObject::update();
}

void SpreadedCPIVolatilitySurface::deepUpdate() {
    baseVol_->update();
    update();
}

QuantLib::Real SpreadedCPIVolatilitySurface::atmStrike(const QuantLib::Date& maturity,
                                                       const QuantLib::Period& obsLag) const {
    // Not relevant for constantCPIVolatiltiy;
    return baseVol_->atmStrike(maturity, obsLag);
};

} // namespace QuantExt
