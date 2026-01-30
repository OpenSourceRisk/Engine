/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/utilities/callablebond.hpp
    \brief callable bond related utilities
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/math/comparison.hpp>
#include <ql/shared_ptr.hpp>
#include <ql/termstructure.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <vector>

namespace QuantExt {

class CallableBondNotionalAndAccrualCalculator {
public:
    CallableBondNotionalAndAccrualCalculator(const QuantLib::Date& today, const QuantLib::Real initialNotional,
                                      const QuantLib::Leg& leg,
                                      const QuantLib::ext::shared_ptr<QuantLib::TermStructure>& ts)
        : notionals_({initialNotional}) {
        for (auto const& c : leg) {
            if (c->date() <= today)
                continue;
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                if (!QuantLib::close_enough(cpn->nominal(), notionals_.back())) {
                    notionalTimes_.push_back(ts->timeFromReference(c->date()));
                    notionals_.push_back(cpn->nominal());
                }
                couponAmounts_.push_back(cpn->amount());
                couponAccrualStartTimes_.push_back(ts->timeFromReference(cpn->accrualStartDate()));
                couponAccrualEndTimes_.push_back(ts->timeFromReference(cpn->accrualEndDate()));
                couponPayTimes_.push_back(ts->timeFromReference(cpn->date()));
            }
        }
    }

    double notional(const Real t) {
        auto cn = std::upper_bound(notionalTimes_.begin(), notionalTimes_.end(), t,
                                   [](Real s, Real t) { return s < t && !QuantLib::close_enough(s, t); });
        return notionals_[std::max<Size>(std::distance(notionalTimes_.begin(), cn), 1) - 1];
    }

    double accrual(const Real t) {
        Real accruals = 0.0;
        for (Size i = 0; i < couponAmounts_.size(); ++i) {
            if (couponPayTimes_[i] > t && t > couponAccrualStartTimes_[i]) {
                accruals += (t - couponAccrualStartTimes_[i]) /
                            (couponAccrualEndTimes_[i] - couponAccrualStartTimes_[i]) * couponAmounts_[i];
            }
        }
        return accruals;
    }

private:
    std::vector<QuantLib::Time> notionalTimes_ = {0.0};
    std::vector<QuantLib::Real> notionals_;
    std::vector<QuantLib::Real> couponAmounts_;
    std::vector<QuantLib::Real> couponAccrualStartTimes_;
    std::vector<QuantLib::Real> couponAccrualEndTimes_;
    std::vector<QuantLib::Real> couponPayTimes_;
};

} // namespace QuantExt