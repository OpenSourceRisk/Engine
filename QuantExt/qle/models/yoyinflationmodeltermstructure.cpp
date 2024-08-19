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

#include <qle/models/yoyinflationmodeltermstructure.hpp>

using QuantLib::Array;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;
using QuantLib::Time;

namespace QuantExt {

YoYInflationModelTermStructure::YoYInflationModelTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                                               Size index, bool indexIsInterpolated)
    : YoYInflationTermStructure(
          inflationTermStructure(model, index)->dayCounter(), inflationTermStructure(model, index)->baseRate(),
          inflationTermStructure(model, index)->observationLag(), inflationTermStructure(model, index)->frequency(),
          indexIsInterpolated), // the QL constructor is not up to date, but the last argument here will go soon
      model_(model), index_(index), indexIsInterpolated_(indexIsInterpolated),
      referenceDate_(inflationTermStructure(model_, index_)->referenceDate()), relativeTime_(0.0) {
    registerWith(model_);
    update();
}

void YoYInflationModelTermStructure::update() {
    notifyObservers();
}

Date YoYInflationModelTermStructure::maxDate() const {
    // we don't care. Let the underlying classes throw exceptions if applicable
    return Date::maxDate();
}

Time YoYInflationModelTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

const Date& YoYInflationModelTermStructure::referenceDate() const {
    return referenceDate_;
}

Date YoYInflationModelTermStructure::baseDate() const {
    if (indexIsInterpolated_) {
        return referenceDate_ - observationLag_;
    } else {
        return inflationPeriod(referenceDate_ - observationLag_, frequency()).first;
    }
}

void YoYInflationModelTermStructure::referenceDate(const Date& d) {
    referenceDate_ = d;
    relativeTime_ = dayCounter().yearFraction(inflationTermStructure(model_, index_)->referenceDate(), referenceDate_);
    update();
}

void YoYInflationModelTermStructure::state(const Array& s) {
    state_ = s;
    checkState();
    notifyObservers();
}

void YoYInflationModelTermStructure::move(const Date& d, const Array& s) {
    state(s);
    referenceDate(d);
}

Real YoYInflationModelTermStructure::yoyRate(const Date& d, const Period& obsLag, bool forceLinearInterpolation,
    bool extrapolate) const {
    return yoyRates({ d }, obsLag).at(d);
}

Real YoYInflationModelTermStructure::yoyRateImpl(Time t) const {
    QL_FAIL("YoYInflationModelTermStructure::yoyRateImpl cannot be called.");
}

}
