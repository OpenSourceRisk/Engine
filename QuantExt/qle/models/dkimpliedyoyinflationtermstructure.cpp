/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>
#include <ql/time/schedule.hpp>

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::MakeSchedule;
using QuantLib::Schedule;
using QuantLib::Size;
using QuantLib::Time;
using std::map;
using std::vector;

namespace QuantExt {

DkImpliedYoYInflationTermStructure::DkImpliedYoYInflationTermStructure(
    const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index, bool indexIsInterpolated)
    : YoYInflationModelTermStructure(model, index, indexIsInterpolated) {}

map<Date, Real> DkImpliedYoYInflationTermStructure::yoyRates(const vector<Date>& dts, const Period& obsLag) const {
    
    map<Date, Real> yoys;
    map<Date, Real> yoyswaplet;
    map<Date, Real> yoydiscount;
    Period useLag = obsLag == -1 * Days ? observationLag() : obsLag;
    Calendar cal = model_->infdk(index_)->termStructure()->calendar();
    DayCounter dc = model_->infdk(index_)->termStructure()->dayCounter();
    Date maturity;

    for (Size j = 0; j < dts.size(); j++) {
        
        if (indexIsInterpolated_) {
            maturity = dts[j] - useLag;
        } else {
            maturity = inflationPeriod(dts[j] - useLag, frequency()).first;
        }

        Schedule schedule = MakeSchedule()
            .from(baseDate())
            .to(maturity)
            .withTenor(1 * Years)
            .withConvention(Unadjusted)
            .withCalendar(cal)
            .backwards();

        Real yoyLegRate = 0.0;
        Real fixedDiscounts = 0.0;
        for (Size i = 1; i < schedule.dates().size(); i++) {
            map<Date, Real>::const_iterator it = yoyswaplet.find(schedule.dates()[i]);
            Real swapletPrice, discount;
            if (it == yoyswaplet.end()) {
                if (schedule.dates()[i - 1] < baseDate()) {
                    // for the first YoY swaplet, I(T_i-1) is known, obtained from a fixing. I(T_i) comes from the model
                    // directly - I(t) * Itilde(t,T).
                    Time t1 = dayCounter().yearFraction(model_->infdk(index_)->termStructure()->baseDate(),
                        schedule.dates()[i - 1]);
                    Real I1 = model_->infdkI(index_, t1, t1, state_[0], state_[1]).first;
                    Time t2 = dc.yearFraction(baseDate(), schedule.dates()[i]);
                    std::pair<Real, Real> II2 =
                        model_->infdkI(index_, relativeTime_, relativeTime_ + t2, state_[0], state_[1]);
                    Real I2 = II2.first * II2.second;
                    discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                        relativeTime_ + t2, state_[2]);
                    swapletPrice = discount * ((I2 / I1) - 1);
                } else {
                    Time t1 = dc.yearFraction(baseDate(), schedule.dates()[i - 1]);
                    Time t2 = dc.yearFraction(baseDate(), schedule.dates()[i]);
                    discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                        relativeTime_ + t2, state_[2]);
                    swapletPrice = yoySwapletRate(t1, t2);
                }
                yoyswaplet[schedule.dates()[i]] = swapletPrice;
                yoydiscount[schedule.dates()[i]] = discount;
            } else {
                swapletPrice = yoyswaplet[schedule.dates()[i]];
                discount = yoydiscount[schedule.dates()[i]];
            }
            yoyLegRate += swapletPrice;
            fixedDiscounts += discount;
        }
        Real yoyRate = (yoyLegRate / fixedDiscounts);

        if (hasSeasonality()) {
            yoyRate = seasonality()->correctYoYRate(dts[j] - useLag, yoyRate, *this);
        }
        yoys[dts[j]] = yoyRate;
    }

    return yoys;
}

Real DkImpliedYoYInflationTermStructure::yoySwapletRate(Time S, Time T) const {
    return model_->infdkYY(index_, relativeTime_, relativeTime_ + S,
        relativeTime_ + T, state_[0], state_[1], state_[2]);
}

void DkImpliedYoYInflationTermStructure::checkState() const {
    // For DK YoY, expect the state to be three variables i.e. z_I and y_I and z_{ir}.
    QL_REQUIRE(state_.size() == 3, "DkImpliedYoYInflationTermStructure: expected state to have " <<
        "three elements but got " << state_.size());
}

}
