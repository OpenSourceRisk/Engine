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

/*! \file orea/aggregation/dimcalculator.hpp
    \brief Dynamic Initial Margin calculator base class
    \ingroup analytics
*/

#include <orea/aggregation/cvaspreadsensitivitycalculator.hpp>

namespace ore {
namespace analytics {

CVASpreadSensitivityCalculator::CVASpreadSensitivityCalculator(const string& key, const Date& asof,
                                                               const vector<Real>& epe, const vector<Date>& dates,
                                                               const Handle<DefaultProbabilityTermStructure>& dts,
                                                               const Real& recovery,
                                                               const Handle<YieldTermStructure>& yts,
                                                               const vector<Period>& shiftTenors, Real shiftSize)
    : key_(key), asof_(asof), epe_(epe), dates_(dates), dts_(dts), recovery_(recovery), yts_(yts),
      shiftTenors_(shiftTenors), shiftSize_(shiftSize) {
    shiftTimes_ = vector<Real>(shiftTenors.size(), 0.0);
    hazardRateSensitivities_ = vector<Real>(shiftTenors.size(), 0.0);
    cdsSpreadSensitivities_ = vector<Real>(shiftTenors.size(), 0.0);
    for (Size i = 0; i < shiftTenors_.size(); ++i)
        shiftTimes_[i] = dts_->timeFromReference(asof_ + shiftTenors_[i]);

    Real cvaBase = cva();
    Array input(shiftTenors_.size(), 0.0);
    for (Size i = 0; i < shiftTenors_.size(); ++i) {
        Real cvaShifted = cva(true, i);
        hazardRateSensitivities_[i] = cvaShifted - cvaBase;
        input[i] = hazardRateSensitivities_[i];
    }
    DLOG("CVA Calculator key=" << key_ << " cvaBase=" << cvaBase);

    jacobi_ = Matrix(shiftTenors_.size(), shiftTenors_.size(), 0.0);
    for (Size i = 0; i < shiftTenors_.size(); ++i) {
        Real cdsSpreadBase = fairCdsSpread(i);
        DLOG("CVA Calculator key=" << key_ << " fairSpread[" << i << "]=" << cdsSpreadBase);
        Real row = 0.0;
        for (Size j = 0; j <= i; ++j) {
            Real cdsSpread = fairCdsSpread(i, true, j);
            jacobi_[j][i] = (cdsSpread - cdsSpreadBase) / shiftSize_;
            row += jacobi_[j][i];
            DLOG("CVA Calculator key=" << key_ << " jacobi[" << j << "][" << i << "]=" << jacobi_[j][i]);
        }
        DLOG("CVA Calculator key=" << key_ << " jacobi column[" << i << "]=" << row);
    }

    Array output = inverse(jacobi_) * input;
    for (Size i = 0; i < shiftTenors_.size(); ++i)
        cdsSpreadSensitivities_[i] = output[i];
}

Real CVASpreadSensitivityCalculator::survivalProbability(Time t, bool shift, Size index) {
    if (shift == false)
        return dts_->survivalProbability(t);

    QL_REQUIRE(index < shiftTimes_.size(), "index " << index << " out of range");
    Real t2 = shiftTimes_[index];
    Real t1 = index == 0 ? 0.0 : shiftTimes_[index - 1];
    bool lastBucket = index == shiftTimes_.size() - 1 ? true : false;
    if (t < t1)
        return dts_->survivalProbability(t);
    else if (t < t2) {
        return dts_->survivalProbability(t) * std::exp(-shiftSize_ * (t - t1));
    } else { // t >= t2
        if (!lastBucket)
            return dts_->survivalProbability(t) * std::exp(-shiftSize_ * (t2 - t1));
        else
            return dts_->survivalProbability(t) * std::exp(-shiftSize_ * (t - t1));
    }
}

Real CVASpreadSensitivityCalculator::survivalProbability(Date d, bool shift, Size index) {
    Real t = dts_->timeFromReference(d);
    return survivalProbability(t, shift, index);
}

Real CVASpreadSensitivityCalculator::cva(bool shift, Size index) {
    Real sum = 0.0;
    for (Size j = 0; j < dates_.size(); ++j) {
        Date d0 = j == 0 ? asof_ : dates_[j - 1];
        Date d1 = dates_[j];
        Real s0 = survivalProbability(d0, shift, index);
        Real s1 = survivalProbability(d1, shift, index);
        Real increment = (1.0 - recovery_) * (s0 - s1) * epe_[j + 1];
        sum += increment;
    }
    DLOG("CVA Calculator key=" << key_ << " shift=" << shift << " index=" << index << " cva=" << sum);
    return sum;
}

Real CVASpreadSensitivityCalculator::fairCdsSpread(Size term, bool shift, Size index) {
    // not following the CDS2015 date rule, but a CDS with 6m periods, paying at period ends, no rebate
    QL_REQUIRE(term < shiftTimes_.size(), "term " << term << " out of range");
    Real T = shiftTimes_[term];
    Real dt = 0.5;
    Size n = Size(floor(T / dt + 0.5));
    QL_REQUIRE(fabs(T - dt * n) < 0.1 * dt, "shift term is not a multiple of 6M");
    Real enumerator = 0.0, denominator = 0.0;
    for (Size i = 1; i <= n; ++i) {
        Real t0 = dt * (i - 1);
        Real t1 = dt * i;
        Real s0 = survivalProbability(t0, shift, index);
        Real s1 = survivalProbability(t1, shift, index);
        Real dis = yts_->discount(t1);
        enumerator += (s0 - s1) * dis;
        denominator += dt * s1 * dis;
    }
    return (1.0 - recovery_) * enumerator / denominator;
}

} // namespace analytics
} // namespace ore
