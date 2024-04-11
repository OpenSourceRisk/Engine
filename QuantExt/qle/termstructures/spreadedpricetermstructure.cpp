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

using namespace QuantLib;

namespace QuantExt {

SpreadedPriceTermStructure::SpreadedPriceTermStructure(
    const QuantLib::Handle<PriceTermStructure>& referenceCurve, const std::vector<QuantLib::Real>& times,
    const std::vector<QuantLib::Handle<QuantLib::Quote>>& priceSpreads)
    : PriceTermStructure(referenceCurve->dayCounter()), referenceCurve_(referenceCurve), times_(times),
      priceSpreads_(priceSpreads), data_(times.size()) {
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

const Date& SpreadedPriceTermStructure::referenceDate() const { return referenceCurve_->referenceDate(); }

Calendar SpreadedPriceTermStructure::calendar() const { return referenceCurve_->calendar(); }

Natural SpreadedPriceTermStructure::settlementDays() const { return referenceCurve_->settlementDays(); }

QuantLib::Time SpreadedPriceTermStructure::minTime() const { return referenceCurve_->minTime(); }

const QuantLib::Currency& SpreadedPriceTermStructure::currency() const { return referenceCurve_->currency(); }

std::vector<QuantLib::Date> SpreadedPriceTermStructure::pillarDates() const { return referenceCurve_->pillarDates(); }

void SpreadedPriceTermStructure::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!priceSpreads_[i].empty(), "SpreadedPriceTermStructure: quote at index " << i << " is empty");
        data_[i] = priceSpreads_[i]->value();
    }
    interpolation_->update();
}

QuantLib::Real SpreadedPriceTermStructure::priceImpl(QuantLib::Time t) const {
    calculate();
    return referenceCurve_->price(t) + (*interpolation_)(t);
}

} // namespace QuantExt
