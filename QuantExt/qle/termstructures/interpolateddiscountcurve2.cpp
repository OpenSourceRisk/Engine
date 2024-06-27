/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file interpolateddiscountcurve2.hpp
    \brief interpolated discount term structure
    \ingroup termstructures
*/

#include <qle/termstructures/interpolateddiscountcurve2.hpp>

namespace QuantExt {

using namespace QuantLib;

InterpolatedDiscountCurve2::InterpolatedDiscountCurve2(const std::vector<Time>& times,
                                                       const std::vector<Handle<Quote>>& quotes, const DayCounter& dc,
                                                       const Interpolation interpolation,
                                                       const Extrapolation extrapolation)
    : YieldTermStructure(dc), times_(times), quotes_(quotes), interpolation_(interpolation),
      extrapolation_(extrapolation), data_(times_.size(), 1.0), today_(Settings::instance().evaluationDate()) {
    for (Size i = 0; i < quotes.size(); ++i) {
        QL_REQUIRE(times_.size() > 1, "at least two times required");
        QL_REQUIRE(times_.size() == quotes.size(), "size of time and quote vectors do not match");
        QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]);
        QL_REQUIRE(!quotes[i].empty(), "quote at index " << i << " is empty");
        registerWith(quotes_[i]);
    }
    if (interpolation_ == Interpolation::logLinear) {
        dataInterpolation_ =
            QuantLib::ext::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    } else {
        dataInterpolation_ =
            QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    }
    registerWith(Settings::instance().evaluationDate());
}

InterpolatedDiscountCurve2::InterpolatedDiscountCurve2(const std::vector<Date>& dates,
                                                       const std::vector<Handle<Quote>>& quotes, const DayCounter& dc,
                                                       const Interpolation interpolation,
                                                       const Extrapolation extrapolation)
    : YieldTermStructure(dc), times_(dates.size(), 0.0), quotes_(quotes), interpolation_(interpolation),
      extrapolation_(extrapolation), data_(dates.size(), 1.0), today_(Settings::instance().evaluationDate()) {
    for (Size i = 0; i < dates.size(); ++i)
        times_[i] = dc.yearFraction(today_, dates[i]);
    for (Size i = 0; i < quotes.size(); ++i) {
        QL_REQUIRE(times_.size() > 1, "at least two times required");
        QL_REQUIRE(times_.size() == quotes.size(), "size of time and quote vectors do not match");
        QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]);
        QL_REQUIRE(!quotes[i].empty(), "quote at index " << i << " is empty");
        registerWith(quotes_[i]);
    }
    if (interpolation_ == Interpolation::logLinear) {
        dataInterpolation_ =
            QuantLib::ext::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    } else {
        dataInterpolation_ =
            QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    }
    registerWith(Settings::instance().evaluationDate());
}

void InterpolatedDiscountCurve2::update() {
    LazyObject::update();
    TermStructure::update();
}

const Date& InterpolatedDiscountCurve2::referenceDate() const {
    calculate();
    return today_;
}

void InterpolatedDiscountCurve2::performCalculations() const {
    today_ = Settings::instance().evaluationDate();
    for (Size i = 0; i < times_.size(); ++i) {
        data_[i] = quotes_[i]->value();
        QL_REQUIRE(data_[i] > 0, "InterpolatedDiscountCurve2: invalid value " << data_[i] << " at index " << i);
        if (!base_.empty()) {
            data_[i] *= base_->discount(times_[i]) / baseOffset_[i];
        }
    }
    if (interpolation_ == Interpolation::linearZero) {
        for (Size i = 0; i < times_.size(); ++i) {
            data_[i] = -std::log(data_[std::max<Size>(i, 1)]) / times_[std::max<Size>(i, 1)];
        }
    }
    dataInterpolation_->update();
}

DiscountFactor InterpolatedDiscountCurve2::discountImpl(Time t) const {
    calculate();
    if (t <= this->times_.back()) {
        Real tmp = (*dataInterpolation_)(t, true);
        if (interpolation_ == Interpolation::logLinear)
            return tmp;
        else
            return std::exp(-tmp * t);
    }
    Time tMax = this->times_.back();
    DiscountFactor dMax =
        interpolation_ == Interpolation::logLinear ? this->data_.back() : std::exp(-this->data_.back() * tMax);
    if (extrapolation_ == Extrapolation::flatFwd) {
        Rate instFwdMax = -(*dataInterpolation_).derivative(tMax) / dMax;
        return dMax * std::exp(-instFwdMax * (t - tMax));
    } else {
        return std::pow(dMax, t / tMax);
    }
}

void InterpolatedDiscountCurve2::makeThisCurveSpreaded(const Handle<YieldTermStructure>& base) {
    base_ = base;
    baseOffset_.resize(times_.size());
    if (base_.empty()) {
        std::fill(baseOffset_.begin(), baseOffset_.end(), 1.0);
    } else {
        registerWith(base_);
        for (Size i = 0; i < times_.size(); ++i) {
            baseOffset_[i] = base_->discount(times_[i]);
        }
    }
    update();
}

} // namespace QuantExt
