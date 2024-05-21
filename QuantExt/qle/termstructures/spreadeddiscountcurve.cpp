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

#include <qle/termstructures/spreadeddiscountcurve.hpp>

#include <ql/math/interpolations/loginterpolation.hpp>

namespace QuantExt {

SpreadedDiscountCurve::SpreadedDiscountCurve(const Handle<YieldTermStructure>& referenceCurve,
                                             const std::vector<Time>& times, const std::vector<Handle<Quote>>& quotes,
                                             const Interpolation interpolation, const Extrapolation extrapolation)
    : YieldTermStructure(referenceCurve->dayCounter()), referenceCurve_(referenceCurve), times_(times), quotes_(quotes),
      interpolation_(interpolation), extrapolation_(extrapolation), data_(times_.size(), 1.0) {
    QL_REQUIRE(times_.size() > 1, "SpreadedDiscountCurve: at least two times required");
    QL_REQUIRE(times_.size() == quotes.size(), "SpreadedDiscountCurve: size of time and quote vectors do not match");
    QL_REQUIRE(times_[0] == 0.0, "SpreadedDiscountCurve: first time must be 0, got " << times_[0]);
    for (Size i = 0; i < quotes.size(); ++i) {
        registerWith(quotes_[i]);
    }
    if (interpolation_ == Interpolation::logLinear) {
        dataInterpolation_ = QuantLib::ext::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    } else {
        dataInterpolation_ = QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    }
    dataInterpolation_->enableExtrapolation();
    registerWith(referenceCurve_);
}

Date SpreadedDiscountCurve::maxDate() const { return referenceCurve_->maxDate(); }

void SpreadedDiscountCurve::update() {
    LazyObject::update();
    TermStructure::update();
}

const Date& SpreadedDiscountCurve::referenceDate() const { return referenceCurve_->referenceDate(); }

Calendar SpreadedDiscountCurve::calendar() const { return referenceCurve_->calendar(); }

Natural SpreadedDiscountCurve::settlementDays() const { return referenceCurve_->settlementDays(); }

void SpreadedDiscountCurve::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!quotes_[i].empty(), "SpreadedDiscountCurve: quote at index " << i << " is empty");
        data_[i] = quotes_[i]->value();
        QL_REQUIRE(data_[i] > 0, "SpreadedDiscountCurve: invalid value " << data_[i] << " at index " << i);
    }
    if (interpolation_ == Interpolation::linearZero) {
        for (Size i = 0; i < times_.size(); ++i) {
            data_[i] = -std::log(data_[std::max<Size>(i, 1)]) / times_[std::max<Size>(i, 1)];
        }
    }
    dataInterpolation_->update();
}

DiscountFactor SpreadedDiscountCurve::discountImpl(Time t) const {
    calculate();
    Time tMax = this->times_.back();
    DiscountFactor dMax =
        interpolation_ == Interpolation::logLinear ? this->data_.back() : std::exp(-this->data_.back() * tMax);
    if (t <= this->times_.back()) {
        Real tmp = (*dataInterpolation_)(t, true);
        if (interpolation_ == Interpolation::logLinear)
            return referenceCurve_->discount(t) * tmp;
        else
            return referenceCurve_->discount(t) * std::exp(-tmp * t);
    }
    if (extrapolation_ == Extrapolation::flatFwd) {
        Rate instFwdMax = -(*dataInterpolation_).derivative(tMax) / dMax;
        return referenceCurve_->discount(t) * dMax * std::exp(-instFwdMax * (t - tMax));
    } else {
        return referenceCurve_->discount(t) * std::pow(dMax, t / tMax);
    }
}

} // namespace QuantExt
