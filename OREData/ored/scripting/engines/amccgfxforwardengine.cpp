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

#include <ored/scripting/engines/amccgfxforwardengine.hpp>

#include <ql/cashflows/simplecashflow.hpp>

namespace ore {
namespace data {

void AmcCgFxForwardEngine::buildComputationGraph(const bool stickyCloseOutDateRun,
                                                 const bool reevaluateExerciseInStickyCloseOutDateRun) const {
    AmcCgBaseEngine::buildComputationGraph(stickyCloseOutDateRun, reevaluateExerciseInStickyCloseOutDateRun);
}

void AmcCgFxForwardEngine::calculate() const {
    QuantLib::Leg foreignLeg{QuantLib::ext::make_shared<QuantLib::SimpleCashFlow>(arguments_.nominal1, arguments_.payDate)};
    QuantLib::Leg domesticLeg{QuantLib::ext::make_shared<QuantLib::SimpleCashFlow>(arguments_.nominal2, arguments_.payDate)};

    leg_ = {foreignLeg, domesticLeg};
    currency_ = {forCcy_, domCcy_};
    payer_ = {false, true};
    exercise_ = nullptr;

    AmcCgBaseEngine::calculate();
}

} // namespace data
} // namespace ore
