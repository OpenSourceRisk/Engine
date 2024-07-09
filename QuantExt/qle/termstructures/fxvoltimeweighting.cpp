/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/termstructures/fxvoltimeweighting.hpp>

#include <ql/math/interpolations/linearinterpolation.hpp>

namespace QuantExt {

FxVolatilityTimeWeighting::FxVolatilityTimeWeighting(const Date& asof, const DayCounter& dayCounter,
                                                     const std::vector<double>& weekdayWeights,
                                                     const std::vector<std::pair<Calendar, double>>& tradingCenters,
                                                     const std::map<Date, double>& events)
    : asof_(asof), dayCounter_(dayCounter), weekdayWeights_(weekdayWeights), tradingCenters_(tradingCenters),
      events_(events) {
    x_.resize(1, -1.0);
    y_.resize(1, 0.0);
}

void FxVolatilityTimeWeighting::update(const double t) const {

    bool hasDataChanged = false;

    do {
        if (maxDate_ == Date())
            maxDate_ = asof_;
        else
            maxDate_ = maxDate_ += 1 * Days;

        maxTime_ = dayCounter_.yearFraction(asof_, maxDate_);

        Real weight;

        if (Weekday wd = maxDate_.weekday(); wd == Weekday::Sat || wd == Weekday::Sun) {

            // prio 1: weekend

            weight = weekdayWeights_[wd];

        } else if (auto e = events_.find(maxDate_); e != events_.end()) {

            // prio 2: event

            weight = e->second;

        } else {
            bool tradingCenterMatch = false;
            Real tmp = 1.0;
            for (auto const& [c, w] : tradingCenters_) {
                if (c.isHoliday(maxDate_)) {

                    // prio 3: trading center (one or more)

                    tmp *= w;
                    tradingCenterMatch = true;
                }
            }

            if (tradingCenterMatch) {
                weight = tmp;
            } else {

                // prio 4: weekday

                weight = weekdayWeights_[wd];
            }
        }

        if (weight != lastSlope_) {
            y_.push_back(((maxTime_ - x_.back()) * weight + y_.back()));
            x_.push_back(maxTime_);
            lastSlope_ = weight;
            hasDataChanged = true;
        }

    } while (maxTime_ < t);

    if(hasDataChanged)
        w_ = std::make_unique<LinearInterpolation>(x_.begin(), x_.end(), y_.begin());

}

Real FxVolatilityTimeWeighting::operator()(const double t) const {
    if (t > maxTime_)
        update(t);
    return w_ ? w_->operator()(t) : t;
}

Real FxVolatilityTimeWeighting::operator()(const Date& d) const {
    return operator()(dayCounter_.yearFraction(asof_, d));
}

} // namespace QuantExt
