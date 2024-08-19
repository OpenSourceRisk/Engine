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
#include <qle/termstructures/spreadedinflationcurve.hpp>

namespace QuantExt {

SpreadedZeroInflationCurve::SpreadedZeroInflationCurve(const Handle<ZeroInflationTermStructure>& referenceCurve,
                                                       const std::vector<Time>& times,
                                                       const std::vector<Handle<Quote>>& quotes)
    : ZeroInflationTermStructure(referenceCurve->dayCounter(), referenceCurve->baseRate(),
                                 referenceCurve->observationLag(), referenceCurve->frequency(),
                                 referenceCurve->seasonality()),
      referenceCurve_(referenceCurve), times_(times), quotes_(quotes), data_(times_.size(), 1.0) {
    QL_REQUIRE(times_.size() > 1, "SpreadedZeroInflationCurve: at least two times required");
    QL_REQUIRE(times_.size() == quotes.size(),
               "SpreadedZeroInflationCurve: size of time and quote vectors do not match");
    for (Size i = 0; i < quotes.size(); ++i) {
        registerWith(quotes_[i]);
    }
    interpolation_ = QuantLib::ext::make_shared<FlatExtrapolation>(
        QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin()));
    interpolation_->enableExtrapolation();
    registerWith(referenceCurve_);
}

Date SpreadedZeroInflationCurve::baseDate() const { return referenceCurve_->baseDate(); }

Date SpreadedZeroInflationCurve::maxDate() const { return referenceCurve_->maxDate(); }

void SpreadedZeroInflationCurve::update() {
    LazyObject::update();
    TermStructure::update();
}

const Date& SpreadedZeroInflationCurve::referenceDate() const { return referenceCurve_->referenceDate(); }

Calendar SpreadedZeroInflationCurve::calendar() const { return referenceCurve_->calendar(); }

Natural SpreadedZeroInflationCurve::settlementDays() const { return referenceCurve_->settlementDays(); }

void SpreadedZeroInflationCurve::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!quotes_[i].empty(), "SpreadedZeroInflationCurve: quote at index " << i << " is empty");
        data_[i] = quotes_[i]->value();
    }
    interpolation_->update();
}

Real SpreadedZeroInflationCurve::zeroRateImpl(Time t) const {
    calculate();
    return referenceCurve_->zeroRate(t) + (*interpolation_)(t);
}

SpreadedYoYInflationCurve::SpreadedYoYInflationCurve(const Handle<YoYInflationTermStructure>& referenceCurve,
                                                     const std::vector<Time>& times,
                                                     const std::vector<Handle<Quote>>& quotes)
    : YoYInflationTermStructure(referenceCurve->dayCounter(), referenceCurve->baseRate(),
                                referenceCurve->observationLag(), referenceCurve->frequency(),
                                referenceCurve->indexIsInterpolated(), referenceCurve->seasonality()),
      referenceCurve_(referenceCurve), times_(times), quotes_(quotes), data_(times_.size(), 1.0) {
    QL_REQUIRE(times_.size() > 1, "SpreadedZeroInflationCurve: at least two times required");
    QL_REQUIRE(times_.size() == quotes.size(),
               "SpreadedZeroInflationCurve: size of time and quote vectors do not match");
    for (Size i = 0; i < quotes.size(); ++i) {
        registerWith(quotes_[i]);
    }
    interpolation_ = QuantLib::ext::make_shared<FlatExtrapolation>(
        QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin()));
    interpolation_->enableExtrapolation();
    registerWith(referenceCurve_);
}

Date SpreadedYoYInflationCurve::baseDate() const { return referenceCurve_->baseDate(); }

Date SpreadedYoYInflationCurve::maxDate() const { return referenceCurve_->maxDate(); }

void SpreadedYoYInflationCurve::update() {
    LazyObject::update();
    TermStructure::update();
}

const Date& SpreadedYoYInflationCurve::referenceDate() const { return referenceCurve_->referenceDate(); }

Calendar SpreadedYoYInflationCurve::calendar() const { return referenceCurve_->calendar(); }

Natural SpreadedYoYInflationCurve::settlementDays() const { return referenceCurve_->settlementDays(); }

void SpreadedYoYInflationCurve::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!quotes_[i].empty(), "SpreadedYoYInflationCurve: quote at index " << i << " is empty");
        data_[i] = quotes_[i]->value();
    }
    interpolation_->update();
}

Real SpreadedYoYInflationCurve::yoyRateImpl(Time t) const {
    calculate();
    return referenceCurve_->yoyRate(t) + (*interpolation_)(t);
}

} // namespace QuantExt
