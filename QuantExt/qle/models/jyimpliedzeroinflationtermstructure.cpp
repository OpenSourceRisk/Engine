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

#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/utilities/inflation.hpp>
#include <qle/utilities/time.hpp>

using QuantLib::Date;
using QuantLib::Size;
using QuantLib::Time;

namespace QuantExt {

JyImpliedZeroInflationTermStructure::JyImpliedZeroInflationTermStructure(
    const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index,
    const std::optional<QuantLib::DayCounter>& simulationDayCounter)
    : ZeroInflationModelTermStructure(model, index, simulationDayCounter) {}

Real JyImpliedZeroInflationTermStructure::zeroRateImpl(Time t) const {
    // Return the desired z(S) = \left( \frac{P_r(S, T)}{P_n(S, T)} \right)^{\frac{1}{t}} - 1

    auto irDayCounter =
        model_->irlgm1f(model_->ccyIndex(model_->infjy(index_)->currency()))->termStructure()->dayCounter(); 
    auto tDate = lowerDate(t, referenceDate_, simulationDayCounter_.value_or(irDayCounter));  
    auto tau = dayCounter().yearFraction(baseDate(), tDate);
    return std::pow(indexGrowth(t), 1 / tau) - 1;
}

std::pair<QuantLib::Real, QuantLib::Real> JyImpliedZeroInflationTermStructure::indexGrowth(Time t) const {
    std::cout << "JyImpliedZeroInflationTermStructure::inflationGrowthImpl called with t=" << t << std::endl;
    QL_REQUIRE(t >= 0.0, "JyImpliedZeroInflationTermStructure::inflationGrowthImpl: negative time (" << t
                                                                                                 << ") given");
    // Zero rate is calculated from the relation: P_n(S, T) (1 + z(S))^t = P_r(S, T)
    // Here, S in the relation is given by relativeTime_ and T := S + t.
    // ratio holds \frac{P_r(S, T)}{P_n(S, T)}.
    std::cout << "JyImpliedZeroInflationTermStructure::inflationGrowthImpl: relativeTime_ = " << relativeTime_ << std::endl;
    auto S = relativeTime_;
    // simulate lag is needed to ensure that the growth is calculated for the correct inflation observation date.
    // at time t, we effectivly observe the inflation at time t - simulationLag, so we need to add the simulation lag to
    // get the correct T.
    auto T = relativeTime_ + t + simulationLag(simulationDayCounter_);
    auto ratio = inflationGrowth(model_, index_, S, T, state_[2], state_[0], true);
    return std::make_pair(std::exp(state_[1]), ratio);
}

void JyImpliedZeroInflationTermStructure::checkState() const {
    // For JY, expect the state to be three variables i.e. z_I, c_I and z_{ir}.
    QL_REQUIRE(state_.size() == 3, "JyImpliedZeroInflationTermStructure: expected state to have " <<
        "three elements but got " << state_.size());
}

Real inflationGrowth(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index, Time S, Time T, Real irState,
                     Real rrState, bool indexIsInterpolated, std::optional<QuantLib::DayCounter> simulationDayCounter) {
    std::cout << "inflationGrowth called with index=" << index << ", S=" << S << ", T=" << T << ", irState=" << irState
              << ", rrState=" << rrState << std::endl;
    QL_REQUIRE(T >= S, "inflationGrowth: end time (" << T << ") must be >= start time (" << S << ")");

    // After this step, p_n holds P_n(S, T) * P_n(0, S) / P_n(0, T)
    // = \exp \left( - \left[ H_n(T) - H_n(S) \right] z_n(S) -
    //   \frac{1}{2} \left[ H^2_n(T) - H^2_n(S) \right] \zeta_n(S) \right)
    auto irIdx = model->ccyIndex(model->infjy(index)->currency());
    auto irTs = model->irlgm1f(irIdx)->termStructure();
    auto p_n = model->discountBond(irIdx, S, T, irState) * irTs->discount(S) / irTs->discount(T);

    // After this step, p_r holds P_r(S, T) * P_r(0, S) / P_r(0, T)
    // = \exp \left( - \left[ H_r(T) - H_r(S) \right] z_r(S) -
    //   \frac{1}{2} \left[ H^2_r(T) - H^2_r(S) \right] \zeta_r(S) \right)
    auto rrParam = model->infjy(index)->realRate();
    auto H_r_S = rrParam->H(S);
    auto H_r_T = rrParam->H(T);
    auto zeta_r_S = rrParam->zeta(S);
    auto p_r = std::exp(-(H_r_T - H_r_S) * rrState - 0.5 * (H_r_T * H_r_T - H_r_S * H_r_S) * zeta_r_S);

    // Now, use the original zero inflation term structure to get P_r(0, S) / P_n(0, S) and P_r(0, T) / P_n(0, T) and
    // return \frac{P_r(S, T)}{P_n(S, T)}
    const auto& zts = model->infjy(index)->realRate()->termStructure();
    return inflationGrowth(zts, T, simulationDayCounter.value_or(irTs->dayCounter()), indexIsInterpolated) /
           inflationGrowth(zts, S, simulationDayCounter.value_or(irTs->dayCounter()), indexIsInterpolated) * p_r / p_n;
}
}
