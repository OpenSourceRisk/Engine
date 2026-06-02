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

#include <ql/time/schedule.hpp>
#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>

using QuantLib::Date;
using QuantLib::MakeSchedule;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Size;
using QuantLib::Time;
using std::map;
using std::vector;

namespace QuantExt {

DkImpliedYoYInflationTermStructure::DkImpliedYoYInflationTermStructure(
    const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index,
    const std::optional<QuantLib::DayCounter>& simulationDayCounter)
    : YoYInflationModelTermStructure(model, index, simulationDayCounter) {}

map<Date, Real> DkImpliedYoYInflationTermStructure::yoyRates(const vector<Date>& dts, const vector<QuantLib::Period>& observationPeriods) const {

    map<Date, Real> yoys;
    map<Date, Real> yoyswaplet;
    map<Date, Real> yoydiscount;

    Calendar cal = model_->infdk(index_)->termStructure()->calendar();
    DayCounter dc = model_->infdk(index_)->termStructure()->dayCounter();
    
    // need to be unadjusted for the schedule generation, it defines the payment
    // date of the yoy coupons
    for (const auto& maturity : dts) {

        Schedule schedule = MakeSchedule()
                                .from(referenceDate())
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
            auto index = model_->infjy(index_)->inflationIndex();
            Date fixingDateStart = inflationPeriod(schedule.dates()[i - 1] - observationPeriods[i - 1], index->frequency()).first;
            Date fixingDateEnd = inflationPeriod(schedule.dates()[i] - observationPeriods[i - 1], index->frequency()).first;
            auto dc = simulationDayCounter_.value_or(dayCounter());
            if (it == yoyswaplet.end()) {
                Time tMaturity = relativeTime_ + dc.yearFraction(referenceDate(), schedule.dates()[i]);
                    // At time T we observe inflation process at T - simulationLag(), therefore add it here
                Time t2Fixing = relativeTime_ + dc.yearFraction(referenceDate(), fixingDateEnd) + simulationLag();
                
                if (fixingDateStart <= baseDate()) {
                    
                    std::pair<Real, Real> II2 =
                        model_->infdkI(index_, relativeTime_, t2Fixing, state_[0], state_[1]);
                    Real I2 = II2.first * II2.second;
                    // Compute I1, if fixingDateStart is before base date, we need historical fixing, otherwise it todays fixing
                    Real I1 = fixingDateStart < baseDate()
                                  ? index->fixing(fixingDateStart)
                                  : model_->infdkI(index_, relativeTime_, relativeTime_, state_[0], state_[1]).first;
                    discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                                                    tMaturity, state_[2]);
                    
                    swapletPrice = discount * ((I2 / I1) - 1);
                } else {
                    Time t1Fixing = relativeTime_ + dc.yearFraction(referenceDate(), fixingDateStart) + simulationLag();
                    discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                                                    tMaturity, state_[2]);
                    swapletPrice = yoySwapletRate(t1Fixing, t2Fixing);
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

        

        yoys[maturity] = yoyRate;
    }

    return modelParRatesToSwapletRates(dts, observationPeriods, yoys, yoydiscount);
;
}

Real DkImpliedYoYInflationTermStructure::yoySwapletRate(Time S, Time T) const {
    return model_->infdkYY(index_, relativeTime_, relativeTime_ + S, relativeTime_ + T, state_[0], state_[1],
                           state_[2]);
}

void DkImpliedYoYInflationTermStructure::checkState() const {
    // For DK YoY, expect the state to be three variables i.e. z_I and y_I and z_{ir}.
    QL_REQUIRE(state_.size() == 3, "DkImpliedYoYInflationTermStructure: expected state to have "
                                       << "three elements but got " << state_.size());
}

} // namespace QuantExt
