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

FxVolatilityTimeWeighting::FxVolatilityTimeWeighting(const FxVolatilityTimeWeighting& w)
    : FxVolatilityTimeWeighting(w.asof(), w.dayCounter(), w.weekdayWeights(), w.tradingCenters(), w.events()) {}

FxVolatilityTimeWeighting::FxVolatilityTimeWeighting(const Date& asof, const DayCounter& dayCounter,
                                                     const std::vector<double>& weekdayWeights,
                                                     const std::vector<std::pair<Calendar, double>>& tradingCenters,
                                                     const std::map<Date, double>& events)
    : asof_(asof), dayCounter_(dayCounter), weekdayWeights_(weekdayWeights), tradingCenters_(tradingCenters),
      events_(events) {
    QL_REQUIRE(weekdayWeights_.empty() || asof_ != Date(),
               "FxVolatilityTimeWeighting: asof is required if weekdayWeights are given.");
    QL_REQUIRE(weekdayWeights_.empty() || weekdayWeights_.size() == 7,
               "FxVolatilityTimeWeighting: weekdayWeights (" << weekdayWeights_.size() << ") should have size 7");
    QL_REQUIRE(asof_ == Date() || asof_ >= Date::minDate() + 2,
               "FxVolatilityTimeWeighting: asof (" << asof_ << ") must be >= min allowed date " << Date::minDate()
                                                   << " plus 2 calendar days. The asof date is probably wrong anyhow?");
    x_.push_back(dayCounter_.yearFraction(asof, asof - 2));
    x_.push_back(dayCounter_.yearFraction(asof, asof - 1));
    y_.push_back(x_[0] - x_[1]);
    y_.push_back(0.0);
    lastSlope_ = 1.0;
    lastDateInInterpolation_ = asof - 1;
    w_ = std::make_unique<LinearInterpolation>(x_.begin(), x_.end(), y_.begin());
    w_->enableExtrapolation();
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

            weight = weekdayWeights_[wd - 1];

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

                weight = weekdayWeights_[wd - 1];
            }
        }

        if (weight != lastSlope_) {
            Date prevDate = maxDate_ - 1 * Days;
            if (lastDateInInterpolation_ != prevDate) {
                Real prevTime = dayCounter_.yearFraction(asof_, prevDate);
                y_.push_back(((prevTime - x_.back()) * lastSlope_ + y_.back()));
                x_.push_back(prevTime);
            }
            y_.push_back(((maxTime_ - x_.back()) * weight + y_.back()));
            x_.push_back(maxTime_);
            lastSlope_ = weight;
            hasDataChanged = true;
        }

    } while (maxTime_ < t);

    if (hasDataChanged) {
        w_ = std::make_unique<LinearInterpolation>(x_.begin(), x_.end(), y_.begin());
        w_->enableExtrapolation();
    }
}

Real FxVolatilityTimeWeighting::operator()(const double t) const {
    if (weekdayWeights_.empty())
        return t;
    if (t > maxTime_)
        update(t);
    return w_->operator()(t);
}

Real FxVolatilityTimeWeighting::operator()(const Date& d) const {
    QL_REQUIRE(asof_ != Date(),
               "FxVolatilityTimeWeighting::operator()(" << d << "): no asof given for date to time conversion.");
    QL_REQUIRE(!dayCounter_.empty(),
               "FxVolatilityTimeWeighting::operator()(" << d << "): no day counter given for date to time conversion.");
    return operator()(dayCounter_.yearFraction(asof_, d));
}

} // namespace QuantExt
