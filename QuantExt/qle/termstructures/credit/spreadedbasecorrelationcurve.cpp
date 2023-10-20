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

#include <qle/termstructures/credit/spreadedbasecorrelationcurve.hpp>

#include <qle/math/flatextrapolation2d.hpp>

namespace QuantExt {
using namespace QuantLib;

SpreadedBaseCorrelationCurve::SpreadedBaseCorrelationCurve(const Handle<BaseCorrelationTermStructure>& baseCurve,
                                                           const std::vector<Period>& tenors,
                                                           const std::vector<double>& detachmentPoints,
                                                           const std::vector<std::vector<Handle<Quote>>>& corrSpreads,
                                                           const Date& startDate,
                                                           boost::optional<DateGeneration::Rule> rule)
    : BaseCorrelationTermStructure(baseCurve->settlementDays(), baseCurve->calendar(),
                                   baseCurve->businessDayConvention(), tenors, detachmentPoints,
                                   baseCurve->dayCounter(), startDate, rule),
      baseCurve_(baseCurve), corrSpreads_(corrSpreads), data_(detachmentPoints_.size(), tenors.size(), 0.0) {
    // Check times and detachment points

    QL_REQUIRE(!times_.empty(), "SpreadedCorrelationCurve: time points are empty");
    QL_REQUIRE(!detachmentPoints_.empty(), "SpreadedBaseCorrelationCurve: detachmentPoints are empty");

    QL_REQUIRE(corrSpreads_.size() == detachmentPoints_.size(), "Mismatch between tenors and correlation quotes");
    for (const auto& row : this->corrSpreads_) {
        QL_REQUIRE(row.size() == tenors_.size(), "Mismatch between number of detachment points and quotes");
    }

    for (auto const& row : corrSpreads_)
        for (auto const& q : row)
            registerWith(q);

    interpolation_ = BilinearFlat().interpolate(times_.begin(), times_.end(), detachmentPoints_.begin(),
                                                detachmentPoints_.end(), data_);
    interpolation_.enableExtrapolation();
    registerWith(baseCurve_);
}

void SpreadedBaseCorrelationCurve::update() {
    LazyObject::update();
    BaseCorrelationTermStructure::update();
}

Real SpreadedBaseCorrelationCurve::correlationImpl(Time t, Real detachmentPoint) const {
    calculate();
    return std::min(
        1.0 - QL_EPSILON,
        std::max(baseCurve_->correlation(t, detachmentPoint) + interpolation_(t, detachmentPoint), QL_EPSILON));
}

void SpreadedBaseCorrelationCurve::performCalculations() const {
    for (Size i = 0; i < this->detachmentPoints_.size(); ++i)
        for (Size j = 0; j < this->times_.size(); ++j)
            this->data_[i][j] = corrSpreads_[i][j]->value();
    interpolation_.update();
}

} // namespace QuantExt
