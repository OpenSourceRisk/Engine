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

#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

SpreadedDiscountCurve::SpreadedDiscountCurve(const Handle<YieldTermStructure>& referenceCurve,
                                             const std::vector<Time>& times, const std::vector<Handle<Quote>>& quotes,
                                             const Interpolation interpolation, const Extrapolation extrapolation,
                                             const YieldCurveRollDown yieldCurveRollDown)
    : YieldTermStructure(0, !referenceCurve->calendar().empty() ? referenceCurve->calendar() : NullCalendar(),
                         referenceCurve->dayCounter()),
      referenceCurve_(referenceCurve), times_(times), quotes_(quotes), interpolation_(interpolation),
      extrapolation_(extrapolation), yieldCurveRollDown_(yieldCurveRollDown), data_(times_.size(), 1.0) {
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

void SpreadedDiscountCurve::performCalculations() const {

    if(!bases_.empty() && basesReferenceDate_ != referenceDate()) {
        updateBasesOffsets();
    }

    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!quotes_[i].empty(), "SpreadedDiscountCurve: quote at index " << i << " is empty");
        data_[i] = quotes_[i]->value();
        for (Size j = 0; j < bases_.size(); ++j) {
            data_[i] *= std::pow(bases_[j]->discount(times_[i]) / basesOffset_[j][i], multiplier_[j]);
        }
        QL_REQUIRE(data_[i] > 0, "SpreadedDiscountCurve: invalid value " << data_[i] << " at index " << i);
    }
    if (interpolation_ == Interpolation::linearZero) {
        for (Size i = 0; i < times_.size(); ++i) {
            data_[i] = -std::log(data_[std::max<Size>(i, 1)]) / times_[std::max<Size>(i, 1)];
        }
    }
    dataInterpolation_->update();
}

DiscountFactor SpreadedDiscountCurve::getDiscount(Time t, bool includeSpread) const {
    calculate();

    DiscountFactor refDf;
    if (referenceDate() == referenceCurve_->referenceDate()) {
        refDf = referenceCurve_->discount(t);
    } else {
        if (yieldCurveRollDown_ == YieldCurveRollDown::ConstantDiscounts) {
            refDf = referenceCurve_->discount(t);
        } else if (yieldCurveRollDown_ == YieldCurveRollDown::ForwardForward) {
            Time t0 = referenceCurve_->timeFromReference(referenceDate());
            refDf = referenceCurve_->discount(t + t0) / referenceCurve_->discount(t0);
        } else {
            QL_FAIL("SpreadedDiscountCurve::getDiscount(): yield curve rolldown not handled, internal error.");
        }
    }

    Time tMax = this->times_.back();
    if (t <= tMax) {
        Real tmp = includeSpread ? (*dataInterpolation_)(t, true) : 1.0;
        if (interpolation_ == Interpolation::logLinear)
            return refDf * tmp;
        else
            return refDf * std::exp(-tmp * t);
    }

    DiscountFactor dMax =
        includeSpread
            ? interpolation_ == Interpolation::logLinear ? this->data_.back() : std::exp(-this->data_.back() * tMax)
            : 1.0;
    if (extrapolation_ == Extrapolation::flatFwd) {
        Rate instFwdMax = includeSpread ? -(*dataInterpolation_).derivative(tMax) / dMax : 0.0;
        return refDf * dMax * std::exp(-instFwdMax * (t - tMax));
    } else {
        return refDf * std::pow(dMax, t / tMax);
    }
}

DiscountFactor SpreadedDiscountCurve::discountImpl(Time t) const { return getDiscount(t, true); }
Real SpreadedDiscountCurve::discountWithoutSpread(Time t) const { return getDiscount(t, false); }

void SpreadedDiscountCurve::updateBasesOffsets() const {
    basesOffset_.resize(bases_.size());
    for (Size i = 0; i < bases_.size(); ++i) {
        auto c = QuantLib::ext::dynamic_pointer_cast<SpreadedDiscountCurve>(*bases_[i]);
        QL_REQUIRE(c,
                   "SpreadedDiscountCurve::updateBasesOffsets(): only SpreadedDiscountCurve is allowed as base curve.");
        basesOffset_[i].resize(times_.size());
        for (Size j = 0; j < times_.size(); ++j) {
            basesOffset_[i][j] = bases_[i].empty() ? 1.0 : c->discountWithoutSpread(times_[j]);
        }
    }
    basesReferenceDate_ = referenceDate();
}

void SpreadedDiscountCurve::makeThisCurveSpreaded(const std::vector<Handle<YieldTermStructure>>& bases,
                                                  const std::vector<double>& multiplier) {
    for (auto const& b : bases_)
        unregisterWith(b);
    bases_ = bases;
    multiplier_ = multiplier;
    QL_REQUIRE(bases_.size() == multiplier_.size(), "SpreadedDiscountCurve::makeThisCurveSpreaded(): bases size ("
                                                        << bases_.size() << ") does not match multiplier size ("
                                                        << multiplier_.size() << ")");
    for (auto const& b : bases_)
        registerWith(b);
    update();
}

} // namespace QuantExt
