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

#include <qle/termstructures/spreadedcorrelationcurve.hpp>

#include <qle/math/flatextrapolation.hpp>

namespace QuantExt {
using namespace QuantLib;

SpreadedCorrelationCurve::SpreadedCorrelationCurve(const Handle<CorrelationTermStructure>& referenceCorrelation,
                                                   const std::vector<Time>& times,
                                                   const std::vector<Handle<Quote>>& corrSpreads,
                                                   const bool useAtmReferenceCorrsOnly)
    : CorrelationTermStructure(referenceCorrelation->dayCounter()), referenceCorrelation_(referenceCorrelation),
      times_(times), corrSpreads_(corrSpreads), useAtmReferenceCorrsOnly_(useAtmReferenceCorrsOnly) {
    QL_REQUIRE(!times_.empty(), "SpreadedCorrelationCurve: times are empty");
    QL_REQUIRE(times_.size() == corrSpreads_.size(),
               "SpreadedCorrelationCurve: size of times and quote vectors do not match");
    if (times.size() == 1) {
        times_.push_back(times_.back() + 1.0);
        corrSpreads_.push_back(corrSpreads_.back());
    }
    data_.resize(times_.size(), 1.0);
    for (auto const& q : corrSpreads_)
        registerWith(q);
    interpolation_ = QuantLib::ext::make_shared<FlatExtrapolation>(
        QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), data_.begin()));
    interpolation_->enableExtrapolation();
    registerWith(referenceCorrelation_);
}

Date SpreadedCorrelationCurve::maxDate() const { return referenceCorrelation_->maxDate(); }
const Date& SpreadedCorrelationCurve::referenceDate() const { return referenceCorrelation_->referenceDate(); }
Calendar SpreadedCorrelationCurve::calendar() const { return referenceCorrelation_->calendar(); }
Natural SpreadedCorrelationCurve::settlementDays() const { return referenceCorrelation_->settlementDays(); }
Time SpreadedCorrelationCurve::minTime() const { return referenceCorrelation_->minTime(); }

void SpreadedCorrelationCurve::update() {
    LazyObject::update();
    CorrelationTermStructure::update();
}

Real SpreadedCorrelationCurve::correlationImpl(Time t, Real strike) const {
    calculate();
    return referenceCorrelation_->correlation(t, useAtmReferenceCorrsOnly_ ? Null<Real>() : strike) +
           (*interpolation_)(t);
}

void SpreadedCorrelationCurve::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!corrSpreads_[i].empty(), "SpreadedCorrelationCurve: quote at index " << i << " is empty");
        data_[i] = corrSpreads_[i]->value();
    }
    interpolation_->update();
}

} // namespace QuantExt
