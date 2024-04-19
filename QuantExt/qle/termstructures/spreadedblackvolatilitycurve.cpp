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

#include <qle/termstructures/spreadedblackvolatilitycurve.hpp>

#include <qle/math/flatextrapolation.hpp>

namespace QuantExt {

SpreadedBlackVolatilityCurve::SpreadedBlackVolatilityCurve(const Handle<BlackVolTermStructure>& referenceVol,
                                                           const std::vector<Time>& times,
                                                           const std::vector<Handle<Quote>>& volSpreads,
                                                           const bool useAtmReferenceVolsOnly)
    : BlackVolatilityTermStructure(referenceVol->businessDayConvention(), referenceVol->dayCounter()),
      referenceVol_(referenceVol), times_(times), volSpreads_(volSpreads),
      useAtmReferenceVolsOnly_(useAtmReferenceVolsOnly), data_(times.size(), 0.0) {
    registerWith(referenceVol_);
    QL_REQUIRE(times_.size() >= 2, "at least two times required");
    QL_REQUIRE(times_.size() == volSpreads_.size(), "size of time and quote vectors do not match");
    for (Size i = 0; i < volSpreads_.size(); ++i)
        registerWith(volSpreads_[i]);
    interpolation_ = QuantLib::ext::make_shared<FlatExtrapolation>(
        QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin()));
    interpolation_->enableExtrapolation();
}

Date SpreadedBlackVolatilityCurve::maxDate() const { return referenceVol_->maxDate(); }

const Date& SpreadedBlackVolatilityCurve::referenceDate() const { return referenceVol_->referenceDate(); }

Calendar SpreadedBlackVolatilityCurve::calendar() const { return referenceVol_->calendar(); }

Natural SpreadedBlackVolatilityCurve::settlementDays() const { return referenceVol_->settlementDays(); }

Real SpreadedBlackVolatilityCurve::minStrike() const { return referenceVol_->minStrike(); }

Real SpreadedBlackVolatilityCurve::maxStrike() const { return referenceVol_->maxStrike(); }

void SpreadedBlackVolatilityCurve::update() {
    LazyObject::update();
    BlackVolatilityTermStructure::update();
}

void SpreadedBlackVolatilityCurve::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!volSpreads_[i].empty(), "SpreadedBlackVolatilityCurve: empty quote at index " << (i - 1));
        data_[i] = volSpreads_[i]->value();
    }
    interpolation_->update();
}

Real SpreadedBlackVolatilityCurve::blackVolImpl(Time t, Real k) const {
    calculate();
    return referenceVol_->blackVol(t, useAtmReferenceVolsOnly_ ? Null<Real>() : k) + (*interpolation_)(t);
}

} // namespace QuantExt
