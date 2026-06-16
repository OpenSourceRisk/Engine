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

#include <qle/termstructures/spreadedpricetermstructure.hpp>

#include <qle/math/flatextrapolation.hpp>

#include <ql/time/calendars/nullcalendar.hpp>

using namespace QuantLib;

namespace QuantExt {

SpreadedPriceTermStructure::SpreadedPriceTermStructure(
    const QuantLib::Handle<PriceTermStructure>& referenceCurve, const std::vector<QuantLib::Real>& times,
    const std::vector<QuantLib::Handle<QuantLib::Quote>>& priceSpreads, const PriceCurveRollDown priceCurveRollDown)
    : PriceTermStructure(0, !referenceCurve->calendar().empty() ? referenceCurve->calendar() : NullCalendar(),
                         referenceCurve->dayCounter()),
      referenceCurve_(referenceCurve), times_(times), priceSpreads_(priceSpreads),
      priceCurveRollDown_(priceCurveRollDown), data_(times.size()) {
    QL_REQUIRE(times_.size() > 1, "SpreadedPriceTermStructure: at least two times required");
    QL_REQUIRE(times_.size() == priceSpreads_.size(),
               "SpreadedPriceTermStructure: size of time and quote vectors do not match");
    QL_REQUIRE(times_[0] == 0.0, "SpreadedPriceTermStructure: first time must be 0, got " << times_[0]);
    for (auto const& q : priceSpreads_)
        registerWith(q);
    interpolation_ = QuantLib::ext::make_shared<FlatExtrapolation>(
        QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin()));
    interpolation_->enableExtrapolation();
    registerWith(referenceCurve_);
}

Date SpreadedPriceTermStructure::maxDate() const { return referenceCurve_->maxDate(); }

void SpreadedPriceTermStructure::update() {
    LazyObject::update();
    TermStructure::update();
}

QuantLib::Time SpreadedPriceTermStructure::minTime() const { return referenceCurve_->minTime(); }

const QuantLib::Currency& SpreadedPriceTermStructure::currency() const { return referenceCurve_->currency(); }

std::vector<QuantLib::Date> SpreadedPriceTermStructure::pillarDates() const { return referenceCurve_->pillarDates(); }

void SpreadedPriceTermStructure::performCalculations() const {
    if (!bases_.empty() && basesReferenceDate_ != referenceDate()) {
        updateBasesOffsets();
    }
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!priceSpreads_[i].empty(), "SpreadedPriceTermStructure: quote at index " << i << " is empty");
        data_[i] = priceSpreads_[i]->value();
        for (Size j = 0; j < bases_.size(); ++j) {
            data_[i] += (bases_[j]->price(times_[i]) - basesOffset_[j][i]) * multiplier_[j];
        }
    }
    interpolation_->update();
}

QuantLib::Real SpreadedPriceTermStructure::getPrice(QuantLib::Time t, bool includeSpread) const {
    calculate();
    Real refPrice;
    if (referenceDate() == referenceCurve_->referenceDate()) {
        refPrice = referenceCurve_->price(t);
    } else {
        if (priceCurveRollDown_ == PriceCurveRollDown::Spot) {
            refPrice = referenceCurve_->price(t);
        } else if (priceCurveRollDown_ == PriceCurveRollDown::Forward) {
            Time t0 = referenceCurve_->timeFromReference(referenceDate());
            refPrice = referenceCurve_->price(t + t0);
        } else {
            QL_FAIL("SpreadedPriceTermStructure::getPrice(): yield curve rolldown not handled, internal error.");
        }
    }
    return refPrice + (*interpolation_)(t);
}

QuantLib::Real SpreadedPriceTermStructure::priceImpl(QuantLib::Time t) const { return getPrice(t, true); }

QuantLib::Real SpreadedPriceTermStructure::priceWithoutSpread(QuantLib::Time t) const { return getPrice(t, false); }

void SpreadedPriceTermStructure::updateBasesOffsets() const {
    basesOffset_.resize(bases_.size());
    for (Size i = 0; i < bases_.size(); ++i) {
        basesOffset_[i].resize(times_.size());
        auto c = QuantLib::ext::dynamic_pointer_cast<SpreadedPriceTermStructure>(*bases_[i]);
        QL_REQUIRE(
            c,
            "SpreadedDiscountCurve::updateBasesOffsets(): only SpreadedPriceTermStructure is allowed as base curve.");
        for (Size j = 0; j < times_.size(); ++j) {
            basesOffset_[i][j] = bases_[i].empty() ? 0.0 : c->priceWithoutSpread(times_[j]);
        }
    }
    basesReferenceDate_ = referenceDate();
}

void SpreadedPriceTermStructure::makeThisCurveSpreaded(const std::vector<QuantLib::Handle<PriceTermStructure>>& bases,
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
